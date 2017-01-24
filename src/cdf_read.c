#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#include <limits.h>
#include <float.h>

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "stream_grb.h"
#include "stream_cdf.h"
#include "cdf_int.h"
#include "vlist.h"
#include "vlist_var.h"

static void
cdfReadGridTraj(stream_t *streamptr, int gridID)
{
  const int vlistID = streamptr->vlistID;
  const int fileID = streamptr->fileID;

  const int gridindex = vlistGridIndex(vlistID, gridID);
  const int lonID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_X];
  const int latID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_Y];

  const int tsID = streamptr->curTsID;
  const size_t index = (size_t) tsID;

  double xlon, xlat;
  cdf_get_var1_double(fileID, lonID, &index, &xlon);
  cdf_get_var1_double(fileID, latID, &index, &xlat);

  gridDefXvals(gridID, &xlon);
  gridDefYvals(gridID, &xlat);
}

static void
cdfGetSlapDescription(stream_t *streamptr, int varID, size_t (*start)[MAX_DIMENSIONS], size_t (*count)[MAX_DIMENSIONS])
{
  const int vlistID = streamptr->vlistID;
  const int tsID = streamptr->curTsID;
  const int gridID = vlistInqVarGrid(vlistID, varID);
  const int zaxisID = vlistInqVarZaxis(vlistID, varID);
  const int timetype = vlistInqVarTimetype(vlistID, varID);
  const int gridindex = vlistGridIndex(vlistID, gridID);

  if (CDI_Debug) Message("tsID = %d", tsID);

  int xid = CDI_UNDEFID, yid = CDI_UNDEFID;
  if (gridInqType(gridID) == GRID_TRAJECTORY)
    {
      cdfReadGridTraj(streamptr, gridID);
    }
  else
    {
      xid = streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_X];
      yid = streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_Y];
    }
  const int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  const int zid = streamptr->zaxisID[zaxisindex];

  int ndims = 0;
#define addDimension(startCoord, length) \
  do                                     \
    {                                    \
      (*start)[ndims] = startCoord;      \
      (*count)[ndims] = length;          \
      ndims++;                           \
    }                                    \
  while (0)
  if (timetype != TIME_CONSTANT) addDimension((size_t) tsID, 1);
  if (zid != CDI_UNDEFID) addDimension(0, (size_t) zaxisInqSize(zaxisID));
  if (yid != CDI_UNDEFID) addDimension(0, gridInqYsize(gridID));
  if (xid != CDI_UNDEFID) addDimension(0, gridInqXsize(gridID));
#undef addDimension

  assert(ndims <= (int) (sizeof(*start) / sizeof(**start)));
  assert(ndims <= (int) (sizeof(*count) / sizeof(**count)));

  if (CDI_Debug)
    for (int idim = 0; idim < ndims; ++idim) Message("dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);
}

