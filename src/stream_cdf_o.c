#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#include "dmemory.h"
#include "cdi_int.h"
#include "cdi_uuid.h"
#include "stream_cdf.h"
#include "stream_cdf_postdef.h"
#include "cdf_int.h"
#include "namespace.h"
#include "vlist.h"
#include "zaxis.h"

static const char bndsName[] = "bnds";

void
cdfCopyRecord(stream_t *streamptr2, stream_t *streamptr1)
{
  const int vlistID1 = streamptr1->vlistID;
  const int tsID = streamptr1->curTsID;
  const int vrecID = streamptr1->tsteps[tsID].curRecID;
  const int recID = streamptr1->tsteps[tsID].recIDs[vrecID];
  const int ivarID = streamptr1->tsteps[tsID].records[recID].varID;
  const int gridID = vlistInqVarGrid(vlistID1, ivarID);
  const size_t datasize = gridInqSize(gridID);
  const int datatype = vlistInqVarDatatype(vlistID1, ivarID);
  const int memtype = (datatype != CDI_DATATYPE_FLT32) ? MEMTYPE_DOUBLE : MEMTYPE_FLOAT;

  void *data = Malloc(datasize * ((memtype == MEMTYPE_DOUBLE) ? sizeof(double) : sizeof(float)));

  size_t nmiss;
  cdf_read_record(streamptr1, memtype, data, &nmiss);
  cdf_write_record(streamptr2, memtype, data, nmiss);

  Free(data);
}

void
cdfDefRecord(stream_t *streamptr)
{
  (void) streamptr;
}

static void
cdfDefComplex(stream_t *streamptr, int gridID, int gridindex)
{
  int dimID;
  ncgrid_t *ncgrid = streamptr->ncgrid;

  for (int index = 0; index < gridindex; ++index)
    {
      if (ncgrid[index].ncIDs[CDF_DIMID_X] != CDI_UNDEFID)
        {
          const int gridID0 = ncgrid[index].gridID;
          const int gridtype0 = gridInqType(gridID0);
          if (gridtype0 == GRID_SPECTRAL || gridtype0 == GRID_FOURIER)
            {
              dimID = ncgrid[index].ncIDs[CDF_DIMID_X];
              goto dimIDEstablished;
            }
        }
    }

  {
    static const char axisname[] = "nc2";
    const size_t dimlen = 2;
    const int fileID = streamptr->fileID;

    bool switchNCMode = streamptr->ncmode == 2;
    if (switchNCMode)
      {
        streamptr->ncmode = 1;
        cdf_redef(fileID);
      }

    cdf_def_dim(fileID, axisname, dimlen, &dimID);

    if (switchNCMode)
      {
        cdf_enddef(fileID);
        streamptr->ncmode = 2;
      }
  }
dimIDEstablished:
  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ncIDs[CDF_DIMID_X] = dimID;
}

struct idSearch
{
  int numNonMatching, foundID;
  size_t foundIdx;
};

static inline struct idSearch
cdfSearchIDBySize(size_t startIdx, size_t numIDs, const ncgrid_t ncgrid[/*numIDs*/], int ncIDType, int searchType, int searchSize,
                  int (*typeInq)(int id), SizeType (*sizeInq)(int id))
{
  int numNonMatching = 0, foundID = CDI_UNDEFID;
  size_t foundIdx = SIZE_MAX;
  for (size_t index = startIdx; index < numIDs; index++)
    {
      if (ncgrid[index].ncIDs[ncIDType] != CDI_UNDEFID)
        {
          int id0 = ncgrid[index].gridID, id0Type = typeInq(id0);
          if (id0Type == searchType)
            {
              int size0 = sizeInq(id0);
              if (searchSize == size0)
                {
                  foundID = ncgrid[index].ncIDs[ncIDType];
                  foundIdx = index;
                  break;
                }
              numNonMatching++;
            }
        }
    }
  return (struct idSearch){ .numNonMatching = numNonMatching, .foundID = foundID, .foundIdx = foundIdx };
}

static SizeType
cdfGridInqHalfSize(int gridID)
{
  return gridInqSize(gridID) / 2;
}

static void
cdfDefSPorFC(stream_t *streamptr, int gridID, int gridindex, char *restrict axisname, int gridRefType)
{
  ncgrid_t *ncgrid = streamptr->ncgrid;

  const size_t dimlen = gridInqSize(gridID) / 2;

  struct idSearch search
      = cdfSearchIDBySize(0, (size_t) gridindex, ncgrid, CDF_DIMID_Y, gridRefType, (int) dimlen, gridInqType, cdfGridInqHalfSize);
  int dimID = search.foundID;
  const int iz = search.numNonMatching;

  if (dimID == CDI_UNDEFID)
    {
      int fileID = streamptr->fileID;
      if (iz == 0)
        axisname[3] = '\0';
      else
        sprintf(&axisname[3], "%1d", iz + 1);

      bool switchNCMode = streamptr->ncmode == 2;
      if (switchNCMode)
        {
          streamptr->ncmode = 1;
          cdf_redef(fileID);
        }

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      if (switchNCMode)
        {
          cdf_enddef(fileID);
          streamptr->ncmode = 2;
        }
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ncIDs[CDF_DIMID_Y] = dimID;
}

static void
cdfDefSP(stream_t *streamptr, int gridID, int gridindex)
{
  // char longname[] = "Spherical harmonic coefficient";
  char axisname[5] = "nspX";
  cdfDefSPorFC(streamptr, gridID, gridindex, axisname, GRID_SPECTRAL);
}

static void
cdfDefFC(stream_t *streamptr, int gridID, int gridindex)
{
  char axisname[5] = "nfcX";
  cdfDefSPorFC(streamptr, gridID, gridindex, axisname, GRID_FOURIER);
}

static const struct cdfDefGridAxisInqs
{
  SizeType (*axisSize)(int gridID);
  double (*axisVal)(int gridID, SizeType index);
  const double *(*axisValsPtr)(int gridID);
  const double *(*axisBoundsPtr)(int gridID);
  enum cdfIDIdx dimIdx, varIdx;
  char axisSym;
  enum gridPropInq valsQueryKey, bndsQueryKey;
  char axisPanoplyName[3];
} gridInqsX = {
  .axisSize = gridInqXsize,
  .axisVal = gridInqXval,
  .axisValsPtr = gridInqXvalsPtr,
  .axisBoundsPtr = gridInqXboundsPtr,
  .dimIdx = CDF_DIMID_X,
  .varIdx = CDF_VARID_X,
  .axisSym = 'X',
  .valsQueryKey = GRID_PROP_XVALS,
  .bndsQueryKey = GRID_PROP_XBOUNDS,
  .axisPanoplyName = "Lon",
}, gridInqsY = {
  .axisSize = gridInqYsize,
  .axisVal = gridInqYval,
  .axisValsPtr = gridInqYvalsPtr,
  .axisBoundsPtr = gridInqYboundsPtr,
  .dimIdx = CDF_DIMID_Y,
  .varIdx = CDF_VARID_Y,
  .axisSym = 'Y',
  .valsQueryKey = GRID_PROP_YVALS,
  .bndsQueryKey = GRID_PROP_YBOUNDS,
  .axisPanoplyName = "Lat",
};

static void
cdfPutGridStdAtts(int fileID, int ncvarid, int gridID, int dimtype)
{
  size_t len;

  int axisKey = (dimtype == 'Z') ? CDI_GLOBAL : ((dimtype == 'X') ? CDI_XAXIS : CDI_YAXIS);

  {
    char stdname[CDI_MAX_NAME];
    int length = CDI_MAX_NAME;
    cdiInqKeyString(gridID, axisKey, CDI_KEY_STDNAME, stdname, &length);
    if (stdname[0] && (len = strlen(stdname))) cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);
  }
  {
    char longname[CDI_MAX_NAME];
    int length = CDI_MAX_NAME;
    cdiInqKeyString(gridID, axisKey, CDI_KEY_LONGNAME, longname, &length);
    if (longname[0] && (len = strlen(longname))) cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
  }
  {
    char units[CDI_MAX_NAME];
    int length = CDI_MAX_NAME;
    cdiInqKeyString(gridID, axisKey, CDI_KEY_UNITS, units, &length);
    if (units[0] && (len = strlen(units))) cdf_put_att_text(fileID, ncvarid, "units", len, units);
  }
}

static int
grid_inq_xtype(int gridID)
{
  int datatype = CDI_UNDEFID;
  cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);
  return (datatype == CDI_DATATYPE_FLT32) ? NC_FLOAT : NC_DOUBLE;
}

static void
cdfDefTrajLatLon(stream_t *streamptr, int gridID, int gridindex, const struct cdfDefGridAxisInqs *inqs)
{
  const nc_type xtype = grid_inq_xtype(gridID);
  ncgrid_t *ncgrid = streamptr->ncgrid;

  const size_t dimlen = inqs->axisSize(gridID);
  if (dimlen != 1) Error("%c size isn't 1 for %s grid!", inqs->axisSym, gridNamePtr(gridInqType(gridID)));

  int ncvarid = ncgrid[gridindex].ncIDs[inqs->dimIdx];
  if (ncvarid == CDI_UNDEFID)
    {
      int dimNcID = streamptr->basetime.ncvarid;
      const int fileID = streamptr->fileID;
      bool switchNCMode = streamptr->ncmode == 2;
      if (switchNCMode)
        {
          cdf_redef(fileID);
          switchNCMode = true;
          streamptr->ncmode = 1;
        }

      char axisname[CDI_MAX_NAME];
      const int axistype = (inqs->axisSym == 'X') ? CDI_XAXIS : CDI_YAXIS;
      int length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, axistype, CDI_KEY_NAME, axisname, &length);
      cdf_def_var(fileID, axisname, xtype, 1, &dimNcID, &ncvarid);
      cdfPutGridStdAtts(fileID, ncvarid, gridID, inqs->axisSym);
      if (switchNCMode)
        {
          cdf_enddef(fileID);
          streamptr->ncmode = 2;
        }
    }

  ncgrid[gridindex].gridID = gridID;
  // var ID for trajectory !!!
  ncgrid[gridindex].ncIDs[inqs->dimIdx] = ncvarid;
}

static void
cdfDefTrajLon(stream_t *streamptr, int gridID, int gridindex)
{
  cdfDefTrajLatLon(streamptr, gridID, gridindex, &gridInqsX);
}

static void
cdfDefTrajLat(stream_t *streamptr, int gridID, int gridindex)
{
  cdfDefTrajLatLon(streamptr, gridID, gridindex, &gridInqsY);
}

static int
checkDimName(int fileID, size_t dimlen, char *dimname)
{
  // check whether the dimenion name is already defined with the same length
  unsigned iz = 0;
  int dimid = CDI_UNDEFID;
  char name[CDI_MAX_NAME];

  const size_t len = strlen(dimname);
  memcpy(name, dimname, len + 1);

  do
    {
      if (iz) sprintf(name + len, "_%u", iz + 1);

      int dimid0;
      const int status = nc_inq_dimid(fileID, name, &dimid0);
      if (status != NC_NOERR) break;
      size_t dimlen0;
      cdf_inq_dimlen(fileID, dimid0, &dimlen0);
      if (dimlen0 == dimlen)
        {
          dimid = dimid0;
          break;
        }
      iz++;
    }
  while (iz <= 99);

  if (iz) sprintf(dimname + len, "_%u", iz + 1);

  return dimid;
}

