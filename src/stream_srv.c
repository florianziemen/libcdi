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
#include "service.h"
#include "vlist.h"


#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID

#define SINGLE_PRECISION  4
#define DOUBLE_PRECISION  8


#if defined (HAVE_LIBSERVICE)

int srvInqDatatype(int prec)
{
  int datatype;

  if ( prec == DOUBLE_PRECISION ) datatype = DATATYPE_FLT64;
  else                            datatype = DATATYPE_FLT32;

  return (datatype);
}


int srvDefDatatype(int datatype)
{
  int prec;

  if ( datatype != DATATYPE_FLT32 && datatype != DATATYPE_FLT64 )
    datatype = DATATYPE_FLT32;

  if ( datatype == DATATYPE_FLT64 ) prec = DOUBLE_PRECISION;
  else                              prec = SINGLE_PRECISION;

  return (prec);
}


int srvInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "srvInqRecord";
  int status;
  int fileID;
  int icode, ilevel;
  int zaxisID = -1;
  int header[8];
  int vlistID;
  SRVREC *srvp;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  srvp    = streams[streamID].record->srvp;

  *varID   = -1;
  *levelID = -1;

  status = srvRead(fileID, srvp);
  if ( status != 0 ) return (0);

  srvInqHeader(srvp, header);

  icode  = header[0];
  ilevel = header[1];

  *varID = vlistInqVarID(vlistID, icode);

  if ( *varID == UNDEFID ) Error(func, "code %d undefined", icode);

  zaxisID = vlistInqVarZaxis(vlistID, *varID);

  *levelID = zaxisInqLevelID(zaxisID, (double) ilevel);
  
  return (1);
}


int srvReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "srvReadRecord";
  int vlistID, fileID;
  int status;
  int recID, vrecID, tsID;
  off_t recpos;
  int header[8];
  int varID, gridID;
  int i, size;
  double missval;
  SRVREC *srvp;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  tsID    = streams[streamID].curTsID;
  vrecID  = streams[streamID].tsteps[tsID].curRecID;
  recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
  recpos  = streams[streamID].tsteps[tsID].records[recID].position;
  varID   = streams[streamID].tsteps[tsID].records[recID].varID;
  srvp    = streams[streamID].record->srvp;

  fileSetPos(fileID, recpos, SEEK_SET);

  status = srvRead(fileID, srvp);
  if ( status != 0 ) return (0);

  srvInqHeader(srvp, header);
  srvInqDataDP(srvp, data);

  gridID  = vlistInqVarGrid(vlistID, varID);
  size    = gridInqSize(gridID);
  missval = vlistInqVarMissval(vlistID, varID);

  *nmiss = 0;
  for ( i = 0; i < size; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }

  return (1);
}


int srvCopyRecord(int ostreamID, int istreamID)
{
  static char func[] = "srvCopyRecord";
  int ifileID, ofileID;
  int tsID, recID, vrecID;
  long recsize;
  off_t recpos;
  int status = 0;
  char *buffer;

  streamCheckID(func, istreamID);
  streamCheckID(func, ostreamID);

  ifileID = streamInqFileID(istreamID);
  ofileID = streamInqFileID(ostreamID);

  tsID    = streams[istreamID].curTsID;
  vrecID  = streams[istreamID].tsteps[tsID].curRecID;
  recID   = streams[istreamID].tsteps[tsID].recIDs[vrecID];
  recpos  = streams[istreamID].tsteps[tsID].records[recID].position;
  recsize = streams[istreamID].tsteps[tsID].records[recID].size;

  fileSetPos(ifileID, recpos, SEEK_SET);

  buffer = (char *) malloc(recsize);

  fileRead(ifileID, buffer, recsize);

  fileWrite(ofileID, buffer, recsize);

  free(buffer);

  return (status);
}


int srvDefRecord(int streamID)
{
  static char func[] = "srvDefRecord";
  int fileID;
  int gridID;
  int header[8];
  int status = 0;
  int xsize, ysize;
  int datatype;
  SRVREC *srvp;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);
  gridID = streams[streamID].record->gridID;
  srvp   = streams[streamID].record->srvp;

  header[0] = streams[streamID].record->code;
  header[1] = streams[streamID].record->level;
  header[2] = streams[streamID].record->date;
  header[3] = streams[streamID].record->time;
  xsize = gridInqXsize(gridID);
  ysize = gridInqYsize(gridID);
  if ( xsize == 0 || ysize == 0 )
    {
      xsize = gridInqSize(gridID);
      ysize = 1;
    }
  header[4] = xsize;
  header[5] = ysize;
  header[6] = 0;
  header[7] = 0;

  datatype = streams[streamID].record->prec;

  srvp->dprec = srvDefDatatype(datatype);

  srvDefHeader(srvp, header);

  return (status);
}


