#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cdi.h"
#include "cdi_int.h"
#include "stream_cgribex.h"
#include "stream_grb.h"
#include "stream_gribapi.h"
#include "gribapi.h"
#include "file.h"
#include "cgribex.h"  /* gribZip gribGetZip gribGinfo */

int cdiDebugExt                        =  0;      //  Debug level for the KNMI extensions
#ifdef HIRLAM_EXTENSIONS
// *** RELATED to GRIB only ***
int cdiGribUseTimeRangeIndicator        = 0;       // normaly cdo looks in grib for attribute called "stepType"
                                                   // but NWP models such as Harmonie 37h1.2, use "timeRangeIndicator"
                                                   // where:  0: for instanteneous fields; 4: for accumulated fields
#endif // HIRLAM_EXTENSIONS


// Regarding operation to change parameter identification:
// change if cdiGribChangeParameterID.active
struct cdiGribParamChange cdiGribChangeParameterID;

// Used only for CDO module Selmulti
void streamGrbChangeParameterIdentification(int code, int ltype, int lev)
{
  // NOTE this is a "PROXY" function for gribapiChangeParameterIdentification();
  // This just sets the globals. There are probably better solutions to this.
  // The parameter change is done by function  gribapiChangeParameterIdentification() in stream_gribapi.c
  // Setting this control variable to true will cause calling fnc. gribapiChangeParameterIdentification later.
  // After grib attributes have been changed this variable goes to false.
  cdiGribChangeParameterID.active = true;
  cdiGribChangeParameterID.code = code;
  cdiGribChangeParameterID.ltype = ltype;
  cdiGribChangeParameterID.lev = lev;
}

struct cdiGribModeChange cdiGribChangeModeUvRelativeToGrid;

// Used only for CDO module WindTrans
void streamGrbChangeModeUvRelativeToGrid(int mode)
{
  cdiGribChangeModeUvRelativeToGrid.active = true;
  cdiGribChangeModeUvRelativeToGrid.mode = (mode > 0);
}

struct cdiGribScanModeChange cdiGribDataScanningMode;

void streamGrbDefDataScanningMode(int scanmode)
{
  cdiGribDataScanningMode.active = true;
  cdiGribDataScanningMode.value = scanmode;
}

int  streamGrbInqDataScanningMode(void)
{
  return cdiGribDataScanningMode.value;
}


enum { grib1ltypeMapSize = 26 };
static const int grib1ltypeMap[2][grib1ltypeMapSize] =
{
  {
    GRIB1_LTYPE_SURFACE,
    GRIB1_LTYPE_CLOUD_BASE,
    GRIB1_LTYPE_CLOUD_TOP,
    GRIB1_LTYPE_ISOTHERM0,
    GRIB1_LTYPE_TOA,
    GRIB1_LTYPE_SEA_BOTTOM,
    GRIB1_LTYPE_ATMOSPHERE,
    GRIB1_LTYPE_MEANSEA,
    GRIB1_LTYPE_ISOBARIC,
    GRIB1_LTYPE_99,
    GRIB1_LTYPE_ISOBARIC_PA,
    GRIB1_LTYPE_HEIGHT,
    GRIB1_LTYPE_ALTITUDE,
    GRIB1_LTYPE_SIGMA,
    GRIB1_LTYPE_SIGMA_LAYER,
    GRIB1_LTYPE_LANDDEPTH,
    GRIB1_LTYPE_LANDDEPTH_LAYER,
    GRIB1_LTYPE_ISENTROPIC,
    GRIB1_LTYPE_SEADEPTH,
    GRIB1_LTYPE_LAKE_BOTTOM,
    GRIB1_LTYPE_SEDIMENT_BOTTOM,
    GRIB1_LTYPE_SEDIMENT_BOTTOM_TA,
    GRIB1_LTYPE_SEDIMENT_BOTTOM_TW,
    GRIB1_LTYPE_MIX_LAYER,
    GRIB1_LTYPE_HYBRID,
    GRIB1_LTYPE_HYBRID_LAYER,
  },
  {
    ZAXIS_SURFACE,
    ZAXIS_CLOUD_BASE,
    ZAXIS_CLOUD_TOP,
    ZAXIS_ISOTHERM_ZERO,
    ZAXIS_TOA,
    ZAXIS_SEA_BOTTOM,
    ZAXIS_ATMOSPHERE,
    ZAXIS_MEANSEA,
    ZAXIS_PRESSURE,
    ZAXIS_PRESSURE,
    ZAXIS_PRESSURE,
    ZAXIS_HEIGHT,
    ZAXIS_ALTITUDE,
    ZAXIS_SIGMA,
    ZAXIS_SIGMA,
    ZAXIS_DEPTH_BELOW_LAND,
    ZAXIS_DEPTH_BELOW_LAND,
    ZAXIS_ISENTROPIC,
    ZAXIS_DEPTH_BELOW_SEA,
    ZAXIS_LAKE_BOTTOM,
    ZAXIS_SEDIMENT_BOTTOM,
    ZAXIS_SEDIMENT_BOTTOM_TA,
    ZAXIS_SEDIMENT_BOTTOM_TW,
    ZAXIS_MIX_LAYER,
    ZAXIS_HYBRID,
    ZAXIS_HYBRID,
  },
};

