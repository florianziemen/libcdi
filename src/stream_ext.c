#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "dmemory.h"

#include "error.h"
#include "file.h"
#include "cdi.h"
#include "stream_int.h"
#include "varscan.h"
#include "datetime.h"
#include "extra.h"
#include "vlist.h"


#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID

#define SINGLE_PRECISION  4
#define DOUBLE_PRECISION  8

#if defined (HAVE_LIBEXTRA)


typedef struct {
  int code;
  int level;
} EXTCOMPVAR; 


int extInqDatatype(int prec)
{
  int datatype;

  if ( prec == DOUBLE_PRECISION ) datatype = DATATYPE_FLT64;
  else                            datatype = DATATYPE_FLT32;

  return (datatype);
}


int extDefDatatype(int datatype)
{
  int prec;

  if ( datatype != DATATYPE_FLT32 && datatype != DATATYPE_FLT64 )
    datatype = DATATYPE_FLT32;

  if ( datatype == DATATYPE_FLT64 ) prec = DOUBLE_PRECISION;
  else                              prec = SINGLE_PRECISION;

  return (prec);
}


int extInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "extInqRecord";
  int status;
  int fileID;
  int icode, ilevel;
  int zaxisID = -1;
  int header[4];
  int vlistID;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  extp    = streamptr->record->extp;

  *varID   = -1;
  *levelID = -1;

  status = extRead(fileID, extp);
  if ( status != 0 ) return (0);

  extInqHeader(extp, header);

  icode  = header[1];
  ilevel = header[2];

  *varID = vlistInqVarID(vlistID, icode);

  if ( *varID == UNDEFID ) Error(func, "code %d undefined", icode);

  zaxisID = vlistInqVarZaxis(vlistID, *varID);

  *levelID = zaxisInqLevelID(zaxisID, (double) ilevel);
  
  return (1);
}


int extReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "extReadRecord";
  int vlistID, fileID;
  int status;
  int recID, vrecID, tsID;
  off_t recpos;
  int header[4];
  int varID, gridID;
  int i, size;
  double missval;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  tsID    = streamptr->curTsID;
  vrecID  = streamptr->tsteps[tsID].curRecID;
  recID   = streamptr->tsteps[tsID].recIDs[vrecID];
  recpos  = streamptr->tsteps[tsID].records[recID].position;
  varID   = streamptr->tsteps[tsID].records[recID].varID;
  extp    = streamptr->record->extp;

  fileSetPos(fileID, recpos, SEEK_SET);

  status = extRead(fileID, extp);
  if ( status != 0 ) return (0);

  extInqHeader(extp, header);
  extInqDataDP(extp, data);

  missval = vlistInqVarMissval(vlistID, varID);
  gridID  = vlistInqVarGrid(vlistID, varID);
  size    = gridInqSize(gridID);

  streamptr->numvals += size;

  *nmiss = 0;
  for ( i = 0; i < size; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }

  return (1);
}


int extCopyRecord(int streamID2, int streamID1)
{
  static char func[] = "extCopyRecord";
  int fileID1, fileID2;
  int tsID, recID, vrecID;
  long recsize;
  off_t recpos;
  int status = 0;
  char *buffer;
  STREAM *streamptr1;
  STREAM *streamptr2;

  streamptr1 = stream_to_pointer(streamID1);
  streamptr2 = stream_to_pointer(streamID2);

  stream_check_ptr(func, streamptr1);
  stream_check_ptr(func, streamptr2);

  fileID1 = streamInqFileID(streamID1);
  fileID2 = streamInqFileID(streamID2);

  tsID    = streamptr1->curTsID;
  vrecID  = streamptr1->tsteps[tsID].curRecID;
  recID   = streamptr1->tsteps[tsID].recIDs[vrecID];
  recpos  = streamptr1->tsteps[tsID].records[recID].position;
  recsize = streamptr1->tsteps[tsID].records[recID].size;

  fileSetPos(fileID1, recpos, SEEK_SET);

  buffer = (char *) malloc(recsize);

  fileRead(fileID1, buffer, recsize);

  fileWrite(fileID2, buffer, recsize);

  free(buffer);

  return (status);
}