int srvWriteRecord(int streamID, const double *data)
{
  static char func[] = "srvWriteRecord";
  int fileID;
  int status = 0;
  SRVREC *srvp;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);
  srvp   = streams[streamID].record->srvp;

  srvDefDataDP(srvp, data);

  srvWrite(fileID, srvp);

  return (status);
}


void srvAddRecord(int streamID, int code, int level, int xsize, int ysize,
		  long recsize, off_t position, int prec)
{
  static char func[] = "srvAddRecord";
  int leveltype;
  int gridID = UNDEFID;
  int levelID = 0;
  int tsID, recID, varID;
  int datatype;
  RECORD *record;
  GRID grid;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  tsID    = streams[streamID].curTsID;
  recID   = recordNewEntry(streamID, tsID);
  record  = &streams[streamID].tsteps[tsID].records[recID];

  (*record).size     = recsize;
  (*record).position = position;
  (*record).code     = code;
  (*record).ilevel   = level;

  memset(&grid, 0, sizeof(GRID));
  grid.type  = GRID_GENERIC; 
  grid.size  = xsize*ysize;
  grid.xsize = xsize;
  grid.ysize = ysize;
  grid.xvals = NULL;
  grid.yvals = NULL;
  gridID = varDefGrid(vlistID, grid, 0);
  /*
  if ( level == 0 ) leveltype = ZAXIS_SURFACE;
  else              leveltype = ZAXIS_GENERIC;
  */
  leveltype = ZAXIS_GENERIC;

  datatype = srvInqDatatype(prec);

  varAddRecord(recID, code, gridID, leveltype, 0, level, 0,
	       datatype, &varID, &levelID, 0, 0);

  (*record).varID   = varID;
  (*record).levelID = levelID;

  streams[streamID].tsteps[tsID].nallrecs++;
  streams[streamID].nrecs++;

  if ( CDI_Debug )
    Message(func, "varID = %d gridID = %d levelID = %d",
	    varID, gridID, levelID);
}


void srvCmpRecord(int streamID, int tsID, int recID, off_t position, int code,
		  int level, int xsize, int ysize)
{
  static char func[] = "srvCmpRecord";
  int varID = 0;
  int levelID = 0;
  RECORD *record;

  record  = &streams[streamID].tsteps[tsID].records[recID];

  if ( code != (*record).code || level != (*record).ilevel )
    Error(func, "inconsistent timestep");

  (*record).position = position;
  /*
  varID   = (*record).varID;
  levelID = (*record).levelID;

  streams[streamID].vars[varID].level[levelID] = recID;

  streams[streamID].tsteps[tsID].nallrecs++;
  streams[streamID].nrecs++;
  */
  if ( CDI_Debug )
    Message(func, "varID = %d levelID = %d", varID, levelID);
}