// Scans the data array for missVals, optionally applying first a scale factor and then an offset.
// Returns the number of missing + out-of-range values encountered.
static size_t
cdfDoInputDataTransformationDP(int vlistID, int varID, size_t valueCount, double *data)
{
  const double missVal = vlistInqVarMissval(vlistID, varID);
  const int haveMissVal = vlistInqVarMissvalUsed(vlistID, varID);
  double validRange[2];
  if (!(haveMissVal && vlistInqVarValidrange(vlistID, varID, validRange))) validRange[0] = DBL_MIN, validRange[1] = DBL_MAX;
  double addoffset = 0.0, scalefactor = 1.0;
  const int haveAddoffset = (cdiInqKeyFloat(vlistID, varID, CDI_KEY_ADDOFFSET, &addoffset) == CDI_NOERR);
  const int haveScalefactor = (cdiInqKeyFloat(vlistID, varID, CDI_KEY_SCALEFACTOR, &scalefactor) == CDI_NOERR);

  const bool missValIsNaN = DBL_IS_NAN(missVal);
  size_t missValCount = 0;

  double validMin = validRange[0];
  double validMax = validRange[1];
  if (IS_EQUAL(validMin, VALIDMISS)) validMin = DBL_MIN;
  if (IS_EQUAL(validMax, VALIDMISS)) validMax = DBL_MAX;

  const int haveRangeCheck = (IS_NOT_EQUAL(validMax, DBL_MAX)) | (IS_NOT_EQUAL(validMin, DBL_MIN));
  assert(!haveRangeCheck || haveMissVal);

  switch (haveMissVal | (haveScalefactor << 1) | (haveAddoffset << 2) | (haveRangeCheck << 3))
    {
    case 15:  // haveRangeCheck & haveMissVal & haveScalefactor & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? missVal : isMissVal ? data[i] : data[i] * scalefactor + addoffset;
        }
      break;
    case 13:  // haveRangeCheck & haveMissVal & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? missVal : isMissVal ? data[i] : data[i] + addoffset;
        }
      break;
    case 11:  // haveRangeCheck & haveMissVal & haveScalefactor
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? missVal : isMissVal ? data[i] : data[i] * scalefactor;
        }
      break;
    case 9:  // haveRangeCheck & haveMissVal
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? missVal : data[i];
        }
      break;
    case 7:  // haveMissVal & haveScalefactor & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        if (DBL_IS_EQUAL(data[i], missVal))
          missValCount++;
        else
          data[i] = data[i] * scalefactor + addoffset;
      break;
    case 6:  // haveAddoffset & haveScalefactor
      for (size_t i = 0; i < valueCount; ++i) data[i] = data[i] * scalefactor + addoffset;
      break;
    case 5:  // haveMissVal & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        if (DBL_IS_EQUAL(data[i], missVal))
          missValCount++;
        else
          data[i] += addoffset;
      break;
    case 4:  // haveAddoffset
      for (size_t i = 0; i < valueCount; ++i) data[i] += addoffset;
      break;
    case 3:  // haveMissVal & haveScalefactor
      for (size_t i = 0; i < valueCount; ++i)
        if (DBL_IS_EQUAL(data[i], missVal))
          missValCount++;
        else
          data[i] *= scalefactor;
      break;
    case 2:  // haveScalefactor
      for (size_t i = 0; i < valueCount; ++i) data[i] *= scalefactor;
      break;
    case 1:  // haveMissVal
      if (missValIsNaN)
        {
          for (size_t i = 0; i < valueCount; ++i) missValCount += (size_t) DBL_IS_NAN(data[i]);
        }
      else
        {
          for (size_t i = 0; i < valueCount; ++i) missValCount += (size_t) DBL_IS_EQUAL(data[i], missVal);
        }
      break;
    }

  return missValCount;
}

static size_t
cdfDoInputDataTransformationSP(int vlistID, int varID, size_t valueCount, float *data)
{
  const double missVal = vlistInqVarMissval(vlistID, varID);
  const int haveMissVal = vlistInqVarMissvalUsed(vlistID, varID);
  double validRange[2];
  if (!(haveMissVal && vlistInqVarValidrange(vlistID, varID, validRange))) validRange[0] = DBL_MIN, validRange[1] = DBL_MAX;
  double addoffset = 0.0, scalefactor = 1.0;
  const int haveAddoffset = (cdiInqKeyFloat(vlistID, varID, CDI_KEY_ADDOFFSET, &addoffset) == CDI_NOERR);
  const int haveScalefactor = (cdiInqKeyFloat(vlistID, varID, CDI_KEY_SCALEFACTOR, &scalefactor) == CDI_NOERR);

  const bool missValIsNaN = DBL_IS_NAN(missVal);
  size_t missValCount = 0;

  double validMin = validRange[0];
  double validMax = validRange[1];
  if (IS_EQUAL(validMin, VALIDMISS)) validMin = DBL_MIN;
  if (IS_EQUAL(validMax, VALIDMISS)) validMax = DBL_MAX;

  const int haveRangeCheck = (IS_NOT_EQUAL(validMax, DBL_MAX)) | (IS_NOT_EQUAL(validMin, DBL_MIN));
  assert(!haveRangeCheck || haveMissVal);

  switch (haveMissVal | (haveScalefactor << 1) | (haveAddoffset << 2) | (haveRangeCheck << 3))
    {
    case 15:  // haveRangeCheck & haveMissVal & haveScalefactor & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? (float) missVal : isMissVal ? data[i] : (float) (data[i] * scalefactor + addoffset);
        }
      break;
    case 13:  // haveRangeCheck & haveMissVal & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? (float) missVal : isMissVal ? data[i] : (float) (data[i] + addoffset);
        }
      break;
    case 11:  // haveRangeCheck & haveMissVal & haveScalefactor
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? (float) missVal : isMissVal ? data[i] : (float) (data[i] * scalefactor);
        }
      break;
    case 9:  // haveRangeCheck & haveMissVal
      for (size_t i = 0; i < valueCount; ++i)
        {
          int outOfRange = (data[i] < validMin || data[i] > validMax);
          int isMissVal = DBL_IS_EQUAL(data[i], missVal);
          missValCount += (size_t) (outOfRange | isMissVal);
          data[i] = outOfRange ? (float) missVal : data[i];
        }
      break;
    case 7:  // haveMissVal & haveScalefactor & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        if (DBL_IS_EQUAL(data[i], missVal))
          missValCount++;
        else
          data[i] = (float) (data[i] * scalefactor + addoffset);
      break;
    case 6:  // haveAddoffset & haveScalefactor
      for (size_t i = 0; i < valueCount; ++i) data[i] = (float) (data[i] * scalefactor + addoffset);
      break;
    case 5:  // haveMissVal & haveAddoffset
      for (size_t i = 0; i < valueCount; ++i)
        if (DBL_IS_EQUAL(data[i], missVal))
          missValCount++;
        else
          data[i] = (float) (data[i] + addoffset);
      break;
    case 4:  // haveAddoffset
      for (size_t i = 0; i < valueCount; ++i) data[i] = (float) (data[i] + addoffset);
      break;
    case 3:  // haveMissVal & haveScalefactor
      for (size_t i = 0; i < valueCount; ++i)
        if (DBL_IS_EQUAL(data[i], missVal))
          missValCount++;
        else
          data[i] = (float) (data[i] * scalefactor);
      break;
    case 2:  // haveScalefactor
      for (size_t i = 0; i < valueCount; ++i) data[i] = (float) (data[i] * scalefactor);
      break;
    case 1:  // haveMissVal
      if (missValIsNaN)
        {
          for (size_t i = 0; i < valueCount; ++i) missValCount += (size_t) DBL_IS_NAN(data[i]);
        }
      else
        {
          for (size_t i = 0; i < valueCount; ++i) missValCount += (size_t) DBL_IS_EQUAL(data[i], missVal);
        }
      break;
    }

  return missValCount;
}