static void
checkGridName(char *axisname, int fileID)
{
  int ncdimid;
  char axisname2[CDI_MAX_NAME];

  // check that the name is not already defined
  unsigned iz = 0;

  const size_t axisnameLen = strlen(axisname);
  memcpy(axisname2, axisname, axisnameLen + 1);

  do
    {
      if (iz) sprintf(axisname2 + axisnameLen, "_%u", iz + 1);

      if (nc_inq_varid(fileID, axisname2, &ncdimid) != NC_NOERR) break;

      ++iz;
    }
  while (iz <= 99);

  if (iz) sprintf(axisname + axisnameLen, "_%u", iz + 1);
}

static int
checkZaxisName(char *axisname, int fileID, int vlistID, int zaxisID, int nzaxis)
{
  char axisname2[CDI_MAX_NAME];

  // check that the name is not already defined
  unsigned iz = 0;

  const size_t axisnameLen = strlen(axisname);
  memcpy(axisname2, axisname, axisnameLen + 1);
  do
    {
      if (iz) sprintf(axisname2 + axisnameLen, "_%u", iz + 1);

      int ncdimid;
      const int status = nc_inq_varid(fileID, axisname2, &ncdimid);
      if (status != NC_NOERR)
        {
          if (iz)
            {
              // check that the name does not exist for other zaxes
              for (int index = 0; index < nzaxis; index++)
                {
                  const int zaxisID0 = vlistZaxis(vlistID, index);
                  if (zaxisID != zaxisID0)
                    {
                      const char *axisname0 = zaxisInqNamePtr(zaxisID0);
                      if (strcmp(axisname0, axisname2) == 0) goto nextSuffix;
                    }
                }
            }
          break;
        }
    nextSuffix:
      ++iz;
    }
  while (iz <= 99);

  if (iz) sprintf(axisname + axisnameLen, "_%u", iz + 1);

  return (int) iz;
}

struct cdfPostDefPutVar
{
  int fileID, ncvarid;
  union
  {
    const void *array;
    int int1;
  } values;
};

static void
cdfDelayedPutVarDouble(void *data)
{
  struct cdfPostDefPutVar *put = (struct cdfPostDefPutVar *) data;
  cdf_put_var_double(put->fileID, put->ncvarid, put->values.array);
}

static void
cdfDelayedPutVarInt1(void *data)
{
  struct cdfPostDefPutVar *put = (struct cdfPostDefPutVar *) data;
  cdf_put_var_int(put->fileID, put->ncvarid, &put->values.int1);
}

static void
cdfDelayedPutVarDeepCleanup(void *data)
{
  struct cdfPostDefPutVar *what = (struct cdfPostDefPutVar *) data;
  Free((void *) what->values.array);
  Free(what);
}

static void
cdfPostDefActionApply(size_t numActions, struct cdfPostDefAction *actions)
{
  for (size_t i = 0; i < numActions; ++i) actions[i].action(actions[i].data);
}

static void
cdfPostDefActionListDelete(struct cdfPostDefActionList *list)
{
  struct cdfPostDefAction *actions = list->actions;
  for (size_t i = 0, len = list->len; i < len; ++i)
    {
      void (*cleanup)(void *) = actions[i].cleanup;
      void *data = actions[i].data;
      if (cleanup == (void (*)(void *)) memFree)
        Free(data);
      else
        cleanup(data);
    }
  Free(list);
}

static struct cdfPostDefActionList *
cdfPostDefActionAdd(struct cdfPostDefActionList *list, struct cdfPostDefAction addendum)
{
  size_t appendPos = list ? list->len : 0;
  if (!list || list->size == list->len)
    {
      enum
      {
        initialListSize = 1
      };
      size_t newSize = list ? (list->size * 2) : initialListSize, newLen = list ? list->len + 1 : 1,
             newAllocSize = sizeof(struct cdfPostDefActionList) + newSize * sizeof(struct cdfPostDefAction);
      list = (struct cdfPostDefActionList *) Realloc(list, newAllocSize);
      list->size = newSize;
      list->len = newLen;
    }
  else
    ++(list->len);
  list->actions[appendPos] = addendum;
  return list;
}

static struct cdfPostDefActionList *
cdfPostDefActionConcat(struct cdfPostDefActionList *listA, const struct cdfPostDefActionList *listB)
{
  size_t appendPos = listA ? listA->len : 0, appendLen = listB ? listB->len : 0;
  if (appendLen)
    {
      size_t newLen = appendPos + appendLen;
      if (!listA || listA->size < newLen)
        {
          enum
          {
            initialListSize = 1
          };
          size_t newSize = listA ? listA->size : initialListSize;
          while (newSize < newLen) newSize *= 2;
          size_t newAllocSize = sizeof(struct cdfPostDefActionList) + newSize * sizeof(struct cdfPostDefAction);
          listA = (struct cdfPostDefActionList *) Realloc(listA, newAllocSize);
          listA->size = newSize;
          listA->len = newLen;
        }
      else
        listA->len = newLen;
      struct cdfPostDefAction *restrict actionsA = listA->actions;
      const struct cdfPostDefAction *restrict actionsB = listB->actions;
      for (size_t i = 0; i < appendLen; ++i) actionsA[appendPos + i] = actionsB[i];
    }
  return listA;
}

static inline void
cdfPostDefActionAddPutVal(struct cdfPostDefActionList **list_, int fileID, int ncvarid, const double *values,
                          void (*cleanup)(void *))
{
  struct cdfPostDefPutVar *delayedPutVals = (struct cdfPostDefPutVar *) Malloc(sizeof(*delayedPutVals));
  delayedPutVals->values.array = values;
  delayedPutVals->fileID = fileID;
  delayedPutVals->ncvarid = ncvarid;
  *list_ = cdfPostDefActionAdd(
      *list_, (struct cdfPostDefAction){ .data = (void *) delayedPutVals, .action = cdfDelayedPutVarDouble, .cleanup = cleanup });
}

static inline void
cdfPostDefActionAddPut1Int(struct cdfPostDefActionList **list_, int fileID, int ncvarid, int iVal, void (*cleanup)(void *))
{
  struct cdfPostDefPutVar *delayedPutVals = (struct cdfPostDefPutVar *) Malloc(sizeof(*delayedPutVals));
  delayedPutVals->values.int1 = iVal;
  delayedPutVals->fileID = fileID;
  delayedPutVals->ncvarid = ncvarid;
  *list_ = cdfPostDefActionAdd(
      *list_, (struct cdfPostDefAction){ .data = (void *) delayedPutVals, .action = cdfDelayedPutVarInt1, .cleanup = cleanup });
}

static struct cdfPostDefActionList *
cdfDefAxisCommon(stream_t *streamptr, int gridID, int gridindex, int ndims, bool addVarToGrid,
                 const struct cdfDefGridAxisInqs *gridAxisInq, int axisKey, char axisLetter,
                 void (*finishCyclicBounds)(double *pbounds, size_t dimlen, const double *pvals))
{
  int dimID = CDI_UNDEFID;
  const size_t dimlen = gridAxisInq->axisSize(gridID);
  const nc_type xtype = grid_inq_xtype(gridID);

  ncgrid_t *ncgrid = streamptr->ncgrid;

  const double *pvals = gridAxisInq->axisValsPtr(gridID);
  char dimname[CDI_MAX_NAME + 3];
  dimname[0] = 0;
  int length = sizeof(dimname);
  if (ndims && pvals == NULL) cdiInqKeyString(gridID, axisKey, CDI_KEY_DIMNAME, dimname, &length);

  for (int index = 0; index < gridindex; ++index)
    {
      const int gridID0 = ncgrid[index].gridID;
      assert(gridID0 != CDI_UNDEFID);
      const int gridtype0 = gridInqType(gridID0);
      if (gridtype0 == GRID_GAUSSIAN || gridtype0 == GRID_LONLAT || gridtype0 == GRID_PROJECTION || gridtype0 == GRID_GENERIC)
        {
          const size_t dimlen0 = gridAxisInq->axisSize(gridID0);
          char dimname0[CDI_MAX_NAME];
          dimname0[0] = 0;
          length = sizeof(dimname0);
          if (dimname[0]) cdiInqKeyString(gridID0, axisKey, CDI_KEY_DIMNAME, dimname0, &length);
          const bool lname = dimname0[0] ? strcmp(dimname, dimname0) == 0 : true;
          if (dimlen == dimlen0 && lname)
            {
              double (*inqVal)(int gridID, SizeType index) = gridAxisInq->axisVal;
              if (IS_EQUAL(inqVal(gridID0, 0), inqVal(gridID, 0))
                  && IS_EQUAL(inqVal(gridID0, dimlen - 1), inqVal(gridID, dimlen - 1)))
                {
                  dimID = ncgrid[index].ncIDs[axisLetter == 'X' ? CDF_DIMID_X : CDF_DIMID_Y];
                  break;
                }
            }
        }
    }

  struct cdfPostDefActionList *delayed = NULL;
  if (dimID == CDI_UNDEFID)
    {
      int ncvarid = CDI_UNDEFID;
      char axisname[CDI_MAX_NAME];
      length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, axisKey, CDI_KEY_NAME, axisname, &length);
      const int fileID = streamptr->fileID;
      if (axisname[0] == 0) Error("axis name undefined!");

      checkGridName(axisname, fileID);
      const size_t axisnameLen = strlen(axisname);

      bool switchNCMode = streamptr->ncmode == 2 && (pvals || ndims);
      if (switchNCMode)
        {
          cdf_redef(fileID);
          streamptr->ncmode = 1;
        }

      if (ndims)
        {
          if (dimname[0] == 0) strcpy(dimname, axisname);
          dimID = checkDimName(fileID, dimlen, dimname);

          if (dimID == CDI_UNDEFID) cdf_def_dim(fileID, dimname, dimlen, &dimID);
        }

      if (pvals)
        {
          cdf_def_var(fileID, axisname, xtype, ndims, &dimID, &ncvarid);

          cdfPutGridStdAtts(fileID, ncvarid, gridID, axisLetter);
          {
            char axisStr[2] = { axisLetter, '\0' };
            cdf_put_att_text(fileID, ncvarid, "axis", 1, axisStr);
          }
          cdfPostDefActionAddPutVal(&delayed, fileID, ncvarid, pvals, (void (*)(void *)) memFree);

          bool gen_bounds = false, hasBounds = gridInqPropPresence(gridID, gridAxisInq->bndsQueryKey);
          const bool grid_is_cyclic = gridIsCircular(gridID) > 0;
          double *restrict pbounds;
          size_t nvertex = gridInqNvertex(gridID);
          if (CDI_CMOR_Mode && grid_is_cyclic && !hasBounds)
            {
              gen_bounds = true;
              nvertex = 2;
              pbounds = (double *) Malloc(2 * dimlen * sizeof(double));
              for (size_t i = 0; i < dimlen - 1; ++i)
                {
                  pbounds[i * 2 + 1] = (pvals[i] + pvals[i + 1]) * 0.5;
                  pbounds[i * 2 + 2] = (pvals[i] + pvals[i + 1]) * 0.5;
                }
              finishCyclicBounds(pbounds, dimlen, pvals);
            }
          else
            pbounds = (double *) gridAxisInq->axisBoundsPtr(gridID);

          int nvdimID = CDI_UNDEFID;
          if (pbounds)
            {
              if (nc_inq_dimid(fileID, bndsName, &nvdimID) != NC_NOERR) cdf_def_dim(fileID, bndsName, nvertex, &nvdimID);
            }
          if (pbounds && nvdimID != CDI_UNDEFID)
            {
              char boundsname[CDI_MAX_NAME];
              memcpy(boundsname, axisname, axisnameLen);
              boundsname[axisnameLen] = '_';
              memcpy(boundsname + axisnameLen + 1, bndsName, sizeof(bndsName));
              int dimIDs[2] = { dimID, nvdimID };
              int ncbvarid;
              cdf_def_var(fileID, boundsname, xtype, 2, dimIDs, &ncbvarid);
              cdf_put_att_text(fileID, ncvarid, "bounds", axisnameLen + sizeof(bndsName), boundsname);
              cdfPostDefActionAddPutVal(&delayed, fileID, ncbvarid, pbounds,
                                        gen_bounds ? cdfDelayedPutVarDeepCleanup : (void (*)(void *)) memFree);
            }
        }

      if (switchNCMode)
        {
          cdf_enddef(fileID);
          streamptr->ncmode = 2;
        }

      if (ndims == 0 || addVarToGrid) ncgrid[gridindex].ncIDs[axisLetter == 'X' ? CDF_VARID_X : CDF_VARID_Y] = ncvarid;
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ncIDs[axisLetter == 'X' ? CDF_DIMID_X : CDF_DIMID_Y] = dimID;
  return delayed;
}