void srvScanTimestep1(int streamID)
{  
  static char func[] = "srvScanTimestep1";
  int header[8];
  int prec = 0;
  int status;
  int fileID;
  int rxsize = 0, rysize = 0;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  DateTime datetime, datetime0;
  long levelcode, levelcode0 = 0;
  int tsID;
  int varID;
  long recsize;
  off_t recpos;
  int nrecords, nrecs, recID;
  int taxisID = -1;
  TAXIS *taxis;
  int vlistID;
  SRVREC *srvp;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 0;

  srvp  = streams[streamID].record->srvp;
  tsID  = tstepsNewEntry(streamID);
  taxis = &streams[streamID].tsteps[tsID].taxis;

  if ( tsID != 0 )
    Error(func, "Internal problem! tstepsNewEntry returns %d", tsID);

  fileID = streamInqFileID(streamID);

  nrecs = 0;
  while ( TRUE )
    {
      recpos = fileGetPos(fileID);
      status = srvRead(fileID, srvp);
      if ( status != 0 )
	{
	  streams[streamID].ntsteps = 1;
	  break;
	}
      recsize = fileGetPos(fileID) - recpos;

      srvInqHeader(srvp, header);

      prec   = srvp->dprec;
      rcode  = header[0];
      rlevel = header[1];
      vdate  = header[2];
      vtime  = header[3];
      rxsize = header[4];
      rysize = header[5];

      if ( nrecs == 0 )
	{
	  datetime0.date = vdate;
	  datetime0.time = vtime;
	}
      else
	{
	  datetime.date = vdate;
	  datetime.time = vtime;
	  levelcode = rlevel*1000 + rcode;
	  for ( recID = 0; recID < nrecs; recID++ )
	    {
	      levelcode0 = streams[streamID].tsteps[0].records[recID].ilevel*1000 +
	 	           streams[streamID].tsteps[0].records[recID].code;
	      if ( levelcode == levelcode0 ) break;
	    }
	  if ( recID < nrecs ) break;
	  if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) )
	    Warning(func, "Inconsistent verification time for code %d level %d", rcode, rlevel);
	}

      nrecs++;

      if ( CDI_Debug )
	Message(func, "%4d%8d%4d%8d%8d%6d", nrecs, (int)recpos, rcode, rlevel, vdate, vtime);

      srvAddRecord(streamID, rcode, rlevel, rxsize, rysize, recsize, recpos, prec);
    }

  streams[streamID].rtsteps = 1;

  cdiGenVars(streamID);

  taxisID = taxisCreate(TAXIS_ABSOLUTE);
  taxis->type  = TAXIS_ABSOLUTE;
  taxis->vdate = datetime0.date;
  taxis->vtime = datetime0.time;

  vlistID = streamInqVlist(streamID);
  vlistDefTaxis(vlistID, taxisID);

  cdiCheckContents(streamID);

  nrecords = streams[streamID].tsteps[0].nallrecs;
  if ( nrecords < streams[streamID].tsteps[0].recordSize )
    {
      streams[streamID].tsteps[0].recordSize = nrecords;
      streams[streamID].tsteps[0].records =
	(RECORD *) realloc(streams[streamID].tsteps[0].records, nrecords*sizeof(RECORD));
    }

  streams[streamID].tsteps[0].recIDs = (int *) malloc(nrecords*sizeof(int));
  streams[streamID].tsteps[0].nrecs = nrecords;
  for ( recID = 0; recID < nrecords; recID++ )
    streams[streamID].tsteps[0].recIDs[recID] = recID;

  if ( streams[streamID].ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streams[streamID].rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streams[streamID].tsteps[tsID-1].next   = TRUE;
      streams[streamID].tsteps[tsID].position = recpos;
    }

  if ( streams[streamID].ntsteps == 1 )
    {
      if ( taxis->vdate == 0 && taxis->vtime == 0 )
	{
	  streams[streamID].ntsteps = 0;
	  for ( varID = 0; varID < streams[streamID].nvars; varID++ )
	    {
	      vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	    }
	}
    }
}