int extDefRecord(int streamID)
{
  static char func[] = "extDefRecord";
  int fileID;
  int gridID;
  int header[4];
  int status = 0;
  int datatype;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  fileID = streamInqFileID(streamID);
  gridID = streamptr->record->gridID;
  extp   = streamptr->record->extp;

  header[0] = streamptr->record->date;
  header[1] = streamptr->record->code;
  header[2] = streamptr->record->level;
  header[3] = gridInqSize(gridID);

  datatype = streamptr->record->prec;

  extp->dprec = extDefDatatype(datatype);

  extDefHeader(extp, header);

  return (status);
}


int extWriteRecord(int streamID, const double *data)
{
  static char func[] = "extWriteRecord";
  int fileID;
  int status = 0;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  fileID = streamInqFileID(streamID);
  extp   = streamptr->record->extp;

  extDefDataDP(extp, data);

  extWrite(fileID, extp);

  return (status);
}

void extAddRecord(int streamID, int code, int level, int xysize,
		  long recsize, off_t position, int prec)
{
  static char func[] = "extAddRecord";
  int leveltype;
  int gridID = UNDEFID;
  int levelID = 0;
  int tsID, recID, varID;
  int datatype;
  RECORD *record;
  grid_t grid;
  int vlistID;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  tsID    = streamptr->curTsID;
  recID   = recordNewEntry(streamID, tsID);
  record  = &streamptr->tsteps[tsID].records[recID];

  (*record).size     = recsize;
  (*record).position = position;
  (*record).code     = code;
  (*record).ilevel   = level;

  memset(&grid, 0, sizeof(grid_t));
  grid.type  = GRID_GENERIC;
  grid.size  = xysize;
  grid.xsize = xysize;
  grid.ysize = 0;
  grid.xvals = NULL;
  grid.yvals = NULL;
  gridID = varDefGrid(vlistID, grid, 0);
  /*
  if ( level == 0 ) leveltype = ZAXIS_SURFACE;
  else              leveltype = ZAXIS_GENERIC;
  */
  leveltype = ZAXIS_GENERIC;

  datatype = extInqDatatype(prec);

  varAddRecord(recID, code, gridID, leveltype, 0, level, 0,
	       datatype, &varID, &levelID, 0, 0, 0);

  (*record).varID   = varID;
  (*record).levelID = levelID;

  streamptr->tsteps[tsID].nallrecs++;
  streamptr->nrecs++;

  if ( CDI_Debug )
    Message(func, "varID = %d gridID = %d levelID = %d",
	    varID, gridID, levelID);
}


void extCmpRecord(int streamID, int tsID, int recID, off_t position, int code,
		  int level, int xysize)
{
  static char func[] = "extCmpRecord";
  int varID = 0;
  int levelID = 0;
  RECORD *record;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  record  = &streamptr->tsteps[tsID].records[recID];

  if ( code != (*record).code || level != (*record).ilevel )
    Error(func, "inconsistent timestep");

  (*record).position = position;
  /*
  varID   = (*record).varID;
  levelID = (*record).levelID;

  streamptr->vars[varID].level[levelID] = recID;

  streamptr->tsteps[tsID].nallrecs++;
  streamptr->nrecs++;
  */
  if ( CDI_Debug )
    Message(func, "varID = %d levelID = %d", varID, levelID);
}