static void
finishCyclicXBounds(double *pbounds, size_t dimlen, const double *pvals)
{
  pbounds[0] = (pvals[0] + pvals[dimlen - 1] - 360) * 0.5;
  pbounds[2 * dimlen - 1] = (pvals[dimlen - 1] + pvals[0] + 360) * 0.5;
}

static void
finishCyclicYBounds(double *pbounds, size_t dimlen, const double *pvals)
{
  pbounds[0] = copysign(90.0, pvals[0]);
  pbounds[2 * dimlen - 1] = copysign(90.0, pvals[dimlen - 1]);
}

static struct cdfPostDefActionList *
cdfDefXaxis(stream_t *streamptr, int gridID, int gridindex, int ndims, bool addVarToGrid)
{
  return cdfDefAxisCommon(streamptr, gridID, gridindex, ndims, addVarToGrid, &gridInqsX, CDI_XAXIS, 'X', finishCyclicXBounds);
}

static struct cdfPostDefActionList *
cdfDefYaxis(stream_t *streamptr, int gridID, int gridindex, int ndims, bool addVarToGrid)
{
  return cdfDefAxisCommon(streamptr, gridID, gridindex, ndims, addVarToGrid, &gridInqsY, CDI_YAXIS, 'Y', finishCyclicYBounds);
}

static void
cdfGridCompress(int fileID, int ncvarid, size_t gridsize, int filetype, int comptype)
{
#ifdef HAVE_NETCDF4
  if (gridsize > 1 && comptype == CDI_COMPRESS_ZIP && (filetype == CDI_FILETYPE_NC4 || filetype == CDI_FILETYPE_NC4C))
    {
      cdf_def_var_chunking(fileID, ncvarid, NC_CHUNKED, NULL);
      cdfDefVarDeflate(fileID, ncvarid, 1);
    }
#endif
}

static void
cdfDefGridReference(stream_t *streamptr, int gridID)
{
  const int fileID = streamptr->fileID;

  int number = 0;
  cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDUSED, &number);
  if (number > 0) cdf_put_att_int(fileID, NC_GLOBAL, "number_of_grid_used", NC_INT, 1, &number);

  grid_t *gridptr = grid_to_pointer(gridID);
  const char *gridfile = cdiInqVarKeyStringPtr(&gridptr->keys, CDI_KEY_REFERENCEURI);
  if (gridfile && gridfile[0] != 0) cdf_put_att_text(fileID, NC_GLOBAL, "grid_file_uri", strlen(gridfile), gridfile);
}

static void
cdfDefGridUUID(stream_t *streamptr, int gridID)
{
  unsigned char uuid[CDI_UUID_SIZE] = { 0 };
  int length = CDI_UUID_SIZE;
  cdiInqKeyBytes(gridID, CDI_GLOBAL, CDI_KEY_UUID, uuid, &length);
  if (!cdiUUIDIsNull(uuid))
    {
      char uuidStr[uuidNumHexChars + 1] = { 0 };
      if (cdiUUID2Str(uuid, uuidStr) == uuidNumHexChars)
        {
          const int fileID = streamptr->fileID;
          // if (streamptr->ncmode == 2) cdf_redef(fileID);
          cdf_put_att_text(fileID, NC_GLOBAL, "uuidOfHGrid", uuidNumHexChars, uuidStr);
          // if (streamptr->ncmode == 2  cdf_enddef(fileID);
        }
    }
}

void
cdfPostDefActionGridProp(int gridID, int fileID, int ncvarid, enum gridPropInq gridProp, struct cdfPostDefActionList **delayed)
{
  const void *valsPtr = NULL;
  switch (gridProp)
    {
    case GRID_PROP_MASK:
    case GRID_PROP_MASK_GME: Error("unsupported key: %d", (int) gridProp); break;
    case GRID_PROP_XVALS: valsPtr = gridInqXvalsPtr(gridID); break;
    case GRID_PROP_YVALS: valsPtr = gridInqYvalsPtr(gridID); break;
    case GRID_PROP_AREA: valsPtr = gridInqAreaPtr(gridID); break;
    case GRID_PROP_XBOUNDS: valsPtr = gridInqXboundsPtr(gridID); break;
    case GRID_PROP_YBOUNDS: valsPtr = gridInqYboundsPtr(gridID); break;
    }
  cdfPostDefActionAddPutVal(delayed, fileID, ncvarid, valsPtr, (void (*)(void *)) memFree);
}

static int
cdfDefIrregularGridAxisSetup(stream_t *streamptr, int gridID, nc_type xtype, int varID, int dimlens, int ndims, int dimIDs[ndims],
                             const struct cdfDefGridAxisInqs *inqs, struct cdfPostDefActionList **delayed)
{
  int ncvarid = CDI_UNDEFID;
  const int fileID = streamptr->fileID;
  if (gridInqPropPresence(gridID, inqs->valsQueryKey))
    {
      char axisname[CDI_MAX_NAME];
      int length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, varID, CDI_KEY_NAME, axisname, &length);
      checkGridName(axisname, fileID);
      cdf_def_var(fileID, axisname, xtype, ndims - 1, dimIDs, &ncvarid);
      cdfGridCompress(fileID, ncvarid, dimlens, streamptr->filetype, streamptr->comptype);

      cdfPutGridStdAtts(fileID, ncvarid, gridID, inqs->axisSym);
      void (*mycdfPostDefActionGridProp)(int, int, int, enum gridPropInq, struct cdfPostDefActionList **)
          = namespaceSwitchGet(NSSWITCH_CDF_POSTDEFACTION_GRID_PROP).func;
      mycdfPostDefActionGridProp(gridID, fileID, ncvarid, inqs->valsQueryKey, delayed);

      // attribute for Panoply
      if (ndims == 3) cdf_put_att_text(fileID, ncvarid, "_CoordinateAxisType", 3, inqs->axisPanoplyName);

      if (gridInqPropPresence(gridID, inqs->bndsQueryKey) && dimIDs[ndims - 1] != CDI_UNDEFID)
        {
          const size_t axisnameLen = strlen(axisname);
          axisname[axisnameLen] = '_';
          memcpy(axisname + axisnameLen + 1, bndsName, sizeof(bndsName));
          int ncbvarid;
          cdf_def_var(fileID, axisname, xtype, ndims, dimIDs, &ncbvarid);
          cdfGridCompress(fileID, ncbvarid, dimlens, streamptr->filetype, streamptr->comptype);

          cdf_put_att_text(fileID, ncvarid, "bounds", axisnameLen + sizeof(bndsName), axisname);
          mycdfPostDefActionGridProp(gridID, fileID, ncbvarid, inqs->bndsQueryKey, delayed);
        }
    }
  return ncvarid;
}

struct cdfDefIrregularGridCommonIDs
{
  int xdimID, ydimID, ncxvarid, ncyvarid, ncavarid;
  struct cdfPostDefActionList *delayed;
};

static struct cdfDefIrregularGridCommonIDs
cdfDefIrregularGridCommon(stream_t *streamptr, int gridID, size_t xdimlen, size_t ydimlen, int ndims, const char *xdimname_default,
                          size_t nvertex, const char *vdimname_default, bool setVdimname)
{
  const nc_type xtype = grid_inq_xtype(gridID);
  int xdimID = CDI_UNDEFID;
  int ydimID = CDI_UNDEFID;
  const int fileID = streamptr->fileID;
  bool switchNCMode = streamptr->ncmode == 2;
  if (switchNCMode)
    {
      cdf_redef(fileID);
      streamptr->ncmode = 1;
    }

  {
    char xdimname[CDI_MAX_NAME + 3];
    int length = sizeof(xdimname);
    cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_DIMNAME, xdimname, &length);
    if (xdimname[0] == 0) strcpy(xdimname, xdimname_default);
    xdimID = checkDimName(fileID, xdimlen, xdimname);
    if (xdimID == CDI_UNDEFID) cdf_def_dim(fileID, xdimname, xdimlen, &xdimID);
  }

  if (ndims == 3)
    {
      char ydimname[CDI_MAX_NAME + 3];
      int length = sizeof(ydimname);
      cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_DIMNAME, ydimname, &length);
      if (ydimname[0] == 0)
        {
          ydimname[0] = 'y';
          ydimname[1] = 0;
        }
      ydimID = checkDimName(fileID, ydimlen, ydimname);
      if (ydimID == CDI_UNDEFID) cdf_def_dim(fileID, ydimname, ydimlen, &ydimID);
    }

  int nvdimID = CDI_UNDEFID;
  int dimIDs[3];
  dimIDs[ndims - 1] = CDI_UNDEFID;
  if (setVdimname)
    {
      char vdimname[CDI_MAX_NAME + 3];
      int length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_VDIMNAME, vdimname, &length);
      if (vdimname[0] == 0) strcpy(vdimname, vdimname_default);
      nvdimID = dimIDs[ndims - 1] = checkDimName(fileID, nvertex, vdimname);
      if (nvdimID == CDI_UNDEFID)
        {
          cdf_def_dim(fileID, vdimname, nvertex, dimIDs + ndims - 1);
          nvdimID = dimIDs[ndims - 1];
        }
    }

  if (ndims == 3)
    {
      dimIDs[0] = ydimID;
      dimIDs[1] = xdimID;
    }
  else /* ndims == 2 */
    {
      dimIDs[0] = xdimID;
      cdfDefGridReference(streamptr, gridID);
      cdfDefGridUUID(streamptr, gridID);
    }

  struct cdfPostDefActionList *delayed = NULL;
  int ncxvarid = cdfDefIrregularGridAxisSetup(streamptr, gridID, xtype, CDI_XAXIS, (int) (xdimlen * ydimlen), ndims, dimIDs,
                                              &gridInqsX, &delayed);
  int ncyvarid = cdfDefIrregularGridAxisSetup(streamptr, gridID, xtype, CDI_YAXIS, (int) (xdimlen * ydimlen), ndims, dimIDs,
                                              &gridInqsY, &delayed);

  int ncavarid = CDI_UNDEFID;
  if (gridInqPropPresence(gridID, GRID_PROP_AREA))
    {
      static const char yaxisname_[] = "cell_area";
      static const char units[] = "m2";
      static const char longname[] = "area of grid cell";
      static const char stdname[] = "cell_area";

      cdf_def_var(fileID, yaxisname_, xtype, ndims - 1, dimIDs, &ncavarid);

      cdf_put_att_text(fileID, ncavarid, "standard_name", sizeof(stdname) - 1, stdname);
      cdf_put_att_text(fileID, ncavarid, "long_name", sizeof(longname) - 1, longname);
      cdf_put_att_text(fileID, ncavarid, "units", sizeof(units) - 1, units);
      void (*mycdfPostDefActionGridProp)(int, int, int, enum gridPropInq, struct cdfPostDefActionList **)
          = namespaceSwitchGet(NSSWITCH_CDF_POSTDEFACTION_GRID_PROP).func;
      mycdfPostDefActionGridProp(gridID, fileID, ncavarid, GRID_PROP_AREA, &delayed);
    }

  if (switchNCMode)
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  return (struct cdfDefIrregularGridCommonIDs){
    .xdimID = xdimID, .ydimID = ydimID, .ncxvarid = ncxvarid, .ncyvarid = ncyvarid, .ncavarid = ncavarid, .delayed = delayed
  };
}

