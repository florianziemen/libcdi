#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "stream_cgribex.h"
#include "stream_gribapi.h"
#include "file.h"
#include "cgribex.h"  /* gribZip gribGetZip gribGinfo */

/*
int grbInqRecord(int streamID, int *varID, int *levelID)
{
  int status;

  status = cgribexInqRecord(streamID, varID, levelID);

  return (status);
}
*/

int grbDefRecord(int streamID)
{
  static char func[] = "grbDefRecord";
  int fileID;
  int gridID;
  int status = 0;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  fileID = streamInqFileID(streamID);
  gridID = streamptr->record->gridID;

  return (status);
}

static
int grbDecode(int filetype, unsigned char *gribbuffer, int gribsize, double *data, int gridsize,
	      int unreduced, int *nmiss, int *zip)
{
  //static char func[] = "grbDecode";
  int status = 0;

  if ( filetype == FILETYPE_GRB )
    {
      status = cgribexDecode(gribbuffer, gribsize, data, gridsize, unreduced, nmiss, zip);
    }
  else
    {
      status = gribapiDecode(gribbuffer, gribsize, data, gridsize, unreduced, nmiss, zip);
    }
 
  return (status);
}


int grbReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "grbReadRecord";
  int status = 0;
  unsigned char *gribbuffer;
  int fileID;
  int recID, vrecID, tsID, gridID, varID;
  long recsize;
  off_t recpos;
  int gridsize;
  int vlistID;
  int zip;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  filetype = streamptr->filetype;

  gribbuffer = (unsigned char *) streamptr->record->buffer;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  tsID    = streamptr->curTsID;
  vrecID  = streamptr->tsteps[tsID].curRecID;
  recID   = streamptr->tsteps[tsID].recIDs[vrecID];
  recpos  = streamptr->tsteps[tsID].records[recID].position;
  recsize = streamptr->tsteps[tsID].records[recID].size;
  varID   = streamptr->tsteps[tsID].records[recID].varID;

  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);

  streamptr->numvals += gridsize;

  fileSetPos(fileID, recpos, SEEK_SET);

  fileRead(fileID, gribbuffer, (size_t) recsize);

  grbDecode(filetype, gribbuffer, recsize, data, gridsize, streamptr->unreduced, nmiss, &zip);

  streamptr->tsteps[tsID].records[recID].zip = zip;

  return (status);
}

static
void grbScanTimestep1(int streamID)
{
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);
  filetype  = streamptr->filetype;

  if ( filetype == FILETYPE_GRB )
    {
      cgribexScanTimestep1(streamID);
    }
  else
    {
      gribapiScanTimestep1(streamID);
    }
}

static
int grbScanTimestep2(int streamID)
{
  int status;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);
  filetype  = streamptr->filetype;

  if ( filetype == FILETYPE_GRB )
    {
      status = cgribexScanTimestep2(streamID);
    }
  else
    {
      status = gribapiScanTimestep2(streamID);
    }

  return (status);
}

static
int grbScanTimestep(int streamID)
{
  int status;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);
  filetype  = streamptr->filetype;

  if ( filetype == FILETYPE_GRB )
    {
      status = cgribexScanTimestep(streamID);
    }
  else
    {
      status = gribapiScanTimestep(streamID);
    }

  return (status);
}


int grbInqContents(int streamID)
{
  static char func[] = "grbInqContents";
  int fileID;
  int status = 0;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  fileID = streamInqFileID(streamID);

  streamptr->curTsID = 0;

  grbScanTimestep1(streamID);
 
  if ( streamptr->ntsteps == -1 ) status = grbScanTimestep2(streamID);

  fileSetPos(fileID, 0, SEEK_SET);

  return (status);
}