static size_t
min_size(size_t a, size_t b)
{
  return a < b ? a : b;
}

static void
transpose2dArrayDP(int gridId, double *data)
{
  size_t inWidth = gridInqYsize(gridId);
  size_t inHeight = gridInqXsize(gridId);

  const size_t cacheBlockSize = 256;  // Purely an optimization parameter. Current value of 32 means we are handling 8kB blocks,
                                      // which should be a decent compromise on many architectures.
  double **out = (double **) malloc(inWidth * sizeof(double *));
  double **temp = (double **) malloc(inHeight * sizeof(double *));
  temp[0] = (double *) malloc(inHeight * inWidth * sizeof(double));
  memcpy(temp[0], data, inHeight * inWidth * sizeof(double));
  for (size_t i = 0; i < inWidth; ++i) out[i] = data + (inHeight * i);
  for (size_t i = 1; i < inHeight; ++i) temp[i] = temp[0] + (inWidth * i);

  /*
  for (size_t y = 0; y < inHeight; ++y)
    for (size_t x = 0; x < inWidth; ++x)
      out[x][y] = temp[y][x];
  */

  for (size_t yBlock = 0; yBlock < inHeight; yBlock += cacheBlockSize)
    for (size_t xBlock = 0; xBlock < inWidth; xBlock += cacheBlockSize)
      for (size_t y = yBlock, yEnd = min_size(yBlock + cacheBlockSize, inHeight); y < yEnd; y++)
        for (size_t x = xBlock, xEnd = min_size(xBlock + cacheBlockSize, inWidth); x < xEnd; x++)
          {
            out[x][y] = temp[y][x];
          }

  free(out);
  free(temp[0]);
  free(temp);
}

static void
transpose2dArraySP(size_t gridId, float *data)
{
  size_t inWidth = gridInqYsize(gridId);
  size_t inHeight = gridInqXsize(gridId);

  const size_t cacheBlockSize = 256;  // Purely an optimization parameter. Current value of 32 means we are handling 8kB blocks,
                                      // which should be a decent compromise on many architectures.
  float **out = (float **) malloc(inWidth * sizeof(float *));
  float **temp = (float **) malloc(inHeight * sizeof(float *));
  temp[0] = (float *) malloc(inHeight * inWidth * sizeof(float));
  memcpy(temp[0], data, inHeight * inWidth * sizeof(float));
  for (size_t i = 0; i < inWidth; i++) out[i] = data + (inHeight * i);
  for (size_t i = 1; i < inHeight; i++) temp[i] = temp[0] + (inWidth * i);

  /*
  for (size_t y = 0; y < inHeight; ++y)
    for (size_t x = 0; x < inWidth; ++x)
      out[x][y] = temp[y][x];
  */

  for (size_t yBlock = 0; yBlock < inHeight; yBlock += cacheBlockSize)
    for (size_t xBlock = 0; xBlock < inWidth; xBlock += cacheBlockSize)
      for (size_t y = yBlock, yEnd = min_size(yBlock + cacheBlockSize, inHeight); y < yEnd; y++)
        for (size_t x = xBlock, xEnd = min_size(xBlock + cacheBlockSize, inWidth); x < xEnd; x++)
          {
            out[x][y] = temp[y][x];
          }

  free(out);
  free(temp[0]);
  free(temp);
}

