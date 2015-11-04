#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "stream_cgribex.h"
#include "stream_grb.h"
#include "stream_gribapi.h"
#include "file.h"
#include "cgribex.h"  /* gribZip gribGetZip gribGinfo */
#include "gribapi.h"
#include "namespace.h"



int grib1ltypeToZaxisType(int grib_ltype)
{
  int zaxistype = ZAXIS_GENERIC;

  switch ( grib_ltype )
    {
    case GRIB1_LTYPE_SURFACE:            { zaxistype = ZAXIS_SURFACE;                break; }
    case GRIB1_LTYPE_CLOUD_BASE:         { zaxistype = ZAXIS_CLOUD_BASE;             break; }
    case GRIB1_LTYPE_CLOUD_TOP:          { zaxistype = ZAXIS_CLOUD_TOP;              break; }
    case GRIB1_LTYPE_ISOTHERM0:          { zaxistype = ZAXIS_ISOTHERM_ZERO;          break; }
    case GRIB1_LTYPE_TOA:                { zaxistype = ZAXIS_TOA;                    break; }
    case GRIB1_LTYPE_SEA_BOTTOM:         { zaxistype = ZAXIS_SEA_BOTTOM;             break; }
    case GRIB1_LTYPE_ATMOSPHERE:         { zaxistype = ZAXIS_ATMOSPHERE;             break; }
    case GRIB1_LTYPE_MEANSEA:            { zaxistype = ZAXIS_MEANSEA;                break; }
    case GRIB1_LTYPE_99:
    case GRIB1_LTYPE_ISOBARIC:           { zaxistype = ZAXIS_PRESSURE;               break; }
    case GRIB1_LTYPE_HEIGHT:             { zaxistype = ZAXIS_HEIGHT;                 break; }
    case GRIB1_LTYPE_ALTITUDE:           { zaxistype = ZAXIS_ALTITUDE;	             break; }
    case GRIB1_LTYPE_SIGMA:
    case GRIB1_LTYPE_SIGMA_LAYER:        { zaxistype = ZAXIS_SIGMA;	             break; }
    case GRIB1_LTYPE_HYBRID:
    case GRIB1_LTYPE_HYBRID_LAYER:       { zaxistype = ZAXIS_HYBRID;	             break; }
    case GRIB1_LTYPE_LANDDEPTH:
    case GRIB1_LTYPE_LANDDEPTH_LAYER:    { zaxistype = ZAXIS_DEPTH_BELOW_LAND;       break; }
    case GRIB1_LTYPE_ISENTROPIC:         { zaxistype = ZAXIS_ISENTROPIC;             break; }
    case GRIB1_LTYPE_SEADEPTH:           { zaxistype = ZAXIS_DEPTH_BELOW_SEA;        break; }
    case GRIB1_LTYPE_LAKE_BOTTOM:        { zaxistype = ZAXIS_LAKE_BOTTOM;            break; }
    case GRIB1_LTYPE_SEDIMENT_BOTTOM:    { zaxistype = ZAXIS_SEDIMENT_BOTTOM;        break; }
    case GRIB1_LTYPE_SEDIMENT_BOTTOM_TA: { zaxistype = ZAXIS_SEDIMENT_BOTTOM_TA;     break; }
    case GRIB1_LTYPE_SEDIMENT_BOTTOM_TW: { zaxistype = ZAXIS_SEDIMENT_BOTTOM_TW;     break; }
    case GRIB1_LTYPE_MIX_LAYER:          { zaxistype = ZAXIS_MIX_LAYER;              break; }
    }

  return (zaxistype);
}