int srvScanTimestep2(int streamID)
{  
  static char func[] = "srvScanTimestep2";
  int header[8];
  int status;
  int fileID;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  long levelcode, levelcode0 = 0;
  int tsID;
  int varID;
  long recsize;
  off_t recpos = 0;
  int nrecords, nrecs, recID, rindex;
  int nextstep;
  TAXIS *taxis;
  int vlistID;
  SRVREC *srvp;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 1;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  srvp    = streams[streamID].record->srvp;

  tsID = streams[streamID].rtsteps;
  if ( tsID != 1 )
    Error(func, "Internal problem! unexpeceted timestep %d", tsID+1);

  taxis = &streams[streamID].tsteps[tsID].taxis;

  fileSetPos(fileID, streams[streamID].tsteps[tsID].position, SEEK_SET);

  cdiCreateRecords(streamID, tsID);

  nrecords = streams[streamID].tsteps[0].nallrecs;
  streams[streamID].tsteps[1].recIDs = (int *) malloc(nrecords*sizeof(int));
  streams[streamID].tsteps[1].nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    streams[streamID].tsteps[1].recIDs[recID] = -1;

  for ( recID = 0; recID < nrecords; recID++ )
    {
      varID = streams[streamID].tsteps[0].records[recID].varID;
      streams[streamID].tsteps[tsID].records[recID].position = 
	streams[streamID].tsteps[0].records[recID].position;
      streams[streamID].tsteps[tsID].records[recID].size     = 
	streams[streamID].tsteps[0].records[recID].size;
    }

  for ( rindex = 0; rindex <= nrecords; rindex++ )
    {
      recpos = fileGetPos(fileID);
      status = srvRead(fileID, srvp);
      if ( status != 0 )
	{
	  streams[streamID].ntsteps = 2;
	  break;
	}
      recsize = fileGetPos(fileID) - recpos;

      srvInqHeader(srvp, header);

      rcode  = header[0];
      rlevel = header[1];
      vdate  = header[2];
      vtime  = header[3];

      if ( rindex == 0 )
	{
	  taxis->type  = TAXIS_ABSOLUTE;
	  taxis->vdate = vdate;
	  taxis->vtime = vtime;
	}

      levelcode = rlevel*1000 + rcode;
      nextstep = FALSE;
      for ( recID = 0; recID < nrecords; recID++ )
	{
	  levelcode0 = streams[streamID].tsteps[tsID].records[recID].ilevel*1000 +
	               streams[streamID].tsteps[tsID].records[recID].code;
	  if ( levelcode == levelcode0 )
	    {
	      if ( streams[streamID].tsteps[tsID].records[recID].used )
		{
		  nextstep = TRUE;
		}
	      else
		{
		  streams[streamID].tsteps[tsID].records[recID].used = TRUE;
		  streams[streamID].tsteps[tsID].recIDs[rindex] = recID;
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

      streams[streamID].tsteps[tsID].records[recID].size = recsize;

      levelcode0 = streams[streamID].tsteps[tsID].records[recID].ilevel*1000 +
	           streams[streamID].tsteps[tsID].records[recID].code;

      if ( levelcode != levelcode0 )
	{
	  Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		  tsID, recID,
		  streams[streamID].tsteps[tsID].records[recID].code, rcode,
		  streams[streamID].tsteps[tsID].records[recID].ilevel, rlevel);
	  return (CDI_EUFSTRUCT);
	}

      streams[streamID].tsteps[1].records[recID].position = recpos;
    }

  nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    {
      if ( ! streams[streamID].tsteps[tsID].records[recID].used )
	{
	  varID = streams[streamID].tsteps[tsID].records[recID].varID;
	  vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	}
      else
	{
	  nrecs++;
	}
    }
  streams[streamID].tsteps[tsID].nrecs = nrecs;

  streams[streamID].rtsteps = 2;

  if ( streams[streamID].ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streams[streamID].rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streams[streamID].tsteps[tsID-1].next   = TRUE;
      streams[streamID].tsteps[tsID].position = recpos;
    }

  return (0);
}


int srvInqContents(int streamID)
{
  static char func[] = "srvInqContents";
  int fileID;
  int status = 0;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);

  streams[streamID].curTsID = 0;

  srvScanTimestep1(streamID);
 
  if ( streams[streamID].ntsteps == -1 ) status = srvScanTimestep2(streamID);

  fileSetPos(fileID, 0, SEEK_SET);

  return (status);
}


int srvScanTimestep(int streamID)
{
  static char func[] = "srvScanTimestep";
  int header[8];
  int status;
  int fileID;
  int tsID;
  int rxsize = 0, rysize = 0;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  long levelcode, levelcode0 = 0;
  long recsize = 0;
  off_t recpos = 0;
  int recID;
  TAXIS *taxis;
  int rindex, nrecs = 0;
  SRVREC *srvp;

  streamCheckID(func, streamID);

  if ( CDI_Debug )
    {
      Message(func, "streamID = %d", streamID);
      Message(func, "cts = %d", streams[streamID].curTsID);
      Message(func, "rts = %d", streams[streamID].rtsteps);
      Message(func, "nts = %d", streams[streamID].ntsteps);
    }

  if ( streams[streamID].rtsteps == 0 )
    Error(func, "Internal problem! Missing contents.");

  srvp  = streams[streamID].record->srvp;
  tsID  = streams[streamID].rtsteps;
  taxis = &streams[streamID].tsteps[tsID].taxis;

  if ( streams[streamID].tsteps[tsID].recordSize == 0 )
    {
      cdiCreateRecords(streamID, tsID);

      nrecs = streams[streamID].tsteps[1].nrecs;

      streams[streamID].tsteps[tsID].nrecs = nrecs;
      streams[streamID].tsteps[tsID].recIDs = (int *) malloc(nrecs*sizeof(int));
      for ( recID = 0; recID < nrecs; recID++ )
	streams[streamID].tsteps[tsID].recIDs[recID] = streams[streamID].tsteps[1].recIDs[recID];

      fileID = streamInqFileID(streamID);

      fileSetPos(fileID, streams[streamID].tsteps[tsID].position, SEEK_SET);

      for ( rindex = 0; rindex <= nrecs; rindex++ )
	{
	  recpos = fileGetPos(fileID);
	  status = srvRead(fileID, srvp);
	  if ( status != 0 )
	    {
	      streams[streamID].ntsteps = streams[streamID].rtsteps + 1;
	      break;
	    }
	  recsize = fileGetPos(fileID) - recpos;

	  srvInqHeader(srvp, header);

	  rcode  = header[0];
	  rlevel = header[1];
	  vdate  = header[2];
	  vtime  = header[3];
	  rxsize = header[4];
	  rysize = header[5];

	  if ( rindex == nrecs ) break;
	  recID = streams[streamID].tsteps[tsID].recIDs[rindex];

	  if ( rindex == 0 )
	    {
	      taxis->type  = TAXIS_ABSOLUTE;
	      taxis->vdate = vdate;
	      taxis->vtime = vtime;
	    }
	  /*
	  srvCmpRecord(streamID, tsID, nrecs, recpos, rcode, rlevel, rxsize, rysize);
	  */
	  levelcode  = rlevel*1000 + rcode;
	  levelcode0 = streams[streamID].tsteps[tsID].records[recID].ilevel*1000 +
	               streams[streamID].tsteps[tsID].records[recID].code;

	  if ( levelcode != levelcode0 )
	    {
	      Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		      tsID, recID,
		      streams[streamID].tsteps[tsID].records[recID].code, rcode,
		      streams[streamID].tsteps[tsID].records[recID].ilevel, rlevel);
	      Error(func, "Invalid, unsupported or inconsistent record structure");
	    }

	  streams[streamID].tsteps[tsID].records[recID].position = recpos;
	  streams[streamID].tsteps[tsID].records[recID].size = recsize;

	  if ( CDI_Debug )
	    Message(func, "%4d%8d%4d%8d%8d%6d", rindex, (int)recpos, rcode, rlevel, vdate, vtime);
	}

      streams[streamID].rtsteps++;

      if ( streams[streamID].ntsteps != streams[streamID].rtsteps )
	{
	  tsID = tstepsNewEntry(streamID);
	  if ( tsID != streams[streamID].rtsteps )
	    Error(func, "Internal error. tsID = %d", tsID);

	  streams[streamID].tsteps[tsID-1].next   = 1;
	  streams[streamID].tsteps[tsID].position = recpos;
	}

      fileSetPos(fileID, streams[streamID].tsteps[tsID].position, SEEK_SET);
      streams[streamID].tsteps[tsID].position = recpos;
    }

  if ( nrecs > 0 && nrecs < streams[streamID].tsteps[tsID].nrecs )
    {
      Warning(func, "Incomplete timestep. Stop scanning at timestep %d.\n", tsID);
      streams[streamID].ntsteps = tsID;
    }

  return (streams[streamID].ntsteps);
}


