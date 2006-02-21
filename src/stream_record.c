#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "dmemory.h"

#include "cdi.h"
#include "cdf_int.h"
#include "stream_int.h"
#include "stream_grb.h"
#include "stream_cdf.h"
#include "stream_srv.h"
#include "stream_ext.h"
#include "stream_ieg.h"
#include "stream_hdf.h"


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

  recordSize = streams[streamID].tsteps[tsID].recordSize;
  records    = streams[streamID].tsteps[tsID].records;
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

  streams[streamID].tsteps[tsID].recordSize = recordSize;
  streams[streamID].tsteps[tsID].records    = records;

  return (recordID);
}


void cdiInitRecord(int streamID)
{
  static char func[] = "cdiInitRecord";

  streams[streamID].record = (Record *) malloc(sizeof(Record));

  streams[streamID].record->used       = 0;
  streams[streamID].record->nrec       = 0;
  streams[streamID].record->dataread   = 1;
  streams[streamID].record->code       = 0;
  streams[streamID].record->level      = 0;
  streams[streamID].record->date       = 0;
  streams[streamID].record->time       = 0;
  streams[streamID].record->gridID     = 0;
  streams[streamID].record->zaxisID    = 0;
  streams[streamID].record->buffer     = NULL;
  streams[streamID].record->buffersize = 0;
  streams[streamID].record->position   = 0;
  streams[streamID].record->varID      = 0;
  streams[streamID].record->levelID    = CDI_UNDEFID;
  streams[streamID].record->recid      = 0;
}


void streamInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "streamInqRecord";
  int rec = 0;
  int recID, tsID, rindex;
  int lindex;

  streamCheckID(func, streamID);

  cdiDefAccesstype(streamID, TYPE_REC);

  if ( ! streams[streamID].record ) cdiInitRecord(streamID);

  tsID   = streams[streamID].curTsID;
  rindex = streams[streamID].tsteps[tsID].curRecID + 1;

  if ( rindex >= streams[streamID].tsteps[tsID].nrecs )
    Error(func, "record %d not available at timestep %d", rindex+1, tsID+1);

  recID  = streams[streamID].tsteps[tsID].recIDs[rindex];

  if ( recID == -1 || recID >= streams[streamID].tsteps[tsID].nallrecs )
    Error(func, "Internal problem! tsID = %d recID = %d", tsID, recID);

  *varID   = streams[streamID].tsteps[tsID].records[recID].varID;
  lindex   = streams[streamID].tsteps[tsID].records[recID].levelID;

  *levelID = streams[streamID].vars[*varID].lindex[lindex];

  if ( CDI_Debug )
    Message(func, "tsID = %d, recID = %d, varID = %d, levelID = %d\n",
	    tsID, recID, *varID, *levelID);

  streams[streamID].curTsID = tsID;
  streams[streamID].tsteps[tsID].curRecID = rindex;

  rec = recID + 1;
  /*
  filetype = streams[streamID].filetype;

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
      {
	rec = cdfInqRecord(streamID, varID, levelID);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
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

  streamCheckID(func, streamID);

  tsID = streams[streamID].curTsID;

  if ( tsID == CDI_UNDEFID )
    {
      tsID++;
      streamDefTimestep(streamID, tsID);
    }

  if ( ! streams[streamID].record ) cdiInitRecord(streamID);

  vlistID = streamInqVlist(streamID);
  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  code    = vlistInqVarCode(vlistID, varID);

  level = (int) zaxisInqLevel(zaxisID, levelID);

  streams[streamID].record->varID    = varID;
  streams[streamID].record->levelID  = levelID;
  streams[streamID].record->code     = code;
  streams[streamID].record->level    = level;
  streams[streamID].record->date     = streams[streamID].tsteps[tsID].taxis.vdate;
  streams[streamID].record->time     = streams[streamID].tsteps[tsID].taxis.vtime;
  streams[streamID].record->gridID   = gridID;
  streams[streamID].record->zaxisID  = zaxisID;
  streams[streamID].record->prec     = vlistInqVarDatatype(vlistID, varID);

  filetype = streams[streamID].filetype;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
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
      {
	if ( streams[streamID].accessmode == 0 ) cdfEndDef(streamID);
	status = cdfDefRecord(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBHDF5)
    case FILETYPE_H5:
      {
	status = hdfDefRecord(streamID);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }
}


void streamReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "streamReadRecord";
  int status = 0;
  int filetype;

  streamCheckID(func, streamID);

  filetype = streams[streamID].filetype;

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
      {
	status = cdfReadRecord(streamID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }
}


void streamWriteRecord(int streamID, double *data, int nmiss)
{
  static char func[] = "streamWriteRecord";
  int status = 0;
  int filetype;

  streamCheckID(func, streamID);

  filetype = streams[streamID].filetype;

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
      {
	cdfWriteRecord(streamID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBHDF5)
    case FILETYPE_H5:
      {
	hdfWriteRecord(streamID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }
}


void streamCopyRecord(int ostreamID, int istreamID)
{
  static char func[] = "streamCopyRecord";
  int status = 0;
  int iofiletype = CDI_UNDEFID, ifiletype, ofiletype;

  streamCheckID(func, istreamID);
  streamCheckID(func, ostreamID);

  ifiletype = streams[istreamID].filetype;
  ofiletype = streams[ostreamID].filetype;

  if ( ifiletype == ofiletype ) iofiletype = ofiletype;

  if ( iofiletype == FILETYPE_GRB )
    {
      status = grbCopyRecord(ostreamID, istreamID);
    }
#if  defined  (HAVE_LIBSERVICE)
  else if ( iofiletype == FILETYPE_SRV )
    {
      status = srvCopyRecord(ostreamID, istreamID);
    }
#endif
#if  defined  (HAVE_LIBEXTRA)
  else if ( iofiletype == FILETYPE_EXT )
    {
      status = extCopyRecord(ostreamID, istreamID);
    }
#endif
#if  defined  (HAVE_LIBIEG)
  else if ( iofiletype == FILETYPE_IEG )
    {
      status = iegCopyRecord(ostreamID, istreamID);
    }
#endif
  else
    {
      status = cdfCopyRecord(ostreamID, istreamID);
    }
}


void cdiCreateRecords(int streamID, int tsID)
{
  static char func[] = "cdiCreateRecords";
  int nrecords, maxrecords;
  int nvars, varID, recID;
  RECORD *records;
  int vlistID;

  streamCheckID(func, streamID);

  if ( streams[streamID].tsteps[tsID].records ) return;

  vlistID  = streamInqVlist(streamID);

  if ( tsID == 0 )
    {
      maxrecords = 0;
      nvars = streams[streamID].nvars;
      for ( varID = 0; varID < nvars; varID++)
	maxrecords += streams[streamID].vars[varID].nlevs;
    }
  else
    maxrecords = streams[streamID].tsteps[0].recordSize;

  if ( tsID == 0 )
    {
      nrecords = maxrecords;
    }
  else if ( tsID == 1 )
    {
      nrecords = 0;
      maxrecords = streams[streamID].tsteps[0].recordSize;
      for ( recID = 0; recID < maxrecords; recID++ )
	{
	  varID = streams[streamID].tsteps[0].records[recID].varID;
	  if ( varID != -1 ) /* varID = -1 for write mode !!! */
	    if ( vlistInqVarTime(vlistID, varID) == TIME_CONSTANT )
	      continue;
	  nrecords++;
	}
    }
  else
    nrecords = streams[streamID].tsteps[1].nallrecs;

  records = (RECORD *) malloc(maxrecords*sizeof(RECORD));
  streams[streamID].tsteps[tsID].records    = records;
  streams[streamID].tsteps[tsID].recordSize = maxrecords;
  streams[streamID].tsteps[tsID].nallrecs   = nrecords;

  if ( tsID == 0 )
    {
      for ( recID = 0; recID < maxrecords; recID++ )
	recordInitEntry(&streams[streamID].tsteps[tsID].records[recID]);
    }
  else
    {
      memcpy(streams[streamID].tsteps[tsID].records,
	     streams[streamID].tsteps[0].records,
	     maxrecords*sizeof(RECORD));

      for ( recID = 0; recID < maxrecords; recID++ )
	{
	  varID = streams[streamID].tsteps[0].records[recID].varID;
	  if ( varID != -1 ) /* varID = -1 for write mode !!! */
	    if ( vlistInqVarTime(vlistID, varID) == TIME_VARIABLE )
	      {
		streams[streamID].tsteps[tsID].records[recID].position = CDI_UNDEFID;
		streams[streamID].tsteps[tsID].records[recID].size     = 0;
		streams[streamID].tsteps[tsID].records[recID].used     = FALSE;
	      }
	}
    }
}