static struct cdfPostDefActionList *
cdfDefCurvilinear(stream_t *streamptr, int gridID, int gridindex)
{
  ncgrid_t *ncgrid = streamptr->ncgrid;

  const size_t dimlen = gridInqSize(gridID);
  const size_t xdimlen = gridInqXsize(gridID);
  const size_t ydimlen = gridInqYsize(gridID);

  int xdimID = CDI_UNDEFID, ydimID = CDI_UNDEFID;
  int ncxvarid = CDI_UNDEFID, ncyvarid = CDI_UNDEFID, ncavarid = CDI_UNDEFID;

  size_t ofs = 0;
  do
    {
      struct idSearch search = cdfSearchIDBySize(ofs, (size_t) gridindex, ncgrid, CDF_DIMID_X, GRID_CURVILINEAR, (int) dimlen,
                                                 gridInqType, gridInqSize);
      const size_t index = search.foundIdx;
      if (index != SIZE_MAX)
        {
          const int gridID0 = ncgrid[index].gridID;
          if (IS_EQUAL(gridInqXval(gridID0, 0), gridInqXval(gridID, 0))
              && IS_EQUAL(gridInqXval(gridID0, dimlen - 1), gridInqXval(gridID, dimlen - 1))
              && IS_EQUAL(gridInqYval(gridID0, 0), gridInqYval(gridID, 0))
              && IS_EQUAL(gridInqYval(gridID0, dimlen - 1), gridInqYval(gridID, dimlen - 1)))
            {
              xdimID = ncgrid[index].ncIDs[CDF_DIMID_X];
              ydimID = ncgrid[index].ncIDs[CDF_DIMID_Y];
              ncxvarid = ncgrid[index].ncIDs[CDF_VARID_X];
              ncyvarid = ncgrid[index].ncIDs[CDF_VARID_Y];
              break;
            }
          ofs = search.foundIdx;
          if (ofs < (size_t) gridindex) continue;
        }
    }
  while (false);

  struct cdfPostDefActionList *delayed = NULL;
  if (xdimID == CDI_UNDEFID || ydimID == CDI_UNDEFID)
    {
      struct cdfDefIrregularGridCommonIDs createdIDs = cdfDefIrregularGridCommon(
          streamptr, gridID, xdimlen, ydimlen, 3, "x", 4, "nv4",
          gridInqPropPresence(gridID, GRID_PROP_XBOUNDS) || gridInqPropPresence(gridID, GRID_PROP_YBOUNDS));
      xdimID = createdIDs.xdimID;
      ydimID = createdIDs.ydimID;
      ncxvarid = createdIDs.ncxvarid;
      ncyvarid = createdIDs.ncyvarid;
      ncavarid = createdIDs.ncavarid;
      delayed = createdIDs.delayed;
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ncIDs[CDF_DIMID_X] = xdimID;
  ncgrid[gridindex].ncIDs[CDF_DIMID_Y] = ydimID;
  ncgrid[gridindex].ncIDs[CDF_VARID_X] = ncxvarid;
  ncgrid[gridindex].ncIDs[CDF_VARID_Y] = ncyvarid;
  ncgrid[gridindex].ncIDs[CDF_VARID_A] = ncavarid;
  return delayed;
}

static struct cdfPostDefActionList *
cdfDefUnstructured(stream_t *streamptr, int gridID, int gridindex)
{
  ncgrid_t *ncgrid = streamptr->ncgrid;

  const size_t dimlen = gridInqSize(gridID);

  int dimID = CDI_UNDEFID;
  int ncxvarid = CDI_UNDEFID, ncyvarid = CDI_UNDEFID, ncavarid = CDI_UNDEFID;

  size_t ofs = 0;
  do
    {
      struct idSearch search = cdfSearchIDBySize(ofs, (size_t) gridindex, ncgrid, CDF_DIMID_X, GRID_UNSTRUCTURED, (int) dimlen,
                                                 gridInqType, gridInqSize);
      const size_t index = search.foundIdx;
      if (index != SIZE_MAX)
        {
          const int gridID0 = ncgrid[index].gridID;
          if (gridInqNvertex(gridID0) == gridInqNvertex(gridID) && IS_EQUAL(gridInqXval(gridID0, 0), gridInqXval(gridID, 0))
              && IS_EQUAL(gridInqXval(gridID0, dimlen - 1), gridInqXval(gridID, dimlen - 1))
              && IS_EQUAL(gridInqYval(gridID0, 0), gridInqYval(gridID, 0))
              && IS_EQUAL(gridInqYval(gridID0, dimlen - 1), gridInqYval(gridID, dimlen - 1)))
            {
              dimID = ncgrid[index].ncIDs[CDF_DIMID_X];
              ncxvarid = ncgrid[index].ncIDs[CDF_VARID_X];
              ncyvarid = ncgrid[index].ncIDs[CDF_VARID_Y];
              ncavarid = ncgrid[index].ncIDs[CDF_VARID_A];
              break;
            }
          ofs = search.foundIdx;
          if (ofs < (size_t) gridindex) continue;
        }
    }
  while (false);

  struct cdfPostDefActionList *delayed = NULL;
  if (dimID == CDI_UNDEFID)
    {
      const size_t nvertex = (size_t) gridInqNvertex(gridID);
      struct cdfDefIrregularGridCommonIDs createdIDs
          = cdfDefIrregularGridCommon(streamptr, gridID, dimlen, 1, 2, "ncells", nvertex, "vertices", nvertex > 0);
      dimID = createdIDs.xdimID;
      ncxvarid = createdIDs.ncxvarid;
      ncyvarid = createdIDs.ncyvarid;
      ncavarid = createdIDs.ncavarid;
      delayed = createdIDs.delayed;
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ncIDs[CDF_DIMID_X] = dimID;
  ncgrid[gridindex].ncIDs[CDF_VARID_X] = ncxvarid;
  ncgrid[gridindex].ncIDs[CDF_VARID_Y] = ncyvarid;
  ncgrid[gridindex].ncIDs[CDF_VARID_A] = ncavarid;
  return delayed;
}

struct attTxtTab
{
  const char *txt;
  size_t txtLen;
};

struct attTxtTab2
{
  const char *attName, *attVal;
  size_t valLen;
};

static struct cdfPostDefActionList *
cdf_def_vct_echam(stream_t *streamptr, int zaxisID)
{
  const int type = zaxisInqType(zaxisID);

  int ilev;
  struct cdfPostDefActionList *delayed = NULL;
  if ((type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF) && (ilev = zaxisInqVctSize(zaxisID) / 2) != 0)
    {
      const int mlev = ilev - 1;

      if (streamptr->vct.ilev > 0)
        {
          if (streamptr->vct.ilev != ilev) Error("More than one VCT for each file unsupported!");
          return delayed;
        }

      const int fileID = streamptr->fileID;

      bool switchNCMode = streamptr->ncmode == 2;
      if (switchNCMode)
        {
          streamptr->ncmode = 1;
          cdf_redef(fileID);
        }

      int ncdimid = -1, ncdimid2 = -1;
      int hyaiid, hybiid, hyamid = -1, hybmid = -1;

      cdf_def_dim(fileID, "nhyi", (size_t) ilev, &ncdimid2);
      cdf_def_var(fileID, "hyai", NC_DOUBLE, 1, &ncdimid2, &hyaiid);
      cdf_def_var(fileID, "hybi", NC_DOUBLE, 1, &ncdimid2, &hybiid);
      if (mlev > 0)
        {
          cdf_def_dim(fileID, "nhym", (size_t) mlev, &ncdimid);
          cdf_def_var(fileID, "hyam", NC_DOUBLE, 1, &ncdimid, &hyamid);
          cdf_def_var(fileID, "hybm", NC_DOUBLE, 1, &ncdimid, &hybmid);
        }

      streamptr->vct.ilev = ilev;
      streamptr->vct.mlev = mlev;
      streamptr->vct.mlevID = ncdimid;
      streamptr->vct.ilevID = ncdimid2;

      {
        static const char lname_n[] = "long_name", units_n[] = "units", lname_v_ai[] = "hybrid A coefficient at layer interfaces",
                          units_v_ai[] = "Pa", lname_v_bi[] = "hybrid B coefficient at layer interfaces", units_v_bi[] = "1";
        static const struct attTxtTab2 tab[] = {
          { lname_n, lname_v_ai, sizeof(lname_v_ai) - 1 },
          { units_n, units_v_ai, sizeof(units_v_ai) - 1 },
          { lname_n, lname_v_bi, sizeof(lname_v_bi) - 1 },
          { units_n, units_v_bi, sizeof(units_v_bi) - 1 },
        };
        enum
        {
          tabLen = sizeof(tab) / sizeof(tab[0])
        };
        const int ids[tabLen] = { hyaiid, hyaiid, hybiid, hybiid };
        for (size_t i = 0; i < tabLen; ++i) cdf_put_att_text(fileID, ids[i], tab[i].attName, tab[i].valLen, tab[i].attVal);
      }

      {
        static const char lname_n[] = "long_name", units_n[] = "units", lname_v_am[] = "hybrid A coefficient at layer midpoints",
                          units_v_am[] = "Pa", lname_v_bm[] = "hybrid B coefficient at layer midpoints", units_v_bm[] = "1";
        static const struct attTxtTab2 tab[] = {
          { lname_n, lname_v_am, sizeof(lname_v_am) - 1 },
          { units_n, units_v_am, sizeof(units_v_am) - 1 },
          { lname_n, lname_v_bm, sizeof(lname_v_bm) - 1 },
          { units_n, units_v_bm, sizeof(units_v_bm) - 1 },
        };
        enum
        {
          tabLen = sizeof(tab) / sizeof(tab[0])
        };
        const int ids[tabLen] = { hyamid, hyamid, hybmid, hybmid };
        for (size_t i = 0; i < tabLen; ++i) cdf_put_att_text(fileID, ids[i], tab[i].attName, tab[i].valLen, tab[i].attVal);
      }

      if (switchNCMode)
        {
          cdf_enddef(fileID);
          streamptr->ncmode = 2;
        }

      const double *vctptr = zaxisInqVctPtr(zaxisID);

      cdfPostDefActionAddPutVal(&delayed, fileID, hyaiid, vctptr, (void (*)(void *)) memFree);
      cdfPostDefActionAddPutVal(&delayed, fileID, hybiid, vctptr + ilev, (void (*)(void *)) memFree);
      {
        double *restrict amidVal = (double *) Malloc((size_t) mlev * sizeof(*amidVal));
        for (size_t i = 0; i < (size_t) mlev; ++i) amidVal[i] = (vctptr[i] + vctptr[i + 1]) * 0.5;
        cdfPostDefActionAddPutVal(&delayed, fileID, hyamid, amidVal, cdfDelayedPutVarDeepCleanup);
      }
      {
        double *restrict bmidVal = (double *) Malloc((size_t) mlev * sizeof(*bmidVal));
        for (size_t i = 0; i < (size_t) mlev; ++i) bmidVal[i] = (vctptr[(size_t) ilev + i] + vctptr[(size_t) ilev + i + 1]) * 0.5;
        cdfPostDefActionAddPutVal(&delayed, fileID, hybmid, bmidVal, cdfDelayedPutVarDeepCleanup);
      }
    }
  return delayed;
}

static struct cdfPostDefActionList *
cdf_def_vct_cf(stream_t *streamptr, int zaxisID, int nclevID, int ncbndsID, int p0status, double p0value)
{
  const int type = zaxisInqType(zaxisID);

  struct cdfPostDefActionList *delayed = NULL;
  int ilev;
  if ((type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF) && (ilev = zaxisInqVctSize(zaxisID) / 2) != 0)
    {
      const int mlev = ilev - 1;

      if (streamptr->vct.ilev > 0)
        {
          if (streamptr->vct.ilev != ilev) Error("more than one VCT for each file unsupported!");
          return delayed;
        }

      const int fileID = streamptr->fileID;

      bool switchNCMode = streamptr->ncmode == 2;
      if (switchNCMode)
        {
          cdf_redef(fileID);
          streamptr->ncmode = 1;
        }

      int dimIDs[2] = { nclevID, ncbndsID };

      streamptr->vct.mlev = mlev;
      streamptr->vct.ilev = ilev;
      streamptr->vct.mlevID = nclevID;
      streamptr->vct.ilevID = nclevID;

      int hyamid, hybmid;
      cdf_def_var(fileID, (p0status == 0) ? "a" : "ap", NC_DOUBLE, 1, dimIDs, &hyamid);
      cdf_def_var(fileID, "b", NC_DOUBLE, 1, dimIDs, &hybmid);

      {
        static const char anametab[][10] = { "long_name", "units" };
        static const char lname_v_a[] = "vertical coordinate formula term: ap(k)",
                          lname_v_b[] = "vertical coordinate formula term: b(k)", units_v_a[] = "Pa", units_v_b[] = "1";
        static struct attTxtTab attvtab[][2] = { { { lname_v_a, sizeof(lname_v_a) - 1 }, { units_v_a, sizeof(units_v_a) - 1 } },
                                                 { { lname_v_b, sizeof(lname_v_b) - 1 }, { units_v_b, sizeof(units_v_b) - 1 } } };
        int termid[] = { hyamid, hybmid };
        enum
        {
          numTerms = sizeof(termid) / sizeof(termid[0]),
          numAtts = sizeof(anametab) / sizeof(anametab[0]),
        };
        for (size_t termIdx = 0; termIdx < numTerms; ++termIdx)
          for (size_t attIdx = 0; attIdx < numAtts; ++attIdx)
            cdf_put_att_text(fileID, termid[termIdx], anametab[attIdx], attvtab[termIdx][attIdx].txtLen,
                             attvtab[termIdx][attIdx].txt);
      }
      double *restrict vctptr = (double *) zaxisInqVctPtr(zaxisID);
      if (p0status == 0 && IS_NOT_EQUAL(p0value, 0))
        {
          double *restrict temp = (double *) Malloc((size_t) ilev * sizeof(*temp));
          for (size_t i = 0; i < (size_t) ilev; ++i) temp[i] = vctptr[i] / p0value;
          vctptr = temp;
        }

      {
        double *restrict mlevValA = (double *) Malloc((size_t) mlev * sizeof(*mlevValA));
        for (size_t i = 0; i < (size_t) mlev; ++i) mlevValA[i] = (vctptr[i] + vctptr[i + 1]) * 0.5;
        cdfPostDefActionAddPutVal(&delayed, fileID, hyamid, mlevValA, cdfDelayedPutVarDeepCleanup);
      }
      {
        double *restrict mlevValB = (double *) Malloc((size_t) mlev * sizeof(*mlevValB));
        for (size_t i = 0; i < (size_t) mlev; ++i) mlevValB[i] = (vctptr[(size_t) ilev + i] + vctptr[(size_t) ilev + i + 1]) * 0.5;
        cdfPostDefActionAddPutVal(&delayed, fileID, hybmid, mlevValB, cdfDelayedPutVarDeepCleanup);
      }

      if (ncbndsID != -1)
        {
          int hyaiid, hybiid;
          cdf_def_var(fileID, (p0status == 0) ? "a_bnds" : "ap_bnds", NC_DOUBLE, 2, dimIDs, &hyaiid);
          cdf_def_var(fileID, "b_bnds", NC_DOUBLE, 2, dimIDs, &hybiid);
          static const char anametab[][10] = { "long_name", "units" };
          static const char lname_v_a[] = "vertical coordinate formula term: ap(k+1/2)",
                            lname_v_b[] = "vertical coordinate formula term: b(k+1/2)", units_v_a[] = "Pa", units_v_b[] = "1";
          static struct attTxtTab attvtab[][2] = { { { lname_v_a, sizeof(lname_v_a) - 1 }, { units_v_a, sizeof(units_v_a) - 1 } },
                                                   { { lname_v_b, sizeof(lname_v_b) - 1 }, { units_v_b, sizeof(units_v_b) - 1 } } };
          int termid[] = { hyaiid, hybiid };
          enum
          {
            numTerms = sizeof(termid) / sizeof(termid[0]),
            numAtts = sizeof(anametab) / sizeof(anametab[0]),
          };
          for (size_t termIdx = 0; termIdx < numTerms; ++termIdx)
            for (size_t attIdx = 0; attIdx < numAtts; ++attIdx)
              cdf_put_att_text(fileID, termid[termIdx], anametab[attIdx], attvtab[termIdx][attIdx].txtLen,
                               attvtab[termIdx][attIdx].txt);

          {
            double *restrict ilevValA = (double *) Malloc((size_t) mlev * 2 * sizeof(*ilevValA));
            for (size_t i = 0; i < (size_t) mlev; ++i)
              {
                ilevValA[2 * i] = vctptr[i];
                ilevValA[2 * i + 1] = vctptr[i + 1];
              }
            cdfPostDefActionAddPutVal(&delayed, fileID, hyaiid, ilevValA, cdfDelayedPutVarDeepCleanup);
          }
          {
            double *restrict ilevValB = (double *) Malloc((size_t) mlev * 2 * sizeof(*ilevValB));
            for (size_t i = 0; i < (size_t) mlev; ++i)
              {
                ilevValB[2 * i] = vctptr[(size_t) ilev + i];
                ilevValB[2 * i + 1] = vctptr[(size_t) ilev + i + 1];
              }
            cdfPostDefActionAddPutVal(&delayed, fileID, hybiid, ilevValB, cdfDelayedPutVarDeepCleanup);
          }
        }
      if (p0status == 0 && IS_NOT_EQUAL(p0value, 0)) Free(vctptr);

      if (switchNCMode)
        {
          cdf_enddef(fileID);
          streamptr->ncmode = 2;
        }
    }
  return delayed;
}

static struct cdfPostDefActionList *
cdf_def_zaxis_hybrid_echam(stream_t *streamptr, int type, int *ncvaridp, int zaxisID, int zaxisindex, int xtype, size_t dimlen,
                           int *dimID, char *axisname)
{
  const int fileID = streamptr->fileID;
  struct cdfPostDefActionList *delayed = NULL;
  bool switchNCMode = streamptr->ncmode == 2;
  if (switchNCMode)
    {
      streamptr->ncmode = 1;
      cdf_redef(fileID);
    }

  cdf_def_dim(fileID, axisname, dimlen, dimID);
  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, dimID, ncvaridp);
  const int ncvarid = *ncvaridp;

  {
    static const char sname[] = "hybrid_sigma_pressure";
    cdf_put_att_text(fileID, ncvarid, "standard_name", sizeof(sname) - 1, sname);
  }
  {
    static const char *attName[] = { "long_name", "formula", "formula_terms" };
    enum
    {
      nAtt = sizeof(attName) / sizeof(attName[0])
    };
    static const char lname_m[] = "hybrid level at layer midpoints", formula_m[] = "hyam hybm (mlev=hyam+hybm*aps)",
                      fterms_m[] = "ap: hyam b: hybm ps: aps", lname_i[] = "hybrid level at layer interfaces",
                      formula_i[] = "hyai hybi (ilev=hyai+hybi*aps)", fterms_i[] = "ap: hyai b: hybi ps: aps";
    static const struct attTxtTab tab[2][nAtt]
        = { { { lname_i, sizeof(lname_i) - 1 }, { formula_i, sizeof(formula_i) - 1 }, { fterms_i, sizeof(fterms_i) - 1 } },
            { { lname_m, sizeof(lname_m) - 1 }, { formula_m, sizeof(formula_m) - 1 }, { fterms_m, sizeof(fterms_m) - 1 } } };

    const size_t tabSelect = type == ZAXIS_HYBRID;
    for (size_t i = 0; i < nAtt; ++i)
      cdf_put_att_text(fileID, ncvarid, attName[i], tab[tabSelect][i].txtLen, tab[tabSelect][i].txt);
  }

  {
    static const char units[] = "level";
    cdf_put_att_text(fileID, ncvarid, "units", sizeof(units) - 1, units);
  }
  {
    static const char direction[] = "down";
    cdf_put_att_text(fileID, ncvarid, "positive", sizeof(direction) - 1, direction);
  }

  cdfPostDefActionAddPutVal(&delayed, fileID, ncvarid, zaxisInqLevelsPtr(zaxisID), (void (*)(void *)) memFree);

  {
    struct cdfPostDefActionList *delayedVct = cdf_def_vct_echam(streamptr, zaxisID);
    delayed = cdfPostDefActionConcat(delayed, delayedVct);
    Free(delayedVct);
  }

  if (*dimID == CDI_UNDEFID) streamptr->zaxisID[zaxisindex] = type == ZAXIS_HYBRID ? streamptr->vct.mlevID : streamptr->vct.ilevID;

  if (switchNCMode)
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  return delayed;
}

static struct cdfPostDefActionList *
cdf_def_zaxis_hybrid_cf(stream_t *streamptr, int type, int *ncvaridp, int zaxisID, int zaxisindex, int xtype, size_t dimlen,
                        int *dimID, char *axisname)
{
  char psname[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_PSNAME, psname, &length);
  if (psname[0] == 0) strcpy(psname, "ps");

  const int fileID = streamptr->fileID;
  bool switchNCMode = streamptr->ncmode == 2;
  if (switchNCMode)
    {
      streamptr->ncmode = 1;
      cdf_redef(fileID);
    }

  char p0name[CDI_MAX_NAME];
  p0name[0] = 0;
  double p0value = 1;
  int p0varid = CDI_UNDEFID;
  const int p0status = cdiInqKeyFloat(zaxisID, CDI_GLOBAL, CDI_KEY_P0VALUE, &p0value);
  if (p0status == CDI_NOERR)
    {
      length = CDI_MAX_NAME;
      cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_P0NAME, p0name, &length);
      if (p0name[0] == 0) strcpy(p0name, "p0");
      cdf_def_var(fileID, p0name, NC_DOUBLE, 0, 0, &p0varid);
      static const char longname[] = "reference pressure";
      cdf_put_att_text(fileID, p0varid, "long_name", sizeof(longname) - 1, longname);
      static const char units[] = "Pa";
      cdf_put_att_text(fileID, p0varid, "units", sizeof(units) - 1, units);
    }

  char zname[CDI_MAX_NAME];
  char zlongname[CDI_MAX_NAME];
  char zunits[CDI_MAX_NAME];
  length = CDI_MAX_NAME;
  cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_NAME, zname, &length);
  if (zname[0]) strcpy(axisname, zname);
  zlongname[0] = 0;
  size_t zlongnameLen;
  if (zlongname[0] == 0)
    {
      static const char default_zlongname[] = "hybrid sigma pressure coordinate";
      memcpy(zlongname, default_zlongname, sizeof(default_zlongname));
      zlongnameLen = sizeof(default_zlongname) - 1;
    }
  else
    zlongnameLen = strlen(zlongname);
  length = CDI_MAX_NAME;
  cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_UNITS, zunits, &length);
  size_t zunitsLen;
  if (zunits[0] == 0)
    {
      zunits[0] = '1';
      zunits[1] = '\0';
      zunitsLen = 1;
    }
  else
    zunitsLen = strlen(zunits);

  cdf_def_dim(fileID, axisname, dimlen, dimID);
  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, dimID, ncvaridp);
  int ncvarid = *ncvaridp;

  {
    static const char sname[] = "standard_name", lname[] = "long_name", sname_v[] = "atmosphere_hybrid_sigma_pressure_coordinate",
                      axis[] = "axis", axis_v[] = "Z", direction[] = "positive", direction_v[] = "down", units[] = "units";
    struct attTxtTab2 tab[] = {
      { sname, sname_v, sizeof(sname_v) - 1 },
      { axis, axis_v, sizeof(axis_v) - 1 },
      { direction, direction_v, sizeof(direction_v) - 1 },
      { units, zunits, zunitsLen },
      { lname, zlongname, zlongnameLen },
    };
    enum
    {
      nAtt = sizeof(tab) / sizeof(tab[0])
    };
    for (size_t i = 0; i < nAtt; ++i) cdf_put_att_text(fileID, ncvarid, tab[i].attName, tab[i].valLen, tab[i].attVal);
  }

  size_t len = 0;
  char txt[CDI_MAX_NAME];
  if (p0status == 0)
    len = (size_t) (sprintf(txt, "%s%s %s%s", "a: a b: b p0: ", p0name, "ps: ", psname));
  else
    len = (size_t) (sprintf(txt, "%s%s", "ap: ap b: b ps: ", psname));
  cdf_put_att_text(fileID, ncvarid, "formula_terms", len, txt);

  int ncbvarid = CDI_UNDEFID;
  int nvdimID = CDI_UNDEFID;

  double *buffer = (double *) malloc(2 * dimlen * sizeof(double));
  double *lbounds = buffer;
  double *ubounds = buffer + dimlen;
  double *restrict levels;

  bool hasLevels = zaxisInqLevels(zaxisID, NULL) != 0;
  if (hasLevels)
    levels = (double *) zaxisInqLevelsPtr(zaxisID);
  else
    {
      levels = (double *) Malloc(sizeof(*levels) * dimlen);
      for (size_t i = 0; i < dimlen; ++i) levels[i] = (double) (i + 1);
    }

  if (zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL))
    {
      zaxisInqLbounds(zaxisID, lbounds);
      zaxisInqUbounds(zaxisID, ubounds);
    }
  else
    {
      for (size_t i = 0; i < dimlen; ++i) lbounds[i] = levels[i];
      for (size_t i = 0; i < dimlen - 1; ++i) ubounds[i] = levels[i + 1];
      ubounds[dimlen - 1] = levels[dimlen - 1] + 1;
    }

  // if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
  {
    const size_t nvertex = 2;
    if (dimlen > 1 && nc_inq_dimid(fileID, bndsName, &nvdimID) != NC_NOERR) cdf_def_dim(fileID, bndsName, nvertex, &nvdimID);

    if (nvdimID != CDI_UNDEFID)
      {
        size_t axisnameLen = strlen(axisname);
        axisname[axisnameLen] = '_';
        memcpy(axisname + axisnameLen + 1, bndsName, sizeof(bndsName));
        axisnameLen += sizeof(bndsName);
        int dimIDs[2] = { *dimID, nvdimID };
        cdf_def_var(fileID, axisname, (nc_type) xtype, 2, dimIDs, &ncbvarid);
        cdf_put_att_text(fileID, ncvarid, "bounds", axisnameLen, axisname);
        size_t formulatermsLen;
        if (p0status == 0)
          formulatermsLen = (size_t) (sprintf(txt, "%s%s %s%s", "a: a_bnds b: b_bnds p0: ", p0name, "ps: ", psname));
        else
          formulatermsLen = (size_t) (sprintf(txt, "%s%s", "ap: ap_bnds b: b_bnds ps: ", psname));
        {
          static const char sname[] = "standard_name", sname_v[] = "atmosphere_hybrid_sigma_pressure_coordinate",
                            formulaterms[] = "formula_terms", units[] = "units";
          struct attTxtTab2 tab[] = {
            { sname, sname_v, sizeof(sname_v) - 1 },
            { units, zunits, zunitsLen },
            { formulaterms, txt, formulatermsLen },
          };
          enum
          {
            nAtt = sizeof(tab) / sizeof(tab[0])
          };
          for (size_t i = 0; i < nAtt; ++i) cdf_put_att_text(fileID, ncbvarid, tab[i].attName, tab[i].valLen, tab[i].attVal);
        }
      }
  }

  if (switchNCMode)
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  if (p0varid != CDI_UNDEFID) cdf_put_var_double(fileID, p0varid, &p0value);

  struct cdfPostDefActionList *delayed = NULL;
  cdfPostDefActionAddPutVal(&delayed, fileID, ncvarid, levels,
                            hasLevels ? (void (*)(void *)) memFree : cdfDelayedPutVarDeepCleanup);

  if (ncbvarid != CDI_UNDEFID)
    {
      double *restrict zbounds = (double *) Malloc(2 * dimlen * sizeof(*zbounds));
      for (size_t i = 0; i < dimlen; ++i)
        {
          zbounds[2 * i] = lbounds[i];
          zbounds[2 * i + 1] = ubounds[i];
        }
      cdfPostDefActionAddPutVal(&delayed, fileID, ncbvarid, zbounds, cdfDelayedPutVarDeepCleanup);
    }

  {
    struct cdfPostDefActionList *delayedVct = cdf_def_vct_cf(streamptr, zaxisID, *dimID, nvdimID, p0status, p0value);
    delayed = cdfPostDefActionConcat(delayed, delayedVct);
    Free(delayedVct);
  }

  if (*dimID == CDI_UNDEFID) streamptr->zaxisID[zaxisindex] = type == ZAXIS_HYBRID ? streamptr->vct.mlevID : streamptr->vct.ilevID;

  free(buffer);
  return delayed;
}