int grib2ltypeToZaxisType(int grib_ltype)
{
  int zaxistype = ZAXIS_GENERIC;

  switch ( grib_ltype )
    {
    case GRIB2_LTYPE_SURFACE:            { zaxistype = ZAXIS_SURFACE;                break; }
    case GRIB2_LTYPE_CLOUD_BASE:         { zaxistype = ZAXIS_CLOUD_BASE;             break; }
    case GRIB2_LTYPE_CLOUD_TOP:          { zaxistype = ZAXIS_CLOUD_TOP;              break; }
    case GRIB2_LTYPE_ISOTHERM0:          { zaxistype = ZAXIS_ISOTHERM_ZERO;          break; }
    case GRIB2_LTYPE_TOA:                { zaxistype = ZAXIS_TOA;                    break; }
    case GRIB2_LTYPE_SEA_BOTTOM:         { zaxistype = ZAXIS_SEA_BOTTOM;             break; }
    case GRIB2_LTYPE_ATMOSPHERE:         { zaxistype = ZAXIS_ATMOSPHERE;             break; }
    case GRIB2_LTYPE_MEANSEA:            { zaxistype = ZAXIS_MEANSEA;                break; }
    case GRIB2_LTYPE_ISOBARIC:           { zaxistype = ZAXIS_PRESSURE;               break; }
    case GRIB2_LTYPE_HEIGHT:             { zaxistype = ZAXIS_HEIGHT;                 break; }
    case GRIB2_LTYPE_ALTITUDE:           { zaxistype = ZAXIS_ALTITUDE;               break; }
    case GRIB2_LTYPE_SIGMA:              { zaxistype = ZAXIS_SIGMA;                  break; }
    case GRIB2_LTYPE_HYBRID:
 /* case GRIB2_LTYPE_HYBRID_LAYER: */    { zaxistype = ZAXIS_HYBRID;                 break; }
    case GRIB2_LTYPE_LANDDEPTH:
 /* case GRIB2_LTYPE_LANDDEPTH_LAYER: */ { zaxistype = ZAXIS_DEPTH_BELOW_LAND;       break; }
    case GRIB2_LTYPE_ISENTROPIC:         { zaxistype = ZAXIS_ISENTROPIC;             break; }
    case GRIB2_LTYPE_SNOW:               { zaxistype = ZAXIS_SNOW;                   break; }
    case GRIB2_LTYPE_SEADEPTH:           { zaxistype = ZAXIS_DEPTH_BELOW_SEA;        break; }
    case GRIB2_LTYPE_LAKE_BOTTOM:        { zaxistype = ZAXIS_LAKE_BOTTOM;            break; }
    case GRIB2_LTYPE_SEDIMENT_BOTTOM:    { zaxistype = ZAXIS_SEDIMENT_BOTTOM;        break; }
    case GRIB2_LTYPE_SEDIMENT_BOTTOM_TA: { zaxistype = ZAXIS_SEDIMENT_BOTTOM_TA;     break; }
    case GRIB2_LTYPE_SEDIMENT_BOTTOM_TW: { zaxistype = ZAXIS_SEDIMENT_BOTTOM_TW;     break; }
    case GRIB2_LTYPE_MIX_LAYER:          { zaxistype = ZAXIS_MIX_LAYER;              break; }
    case GRIB2_LTYPE_REFERENCE:          { zaxistype = ZAXIS_REFERENCE;              break; }
    }

  return (zaxistype);
}