static void
cdfInqDimIds(stream_t *streamptr, int varId, int (*outDimIds)[4])
{
  const int vlistID = streamptr->vlistID;
  const int gridId = vlistInqVarGrid(vlistID, varId);
  const int gridindex = vlistGridIndex(vlistID, gridId);
  const int *ncIDs = streamptr->ncgrid[gridindex].ncIDs;

  (*outDimIds)[0] = (*outDimIds)[1] = (*outDimIds)[2] = (*outDimIds)[3] = CDI_UNDEFID;
  switch (gridInqType(gridId))
    {
    case GRID_TRAJECTORY: cdfReadGridTraj(streamptr, gridId); break;
    case GRID_UNSTRUCTURED:
      (*outDimIds)[0] = ncIDs[CDF_DIMID_X];
      (*outDimIds)[3] = ncIDs[CDF_DIMID_E];                                      // used only for cube_sphere grids
      if ((*outDimIds)[3] != CDI_UNDEFID) (*outDimIds)[1] = ncIDs[CDF_DIMID_Y];  // used only for cube_sphere grids
      break;
    case GRID_GAUSSIAN_REDUCED: (*outDimIds)[0] = ncIDs[CDF_DIMID_X]; break;
    default:
      (*outDimIds)[0] = ncIDs[CDF_DIMID_X];
      (*outDimIds)[1] = ncIDs[CDF_DIMID_Y];
      break;
    }

  const int zaxisID = vlistInqVarZaxis(vlistID, varId);
  const int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  (*outDimIds)[2] = streamptr->zaxisID[zaxisindex];
}

static int
cdfGetSkipDim(int fileId, int ncvarid, int dimIds[3])
{
  if (dimIds[0] != CDI_UNDEFID || dimIds[1] != CDI_UNDEFID) return 0;

  int nvdims;
  cdf_inq_varndims(fileId, ncvarid, &nvdims);
  if (nvdims != 3) return 0;

  int varDimIds[3];
  cdf_inq_vardimid(fileId, ncvarid, varDimIds);

  size_t size = 0;
  if (dimIds[2] == varDimIds[2])
    {
      cdf_inq_dimlen(fileId, varDimIds[1], &size);
      if (size == 1) return 1;
    }
  else if (dimIds[2] == varDimIds[1])
    {
      cdf_inq_dimlen(fileId, varDimIds[2], &size);
      if (size == 1) return 2;
    }

  return 0;
}

enum
{
  cdfSliceNDim = MAX_DIMENSIONS
};