void extScanTimestep1(int streamID)
{  
  static char func[] = "extScanTimestep1";
  int header[4];
  int prec = 0;
  int status;
  int fileID;
  int rxysize = 0;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  DateTime datetime, datetime0;
  int tsID;
  int varID;
  long recsize;
  off_t recpos;
  int nrecords, nrecs, recID;
  int taxisID = -1;
  TAXIS *taxis;
  int vlistID;
  EXTCOMPVAR compVar, compVar0;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  streamptr->curTsID = 0;

  extp  = streamptr->record->extp;
  tsID  = tstepsNewEntry(streamID);
  taxis = &streamptr->tsteps[tsID].taxis;

  if ( tsID != 0 )
    Error(func, "Internal problem! tstepsNewEntry returns %d", tsID);

  fileID = streamInqFileID(streamID);

  nrecs = 0;
  while ( TRUE )
    {
      recpos = fileGetPos(fileID);
      status = extRead(fileID, extp);
      if ( status != 0 )
	{
	  streamptr->ntsteps = 1;
	  break;
	}
      recsize = fileGetPos(fileID) - recpos;

      extInqHeader(extp, header);

      prec   = extp->dprec;
      vdate  = header[0];
      vtime  = 0;
      rcode  = header[1];
      rlevel = header[2];
      rxysize = header[3];

      if ( nrecs == 0 )
	{
	  datetime0.date = vdate;
	  datetime0.time = vtime;
	}
      else
	{
	  datetime.date = vdate;
	  datetime.time = vtime;
	  compVar.code  = rcode;
          compVar.level = rlevel;
	  for ( recID = 0; recID < nrecs; recID++ )
	    {
	      compVar0.code  = streamptr->tsteps[0].records[recID].code;
	      compVar0.level = streamptr->tsteps[0].records[recID].ilevel;

	      if ( memcmp(&compVar0, &compVar, sizeof(EXTCOMPVAR)) == 0 ) break;
	    }
	  if ( recID < nrecs ) break;
	  if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) )
	    Warning(func, "Inconsistent verification time for code %d level %d", rcode, rlevel);
	}

      nrecs++;

      if ( CDI_Debug )
	Message(func, "%4d%8d%4d%8d%8d%6d", nrecs, (int)recpos, rcode, rlevel, vdate, vtime);

      extAddRecord(streamID, rcode, rlevel, rxysize, recsize, recpos, prec);
    }

  streamptr->rtsteps = 1;

  cdiGenVars(streamID);

  taxisID = taxisCreate(TAXIS_ABSOLUTE);
  taxis->type  = TAXIS_ABSOLUTE;
  taxis->vdate = datetime0.date;
  taxis->vtime = datetime0.time;

  vlistID = streamInqVlist(streamID);
  vlistDefTaxis(vlistID, taxisID);

  cdiCheckContents(streamID);

  nrecords = streamptr->tsteps[0].nallrecs;
  if ( nrecords < streamptr->tsteps[0].recordSize )
    {
      streamptr->tsteps[0].recordSize = nrecords;
      streamptr->tsteps[0].records =
      (RECORD *) realloc(streamptr->tsteps[0].records, nrecords*sizeof(RECORD));
    }

  streamptr->tsteps[0].recIDs = (int *) malloc(nrecords*sizeof(int));
  streamptr->tsteps[0].nrecs = nrecords;
  for ( recID = 0; recID < nrecords; recID++ )
    streamptr->tsteps[0].recIDs[recID] = recID;

  if ( streamptr->ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streamptr->rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streamptr->tsteps[tsID-1].next   = TRUE;
      streamptr->tsteps[tsID].position = recpos;
    }

  if ( streamptr->ntsteps == 1 )
    {
      if ( taxis->vdate == 0 && taxis->vtime == 0 )
	{
	  streamptr->ntsteps = 0;
	  for ( varID = 0; varID < streamptr->nvars; varID++ )
	    {
	      vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	    }
	}
    }
}