int zaxisTypeToGrib1ltype(int zaxistype)
{
  int grib_ltype = -1;

  switch (zaxistype)
    {
    case ZAXIS_SURFACE:               { grib_ltype = GRIB1_LTYPE_SURFACE;            break; }
    case ZAXIS_MEANSEA:               { grib_ltype = GRIB1_LTYPE_MEANSEA;            break; }
    case ZAXIS_HEIGHT:                { grib_ltype = GRIB1_LTYPE_HEIGHT;             break; }
    case ZAXIS_ALTITUDE:              { grib_ltype = GRIB1_LTYPE_ALTITUDE;           break; }
    case ZAXIS_SIGMA:                 { grib_ltype = GRIB1_LTYPE_SIGMA;              break; }
    case ZAXIS_DEPTH_BELOW_SEA:       { grib_ltype = GRIB1_LTYPE_SEADEPTH;           break; }
    case ZAXIS_ISENTROPIC:            { grib_ltype = GRIB1_LTYPE_ISENTROPIC;         break; }
    case ZAXIS_CLOUD_BASE:            { grib_ltype = GRIB1_LTYPE_CLOUD_BASE;         break; }
    case ZAXIS_CLOUD_TOP:             { grib_ltype = GRIB1_LTYPE_CLOUD_TOP;          break; }
    case ZAXIS_ISOTHERM_ZERO:         { grib_ltype = GRIB1_LTYPE_ISOTHERM0;          break; }
    case ZAXIS_TOA:                   { grib_ltype = GRIB1_LTYPE_TOA;                break; }
    case ZAXIS_SEA_BOTTOM:            { grib_ltype = GRIB1_LTYPE_SEA_BOTTOM;         break; }
    case ZAXIS_LAKE_BOTTOM:           { grib_ltype = GRIB1_LTYPE_LAKE_BOTTOM;        break; }
    case ZAXIS_SEDIMENT_BOTTOM:       { grib_ltype = GRIB1_LTYPE_SEDIMENT_BOTTOM;    break; }
    case ZAXIS_SEDIMENT_BOTTOM_TA:    { grib_ltype = GRIB1_LTYPE_SEDIMENT_BOTTOM_TA; break; }
    case ZAXIS_SEDIMENT_BOTTOM_TW:    { grib_ltype = GRIB1_LTYPE_SEDIMENT_BOTTOM_TW; break; }
    case ZAXIS_MIX_LAYER:             { grib_ltype = GRIB1_LTYPE_MIX_LAYER;          break; }
    case ZAXIS_ATMOSPHERE:            { grib_ltype = GRIB1_LTYPE_ATMOSPHERE;         break; }
    }

  return (grib_ltype);
}


int zaxisTypeToGrib2ltype(int zaxistype)
{
  int grib_ltype = -1;

  switch (zaxistype)
    {
    case ZAXIS_SURFACE:               { grib_ltype = GRIB2_LTYPE_SURFACE;            break; }
    case ZAXIS_MEANSEA:               { grib_ltype = GRIB2_LTYPE_MEANSEA;            break; }
    case ZAXIS_HEIGHT:                { grib_ltype = GRIB2_LTYPE_HEIGHT;             break; }
    case ZAXIS_ALTITUDE:              { grib_ltype = GRIB2_LTYPE_ALTITUDE;           break; }
    case ZAXIS_SIGMA:                 { grib_ltype = GRIB2_LTYPE_SIGMA;              break; }
    case ZAXIS_DEPTH_BELOW_SEA:       { grib_ltype = GRIB2_LTYPE_SEADEPTH;           break; }
    case ZAXIS_ISENTROPIC:            { grib_ltype = GRIB2_LTYPE_ISENTROPIC;         break; }
    case ZAXIS_CLOUD_BASE:            { grib_ltype = GRIB2_LTYPE_CLOUD_BASE;         break; }
    case ZAXIS_CLOUD_TOP:             { grib_ltype = GRIB2_LTYPE_CLOUD_TOP;          break; }
    case ZAXIS_ISOTHERM_ZERO:         { grib_ltype = GRIB2_LTYPE_ISOTHERM0;          break; }
    case ZAXIS_TOA:                   { grib_ltype = GRIB2_LTYPE_TOA;                break; }
    case ZAXIS_SEA_BOTTOM:            { grib_ltype = GRIB2_LTYPE_SEA_BOTTOM;         break; }
    case ZAXIS_LAKE_BOTTOM:           { grib_ltype = GRIB2_LTYPE_LAKE_BOTTOM;        break; }
    case ZAXIS_SEDIMENT_BOTTOM:       { grib_ltype = GRIB2_LTYPE_SEDIMENT_BOTTOM;    break; }
    case ZAXIS_SEDIMENT_BOTTOM_TA:    { grib_ltype = GRIB2_LTYPE_SEDIMENT_BOTTOM_TA; break; }
    case ZAXIS_SEDIMENT_BOTTOM_TW:    { grib_ltype = GRIB2_LTYPE_SEDIMENT_BOTTOM_TW; break; }
    case ZAXIS_MIX_LAYER:             { grib_ltype = GRIB2_LTYPE_MIX_LAYER;          break; }
    case ZAXIS_ATMOSPHERE:            { grib_ltype = GRIB2_LTYPE_ATMOSPHERE;         break; }
    }

  return (grib_ltype);
}