static struct cdfPostDefActionList *
cdf_def_zaxis_hybrid(stream_t *streamptr, int type, int *ncvarid, int zaxisID, int zaxisindex, int xtype, size_t dimlen, int *dimID,
                     char *axisname)
{
  struct cdfPostDefActionList *(*def_zaxis_hybrid_delegate)(stream_t * streamptr, int type, int *ncvarid, int zaxisID,
                                                            int zaxisindex, int xtype, size_t dimlen, int *dimID, char *axisname)
      = ((!CDI_CMOR_Mode && CDI_Convention == CDI_CONVENTION_ECHAM) || type == ZAXIS_HYBRID_HALF) ? cdf_def_zaxis_hybrid_echam
                                                                                                  : cdf_def_zaxis_hybrid_cf;
  return def_zaxis_hybrid_delegate(streamptr, type, ncvarid, zaxisID, zaxisindex, xtype, dimlen, dimID, axisname);
}

static void
cdfDefZaxisUUID(stream_t *streamptr, int zaxisID)
{
  unsigned char uuid[CDI_UUID_SIZE] = { 0 };
  int length = CDI_UUID_SIZE;
  cdiInqKeyBytes(zaxisID, CDI_GLOBAL, CDI_KEY_UUID, uuid, &length);
  if (!cdiUUIDIsNull(uuid))
    {
      char uuidStr[uuidNumHexChars + 1] = { 0 };
      if (cdiUUID2Str(uuid, uuidStr) == uuidNumHexChars)
        {
          const int fileID = streamptr->fileID;
          bool switchNCMode = streamptr->ncmode == 2;
          if (switchNCMode)
            {
              streamptr->ncmode = 1;
              cdf_redef(fileID);
            }

          cdf_put_att_text(fileID, NC_GLOBAL, "uuidOfVGrid", uuidNumHexChars, uuidStr);
          if (switchNCMode)
            {
              cdf_enddef(fileID);
              streamptr->ncmode = 2;
            }
        }
    }
}

