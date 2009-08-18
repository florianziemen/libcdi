#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "dmemory.h"

#include "cdi.h"
#include "cdf_int.h"
#include "stream_int.h"
#include "stream_cgribex.h"
#include "stream_cdf.h"
#include "stream_srv.h"
#include "stream_ext.h"
#include "stream_ieg.h"


void recordInitEntry(RECORD *record)
{
  (*record).position = CDI_UNDEFID;
  (*record).size     = 0;
  (*record).code     = 0;
  (*record).ilevel   = CDI_UNDEFID;
  (*record).used     = FALSE;
  (*record).varID    = CDI_UNDEFID;
  (*record).levelID  = CDI_UNDEFID;
}


int recordNewEntry(int streamID, int tsID)
{
  static char func[] = "recordNewEntry";
  int recordID = 0;
  int recordSize;
  RECORD *records;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  recordSize = streamptr->tsteps[tsID].recordSize;
  records    = streamptr->tsteps[tsID].records;
  /*
    Look for a free slot in record.
    (Create the table the first time through).
  */
  if ( ! recordSize )
    {
      int i;
      recordSize = 1;   /*  <<<<----  */
      records = (RECORD *) malloc(recordSize*sizeof(RECORD));
      if ( records == NULL )
	{
          Message(func, "recordSize = %d", recordSize);
	  SysError(func, "Allocation of RECORDTABLE failed");
	}

      for ( i = 0; i < recordSize; i++ )
	records[i].used = CDI_UNDEFID;
    }
  else
    {
      while ( recordID < recordSize )
	{
	  if ( records[recordID].used == CDI_UNDEFID ) break;
	  recordID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if ( recordID == recordSize )
    {
      int i;

      recordSize = 2*recordSize;
      records    = (RECORD *) realloc(records, recordSize*sizeof(RECORD));
      if ( records == NULL )
	{
          Message(func, "recordSize = %d", recordSize);
	  SysError(func, "Reallocation of RECORDTABLE failed");
	}
      recordID = recordSize/2;

      for ( i = recordID; i < recordSize; i++ )
	records[i].used = CDI_UNDEFID;
    }


  recordInitEntry(&records[recordID]);

  records[recordID].used = 1;

  streamptr->tsteps[tsID].recordSize = recordSize;
  streamptr->tsteps[tsID].records    = records;

  return (recordID);
}


void cdiInitRecord(int streamID)
{
  static char func[] = "cdiInitRecord";
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  streamptr->record = (Record *) malloc(sizeof(Record));

  streamptr->record->used       = 0;
  streamptr->record->nrec       = 0;
  streamptr->record->dataread   = 1;
  streamptr->record->code       = 0;
  streamptr->record->level      = 0;
  streamptr->record->date       = 0;
  streamptr->record->time       = 0;
  streamptr->record->gridID     = 0;
  streamptr->record->zaxisID    = 0;
  streamptr->record->buffer     = NULL;
  streamptr->record->buffersize = 0;
  streamptr->record->position   = 0;
  streamptr->record->varID      = 0;
  streamptr->record->levelID    = CDI_UNDEFID;
  streamptr->record->recid      = 0;
}


void streamInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "streamInqRecord";
  int rec = 0;
  int recID, tsID, rindex;
  int lindex;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  cdiDefAccesstype(streamID, TYPE_REC);

  if ( ! streamptr->record ) cdiInitRecord(streamID);

  tsID   = streamptr->curTsID;
  rindex = streamptr->tsteps[tsID].curRecID + 1;

  if ( rindex >= streamptr->tsteps[tsID].nrecs )
    Error(func, "record %d not available at timestep %d", rindex+1, tsID+1);

  recID  = streamptr->tsteps[tsID].recIDs[rindex];

  if ( recID == -1 || recID >= streamptr->tsteps[tsID].nallrecs )
    Error(func, "Internal problem! tsID = %d recID = %d", tsID, recID);

  *varID   = streamptr->tsteps[tsID].records[recID].varID;
  lindex   = streamptr->tsteps[tsID].records[recID].levelID;

  *levelID = streamptr->vars[*varID].lindex[lindex];

  if ( CDI_Debug )
    Message(func, "tsID = %d, recID = %d, varID = %d, levelID = %d\n",
	    tsID, recID, *varID, *levelID);

  streamptr->curTsID = tsID;
  streamptr->tsteps[tsID].curRecID = rindex;

  rec = recID + 1;
  /*
  filetype = streamptr->filetype;

  switch ( filetype )
    {
    case FILETYPE_GRB:
      {
        rec = grbInqRecord(streamID, varID, levelID);
	break;
      }
    case FILETYPE_SRV:
      {
        rec = srvInqRecord(streamID, varID, levelID);
	break;
      }
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
    case FILETYPE_NC4:
      {
	rec = cdfInqRecord(streamID, varID, levelID);
	break;
      }
#endif
    default:
      {
	Error(func, "%s support not compiled in!", strfiletype(filetype));
	break;
      }
    }
  */
}


void streamDefRecord(int streamID, int varID, int levelID)
{
  static char func[] = "streamDefRecord";
  int status = 0;
  int filetype;
  int code, gridID, zaxisID, level;
  int tsID;
  int vlistID;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  tsID = streamptr->curTsID;

  if ( tsID == CDI_UNDEFID )
    {
      tsID++;
      streamDefTimestep(streamID, tsID);
    }

  if ( ! streamptr->record ) cdiInitRecord(streamID);

  vlistID = streamInqVlist(streamID);
  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  code    = vlistInqVarCode(vlistID, varID);
  level   = (int) zaxisInqLevel(zaxisID, levelID);

  streamptr->record->varID    = varID;
  streamptr->record->levelID  = levelID;
  streamptr->record->code     = code;
  streamptr->record->level    = level;
  streamptr->record->date     = streamptr->tsteps[tsID].taxis.vdate;
  streamptr->record->time     = streamptr->tsteps[tsID].taxis.vtime;
  streamptr->record->gridID   = gridID;
  streamptr->record->zaxisID  = zaxisID;
  streamptr->record->prec     = vlistInqVarDatatype(vlistID, varID);

  filetype = streamptr->filetype;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        status = grbDefRecord(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBGRIB_API)
    case FILETYPE_GRB2:
      {
        status = grbDefRecord(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        status = srvDefRecord(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        status = extDefRecord(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        status = iegDefRecord(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
    case FILETYPE_NC4:
      {
	if ( streamptr->accessmode == 0 ) cdfEndDef(streamID);
	status = cdfDefRecord(streamID);
	break;
      }
#endif
    default:
      {
	Error(func, "%s support not compiled in!", strfiletype(filetype));
	break;
      }
    }
}


void streamReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "streamReadRecord";
  int status = 0;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  filetype = streamptr->filetype;

  *nmiss = 0;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        status = grbReadRecord(streamID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        status = srvReadRecord(streamID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        status = extReadRecord(streamID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        status = iegReadRecord(streamID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
    case FILETYPE_NC4:
      {
	status = cdfReadRecord(streamID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s support not compiled in!", strfiletype(filetype));
	break;
      }
    }
}


void streamWriteRecord(int streamID, double *data, int nmiss)
{
  static char func[] = "streamWriteRecord";
  int status = 0;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  filetype = streamptr->filetype;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        status = grbWriteRecord(streamID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        status = srvWriteRecord(streamID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        status = extWriteRecord(streamID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        status = iegWriteRecord(streamID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
    case FILETYPE_NC4:
      {
	cdfWriteRecord(streamID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s support not compiled in!", strfiletype(filetype));
	break;
      }
    }
}


void streamCopyRecord(int streamID2, int streamID1)
{
  static char func[] = "streamCopyRecord";
  int status = 0;
  int filetype = CDI_UNDEFID, filetype1, filetype2;
  stream_t *streamptr1;
  stream_t *streamptr2;

  streamptr1 = stream_to_pointer(streamID1);
  streamptr2 = stream_to_pointer(streamID2);

  stream_check_ptr(func, streamptr1);
  stream_check_ptr(func, streamptr2);

  filetype1 = streamptr1->filetype;
  filetype2 = streamptr2->filetype;

  if ( filetype1 == filetype2 ) filetype = filetype2;

  if ( filetype == FILETYPE_GRB )
    {
      status = grbCopyRecord(streamID2, streamID1);
    }
#if  defined  (HAVE_LIBSERVICE)
  else if ( filetype == FILETYPE_SRV )
    {
      status = srvCopyRecord(streamID2, streamID1);
    }
#endif
#if  defined  (HAVE_LIBEXTRA)
  else if ( filetype == FILETYPE_EXT )
    {
      status = extCopyRecord(streamID2, streamID1);
    }
#endif
#if  defined  (HAVE_LIBIEG)
  else if ( filetype == FILETYPE_IEG )
    {
      status = iegCopyRecord(streamID2, streamID1);
    }
#endif
  else
    {
      status = cdfCopyRecord(streamID2, streamID1);
    }
}


void cdiCreateRecords(int streamID, int tsID)
{
  static char func[] = "cdiCreateRecords";
  int nrecords, maxrecords;
  int nvars, varID, recID;
  RECORD *records;
  int vlistID;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  if ( streamptr->tsteps[tsID].records ) return;

  vlistID  = streamInqVlist(streamID);

  if ( tsID == 0 )
    {
      maxrecords = 0;
      nvars = streamptr->nvars;
      for ( varID = 0; varID < nvars; varID++)
	maxrecords += streamptr->vars[varID].nlevs;
    }
  else
    maxrecords = streamptr->tsteps[0].recordSize;

  if ( tsID == 0 )
    {
      nrecords = maxrecords;
    }
  else if ( tsID == 1 )
    {
      nrecords = 0;
      maxrecords = streamptr->tsteps[0].recordSize;
      for ( recID = 0; recID < maxrecords; recID++ )
	{
	  varID = streamptr->tsteps[0].records[recID].varID;
	  if ( varID != -1 ) /* varID = -1 for write mode !!! */
	    if ( vlistInqVarTime(vlistID, varID) == TIME_CONSTANT )
	      continue;
	  nrecords++;
	}
    }
  else
    nrecords = streamptr->tsteps[1].nallrecs;

  if ( maxrecords > 0 )
    records = (RECORD *) malloc(maxrecords*sizeof(RECORD));
  else
    records = NULL;

  streamptr->tsteps[tsID].records    = records;
  streamptr->tsteps[tsID].recordSize = maxrecords;
  streamptr->tsteps[tsID].nallrecs   = nrecords;

  if ( tsID == 0 )
    {
      for ( recID = 0; recID < maxrecords; recID++ )
	recordInitEntry(&streamptr->tsteps[tsID].records[recID]);
    }
  else
    {
      memcpy(streamptr->tsteps[tsID].records,
	     streamptr->tsteps[0].records,
	     maxrecords*sizeof(RECORD));

      for ( recID = 0; recID < maxrecords; recID++ )
	{
	  varID = streamptr->tsteps[0].records[recID].varID;
	  if ( varID != -1 ) /* varID = -1 for write mode !!! */
	    if ( vlistInqVarTime(vlistID, varID) == TIME_VARIABLE )
	      {
		streamptr->tsteps[tsID].records[recID].position = CDI_UNDEFID;
		streamptr->tsteps[tsID].records[recID].size     = 0;
		streamptr->tsteps[tsID].records[recID].used     = FALSE;
	      }
	}
    }
}