int grbBitsPerValue(int datatype)
{
  int bitsPerValue = 16;

  if ( datatype == DATATYPE_CPX32 || datatype == DATATYPE_CPX64 )
    Error("CDI/GRIB library does not support complex numbers!");

  if ( datatype != CDI_UNDEFID )
    {
      if ( datatype > 0 && datatype <= 32 )
	bitsPerValue = datatype;
      else if ( datatype == DATATYPE_FLT64 )
	bitsPerValue = 24;
      else
	bitsPerValue = 16;
    }

  return (bitsPerValue);
}


/*
int grbInqRecord(stream_t * streamptr, int *varID, int *levelID)
{
  int status;

  status = cgribexInqRecord(streamptr, varID, levelID);

  return (status);
}
*/

void grbDefRecord(stream_t * streamptr)
{
  UNUSED(streamptr);
}

static
int grbDecode(int filetype, unsigned char *gribbuffer, int gribsize, double *data, int gridsize,
	      int unreduced, int *nmiss, double missval, int vlistID, int varID)
{
  int status = 0;

#if  defined  (HAVE_LIBCGRIBEX)
  if ( filetype == FILETYPE_GRB )
    {
#if  defined  (HAVE_LIBGRIB_API)
      extern int cdiNAdditionalGRIBKeys;
      if ( cdiNAdditionalGRIBKeys > 0 )
	Error("CGRIBEX decode does not support reading of additional GRIB keys!");
#endif
      status = cgribexDecode(gribbuffer, gribsize, data, gridsize, unreduced, nmiss, missval);
    }
  else
#endif
#ifdef HAVE_LIBGRIB_API
    status = gribapiDecode(gribbuffer, gribsize, data, gridsize, unreduced, nmiss, missval, vlistID, varID);
#else
    {
      (void)vlistID; (void)varID;
      Error("GRIB_API support not compiled in!");
    }
#endif

  return (status);
}


static int grbUnzipRecord(unsigned char *gribbuffer, size_t *gribsize)
{
  int zip = 0;
  int izip;
  size_t igribsize;
  size_t ogribsize;
  long unzipsize;

  igribsize = *gribsize;
  ogribsize = *gribsize;

  if ( (izip = gribGetZip((long)igribsize, gribbuffer, &unzipsize)) > 0 )
    {
      zip = izip;
      if ( izip == 128 ) /* szip */
	{
	  unsigned char *itmpbuffer = NULL;
	  size_t itmpbuffersize = 0;

	  if ( unzipsize < (long) igribsize )
	    {
	      fprintf(stderr, "Decompressed size smaller than compressed size (in %ld; out %ld)!\n", (long)igribsize, unzipsize);
	      return (0);
	    }

	  if ( itmpbuffersize < igribsize )
	    {
	      itmpbuffersize = igribsize;
	      itmpbuffer = (unsigned char *) Realloc(itmpbuffer, itmpbuffersize);
	    }

	  memcpy(itmpbuffer, gribbuffer, itmpbuffersize);

	  unzipsize += 100; /* need 0 to 1 bytes for rounding of bds */

	  ogribsize = (size_t)gribUnzip(gribbuffer, unzipsize, itmpbuffer, (long)igribsize);

	  Free(itmpbuffer);

	  if ( ogribsize <= 0 ) Error("Decompression problem!");
	}
      else
	{
	  Error("Decompression for %d not implemented!", izip);
	}
    }

  *gribsize = ogribsize;

  return zip;
}