int extScanTimestep2(int streamID)
{  
  static char func[] = "extScanTimestep2";
  int header[4];
  int status;
  int fileID;
  int rxysize = 0;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  int tsID;
  int varID;
  long recsize;
  off_t recpos = 0;
  int nrecords, nrecs, recID, rindex;
  int nextstep;
  TAXIS *taxis;
  int vlistID;
  EXTCOMPVAR compVar, compVar0;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  streamptr->curTsID = 1;

  fileID  = streamInqFileID(streamID);
  vlistID = streamInqVlist(streamID);
  extp    = streamptr->record->extp;

  tsID = streamptr->rtsteps;
  if ( tsID != 1 )
    Error(func, "Internal problem! unexpeceted timestep %d", tsID+1);

  taxis = &streamptr->tsteps[tsID].taxis;

  fileSetPos(fileID, streamptr->tsteps[tsID].position, SEEK_SET);

  cdiCreateRecords(streamID, tsID);

  nrecords = streamptr->tsteps[0].nallrecs;
  streamptr->tsteps[1].recIDs = (int *) malloc(nrecords*sizeof(int));
  streamptr->tsteps[1].nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    streamptr->tsteps[1].recIDs[recID] = -1;

  for ( recID = 0; recID < nrecords; recID++ )
    {
      varID = streamptr->tsteps[0].records[recID].varID;
      streamptr->tsteps[tsID].records[recID].position = 
	streamptr->tsteps[0].records[recID].position;
      streamptr->tsteps[tsID].records[recID].size     = 
	streamptr->tsteps[0].records[recID].size;
    }

  for ( rindex = 0; rindex <= nrecords; rindex++ )
    {
      recpos = fileGetPos(fileID);
      status = extRead(fileID, extp);
      if ( status != 0 )
	{
	  streamptr->ntsteps = 2;
	  break;
	}
      recsize = fileGetPos(fileID) - recpos;

      extInqHeader(extp, header);

      vdate  = header[0];
      vtime  = 0;
      rcode  = header[1];
      rlevel = header[2];
      rxysize = header[3];

      if ( rindex == 0 )
	{
	  taxis->type  = TAXIS_ABSOLUTE;
	  taxis->vdate = vdate;
	  taxis->vtime = vtime;
	}

      compVar.code  = rcode;
      compVar.level = rlevel;
      nextstep = FALSE;
      for ( recID = 0; recID < nrecords; recID++ )
	{
	  compVar0.code  = streamptr->tsteps[tsID].records[recID].code;
	  compVar0.level = streamptr->tsteps[tsID].records[recID].ilevel;

	  if ( memcmp(&compVar0, &compVar, sizeof(EXTCOMPVAR)) == 0 )
	    {
	      if ( streamptr->tsteps[tsID].records[recID].used )
		{
		  nextstep = TRUE;
		}
	      else
		{
		  streamptr->tsteps[tsID].records[recID].used = TRUE;
		  streamptr->tsteps[tsID].recIDs[rindex] = recID;
		}
	      break;
	    }
	}
      if ( recID == nrecords )
	{
	  Warning(func, "code %d level %d not found at timestep %d", rcode, rlevel, tsID+1);
	  return (CDI_EUFSTRUCT);
	}

      if ( nextstep ) break;

      if ( CDI_Debug )
	Message(func, "%4d%8d%4d%8d%8d%6d", rindex+1, (int)recpos, rcode, rlevel, vdate, vtime);

      streamptr->tsteps[tsID].records[recID].size = recsize;

      compVar0.code  = streamptr->tsteps[tsID].records[recID].code;
      compVar0.level = streamptr->tsteps[tsID].records[recID].ilevel;

      if ( memcmp(&compVar0, &compVar, sizeof(EXTCOMPVAR)) != 0 )
	{
	  Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		  tsID, recID,
		  streamptr->tsteps[tsID].records[recID].code, rcode,
		  streamptr->tsteps[tsID].records[recID].ilevel, rlevel);
	  return (CDI_EUFSTRUCT);
	}

      streamptr->tsteps[1].records[recID].position = recpos;
    }

  nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    {
      if ( ! streamptr->tsteps[tsID].records[recID].used )
	{
	  varID = streamptr->tsteps[tsID].records[recID].varID;
	  vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	}
      else
	{
	  nrecs++;
	}
    }
  streamptr->tsteps[tsID].nrecs = nrecs;

  streamptr->rtsteps = 2;

  if ( streamptr->ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streamptr->rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streamptr->tsteps[tsID-1].next   = TRUE;
      streamptr->tsteps[tsID].position = recpos;
    }

  return (0);
}


int extInqContents(int streamID)
{
  static char func[] = "extInqContents";
  int fileID;
  int status = 0;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  fileID = streamInqFileID(streamID);

  streamptr->curTsID = 0;

  extScanTimestep1(streamID);
 
  if ( streamptr->ntsteps == -1 ) status = extScanTimestep2(streamID);

  fileSetPos(fileID, 0, SEEK_SET);

  return (status);
}