static void
cdfGetSliceSlapDescription(stream_t *streamptr, int varID, int levelID, bool *outSwapXY, size_t start[cdfSliceNDim],
                           size_t count[cdfSliceNDim])
{
  const int tsID = streamptr->curTsID;
  if (CDI_Debug) Message("tsID = %d", tsID);

  const int fileId = streamptr->fileID;
  const int vlistID = streamptr->vlistID;
  const int ncvarid = streamptr->vars[varID].ncvarid;

  const int gridId = vlistInqVarGrid(vlistID, varID);
  const int timetype = vlistInqVarTimetype(vlistID, varID);
  const size_t gridsize = gridInqSize(gridId);

  streamptr->numvals += gridsize;

  int dimIds[4];  // this array joins the old variables xid, yid, and zid
  cdfInqDimIds(streamptr, varID, &dimIds);

  const int skipdim = cdfGetSkipDim(fileId, ncvarid, dimIds);

  int dimorder[4] = { 3, 4, 2, 1 };  // order of cube sphere grid
  if (dimIds[3] == CDI_UNDEFID)
    {
      int tmpdimorder[3];
      vlistInqVarDimorder(vlistID, varID, &tmpdimorder);
      for (int i = 0; i < 3; ++i) dimorder[i] = tmpdimorder[i];
      dimorder[3] = 4;
      *outSwapXY = ((dimorder[2] == 2 || dimorder[0] == 1) && (dimIds[0] != CDI_UNDEFID) && (dimIds[1] != CDI_UNDEFID));
    }

  int ndims = 0;

#define addDimension(startIndex, extent) \
  do                                     \
    {                                    \
      start[ndims] = startIndex;         \
      count[ndims] = extent;             \
      ndims++;                           \
    }                                    \
  while (0)

  if (timetype != TIME_CONSTANT) addDimension((size_t) tsID, 1);
  if (skipdim == 1) addDimension(0, 1);

  for (int id = 0; id < 4; ++id)
    {
      size_t size;
      const int curDimId = dimIds[dimorder[id] - 1];
      if (curDimId == CDI_UNDEFID) continue;
      switch (dimorder[id])
        {
        case 1:
        case 2:
        case 4:
          cdf_inq_dimlen(fileId, curDimId, &size);
          addDimension(0, size);
          break;
        case 3: addDimension((size_t) levelID, 1); break;
        default: Error("Internal errror: Malformed dimension order encountered. Please report this bug.\n");
        }
    }

  if (skipdim == 2) addDimension(0, 1);

  assert(ndims <= cdfSliceNDim);

#undef addDimension

  if (CDI_Debug)
    for (int idim = 0; idim < ndims; ++idim) Message("dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  int nvdims;
  cdf_inq_varndims(fileId, ncvarid, &nvdims);

  if (nvdims != ndims)
    {
      char name[CDI_MAX_NAME];
      vlistInqVarName(vlistID, varID, name);
      Error("Internal error, variable %s has an unsupported array structure!", name);
    }
}

static size_t
getSizeVar3D(int vlistID, int varID)
{
  const int gridID = vlistInqVarGrid(vlistID, varID);
  const int zaxisID = vlistInqVarZaxis(vlistID, varID);
  return gridInqSize(gridID) * (size_t) zaxisInqSize(zaxisID);
}

static void
cdfReadDataSliceSP2DP(int fileID, int ncvarid, size_t length, size_t start[MAX_DIMENSIONS], size_t count[MAX_DIMENSIONS],
                      double *data)
{
  float *data_fp = (float *) Malloc(length * sizeof(*data_fp));
  cdf_get_vara_float(fileID, ncvarid, start, count, data_fp);
  for (size_t i = 0; i < length; ++i) data[i] = (double) data_fp[i];
  Free(data_fp);
}

static void
cdfReadDataSliceDP2SP(int fileID, int ncvarid, size_t length, size_t start[MAX_DIMENSIONS], size_t count[MAX_DIMENSIONS],
                      float *data)
{
  double *data_dp = (double *) Malloc(length * sizeof(*data_dp));
  cdf_get_vara_double(fileID, ncvarid, start, count, data_dp);
  for (size_t i = 0; i < length; ++i) data[i] = (float) data_dp[i];
  Free(data_dp);
}

static void
cdfCheckDataDP_UINT8(int fileID, int ncvarid, int vlistID, int varID, size_t length, double *data)
{
  if (vlistInqVarDatatype(vlistID, varID) == CDI_DATATYPE_UINT8)
    {
      nc_type xtype;
      cdf_inq_vartype(fileID, ncvarid, &xtype);
      if (xtype == NC_BYTE)
        {
          for (size_t i = 0; i < length; ++i)
            if (data[i] < 0) data[i] += 256;
        }
    }
}

static void
cdfCheckDataSP_UINT8(int fileID, int ncvarid, int vlistID, int varID, size_t length, float *data)
{
  if (vlistInqVarDatatype(vlistID, varID) == CDI_DATATYPE_UINT8)
    {
      nc_type xtype;
      cdf_inq_vartype(fileID, ncvarid, &xtype);
      if (xtype == NC_BYTE)
        {
          for (size_t i = 0; i < length; ++i)
            if (data[i] < 0) data[i] += 256;
        }
    }
}

static void
cdfReadDataDP(stream_t *streamptr, int varID, size_t length, size_t start[MAX_DIMENSIONS], size_t count[MAX_DIMENSIONS],
              double *data)
{
  const int vlistID = streamptr->vlistID;
  const int fileID = streamptr->fileID;
  const int ncvarid = streamptr->vars[varID].ncvarid;
  const int datatype = vlistInqVarDatatype(vlistID, varID);

  if (datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64)
    {
      cdf_get_vara(fileID, ncvarid, start, count, data);
      if (datatype == CDI_DATATYPE_CPX32)
        {
          for (long i = (long) length - 1; i >= 0; --i)
            {
              data[2 * i] = (double) (((float *) data)[2 * i]);
              data[2 * i + 1] = (double) (((float *) data)[2 * i + 1]);
            }
        }
    }
  else
    {
      if (datatype == CDI_DATATYPE_FLT32)
        {
          cdfReadDataSliceSP2DP(fileID, ncvarid, length, start, count, data);
        }
      else
        {
          cdf_get_vara_double(fileID, ncvarid, start, count, data);

          cdfCheckDataDP_UINT8(fileID, ncvarid, vlistID, varID, length, data);
        }
    }
}

static void
cdfReadDataSP(stream_t *streamptr, int varID, size_t length, size_t start[MAX_DIMENSIONS], size_t count[MAX_DIMENSIONS],
              float *data)
{
  const int vlistID = streamptr->vlistID;
  const int fileID = streamptr->fileID;
  const int ncvarid = streamptr->vars[varID].ncvarid;
  const int datatype = vlistInqVarDatatype(vlistID, varID);

  if (datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64)
    {
      if (datatype == CDI_DATATYPE_CPX64)
        {
          double *cdata = (double *) Malloc(2 * length * sizeof(double));
          cdf_get_vara(fileID, ncvarid, start, count, cdata);
          for (size_t i = 0; i < length; ++i)
            {
              data[2 * i] = (float) (cdata[2 * i]);
              data[2 * i + 1] = (float) (cdata[2 * i + 1]);
            }
          Free(cdata);
        }
      else
        {
          cdf_get_vara(fileID, ncvarid, start, count, data);
        }
    }
  else
    {
      if (datatype == CDI_DATATYPE_FLT64)
        {
          cdfReadDataSliceDP2SP(fileID, ncvarid, length, start, count, data);
        }
      else
        {
          cdf_get_vara_float(fileID, ncvarid, start, count, data);

          cdfCheckDataSP_UINT8(fileID, ncvarid, vlistID, varID, length, data);
        }
    }
}

static void
cdfReadVarDP(stream_t *streamptr, int varID, double *data, size_t *nmiss)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamptr->self, varID);

  const int vlistID = streamptr->vlistID;

  size_t start[MAX_DIMENSIONS], count[MAX_DIMENSIONS];
  cdfGetSlapDescription(streamptr, varID, &start, &count);

  const size_t length = getSizeVar3D(vlistID, varID);
  cdfReadDataDP(streamptr, varID, length, start, count, data);

  *nmiss = cdfDoInputDataTransformationDP(vlistID, varID, length, data);
}