int srvInqTimestep(int streamID, int tsID)
{
  static char func[] = "srvInqTimestep";
  int ntsteps, nrecs;

  streamCheckID(func, streamID);

  if ( tsID == 0 && streams[streamID].rtsteps == 0 )
    Error(func, "Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message(func, "tsID = %d rtsteps = %d", tsID, streams[streamID].rtsteps);
  
  ntsteps = UNDEFID;
  while ( ( tsID + 1 ) > streams[streamID].rtsteps && ntsteps == UNDEFID )
    ntsteps = srvScanTimestep(streamID);

  if ( tsID >= streams[streamID].ntsteps && streams[streamID].ntsteps != CDI_UNDEFID )
    {
      nrecs = 0;
    }
  else
    {
      streams[streamID].curTsID = tsID;
      nrecs = streams[streamID].tsteps[tsID].nrecs;
    }

  return (nrecs);
}


void srvReadVarDP(int streamID, int varID, double *data, int *nmiss)
{
  static char func[] = "srvReadVarDP";
  int vlistID, fileID;
  int levID, nlevs, gridID, gridsize;
  off_t recpos, currentfilepos;
  int header[8];
  int tsid;
  int recID;
  int i;
  int status;
  double missval;
  SRVREC *srvp;

  srvp     = streams[streamID].record->srvp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  nlevs    = streams[streamID].vars[varID].nlevs;
  missval  = vlistInqVarMissval(vlistID, varID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsid     = streams[streamID].curTsID;

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  for (levID = 0; levID < nlevs; levID++)
    {
      recID = streams[streamID].vars[varID].level[levID];
      recpos = streams[streamID].tsteps[tsid].records[recID].position;
      fileSetPos(fileID, recpos, SEEK_SET);
      status = srvRead(fileID, srvp);
      srvInqHeader(srvp, header);
      srvInqDataDP(srvp, &data[levID*gridsize]);
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


void srvReadVarSliceDP(int streamID, int varID, int levID, double *data, int *nmiss)
{
  static char func[] = "srvReadVarSliceDP";
  int vlistID, fileID;
  int nlevs, gridID, gridsize;
  off_t recpos, currentfilepos;
  int header[8];
  int tsid;
  int recID;
  int i;
  int status;
  double missval;
  SRVREC *srvp;

  srvp     = streams[streamID].record->srvp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  nlevs    = streams[streamID].vars[varID].nlevs;
  missval  = vlistInqVarMissval(vlistID, varID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsid     = streams[streamID].curTsID;

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d",
	     nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  recID = streams[streamID].vars[varID].level[levID];
  recpos = streams[streamID].tsteps[tsid].records[recID].position;
  fileSetPos(fileID, recpos, SEEK_SET);
  status = srvRead(fileID, srvp);
  srvInqHeader(srvp, header);
  srvInqDataDP(srvp, data);

  fileSetPos(fileID, currentfilepos, SEEK_SET);

  *nmiss = 0;
  for ( i = 0; i < gridsize; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }
}


void srvWriteVarDP(int streamID, int varID, const double *data)
{
  static char func[] = "srvWriteVarDP";
  int fileID;
  int levID, nlevs, gridID, gridsize;
  int zaxisID;
  double level;
  int header[8];
  int datatype;
  int tsID;
  int vlistID;
  SRVREC *srvp;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  srvp     = streams[streamID].record->srvp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  nlevs    = zaxisInqSize(zaxisID);

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  header[0] = vlistInqVarCode(vlistID, varID);
  header[2] = streams[streamID].tsteps[tsID].taxis.vdate;
  header[3] = streams[streamID].tsteps[tsID].taxis.vtime;
  header[4] = gridInqXsize(gridID);
  header[5] = gridInqYsize(gridID);
  if ( header[4] == 0 )
    {
      header[4] = gridInqSize(gridID);
      header[5] = 1;
    }
  header[6] = 0;
  header[7] = 0;

  datatype = vlistInqVarDatatype(vlistID, varID);

  srvp->dprec = srvDefDatatype(datatype);

  for ( levID = 0;  levID < nlevs; levID++ )
    {
      level = zaxisInqLevel(zaxisID, levID);

      header[1] = (int) level;
      srvDefHeader(srvp, header);
      srvDefDataDP(srvp, &data[levID*gridsize]);
      srvWrite(fileID, srvp);
    }
}


void srvWriteVarSliceDP(int streamID, int varID, int levID, const double *data)
{
  static char func[] = "srvWriteVarSliceDP";
  int fileID;
  int gridID;
  int zaxisID;
  double level;
  int header[8];
  int datatype;
  int tsID;
  int vlistID;
  SRVREC *srvp;

  srvp     = streams[streamID].record->srvp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  level    = zaxisInqLevel(zaxisID, levID);

  if ( CDI_Debug )
    Message(func, "gridID = %d zaxisID = %d", gridID, zaxisID);

  header[0] = vlistInqVarCode(vlistID, varID);
  header[1] = (int) level;
  header[2] = streams[streamID].tsteps[tsID].taxis.vdate;
  header[3] = streams[streamID].tsteps[tsID].taxis.vtime;
  header[4] = gridInqXsize(gridID);
  header[5] = gridInqYsize(gridID);
  if ( header[4] == 0 )
    {
      header[4] = gridInqSize(gridID);
      header[5] = 1;
    }
  header[6] = 0;
  header[7] = 0;

  datatype = vlistInqVarDatatype(vlistID, varID);

  srvp->dprec = srvDefDatatype(datatype);

  srvDefHeader(srvp, header);
  srvDefDataDP(srvp, data);
  srvWrite(fileID, srvp);
}

#endif /* HAVE_LIBSERVICE */