#ifndef USE_MPI
static void
cdfDefZaxisChar(stream_t *streamptr, int zaxisID, char *axisname, int *dimID, size_t dimlen, int zaxisindex)
{
  const int fileID = streamptr->fileID;
  int ncvarID = CDI_UNDEFID;
  if (streamptr->ncmode == 2) cdf_redef(fileID);

  // Check StrlenID
  char strlen[8] = "strlen\0";
  const size_t clen = (size_t) zaxisInqCLen(zaxisID);
  if (clen == 0)
    Error("Maximal string length value is 0.\nA given character axis requires a dimension to save the maximal string length.");
  int strlenID = CDI_UNDEFID;
  strlenID = checkDimName(fileID, clen, strlen);

  if (strlenID == CDI_UNDEFID) cdf_def_dim(fileID, strlen, clen, &strlenID);

  // Check 'areatype'dimID
  char dimname[CDI_MAX_NAME + 3];
  int length = sizeof(dimname);
  cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_DIMNAME, dimname, &length);
  *dimID = checkDimName(fileID, dimlen, dimname);
  if (dimlen <= 0) Error("No strings delivered for a character axis.");
  if (dimname[0] == 0)
    {
      memcpy(dimname, "area_type", 10);
      dimname[10] = 0;
    }

  if (*dimID == CDI_UNDEFID) cdf_def_dim(fileID, dimname, dimlen, dimID);

  int dimIDs[2];
  dimIDs[0] = *dimID;
  dimIDs[1] = strlenID;

  // Get Stringvalues
  char **cvals = zaxisInqCValsPtr(zaxisID);

  if (cvals)
    {
      // Define variable and its attributes
      cdf_def_var(fileID, axisname, NC_CHAR, 2, dimIDs, &ncvarID);

      cdfPutGridStdAtts(fileID, ncvarID, zaxisID, 'Z');
      cdf_put_att_text(fileID, ncvarID, "axis", 1, "Z");
      cdfDefineAttributes(zaxisID, CDI_GLOBAL, fileID, ncvarID);

      streamptr->nczvarID[zaxisindex] = ncvarID;
      cdf_enddef(fileID);

      // Write Stringvalues
      size_t start[2], count[2];
      start[1] = 0;
      count[0] = 1;
      count[1] = clen;
      for (size_t i = 0; i < dimlen; i++)
        {
          start[0] = i;
          nc_put_vara_text(fileID, ncvarID, start, count, cvals[i]);
        }
    }

  streamptr->ncmode = 2;
}
#endif

static int
zaxis_inq_xtype(int zaxisID)
{
  int datatype = CDI_UNDEFID;
  cdiInqKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);
  int xtype = NC_DOUBLE;
  // clang-format off
  if      (datatype == CDI_DATATYPE_FLT32) xtype = NC_FLOAT;
  else if (datatype == CDI_DATATYPE_INT32) xtype = NC_INT;
  else if (datatype == CDI_DATATYPE_INT16) xtype = NC_SHORT;
  // clang-format on
  return xtype;
}