static void
cdfReadVarSP(stream_t *streamptr, int varID, float *data, size_t *nmiss)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamptr->self, varID);

  const int vlistID = streamptr->vlistID;

  size_t start[MAX_DIMENSIONS], count[MAX_DIMENSIONS];
  cdfGetSlapDescription(streamptr, varID, &start, &count);

  const size_t length = getSizeVar3D(vlistID, varID);
  cdfReadDataSP(streamptr, varID, length, start, count, data);

  *nmiss = cdfDoInputDataTransformationSP(vlistID, varID, length, data);
}

void
cdf_read_var(stream_t *streamptr, int varID, int memtype, void *data, size_t *nmiss)
{
  if (memtype == MEMTYPE_DOUBLE)
    cdfReadVarDP(streamptr, varID, (double *) data, nmiss);
  else
    cdfReadVarSP(streamptr, varID, (float *) data, nmiss);
}

static void
cdfReadVarSliceDP(stream_t *streamptr, int varID, int levelID, double *data, size_t *nmiss)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d  levelID = %d", streamptr->self, varID, levelID);

  bool swapxy = false;
  size_t start[cdfSliceNDim], count[cdfSliceNDim];
  cdfGetSliceSlapDescription(streamptr, varID, levelID, &swapxy, start, count);

  const int vlistID = streamptr->vlistID;
  const int gridId = vlistInqVarGrid(vlistID, varID);
  const size_t length = gridInqSize(gridId);
  cdfReadDataDP(streamptr, varID, length, start, count, data);

  if (swapxy) transpose2dArrayDP(gridId, data);

  *nmiss = cdfDoInputDataTransformationDP(vlistID, varID, length, data);
}

