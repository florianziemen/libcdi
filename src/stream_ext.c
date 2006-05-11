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

int extInqDatatype(int prec)
{
  int datatype;

  if ( prec == DOUBLE_PRECISION ) datatype = DATATYPE_REAL8;
  else                            datatype = DATATYPE_REAL4;

  return (datatype);
}


int extDefDatatype(int datatype)
{
  int prec;

  if ( datatype != DATATYPE_REAL4 && datatype != DATATYPE_REAL8 )
    datatype = DATATYPE_REAL4;

  if ( datatype == DATATYPE_REAL8 ) prec = DOUBLE_PRECISION;
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

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  extp    = streams[streamID].record->extp;

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

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  tsID    = streams[streamID].curTsID;
  vrecID  = streams[streamID].tsteps[tsID].curRecID;
  recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
  recpos  = streams[streamID].tsteps[tsID].records[recID].position;
  varID   = streams[streamID].tsteps[tsID].records[recID].varID;
  extp    = streams[streamID].record->extp;

  fileSetPos(fileID, recpos, SEEK_SET);

  status = extRead(fileID, extp);
  if ( status != 0 ) return (0);

  extInqHeader(extp, header);
  extInqDataDP(extp, data);

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


int extCopyRecord(int ostreamID, int istreamID)
{
  static char func[] = "extCopyRecord";
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


int extDefRecord(int streamID)
{
  static char func[] = "extDefRecord";
  int fileID;
  int gridID;
  int header[4];
  int status = 0;
  int datatype;
  EXTREC *extp;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);
  gridID = streams[streamID].record->gridID;
  extp   = streams[streamID].record->extp;

  header[0] = streams[streamID].record->date;
  header[1] = streams[streamID].record->code;
  header[2] = streams[streamID].record->level;
  header[3] = gridInqSize(gridID);

  datatype = streams[streamID].record->prec;

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

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);
  extp   = streams[streamID].record->extp;

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
	       datatype, &varID, &levelID, 0, 0);

  (*record).varID   = varID;
  (*record).levelID = levelID;

  streams[streamID].tsteps[tsID].nallrecs++;
  streams[streamID].nrecs++;

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
  long levelcode, levelcode0 = 0;
  int tsID;
  int varID;
  long recsize;
  off_t recpos;
  int nrecords, nrecs, recID;
  int taxisID = -1;
  TAXIS *taxis;
  int vlistID;
  EXTREC *extp;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 0;

  extp  = streams[streamID].record->extp;
  tsID  = tstepsNewEntry(streamID);
  taxis = &streams[streamID].tsteps[tsID].taxis;

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
	  streams[streamID].ntsteps = 1;
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

      extAddRecord(streamID, rcode, rlevel, rxysize, recsize, recpos, prec);
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


int extScanTimestep2(int streamID)
{  
  static char func[] = "extScanTimestep2";
  int header[4];
  int status;
  int fileID;
  int rxysize = 0;
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
  EXTREC *extp;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 1;

  fileID  = streamInqFileID(streamID);
  vlistID = streamInqVlist(streamID);
  extp    = streams[streamID].record->extp;

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
      status = extRead(fileID, extp);
      if ( status != 0 )
	{
	  streams[streamID].ntsteps = 2;
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


int extInqContents(int streamID)
{
  static char func[] = "extInqContents";
  int fileID;
  int status = 0;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);

  streams[streamID].curTsID = 0;

  extScanTimestep1(streamID);
 
  if ( streams[streamID].ntsteps == -1 ) status = extScanTimestep2(streamID);

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
  long levelcode, levelcode0 = 0;
  long recsize = 0;
  off_t recpos = 0;
  int recID;
  TAXIS *taxis;
  int rindex, nrecs = 0;
  EXTREC *extp;

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

  extp  = streams[streamID].record->extp;
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
	  status = extRead(fileID, extp);
	  if ( status != 0 )
	    {
	      streams[streamID].ntsteps = streams[streamID].rtsteps + 1;
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
	  recID = streams[streamID].tsteps[tsID].recIDs[rindex];

	  if ( rindex == 0 )
	    {
	      taxis->type  = TAXIS_ABSOLUTE;
	      taxis->vdate = vdate;
	      taxis->vtime = vtime;
	    }
	  /*
	  extCmpRecord(streamID, tsID, nrecs, recpos, rcode, rlevel, rxysize);
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


int extInqTimestep(int streamID, int tsID)
{
  static char func[] = "extInqTimestep";
  int ntsteps, nrecs;

  streamCheckID(func, streamID);

  if ( tsID == 0 && streams[streamID].rtsteps == 0 )
    Error(func, "Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message(func, "tsID = %d rtsteps = %d", tsID, streams[streamID].rtsteps);
  
  ntsteps = UNDEFID;
  while ( ( tsID + 1 ) > streams[streamID].rtsteps && ntsteps == UNDEFID )
    ntsteps = extScanTimestep(streamID);

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

  extp     = streams[streamID].record->extp;
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

  extp     = streams[streamID].record->extp;
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

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  extp     = streams[streamID].record->extp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  nlevs    = zaxisInqSize(zaxisID);

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  header[0] = streams[streamID].tsteps[tsID].taxis.vdate;
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

  extp     = streams[streamID].record->extp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  level    = zaxisInqLevel(zaxisID, levID);

  if ( CDI_Debug )
    Message(func, "gridID = %d zaxisID = %d", gridID, zaxisID);

  header[0] = streams[streamID].tsteps[tsID].taxis.vdate;
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