static int
getInt2IntMap(int searchKey, size_t tableSize, const int *keys,
              const int *values, int defaultValue)
{
  int value = defaultValue;
  for (size_t i = 0; i < tableSize; ++i)
    if (keys[i] == searchKey)
      {
        value = values[i];
        break;
      }
  return value;
}


int grib1ltypeToZaxisType(int grib_ltype)
{
  int zaxistype = getInt2IntMap(grib_ltype, grib1ltypeMapSize, grib1ltypeMap[0],
                                grib1ltypeMap[1], ZAXIS_GENERIC);
  return zaxistype;
}

int zaxisTypeToGrib1ltype(int zaxistype)
{
  /* -2 because ZAXIS_HYBRID is not matched to GRIB1_LTYPE_HYBRID for
      some reason */
  int grib_ltype = getInt2IntMap(zaxistype, grib1ltypeMapSize-2,
                                 grib1ltypeMap[1], grib1ltypeMap[0], -1);
  return grib_ltype;
}

enum { grib2ltypeMapSize = 24 };
static const int grib2ltypeMap[2][grib2ltypeMapSize] =
{
  {
    GRIB2_LTYPE_SURFACE,
    GRIB2_LTYPE_CLOUD_BASE,
    GRIB2_LTYPE_CLOUD_TOP,
    GRIB2_LTYPE_ISOTHERM0,
    GRIB2_LTYPE_TOA,
    GRIB2_LTYPE_SEA_BOTTOM,
    GRIB2_LTYPE_ATMOSPHERE,
    GRIB2_LTYPE_MEANSEA,
    GRIB2_LTYPE_ISOBARIC,
    GRIB2_LTYPE_HEIGHT,
    GRIB2_LTYPE_ALTITUDE,
    GRIB2_LTYPE_SIGMA,
    GRIB2_LTYPE_HYBRID,
    GRIB2_LTYPE_HYBRID,
    GRIB2_LTYPE_LANDDEPTH,
    GRIB2_LTYPE_ISENTROPIC,
    GRIB2_LTYPE_SNOW,
    GRIB2_LTYPE_SEADEPTH,
    GRIB2_LTYPE_LAKE_BOTTOM,
    GRIB2_LTYPE_SEDIMENT_BOTTOM,
    GRIB2_LTYPE_SEDIMENT_BOTTOM_TA,
    GRIB2_LTYPE_SEDIMENT_BOTTOM_TW,
    GRIB2_LTYPE_MIX_LAYER,
    GRIB2_LTYPE_REFERENCE,
  },
  {
    ZAXIS_SURFACE,
    ZAXIS_CLOUD_BASE,
    ZAXIS_CLOUD_TOP,
    ZAXIS_ISOTHERM_ZERO,
    ZAXIS_TOA,
    ZAXIS_SEA_BOTTOM,
    ZAXIS_ATMOSPHERE,
    ZAXIS_MEANSEA,
    ZAXIS_PRESSURE,
    ZAXIS_HEIGHT,
    ZAXIS_ALTITUDE,
    ZAXIS_SIGMA,
    ZAXIS_HYBRID,
    ZAXIS_HYBRID_HALF,
    ZAXIS_DEPTH_BELOW_LAND,
    ZAXIS_ISENTROPIC,
    ZAXIS_SNOW,
    ZAXIS_DEPTH_BELOW_SEA,
    ZAXIS_LAKE_BOTTOM,
    ZAXIS_SEDIMENT_BOTTOM,
    ZAXIS_SEDIMENT_BOTTOM_TA,
    ZAXIS_SEDIMENT_BOTTOM_TW,
    ZAXIS_MIX_LAYER,
    ZAXIS_REFERENCE,
  }
};