static void
cdfReadVarSliceSP(stream_t *streamptr, int varID, int levelID, float *data, size_t *nmiss)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d  levelID = %d", streamptr->self, varID, levelID);

  bool swapxy = false;
  size_t start[cdfSliceNDim], count[cdfSliceNDim];
  cdfGetSliceSlapDescription(streamptr, varID, levelID, &swapxy, start, count);

  const int vlistID = streamptr->vlistID;
  const int gridId = vlistInqVarGrid(vlistID, varID);
  const size_t length = gridInqSize(gridId);
  cdfReadDataSP(streamptr, varID, length, start, count, data);

  if (swapxy) transpose2dArraySP(gridId, data);

  *nmiss = cdfDoInputDataTransformationSP(vlistID, varID, length, data);
}

void
cdf_read_var_slice(stream_t *streamptr, int varID, int levelID, int memtype, void *data, size_t *nmiss)
{
  if (memtype == MEMTYPE_DOUBLE)
    cdfReadVarSliceDP(streamptr, varID, levelID, (double *) data, nmiss);
  else
    cdfReadVarSliceSP(streamptr, varID, levelID, (float *) data, nmiss);
}

void
cdf_read_record(stream_t *streamptr, int memtype, void *data, size_t *nmiss)
{
  if (CDI_Debug) Message("streamID = %d", streamptr->self);

  const int tsID = streamptr->curTsID;
  const int vrecID = streamptr->tsteps[tsID].curRecID;
  const int recID = streamptr->tsteps[tsID].recIDs[vrecID];
  const int varID = streamptr->tsteps[tsID].records[recID].varID;
  const int levelID = streamptr->tsteps[tsID].records[recID].levelID;

  if (memtype == MEMTYPE_DOUBLE)
    cdfReadVarSliceDP(streamptr, varID, levelID, (double *) data, nmiss);
  else
    cdfReadVarSliceSP(streamptr, varID, levelID, (float *) data, nmiss);
}

//----------------------------------------------------------------------------
// Parallel Version
//----------------------------------------------------------------------------

void
cdfReadVarSliceDPPart(stream_t *streamptr, int varID, int levelID, int varType, int startpoint, size_t length, double *data,
                      size_t *nmiss)
{
  (void) (varType);

  if (CDI_Debug) Message("streamID = %d  varID = %d  levelID = %d", streamptr->self, varID, levelID);

  const int vlistID = streamptr->vlistID;

  bool swapxy = false;
  size_t start[cdfSliceNDim], count[cdfSliceNDim];
  cdfGetSliceSlapDescription(streamptr, varID, levelID, &swapxy, start, count);

  const int gridId = vlistInqVarGrid(vlistID, varID);
  const size_t gridsize = gridInqSize(gridId);

  unsigned int position = 0;
  for (int i = 0; i < MAX_DIMENSIONS; ++i)
    if (count[i] == gridsize) position = i;

  start[position] = start[position] + startpoint;
  count[position] = length;

  cdfReadDataDP(streamptr, varID, length, start, count, data);

  if (swapxy) transpose2dArrayDP(gridId, data);

  *nmiss = cdfDoInputDataTransformationDP(vlistID, varID, length, data);
}

void
cdfReadVarSliceSPPart(stream_t *streamptr, int varID, int levelID, int varType, int startpoint, size_t length, float *data,
                      size_t *nmiss)
{
  (void) (varType);

  if (CDI_Debug) Message("streamID = %d  varID = %d  levelID = %d", streamptr->self, varID, levelID);

  const int vlistID = streamptr->vlistID;

  bool swapxy = false;
  size_t start[cdfSliceNDim], count[cdfSliceNDim];
  cdfGetSliceSlapDescription(streamptr, varID, levelID, &swapxy, start, count);

  const int gridId = vlistInqVarGrid(vlistID, varID);
  size_t gridsize = gridInqSize(gridId);

  unsigned int position = 0;
  for (int i = 0; i < MAX_DIMENSIONS; ++i)
    if (count[i] == gridsize) position = i;

  start[position] = start[position] + startpoint;
  count[position] = length;

  cdfReadDataSP(streamptr, varID, length, start, count, data);

  if (swapxy) transpose2dArraySP(gridId, data);

  *nmiss = cdfDoInputDataTransformationSP(vlistID, varID, length, data);
}