int extScanTimestep(int streamID)
{
  static char func[] = "extScanTimestep";
  int header[4];
  int status;
  int fileID;
  int tsID;
  int rxysize = 0;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  long recsize = 0;
  off_t recpos = 0;
  int recID;
  TAXIS *taxis;
  int rindex, nrecs = 0;
  EXTCOMPVAR compVar, compVar0;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  if ( CDI_Debug )
    {
      Message(func, "streamID = %d", streamID);
      Message(func, "cts = %d", streamptr->curTsID);
      Message(func, "rts = %d", streamptr->rtsteps);
      Message(func, "nts = %d", streamptr->ntsteps);
    }

  if ( streamptr->rtsteps == 0 )
    Error(func, "Internal problem! Missing contents.");

  extp  = streamptr->record->extp;
  tsID  = streamptr->rtsteps;
  taxis = &streamptr->tsteps[tsID].taxis;

  if ( streamptr->tsteps[tsID].recordSize == 0 )
    {
      cdiCreateRecords(streamID, tsID);

      nrecs = streamptr->tsteps[1].nrecs;

      streamptr->tsteps[tsID].nrecs = nrecs;
      streamptr->tsteps[tsID].recIDs = (int *) malloc(nrecs*sizeof(int));
      for ( recID = 0; recID < nrecs; recID++ )
	streamptr->tsteps[tsID].recIDs[recID] = streamptr->tsteps[1].recIDs[recID];

      fileID = streamInqFileID(streamID);

      fileSetPos(fileID, streamptr->tsteps[tsID].position, SEEK_SET);

      for ( rindex = 0; rindex <= nrecs; rindex++ )
	{
	  recpos = fileGetPos(fileID);
	  status = extRead(fileID, extp);
	  if ( status != 0 )
	    {
	      streamptr->ntsteps = streamptr->rtsteps + 1;
	      break;
	    }
	  recsize = fileGetPos(fileID) - recpos;

	  extInqHeader(extp, header);

	  vdate  = header[0];
	  vtime  = 0;
	  rcode  = header[1];
	  rlevel = header[2];
	  rxysize = header[3];

	  if ( rindex == nrecs ) break;
	  recID = streamptr->tsteps[tsID].recIDs[rindex];

	  if ( rindex == 0 )
	    {
	      taxis->type  = TAXIS_ABSOLUTE;
	      taxis->vdate = vdate;
	      taxis->vtime = vtime;
	    }
	  /*
	  extCmpRecord(streamID, tsID, nrecs, recpos, rcode, rlevel, rxysize);
	  */
	  compVar.code  = rcode;
          compVar.level = rlevel;
	  compVar0.code  = streamptr->tsteps[tsID].records[recID].code;
	  compVar0.level = streamptr->tsteps[tsID].records[recID].ilevel;

	  if ( memcmp(&compVar0, &compVar, sizeof(EXTCOMPVAR)) != 0 )
	    {
	      Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		      tsID, recID,
		      streamptr->tsteps[tsID].records[recID].code, rcode,
		      streamptr->tsteps[tsID].records[recID].ilevel, rlevel);
	      Error(func, "Invalid, unsupported or inconsistent record structure");
	    }

	  streamptr->tsteps[tsID].records[recID].position = recpos;
	  streamptr->tsteps[tsID].records[recID].size = recsize;

	  if ( CDI_Debug )
	    Message(func, "%4d%8d%4d%8d%8d%6d", rindex, (int)recpos, rcode, rlevel, vdate, vtime);
	}

      streamptr->rtsteps++;

      if ( streamptr->ntsteps != streamptr->rtsteps )
	{
	  tsID = tstepsNewEntry(streamID);
	  if ( tsID != streamptr->rtsteps )
	    Error(func, "Internal error. tsID = %d", tsID);

	  streamptr->tsteps[tsID-1].next   = 1;
	  streamptr->tsteps[tsID].position = recpos;
	}

      fileSetPos(fileID, streamptr->tsteps[tsID].position, SEEK_SET);
      streamptr->tsteps[tsID].position = recpos;
    }

  if ( nrecs > 0 && nrecs < streamptr->tsteps[tsID].nrecs )
    {
      Warning(func, "Incomplete timestep. Stop scanning at timestep %d.\n", tsID);
      streamptr->ntsteps = tsID;
    }

  return (streamptr->ntsteps);
}