void grbReadRecord(stream_t * streamptr, double *data, int *nmiss)
{
  int filetype = streamptr->filetype;

  unsigned char *gribbuffer = (unsigned char *) streamptr->record->buffer;

  int vlistID = streamptr->vlistID;
  int fileID  = streamptr->fileID;
  int tsID    = streamptr->curTsID;
  int vrecID  = streamptr->tsteps[tsID].curRecID;
  int recID   = streamptr->tsteps[tsID].recIDs[vrecID];
  off_t recpos  = streamptr->tsteps[tsID].records[recID].position;
  size_t recsize = streamptr->tsteps[tsID].records[recID].size;
  int varID   = streamptr->tsteps[tsID].records[recID].varID;

  int gridID   = vlistInqVarGrid(vlistID, varID);
  int gridsize = gridInqSize(gridID);

  streamptr->numvals += gridsize;

  fileSetPos(fileID, recpos, SEEK_SET);

  if (fileRead(fileID, gribbuffer, recsize) != recsize)
    Error("Failed to read GRIB record");

  double missval = vlistInqVarMissval(vlistID, varID);

  streamptr->tsteps[tsID].records[recID].zip = grbUnzipRecord(gribbuffer, &recsize);

  grbDecode(filetype, gribbuffer, (int)recsize, data, gridsize, streamptr->unreduced, nmiss, missval, vlistID, varID);
}

static
int grbScanTimestep1(stream_t * streamptr)
{
  int status = CDI_EUFTYPE;

#if  defined  (HAVE_LIBCGRIBEX)
  int filetype  = streamptr->filetype;

  if ( filetype == FILETYPE_GRB )
    status = cgribexScanTimestep1(streamptr);
#endif
#if defined(HAVE_LIBCGRIBEX) && defined (HAVE_LIBGRIB_API)
  else
#endif
#ifdef HAVE_LIBGRIB_API
    status = gribapiScanTimestep1(streamptr);
#endif

  return (status);
}

static
int grbScanTimestep2(stream_t * streamptr)
{
  int status = CDI_EUFTYPE;

#if  defined  (HAVE_LIBCGRIBEX)
  int filetype = streamptr->filetype;

  if ( filetype == FILETYPE_GRB )
    {
      status = cgribexScanTimestep2(streamptr);
    }
#endif
#if defined(HAVE_LIBCGRIBEX) && defined (HAVE_LIBGRIB_API)
  else
#endif
#ifdef HAVE_LIBGRIB_API
    status = gribapiScanTimestep2(streamptr);
#endif

  return (status);
}

static
int grbScanTimestep(stream_t * streamptr)
{
  int status = CDI_EUFTYPE;
  int filetype;

  filetype  = streamptr->filetype;

#if  defined  (HAVE_LIBCGRIBEX)
  if ( filetype == FILETYPE_GRB )
    {
      status = cgribexScanTimestep(streamptr);
    }
  else
#endif
#ifdef HAVE_LIBGRIB_API
    status = gribapiScanTimestep(streamptr);
#else
    Error("Sufficient GRIB support unavailable!");
#endif

  return (status);
}


#if  defined  (HAVE_LIBGRIB)
int grbInqContents(stream_t * streamptr)
{
  int fileID;
  int status = 0;

  fileID = streamptr->fileID;

  streamptr->curTsID = 0;

  status = grbScanTimestep1(streamptr);

  if ( status == 0 && streamptr->ntsteps == -1 ) status = grbScanTimestep2(streamptr);

  fileSetPos(fileID, 0, SEEK_SET);

  return (status);
}
#endif