static int
cdiStreamReadVarSlicePart(int streamID, int varID, int levelID, int varType, int start, size_t size, int memtype, void *data,
                          size_t *nmiss)
{
  int status = 0;

  if (CDI_Debug) Message("streamID = %d  varID = %d", streamID, varID);

  check_parg(data);
  check_parg(nmiss);

  stream_t *streamptr = stream_to_pointer(streamID);
  const int filetype = streamptr->filetype;

  *nmiss = 0;

  // currently we only care for netcdf data
  switch (cdiBaseFiletype(filetype))
    {
#ifdef HAVE_LIBGRIB
    case CDI_FILETYPE_GRB:
    case CDI_FILETYPE_GRB2:
      {
        grb_read_var_slice(streamptr, varID, levelID, memtype, data, nmiss);
        break;
      }
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NETCDF:
      {
        if (memtype == MEMTYPE_FLOAT)
          cdfReadVarSliceSPPart(streamptr, varID, levelID, varType, start, size, (float *) data, nmiss);
        else
          cdfReadVarSliceDPPart(streamptr, varID, levelID, varType, start, size, (double *) data, nmiss);
        break;
      }
#endif
    default:
      {
        Error("%s support not compiled in!", strfiletype(filetype));
        status = 2;
        break;
      }
    }

  return status;
}

void
cdfReadVarDPPart(stream_t *streamptr, int varID, int varType, int startpoint, size_t length, double *data, size_t *nmiss)
{
  (void) (varType);
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamptr->self, varID);

  const int vlistID = streamptr->vlistID;
  const int ncvarid = streamptr->vars[varID].ncvarid;

  size_t start[MAX_DIMENSIONS], count[MAX_DIMENSIONS];
  cdfGetSlapDescription(streamptr, varID, &start, &count);

  const int ltime = (TIME_CONSTANT != vlistInqVarTimetype(vlistID, varID));
  start[1 + ltime] = start[1 + ltime] + startpoint;
  count[1 + ltime] = length;

  cdf_get_vara_double(streamptr->fileID, ncvarid, start, count, data);

  *nmiss = cdfDoInputDataTransformationDP(vlistID, varID, length, data);
}

void
cdfReadVarSPPart(stream_t *streamptr, int varID, int varType, int startpoint, size_t length, float *data, size_t *nmiss)
{
  (void) (varType);
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamptr->self, varID);

  const int vlistID = streamptr->vlistID;
  const int ncvarid = streamptr->vars[varID].ncvarid;

  size_t start[MAX_DIMENSIONS], count[MAX_DIMENSIONS];
  cdfGetSlapDescription(streamptr, varID, &start, &count);

  const int ltime = (TIME_CONSTANT != vlistInqVarTimetype(vlistID, varID));
  start[1 + ltime] = start[1 + ltime] + startpoint;
  count[1 + ltime] = length;

  cdf_get_vara_float(streamptr->fileID, ncvarid, start, count, data);

  *nmiss = cdfDoInputDataTransformationSP(vlistID, varID, length, data);
}

static void
cdiStreamReadVarPart(int streamID, int varID, int varType, int start, size_t size, int memtype, void *data, size_t *nmiss)
{
  (void) (varType);
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamID, varID);

  check_parg(data);
  check_parg(nmiss);

  stream_t *streamptr = stream_to_pointer(streamID);
  const int filetype = streamptr->filetype;

  *nmiss = 0;

  // currently we only care for netcdf data
  switch (cdiBaseFiletype(filetype))
    {
#ifdef HAVE_LIBGRIB
    case CDI_FILETYPE_GRB:
    case CDI_FILETYPE_GRB2:
      {
        grb_read_var(streamptr, varID, memtype, data, nmiss);
        break;
      }
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NETCDF:
      {
        if (memtype == MEMTYPE_FLOAT)
          cdfReadVarSPPart(streamptr, varID, varType, start, size, (float *) data, nmiss);
        else
          cdfReadVarDPPart(streamptr, varID, varType, start, size, (double *) data, nmiss);

        break;
      }
#endif
    default:
      {
        Error("%s support not compiled in!", strfiletype(filetype));
        break;
      }
    }
}

void
streamReadVarSlicePart(int streamID, int varID, int levelID, int varType, int start, SizeType size, void *data, SizeType *nmiss,
                       int memtype)
{
  size_t numMiss = 0;
  if (cdiStreamReadVarSlicePart(streamID, varID, levelID, varType, start, size, memtype, data, &numMiss))
    {
      Error("Unexpected error returned from cdiStreamReadVarSlicePart()!");
    }
  *nmiss = (SizeType) numMiss;
}

void
streamReadVarPart(int streamID, int varID, int varType, int start, SizeType size, void *data, SizeType *nmiss, int memtype)
{
  size_t numMiss = 0;
  cdiStreamReadVarPart(streamID, varID, varType, start, size, memtype, data, &numMiss);
  *nmiss = (SizeType) numMiss;
}

#endif /* HAVE_LIBNETCDF */