static struct cdfPostDefActionList *
cdfDefZaxis(stream_t *streamptr, int zaxisID)
{
  // char zaxisname0[CDI_MAX_NAME];
  int ncvarid = CDI_UNDEFID, ncbvarid = CDI_UNDEFID;
  const int xtype = zaxis_inq_xtype(zaxisID);

  const size_t dimlen = (size_t) zaxisInqSize(zaxisID);
  const int type = zaxisInqType(zaxisID);

  int ndims = 1;
  struct cdfPostDefActionList *delayed = NULL;

  if (dimlen == 1)
    {
      bool is_scalar = zaxisInqScalar(zaxisID) > 0;
      if (!is_scalar && CDI_CMOR_Mode)
        {
          is_scalar = true;
          zaxisDefScalar(zaxisID);
        }

      if (is_scalar) ndims = 0;
      if (CDI_Reduce_Dim) return delayed;

      switch (type)
        {
        case ZAXIS_SURFACE:
        case ZAXIS_CLOUD_BASE:
        case ZAXIS_CLOUD_TOP:
        case ZAXIS_ISOTHERM_ZERO:
        case ZAXIS_TROPOPAUSE:
        case ZAXIS_TOA:
        case ZAXIS_SEA_BOTTOM:
        case ZAXIS_ATMOSPHERE:
        case ZAXIS_MEANSEA:
        case ZAXIS_LAKE_BOTTOM:
        case ZAXIS_SEDIMENT_BOTTOM:
        case ZAXIS_SEDIMENT_BOTTOM_TA:
        case ZAXIS_SEDIMENT_BOTTOM_TW:
        case ZAXIS_MIX_LAYER: return delayed;
        }
    }

  const int vlistID = streamptr->vlistID;
  char axisname[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_NAME, axisname, &length);
  const int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  const int nzaxis = vlistNzaxis(vlistID);
  const int fileID = streamptr->fileID;

  checkZaxisName(axisname, fileID, vlistID, zaxisID, nzaxis);

  char dimname[CDI_MAX_NAME + 3];
  dimname[0] = 0;
  if (dimname[0] == 0) strcpy(dimname, axisname);

  if (type == ZAXIS_REFERENCE) cdfDefZaxisUUID(streamptr, zaxisID);

  int dimID;
  if (type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF)
    {
      delayed = cdf_def_zaxis_hybrid(streamptr, type, &ncvarid, zaxisID, zaxisindex, xtype, dimlen, &dimID, axisname);

      int natts;
      cdiInqNatts(zaxisID, CDI_GLOBAL, &natts);
      cdfDefineAttributes(zaxisID, CDI_GLOBAL, fileID, ncvarid);
    }
#ifndef USE_MPI
  else if (type == ZAXIS_CHAR)
    cdfDefZaxisChar(streamptr, zaxisID, axisname, &dimID, dimlen, zaxisindex);
#endif
  else
    {
      dimID = checkDimName(fileID, dimlen, dimname);

      bool switchNCMode = streamptr->ncmode == 2;
      if (switchNCMode)
        {
          streamptr->ncmode = 1;
          cdf_redef(fileID);
        }

      if (ndims && dimID == CDI_UNDEFID) cdf_def_dim(fileID, dimname, dimlen, &dimID);

      if (zaxisInqLevels(zaxisID, NULL))
        {
          cdf_def_var(fileID, axisname, (nc_type) xtype, ndims, &dimID, &ncvarid);

          cdfPutGridStdAtts(fileID, ncvarid, zaxisID, 'Z');

          {
            const int positive = zaxisInqPositive(zaxisID);
            static const char positive_up[] = "up", positive_down[] = "down";
            static const struct attTxtTab tab[2] = {
              { positive_up, sizeof(positive_up) - 1 },
              { positive_down, sizeof(positive_down) - 1 },
            };
            if (positive == POSITIVE_UP || positive == POSITIVE_DOWN)
              {
                const size_t select = (positive == POSITIVE_DOWN);
                cdf_put_att_text(fileID, ncvarid, "positive", tab[select].txtLen, tab[select].txt);
              }
          }
          cdf_put_att_text(fileID, ncvarid, "axis", 1, "Z");
          cdfPostDefActionAddPutVal(&delayed, fileID, ncvarid, zaxisInqLevelsPtr(zaxisID), (void (*)(void *)) memFree);

          if (zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL))
            {
              int nvdimID = CDI_UNDEFID;
              const size_t nvertex = 2;
              if (nc_inq_dimid(fileID, bndsName, &nvdimID) != NC_NOERR) cdf_def_dim(fileID, bndsName, nvertex, &nvdimID);

              if (nvdimID != CDI_UNDEFID)
                {
                  {
                    const size_t axisnameLen = strlen(axisname);
                    axisname[axisnameLen] = '_';
                    memcpy(axisname + axisnameLen + 1, bndsName, sizeof(bndsName));
                    int dimIDs[2];
                    dimIDs[0] = dimID;
                    dimIDs[ndims] = nvdimID;
                    cdf_def_var(fileID, axisname, (nc_type) xtype, ndims + 1, dimIDs, &ncbvarid);
                    cdf_put_att_text(fileID, ncvarid, "bounds", axisnameLen + sizeof(bndsName), axisname);
                  }
                  {
                    double *restrict zbounds = (double *) Malloc(4 * dimlen * sizeof(*zbounds)),
                                     *restrict lbounds = zbounds + 2 * dimlen, *restrict ubounds = zbounds + 3 * dimlen;
                    zaxisInqLbounds(zaxisID, lbounds);
                    zaxisInqUbounds(zaxisID, ubounds);
                    for (size_t i = 0; i < dimlen; ++i)
                      {
                        zbounds[2 * i] = lbounds[i];
                        zbounds[2 * i + 1] = ubounds[i];
                      }
                    zbounds = (double *) Realloc(zbounds, 2 * dimlen * sizeof(*zbounds));
                    cdfPostDefActionAddPutVal(&delayed, fileID, ncbvarid, zbounds, cdfDelayedPutVarDeepCleanup);
                  }
                }
            }
          cdfDefineAttributes(zaxisID, CDI_GLOBAL, fileID, ncvarid);
        }

      if (switchNCMode)
        {
          cdf_enddef(fileID);
          streamptr->ncmode = 2;
        }

      if (zaxisInqLevels(zaxisID, NULL) && ndims == 0) streamptr->nczvarID[zaxisindex] = ncvarid;
    }

  if (dimID != CDI_UNDEFID) streamptr->zaxisID[zaxisindex] = dimID;
  return delayed;
}

static struct cdfPostDefActionList *
cdf_def_mapping(stream_t *streamptr, int gridID)
{
  struct cdfPostDefActionList *delayed = NULL;

  int natts;
  cdiInqNatts(gridID, CDI_GLOBAL, &natts);
  if (natts == 0) return delayed;

  int datatype = -1;
  int status = cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARTYPE, &datatype);
  nc_type gmapvartype = (status == CDI_NOERR) ? (nc_type) datatype : NC_INT;
  char gmapvarname[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARNAME, gmapvarname, &length);
  if (!gmapvarname[0]) strcpy(gmapvarname, "crs");

  const int fileID = streamptr->fileID;
  bool switchNCMode = streamptr->ncmode == 2;
  if (switchNCMode)
    {
      streamptr->ncmode = 1;
      cdf_redef(fileID);
    }

  int ncvarid;
  const int ncerrcode = nc_def_var(fileID, gmapvarname, (nc_type) gmapvartype, 0, NULL, &ncvarid);
  if (ncerrcode == NC_NOERR) cdfDefineAttributes(gridID, CDI_GLOBAL, fileID, ncvarid);

  if (switchNCMode)
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  if (ncerrcode == NC_NOERR && gmapvartype != NC_CHAR)
    {
      cdfPostDefActionAddPut1Int(&delayed, fileID, ncvarid, 1, (void (*)(void *)) memFree);
    }
  return delayed;
}

static void
cdfDefCharacter(stream_t *streamptr, int gridID, int gridindex, int cdiAxisID, int strlen)
{
  if (streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_X] != CDI_UNDEFID) return;

  const bool isXaxis = (cdiAxisID == CDI_XAXIS);

  const size_t dimlen = isXaxis ? gridInqXsize(gridID) : gridInqYsize(gridID);
  ncgrid_t *ncgrid = streamptr->ncgrid;

  // Check for all grids up to gridindex whether it already is defined

  for (int index = 0; index < gridindex; index++)
    {
      const int gridID0 = ncgrid[index].gridID;
      const int gridtype0 = gridInqType(gridID0);
      if (gridtype0 == GRID_CHARXY)
        {
          if (gridInqXIsc(gridID0) == strlen && (size_t) gridInqXsize(gridID0) == dimlen)
            return;
          else if (gridInqYIsc(gridID0) == strlen && (size_t) gridInqYsize(gridID0) == dimlen)
            return;
        }
    }

  const int fileID = streamptr->fileID;

  if (streamptr->ncmode == 2) cdf_redef(fileID);

  // Define Dims

  char dimname[CDI_MAX_NAME + 3];
  int length = sizeof(dimname);
  cdiInqKeyString(gridID, cdiAxisID, CDI_KEY_DIMNAME, dimname, &length);
  if (dimname[0] == 0)
    {
      memcpy(dimname, "region", 7);
      dimname[6] = 0;
    }
  int dimID = checkDimName(fileID, dimlen, dimname);
  if (dimID == CDI_UNDEFID) cdf_def_dim(fileID, dimname, dimlen, &dimID);

  // Define strlength dim

  strcpy(dimname, "strlen");
  int strlenID = checkDimName(fileID, strlen, dimname);
  if (strlenID == CDI_UNDEFID) cdf_def_dim(fileID, dimname, strlen, &strlenID);

  // Define Variable

  int dimIDs[2];
  dimIDs[0] = dimID;
  dimIDs[1] = strlenID;

  char axisname[CDI_MAX_NAME];
  char **cvals = (char **) Malloc(dimlen * sizeof(char *));
  for (size_t i = 0; i < dimlen; i++) cvals[i] = (char *) Malloc(strlen * sizeof(char));
  int ncaxisid;
  length = CDI_MAX_NAME;
  cdiInqKeyString(gridID, cdiAxisID, CDI_KEY_NAME, axisname, &length);
  gridInqXCvals(gridID, cvals);

  int status = nc_inq_varid(fileID, axisname, &ncaxisid);
  if (status == NC_NOERR) return;

  cdf_def_var(fileID, axisname, NC_CHAR, 2, dimIDs, &ncaxisid);
  cdfPutGridStdAtts(fileID, ncaxisid, gridID, isXaxis ? 'X' : 'Y');

  cdf_enddef(fileID);

  // Write Var

  size_t start[2], count[2];
  start[1] = 0;
  count[0] = 1;
  count[1] = strlen;
  for (size_t i = 0; i < dimlen; i++)
    {
      start[0] = i;
      status = nc_put_vara_text(fileID, ncaxisid, start, count, cvals[i]);
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ncIDs[isXaxis ? CDF_DIMID_X : CDF_DIMID_Y] = dimID;
  ncgrid[gridindex].ncIDs[isXaxis ? CDF_VARID_X : CDF_VARID_Y] = ncaxisid;

  streamptr->ncmode = 2;
}