int grbInqTimestep(stream_t * streamptr, int tsID)
{
  int ntsteps, nrecs;

  if ( tsID == 0 && streamptr->rtsteps == 0 )
    Error("Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message("tsid = %d rtsteps = %d", tsID, streamptr->rtsteps);

  ntsteps = CDI_UNDEFID;
  while ( (tsID + 1) > streamptr->rtsteps && ntsteps == CDI_UNDEFID )
    {
      ntsteps = grbScanTimestep(streamptr);
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


void grbReadVarDP(stream_t * streamptr, int varID, double *data, int *nmiss)
{
  int filetype = streamptr->filetype;

  unsigned char *gribbuffer = (unsigned char *) streamptr->record->buffer;

  int vlistID = streamptr->vlistID;
  int fileID  = streamptr->fileID;
  int tsID    = streamptr->curTsID;

  int gridID   = vlistInqVarGrid(vlistID, varID);
  int gridsize = gridInqSize(gridID);

  off_t currentfilepos = fileGetPos(fileID);


  int isub     = subtypeInqActiveIndex(streamptr->vars[varID].subtypeID);
  int nlevs    = streamptr->vars[varID].recordTable[0].nlevs;
  if ( CDI_Debug )
    Message("nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);
  *nmiss = 0;
  for (int levelID = 0; levelID < nlevs; levelID++ )
    {
      int    recID   = streamptr->vars[varID].recordTable[isub].recordID[levelID];
      off_t  recpos  = streamptr->tsteps[tsID].records[recID].position;
      size_t recsize = streamptr->tsteps[tsID].records[recID].size;

      fileSetPos(fileID, recpos, SEEK_SET);

      fileRead(fileID, gribbuffer, recsize);

      double missval = vlistInqVarMissval(vlistID, varID);

      int imiss;

      streamptr->tsteps[tsID].records[recID].zip = grbUnzipRecord(gribbuffer, &recsize);

      grbDecode(filetype, gribbuffer, (int)recsize, &data[levelID*gridsize], gridsize,
                streamptr->unreduced, &imiss, missval, vlistID, varID);

      *nmiss += imiss;
    }

  fileSetPos(fileID, currentfilepos, SEEK_SET);
}


void grbReadVarSliceDP(stream_t * streamptr, int varID, int levelID, double *data, int *nmiss)
{
  int filetype = streamptr->filetype;

  unsigned char *gribbuffer = (unsigned char *) streamptr->record->buffer;

  int vlistID = streamptr->vlistID;
  int gridID   = vlistInqVarGrid(vlistID, varID);
  int gridsize = gridInqSize(gridID);
  int tsID = streamptr->curTsID;

  if ( CDI_Debug )
    Message("gridID = %d gridsize = %d", gridID, gridsize);

  int fileID = streamptr->fileID;

  off_t currentfilepos = fileGetPos(fileID);

  int    isub    = subtypeInqActiveIndex(streamptr->vars[varID].subtypeID);


  int    recID   = streamptr->vars[varID].recordTable[isub].recordID[levelID];
  off_t  recpos  = streamptr->tsteps[tsID].records[recID].position;
  size_t recsize = streamptr->tsteps[tsID].records[recID].size;

  if ( recsize == 0 )
    Error("Internal problem! Recordsize is zero for record %d at timestep %d",
	  recID+1, tsID+1);

  fileSetPos(fileID, recpos, SEEK_SET);

  fileRead(fileID, gribbuffer, recsize);

  double missval = vlistInqVarMissval(vlistID, varID);

  streamptr->tsteps[tsID].records[recID].zip = grbUnzipRecord(gribbuffer, &recsize);

  grbDecode(filetype, gribbuffer, (int)recsize, data, gridsize, streamptr->unreduced, nmiss, missval, vlistID, varID);

  fileSetPos(fileID, currentfilepos, SEEK_SET);
}


void streamInqGRIBinfo(int streamID, int *intnum, float *fltnum, off_t *bignum)
{
  stream_t *streamptr = stream_to_pointer(streamID);

  int filetype = streamptr->filetype;

  if ( filetype == FILETYPE_GRB )
    {
      int tsID     = streamptr->curTsID;
      int vrecID   = streamptr->tsteps[tsID].curRecID;
      int recID    = streamptr->tsteps[tsID].recIDs[vrecID];
      off_t recpos = streamptr->tsteps[tsID].records[recID].position;
      int zip      = streamptr->tsteps[tsID].records[recID].zip;

      void *gribbuffer = streamptr->record->buffer;
      size_t gribbuffersize = streamptr->record->buffersize;

      if ( zip > 0 )
	Error("Compressed GRIB records unsupported!");
      else
        grib_info_for_grads(recpos, (long)gribbuffersize, (unsigned char *) gribbuffer, intnum, fltnum, bignum);
    }
}
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