int extInqTimestep(int streamID, int tsID)
{
  static char func[] = "extInqTimestep";
  int ntsteps, nrecs;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  if ( tsID == 0 && streamptr->rtsteps == 0 )
    Error(func, "Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message(func, "tsID = %d rtsteps = %d", tsID, streamptr->rtsteps);
  
  ntsteps = UNDEFID;
  while ( ( tsID + 1 ) > streamptr->rtsteps && ntsteps == UNDEFID )
    ntsteps = extScanTimestep(streamID);

  if ( tsID >= streamptr->ntsteps && streamptr->ntsteps != CDI_UNDEFID )
    {
      nrecs = 0;
    }
  else
    {
      streamptr->curTsID = tsID;
      nrecs = streamptr->tsteps[tsID].nrecs;
    }

  return (nrecs);
}


void extReadVarDP(int streamID, int varID, double *data, int *nmiss)
{
  static char func[] = "extReadVarDP";
  int vlistID, fileID;
  int levID, nlevs, gridID, gridsize;
  off_t recpos, currentfilepos;
  int header[4];
  int tsid;
  int recID;
  int i;
  int status;
  double missval;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  extp     = streamptr->record->extp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  nlevs    = streamptr->vars[varID].nlevs;
  missval  = vlistInqVarMissval(vlistID, varID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsid     = streamptr->curTsID;

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  for (levID = 0; levID < nlevs; levID++)
    {
      recID = streamptr->vars[varID].level[levID];
      recpos = streamptr->tsteps[tsid].records[recID].position;
      fileSetPos(fileID, recpos, SEEK_SET);
      status = extRead(fileID, extp);
      extInqHeader(extp, header);
      extInqDataDP(extp, &data[levID*gridsize]);
    }
  fileSetPos(fileID, currentfilepos, SEEK_SET);

  *nmiss = 0;
  for ( i = 0; i < nlevs*gridsize; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }
}


void extReadVarSliceDP(int streamID, int varID, int levID, double *data, int *nmiss)
{
  static char func[] = "extReadVarSliceDP";
  int vlistID, fileID;
  int nlevs, gridID, gridsize;
  off_t recpos, currentfilepos;
  int header[4];
  int tsid;
  int recID;
  int i;
  int status;
  double missval;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  extp     = streamptr->record->extp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  nlevs    = streamptr->vars[varID].nlevs;
  missval  = vlistInqVarMissval(vlistID, varID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsid     = streamptr->curTsID;

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d",
	     nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  recID = streamptr->vars[varID].level[levID];
  recpos = streamptr->tsteps[tsid].records[recID].position;
  fileSetPos(fileID, recpos, SEEK_SET);
  status = extRead(fileID, extp);
  extInqHeader(extp, header);
  extInqDataDP(extp, data);
 
  fileSetPos(fileID, currentfilepos, SEEK_SET);

  *nmiss = 0;
  for ( i = 0; i < gridsize; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }
}


void extWriteVarDP(int streamID, int varID, const double *data)
{
  static char func[] = "extWriteVarDP";
  int fileID;
  int levID, nlevs, gridID, gridsize;
  int zaxisID;
  double level;
  int header[4];
  int datatype;
  int tsID;
  int vlistID;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  extp     = streamptr->record->extp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streamptr->curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  nlevs    = zaxisInqSize(zaxisID);

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  header[0] = streamptr->tsteps[tsID].taxis.vdate;
  header[1] = vlistInqVarCode(vlistID, varID);
  header[3] = gridInqSize(gridID);

  datatype = vlistInqVarDatatype(vlistID, varID);

  extp->dprec = extDefDatatype(datatype);

  for ( levID = 0;  levID < nlevs; levID++ )
    {
      level = zaxisInqLevel(zaxisID, levID);

      header[2] = (int) level;
      extDefHeader(extp, header);
      extDefDataDP(extp, &data[levID*gridsize]);
      extWrite(fileID, extp);
    }
}


void extWriteVarSliceDP(int streamID, int varID, int levID, const double *data)
{
  static char func[] = "extWriteVarSliceDP";
  int fileID;
  int gridID;
  int zaxisID;
  double level;
  int header[4];
  int datatype;
  int tsID;
  int vlistID;
  EXTREC *extp;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  extp     = streamptr->record->extp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streamptr->curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  level    = zaxisInqLevel(zaxisID, levID);

  if ( CDI_Debug )
    Message(func, "gridID = %d zaxisID = %d", gridID, zaxisID);

  header[0] = streamptr->tsteps[tsID].taxis.vdate;
  header[1] = vlistInqVarCode(vlistID, varID);
  header[2] = (int) level;
  header[3] = gridInqSize(gridID);

  datatype = vlistInqVarDatatype(vlistID, varID);

  extp->dprec = extDefDatatype(datatype);

  extDefHeader(extp, header);
  extDefDataDP(extp, data);
  extWrite(fileID, extp);
}

#endif /* HAVE_LIBEXTRA */