static void
cdfDefRgrid(stream_t *streamptr, int gridID, int gridindex)
{
  ncgrid_t *ncgrid = streamptr->ncgrid;

  ncgrid[gridindex].gridID = gridID;

  {
    const size_t dimlen = gridInqSize(gridID);

    struct idSearch search = cdfSearchIDBySize(0, (size_t) gridindex, ncgrid, CDF_DIMID_X, GRID_GAUSSIAN_REDUCED, (int) dimlen,
                                               gridInqType, gridInqSize);
    const int iz = search.numNonMatching;
    int dimID = search.foundID;

    if (dimID == CDI_UNDEFID)
      {
        const int fileID = streamptr->fileID;

        char axisname[16] = "rgridX";
        if (iz == 0)
          axisname[5] = '\0';
        else
          sprintf(&axisname[5], "%1d", iz + 1);

        bool switchNCMode = streamptr->ncmode == 2;
        if (switchNCMode)
          {
            streamptr->ncmode = 1;
            cdf_redef(fileID);
          }

        cdf_def_dim(fileID, axisname, dimlen, &dimID);

        if (switchNCMode)
          {
            cdf_enddef(fileID);
            streamptr->ncmode = 2;
          }
      }

    ncgrid[gridindex].ncIDs[CDF_DIMID_X] = dimID;
  }

  {
    const size_t dimlen = gridInqYsize(gridID);

    struct idSearch search = cdfSearchIDBySize(0, (size_t) gridindex, ncgrid, CDF_DIMID_RP, GRID_GAUSSIAN_REDUCED, (int) dimlen,
                                               gridInqType, gridInqSize);
    const int iz = search.numNonMatching;
    int dimID = search.foundID;

    if (dimID == CDI_UNDEFID)
      {
        const int fileID = streamptr->fileID;

        char axisname[32] = "reduced_pointsX";
        if (iz == 0)
          axisname[14] = '\0';
        else
          sprintf(&axisname[5], "%1d", iz + 1);

        bool switchNCMode = streamptr->ncmode == 2;
        if (switchNCMode)
          {
            streamptr->ncmode = 1;
            cdf_redef(fileID);
          }

        cdf_def_dim(fileID, axisname, dimlen, &dimID);

        int ncvarid = CDI_UNDEFID;

        int ndims = 1;
        const nc_type xtype = NC_INT;
        cdf_def_var(fileID, axisname, xtype, ndims, &dimID, &ncvarid);

        if (switchNCMode)
          {
            cdf_enddef(fileID);
            streamptr->ncmode = 2;
          }

        int *reducedPoints = (int *) Malloc(dimlen * sizeof(int));
        gridInqReducedPoints(gridID, reducedPoints);
        cdf_put_var_int(fileID, ncvarid, reducedPoints);
        Free(reducedPoints);

        ncgrid[gridindex].ncIDs[CDF_VARID_RP] = ncvarid;
      }

    ncgrid[gridindex].ncIDs[CDF_DIMID_RP] = dimID;
  }
}

static void
cdfDefGdim(stream_t *streamptr, int gridID, int gridindex)
{
  ncgrid_t *ncgrid = streamptr->ncgrid;
  int dimID = CDI_UNDEFID;

  const size_t dimlen = gridInqSize(gridID);

  if (gridInqYsize(gridID) == 0)
    {
      struct idSearch search
          = cdfSearchIDBySize(0, (size_t) gridindex, ncgrid, CDF_DIMID_X, GRID_GENERIC, (int) dimlen, gridInqType, gridInqSize);
      dimID = search.foundID;
    }

  if (gridInqXsize(gridID) == 0)
    {
      struct idSearch search
          = cdfSearchIDBySize(0, (size_t) gridindex, ncgrid, CDF_DIMID_Y, GRID_GENERIC, (int) dimlen, gridInqType, gridInqSize);
      dimID = search.foundID;
    }

  if (dimID == CDI_UNDEFID)
    {
      int fileID = streamptr->fileID;
      char dimname[CDI_MAX_NAME];
      strcpy(dimname, "gsize");

      dimID = checkDimName(fileID, dimlen, dimname);

      if (dimID == CDI_UNDEFID)
        {
          bool switchNCMode = streamptr->ncmode == 2;
          if (switchNCMode)
            {
              streamptr->ncmode = 1;
              cdf_redef(fileID);
            }

          cdf_def_dim(fileID, dimname, dimlen, &dimID);

          if (switchNCMode)
            {
              cdf_enddef(fileID);
              streamptr->ncmode = 2;
            }
        }
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ncIDs[CDF_DIMID_X] = dimID;
}

static struct cdfPostDefActionList *
cdfDefGrid(stream_t *streamptr, int gridID, int gridindex)
{
  struct cdfPostDefActionList *delayed = NULL;

  if (streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_X] != CDI_UNDEFID) return delayed;

  const int gridtype = gridInqType(gridID);
  const size_t size = gridInqSize(gridID);

  if (CDI_Debug) Message("gridtype = %d  size = %zu", gridtype, size);

  if (CDI_Reduce_Dim && size == 1)  // no grid information
    {
      streamptr->ncgrid[gridindex].gridID = gridID;
      return delayed;
    }

  if (gridtype == GRID_GAUSSIAN || gridtype == GRID_LONLAT || gridtype == GRID_PROJECTION || gridtype == GRID_GENERIC)
    {
      if (gridtype == GRID_GENERIC)
        {
          if (size == 1 && gridInqXsize(gridID) == 0 && gridInqYsize(gridID) == 0)
            {
              // no grid information
              streamptr->ncgrid[gridindex].gridID = gridID;
            }
          else
            {
              bool lx = false, ly = false;
              if (gridInqXsize(gridID) /*&& gridInqXvals(gridID, NULL) */)
                {
                  struct cdfPostDefActionList *xdelayed = cdfDefXaxis(streamptr, gridID, gridindex, 1, false);
                  delayed = cdfPostDefActionConcat(delayed, xdelayed);
                  Free(xdelayed);
                  lx = true;
                }

              if (gridInqYsize(gridID) /*&& gridInqYvals(gridID, NULL) */)
                {
                  struct cdfPostDefActionList *ydelayed = cdfDefYaxis(streamptr, gridID, gridindex, 1, false);
                  delayed = cdfPostDefActionConcat(delayed, ydelayed);
                  Free(ydelayed);
                  ly = true;
                }

              if (!lx && !ly) cdfDefGdim(streamptr, gridID, gridindex);
            }
        }
      else
        {
          const int ndims = !(gridtype == GRID_LONLAT && size == 1 && !gridInqHasDims(gridID));

          if (gridInqXsize(gridID) > 0)
            {
              struct cdfPostDefActionList *xdelayed = cdfDefXaxis(streamptr, gridID, gridindex, ndims, false);
              delayed = cdfPostDefActionConcat(delayed, xdelayed);
              Free(xdelayed);
            }
          if (gridInqYsize(gridID) > 0)
            {
              struct cdfPostDefActionList *ydelayed = cdfDefYaxis(streamptr, gridID, gridindex, ndims, false);
              delayed = cdfPostDefActionConcat(delayed, ydelayed);
              Free(ydelayed);
            }

          struct cdfPostDefActionList *mdelayed = cdf_def_mapping(streamptr, gridID);
          delayed = cdfPostDefActionConcat(delayed, mdelayed);
          Free(mdelayed);
        }
    }
  else if (gridtype == GRID_CURVILINEAR)
    {
      delayed = cdfDefCurvilinear(streamptr, gridID, gridindex);
    }
  else if (gridtype == GRID_UNSTRUCTURED)
    {
      delayed = cdfDefUnstructured(streamptr, gridID, gridindex);
    }
  else if (gridtype == GRID_GAUSSIAN_REDUCED)
    {
      cdfDefRgrid(streamptr, gridID, gridindex);
      if (gridInqYsize(gridID)) cdfDefYaxis(streamptr, gridID, gridindex, 1, true);
    }
  else if (gridtype == GRID_SPECTRAL)
    {
      cdfDefComplex(streamptr, gridID, gridindex);
      cdfDefSP(streamptr, gridID, gridindex);
    }
  else if (gridtype == GRID_FOURIER)
    {
      cdfDefComplex(streamptr, gridID, gridindex);
      cdfDefFC(streamptr, gridID, gridindex);
    }
  else if (gridtype == GRID_TRAJECTORY)
    {
      cdfDefTrajLon(streamptr, gridID, gridindex);
      cdfDefTrajLat(streamptr, gridID, gridindex);
    }
  else if (gridtype == GRID_CHARXY)
    {
      int strlen = 0;
      if ((strlen = gridInqXIsc(gridID)))
        cdfDefCharacter(streamptr, gridID, gridindex, CDI_XAXIS, strlen);
      else if (gridInqXsize(gridID))
        cdfDefXaxis(streamptr, gridID, gridindex, 1, false);

      if ((strlen = gridInqYIsc(gridID)))
        cdfDefCharacter(streamptr, gridID, gridindex, CDI_YAXIS, strlen);
      else if (gridInqYsize(gridID))
        cdfDefYaxis(streamptr, gridID, gridindex, 1, false);
    }
  else
    {
      Error("Unsupported grid type: %s", gridNamePtr(gridtype));
    }
  return delayed;
}

void
cdfDefCoordinateVars(stream_t *streamptr)
{
  const int vlistID = streamptr->vlistID;
  if (vlistID == CDI_UNDEFID) Error("Internal problem! vlist undefined for streamptr %p", streamptr);

  if (vlistHasTime(vlistID)) cdfDefTime(streamptr);

  const int ngrids = vlistNgrids(vlistID);
  if (2 * ngrids > MAX_GRIDS_PS) Error("Internal problem! Too many grids per stream (max=%d)\n", MAX_GRIDS_PS);

  struct cdfPostDefActionList *delayed = NULL;

  ncgrid_t *restrict ncgrid = streamptr->ncgrid;
  for (int index = 0; index < 2 * ngrids; ++index)
    {
      ncgrid[index].gridID = CDI_UNDEFID;
      for (size_t i = 0; i < CDF_SIZE_ncIDs; ++i) ncgrid[index].ncIDs[i] = CDI_UNDEFID;
    }

  for (int index = 0; index < ngrids; ++index)
    {
      const int gridID = vlistGrid(vlistID, index);
      struct cdfPostDefActionList *griddelayed = cdfDefGrid(streamptr, gridID, index);
      delayed = cdfPostDefActionConcat(delayed, griddelayed);
      Free(griddelayed);
    }
  {
    int index = ngrids - 1;
    for (int i = 0; i < ngrids; ++i)
      {
        const int gridID = vlistGrid(vlistID, i);
        const int projID = gridInqProj(gridID);
        if (projID != CDI_UNDEFID)
          {
            struct cdfPostDefActionList *griddelayed = cdfDefGrid(streamptr, projID, ++index);
            delayed = cdfPostDefActionConcat(delayed, griddelayed);
            Free(griddelayed);
          }
      }
  }
  const int nzaxis = vlistNzaxis(vlistID);
  for (int index = 0; index < nzaxis; ++index)
    {
      const int zaxisID = vlistZaxis(vlistID, index);
      if (streamptr->zaxisID[index] == CDI_UNDEFID)
        {
          struct cdfPostDefActionList *zaxisdelayed = cdfDefZaxis(streamptr, zaxisID);
          delayed = cdfPostDefActionConcat(delayed, zaxisdelayed);
          Free(zaxisdelayed);
        }
    }

  if (streamptr->ncmode != 2)
    {
      cdf_enddef(streamptr->fileID);
      streamptr->ncmode = 2;
    }

  int nvars = vlistNvars(vlistID);
  for (int varID = 0; varID < nvars; varID++) cdfDefVar(streamptr, varID);

  cdfEndDef(streamptr);
  if (delayed)
    {
      cdfPostDefActionApply(delayed->len, delayed->actions);
      cdfPostDefActionListDelete(delayed);
    }
}

#endif

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