int grib2ltypeToZaxisType(int grib_ltype)
{
  int zaxistype = getInt2IntMap(grib_ltype, grib2ltypeMapSize, grib2ltypeMap[0],
                                grib2ltypeMap[1], ZAXIS_GENERIC);
  return zaxistype;
}




int zaxisTypeToGrib2ltype(int zaxistype)
{
  int grib_ltype = getInt2IntMap(zaxistype, grib2ltypeMapSize, grib2ltypeMap[1],
                                 grib2ltypeMap[0], -1);
  return grib_ltype;
}


int grbBitsPerValue(int datatype)
{
  int bitsPerValue = 16;

  if ( datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64 )
    Error("CDI/GRIB library does not support complex numbers!");

  if ( datatype != CDI_UNDEFID )
    {
      if ( datatype > 0 && datatype <= 32 )
	bitsPerValue = datatype;
      else if ( datatype == CDI_DATATYPE_FLT64 )
	bitsPerValue = 24;
      else
	bitsPerValue = 16;
    }

  return bitsPerValue;
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
int grbScanTimestep1(stream_t * streamptr)
{
  int status = CDI_EUFTYPE;

#if  defined  (HAVE_LIBCGRIBEX)
  int filetype  = streamptr->filetype;

  if ( filetype == CDI_FILETYPE_GRB )
    status = cgribexScanTimestep1(streamptr);
#endif
#if defined(HAVE_LIBCGRIBEX) && defined (HAVE_LIBGRIB_API)
  else
#endif
#ifdef HAVE_LIBGRIB_API
    status = gribapiScanTimestep1(streamptr);
#endif

  return status;
}

static
int grbScanTimestep2(stream_t * streamptr)
{
  int status = CDI_EUFTYPE;

#if  defined  (HAVE_LIBCGRIBEX)
  int filetype = streamptr->filetype;

  if ( filetype == CDI_FILETYPE_GRB )
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

  return status;
}

static
int grbScanTimestep(stream_t * streamptr)
{
  int status = CDI_EUFTYPE;
  int filetype  = streamptr->filetype;

#if  defined  (HAVE_LIBCGRIBEX)
  if ( filetype == CDI_FILETYPE_GRB )
    status = cgribexScanTimestep(streamptr);
  else
#endif
#ifdef HAVE_LIBGRIB_API
    status = gribapiScanTimestep(streamptr);
#else
    Error("Sufficient GRIB support unavailable!");
#endif

  return status;
}


#if  defined  (HAVE_LIBGRIB)
int grbInqContents(stream_t * streamptr)
{
  streamptr->curTsID = 0;

  int status = grbScanTimestep1(streamptr);
  if ( status == 0 && streamptr->ntsteps == -1 ) status = grbScanTimestep2(streamptr);

  int fileID = streamptr->fileID;
  fileSetPos(fileID, 0, SEEK_SET);

  return status;
}
#endif

int grbInqTimestep(stream_t * streamptr, int tsID)
{
  if ( tsID == 0 && streamptr->rtsteps == 0 )
    Error("Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message("tsid = %d rtsteps = %d", tsID, streamptr->rtsteps);

  int ntsteps = CDI_UNDEFID;
  while ( (tsID + 1) > streamptr->rtsteps && ntsteps == CDI_UNDEFID )
    {
      ntsteps = grbScanTimestep(streamptr);
      if ( ntsteps == CDI_EUFSTRUCT )
	{
	  streamptr->ntsteps = streamptr->rtsteps;
	  break;
	}
    }

  int nrecs;

  if ( tsID >= streamptr->ntsteps && streamptr->ntsteps != CDI_UNDEFID )
    {
      nrecs = 0;
    }
  else
    {
      streamptr->curTsID = tsID;
      nrecs = streamptr->tsteps[tsID].nrecs;
    }

  return nrecs;
}


void streamInqGRIBinfo(int streamID, int *intnum, float *fltnum, off_t *bignum)
{
  stream_t *streamptr = stream_to_pointer(streamID);

  int filetype = streamptr->filetype;

  if ( filetype == CDI_FILETYPE_GRB )
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