int grbInqTimestep(int streamID, int tsID)
{
  static char func[] = "grbInqTimestep";
  int ntsteps, nrecs;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  if ( tsID == 0 && streamptr->rtsteps == 0 )
    Error(func, "Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message(func, "tsid = %d rtsteps = %d", tsID, streamptr->rtsteps);
  
  ntsteps = CDI_UNDEFID;
  while ( (tsID + 1) > streamptr->rtsteps && ntsteps == CDI_UNDEFID )
    {
      ntsteps = grbScanTimestep(streamID);
      if ( ntsteps == CDI_EUFSTRUCT )
	{
	  streamptr->ntsteps = streamptr->rtsteps;
	  break;
	}
    }

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


void grbReadVarDP(int streamID, int varID, double *data, int *nmiss)
{
  static char func[] = "grbReadVarDP";
  int fileID;
  int levelID, nlevs, gridID, gridsize;
  unsigned char *gribbuffer;
  int tsID, recID;
  long recsize;
  off_t recpos, currentfilepos;
  int imiss;
  int vlistID;
  int zip;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  filetype = streamptr->filetype;

  gribbuffer = (unsigned char *) streamptr->record->buffer;

  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streamptr->curTsID;

  nlevs    = streamptr->vars[varID].nlevs;
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  *nmiss = 0;
  for ( levelID = 0; levelID < nlevs; levelID++ )
    {
      recID   = streamptr->vars[varID].level[levelID];
      recpos  = streamptr->tsteps[tsID].records[recID].position;
      recsize = streamptr->tsteps[tsID].records[recID].size;

      fileSetPos(fileID, recpos, SEEK_SET);

      fileRead(fileID, gribbuffer, recsize);

      grbDecode(filetype, gribbuffer, recsize, &data[levelID*gridsize], gridsize, streamptr->unreduced, &imiss, &zip);

      *nmiss += imiss;

      streamptr->tsteps[tsID].records[recID].zip = zip;
    }

  fileSetPos(fileID, currentfilepos, SEEK_SET);
}


void grbReadVarSliceDP(int streamID, int varID, int levelID, double *data, int *nmiss)
{
  static char func[] = "grbReadVarSliceDP";
  int fileID;
  int gridID, gridsize;
  unsigned char *gribbuffer;
  long recsize;
  off_t recpos, currentfilepos;
  int tsID, recID;
  int vlistID;
  int zip;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  filetype = streamptr->filetype;

  gribbuffer = (unsigned char *) streamptr->record->buffer;

  vlistID  = streamInqVlist(streamID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsID     = streamptr->curTsID;

  if ( CDI_Debug )
    Message(func, "gridID = %d gridsize = %d", gridID, gridsize);

  fileID = streamInqFileID(streamID);

  currentfilepos = fileGetPos(fileID);

  recID   = streamptr->vars[varID].level[levelID];
  recpos  = streamptr->tsteps[tsID].records[recID].position;
  recsize = streamptr->tsteps[tsID].records[recID].size;

  if ( recsize == 0 )
    Error(func, "Internal problem! Recordsize is zero for record %d at timestep %d",
	  recID+1, tsID+1);

  fileSetPos(fileID, recpos, SEEK_SET);

  fileRead(fileID, gribbuffer, recsize);

  grbDecode(filetype, gribbuffer, recsize, data, gridsize, streamptr->unreduced, nmiss, &zip);

  fileSetPos(fileID, currentfilepos, SEEK_SET);

  streamptr->tsteps[tsID].records[recID].zip = zip;
}

static
size_t grbEncode(int filetype, int varID, int levelID, int vlistID, int gridID, int zaxisID, int date, int time, int numavg, 
		 long datasize, const double *data, int nmiss, unsigned char *gribbuffer, size_t gribbuffersize)
{
  size_t nbytes;

  if ( filetype == FILETYPE_GRB )
    {
      nbytes = cgribexEncode(varID, levelID, vlistID, gridID, zaxisID, date, time, numavg, 
			     datasize, data, nmiss, gribbuffer, gribbuffersize);
    }
  else
    {
      nbytes = gribapiEncode(varID, levelID, vlistID, gridID, zaxisID, date, time, numavg, 
			     datasize, data, nmiss, gribbuffer, gribbuffersize);
    }

  return (nbytes);
}

static
size_t grbSzip(int filetype, unsigned char *gribbuffer, size_t gribbuffersize)
{
  static char func[] = "grbSzip";
  size_t nbytes = 0;
  unsigned char *buffer;
  size_t buffersize;

  buffersize = gribbuffersize;
  buffer = (unsigned char *) malloc(buffersize);

  memcpy(buffer, gribbuffer, gribbuffersize);
  
  if ( filetype == FILETYPE_GRB )
    {
      nbytes = gribZip(gribbuffer, (long) gribbuffersize, buffer, (long) buffersize);
    }
  else
    {
      Warning(func, "Szip compression of GRIB2 not implemented!");
      nbytes = gribbuffersize;
    }
      
  free(buffer);

  return (nbytes);
}


int grbWriteVarSliceDP(int streamID, int varID, int levelID, const double *data, int nmiss)
{
  static char func[] = "grbWriteVarSliceDP";
  size_t nwrite;
  int fileID;
  int gridID;
  int zaxisID;
  unsigned char *gribbuffer = NULL;
  size_t gribbuffersize = 0;
  long datasize;
  int tsID;
  int vlistID;
  int date, time;
  int numavg = 0;
  size_t nbytes;
  int filetype;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  filetype = streamptr->filetype;

  fileID   = streamInqFileID(streamID);
  vlistID  = streamInqVlist(streamID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);

  tsID     = streamptr->curTsID;
  date     = streamptr->tsteps[tsID].taxis.vdate;
  time     = streamptr->tsteps[tsID].taxis.vtime;
  if ( vlistInqVarTimave(vlistID, varID) )
    numavg = streamptr->tsteps[tsID].taxis.numavg;

  if ( CDI_Debug )
    Message(func, "gridID = %d zaxisID = %d", gridID, zaxisID);

  datasize = gridInqSize(gridID);

  gribbuffersize = datasize*4+3000;
  gribbuffer = (unsigned char *) malloc(gribbuffersize);

  nbytes = grbEncode(filetype, varID, levelID, vlistID, gridID, zaxisID, date, time, numavg, 
		     datasize, data, nmiss, gribbuffer, gribbuffersize);

  if ( streamptr->ztype == COMPRESS_SZIP )
    nbytes = grbSzip(filetype, gribbuffer, nbytes);

  nwrite = fileWrite(fileID, gribbuffer, nbytes);
  if ( nwrite != nbytes ) perror(func);

  if ( gribbuffer ) free(gribbuffer);

  return ((int)nwrite);
}


void grbWriteVarDP(int streamID, int varID, const double *data, int nmiss)
{
  int vlistID, zaxisID, levelID, nlevs;

  vlistID = streamInqVlist(streamID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  nlevs   = zaxisInqSize(zaxisID);

  for ( levelID = 0; levelID < nlevs; levelID++ )
    {
      grbWriteVarSliceDP(streamID, varID, levelID, data, nmiss);
    }
}


int grbCopyRecord(int streamID2, int streamID1)
{
  static char func[] = "grbCopyRecord";
  int fileID1, fileID2;
  int tsID, recID, vrecID;
  long recsize;
  size_t gribbuffersize;
  off_t recpos;
  size_t nwrite;
  unsigned char *gribbuffer;
  int filetype;
  size_t nbytes;
  long unzipsize;
  int izip;
  stream_t *streamptr1;
  stream_t *streamptr2;

  streamptr1 = stream_to_pointer(streamID1);
  streamptr2 = stream_to_pointer(streamID2);

  stream_check_ptr(func, streamptr1);
  stream_check_ptr(func, streamptr2);

  filetype = streamptr1->filetype;

  fileID1 = streamInqFileID(streamID1);
  fileID2 = streamInqFileID(streamID2);

  tsID    = streamptr1->curTsID;
  vrecID  = streamptr1->tsteps[tsID].curRecID;
  recID   = streamptr1->tsteps[tsID].recIDs[vrecID];
  recpos  = streamptr1->tsteps[tsID].records[recID].position;
  recsize = streamptr1->tsteps[tsID].records[recID].size;

  fileSetPos(fileID1, recpos, SEEK_SET);

  gribbuffersize = recsize == (recsize>>3)<<3 ? recsize : (1+(recsize>>3))<<3;

  gribbuffer = (unsigned char *) malloc(gribbuffersize);

  fileRead(fileID1, gribbuffer, recsize);

  nbytes = recsize;

  izip = gribGetZip(recsize, gribbuffer, &unzipsize);
  
  if ( izip == 0 )
    if ( streamptr2->ztype == COMPRESS_SZIP )
      nbytes = grbSzip(filetype, gribbuffer, nbytes);

  while ( nbytes & 7 ) gribbuffer[nbytes++] = 0;

  nwrite = fileWrite(fileID2, gribbuffer, nbytes);
  if ( nwrite != nbytes ) perror(func);

  free(gribbuffer);

  return ((int)nwrite);
}


int grbWriteRecord(int streamID, const double *data, int nmiss)
{
  static char func[] = "grbWriteRecord";
  int status = 0;
  int varID, levelID;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  varID   = streamptr->record->varID;
  levelID = streamptr->record->levelID;

  status = grbWriteVarSliceDP(streamID, varID, levelID, data, nmiss);

  return (status);
}


void streamInqGinfo(int streamID, int *intnum, float *fltnum)
{
  static char func[] = "streamInqGinfo";
  int recID, vrecID, tsID;
  int filetype;
  void *gribbuffer;
  long recsize;
  long gribbuffersize;
  off_t recpos;
  int zip;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  filetype = streamptr->filetype;

  if ( filetype == FILETYPE_GRB )
    {
      tsID    = streamptr->curTsID;
      vrecID  = streamptr->tsteps[tsID].curRecID;
      recID   = streamptr->tsteps[tsID].recIDs[vrecID];
      recpos  = streamptr->tsteps[tsID].records[recID].position;
      recsize = streamptr->tsteps[tsID].records[recID].size;
      zip     = streamptr->tsteps[tsID].records[recID].zip;

      gribbuffer = streamptr->record->buffer;
      gribbuffersize = streamptr->record->buffersize;

      if ( zip > 0 )
	Error(func, "Compressed GRIB records unsupported!");
      else
	gribGinfo(recpos, gribbuffersize, (unsigned char *) gribbuffer, intnum, fltnum);
    }
}
