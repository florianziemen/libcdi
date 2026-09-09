#include <limits.h>
#include <stdbool.h>

#include "cdi_int.h"
#include "grid.h"
#include "grid_pack.h"
#include "namespace.h"
#include "serialize.h"

extern const resOps gridOps;

int
gridTxCode(void *voidP)
{
  grid_t *gridP = (grid_t *) voidP;
  return gridP->vtable->txCode;
}

enum
{
  GRID_PACK_INT_IDX_SELF,
  GRID_PACK_INT_IDX_TYPE,
  GRID_PACK_INT_IDX_IS_CYCLIC,
  GRID_PACK_INT_IDX_X_FLAG,
  GRID_PACK_INT_IDX_Y_FLAG,
  GRID_PACK_INT_IDX_GME_ND,
  GRID_PACK_INT_IDX_GME_NI,
  GRID_PACK_INT_IDX_GME_NI2,
  GRID_PACK_INT_IDX_GME_NI3,
  GRID_PACK_INT_IDX_TRUNC,
  GRID_PACK_INT_IDX_NVERTEX,
  GRID_PACK_INT_IDX_REDUCED_POINTS_SIZE,
  GRID_PACK_INT_IDX_SIZE,
  GRID_PACK_INT_IDX_X_SIZE,
  GRID_PACK_INT_IDX_Y_SIZE,
  GRID_PACK_INT_IDX_LCOMPLEX,
  GRID_PACK_INT_IDX_MEMBERMASK,
  /*
  GRID_PACK_INT_IDX_XTSTDNNAME,
  GRID_PACK_INT_IDX_YTSTDNNAME,
  GRID_PACK_INT_IDX_ISCANSNEGATIVELY,
  GRID_PACK_INT_IDX_JSCANSPOSITIVELY,
  GRID_PACK_INT_IDX_JPOINTSARECONSECUTIVE,
  */
  gridNint
};

enum
{
  GRID_PACK_DBL_IDX_X_FIRST,
  GRID_PACK_DBL_IDX_Y_FIRST,
  GRID_PACK_DBL_IDX_X_LAST,
  GRID_PACK_DBL_IDX_Y_LAST,
  GRID_PACK_DBL_IDX_X_INC,
  GRID_PACK_DBL_IDX_Y_INC,
  gridNdouble
};

enum
{
  gridHasMaskFlag = 1 << 0,
  gridHasGMEMaskFlag = 1 << 1,
  gridHasXValsFlag = 1 << 2,
  gridHasYValsFlag = 1 << 3,
  gridHasAreaFlag = 1 << 4,
  gridHasXBoundsFlag = 1 << 5,
  gridHasYBoundsFlag = 1 << 6,
  gridHasReducedPointsFlag = 1 << 7,
};

static int
gridGetComponentFlags(const grid_t *gridP)
{
  int flags = 0;
  for (int prop = 0; prop < GRID_PROP_YBOUNDS + 1; ++prop)
    flags |= (gridP->vtable->inqPropPresence((grid_t *) gridP, (enum gridPropInq) prop) << prop);
  flags |= (gridHasReducedPointsFlag & (int) ((unsigned) (gridP->reducedPoints == NULL) - 1U));
  return flags;
}

int
gridGetPackSize(void *voidP, void *context)
{
  grid_t *gridP = (grid_t *) voidP;
  return gridP->vtable->getPackSize(gridP, context);
}

int
gridGetPackSizeBase(grid_t *gridP, void *context)
{
  return gridP->vtable->getPackSizeScalars(gridP, context) + gridP->vtable->getPackSizeArrays(gridP, context);
}

int
gridGetPackSizeScalars(grid_t *gridP, void *context)
{
  int packBuffSize = 0, ui32PackSize = serializeGetSize(1, CDI_DATATYPE_UINT32, context);

  packBuffSize += serializeGetSize(gridNint, CDI_DATATYPE_INT, context) + ui32PackSize;
  packBuffSize += serializeGetSize(gridNdouble, CDI_DATATYPE_FLT64, context) + ui32PackSize;

  packBuffSize += serializeKeysGetPackSize(&gridP->keys, context);
  packBuffSize += serializeKeysGetPackSize(&gridP->x.keys, context);
  packBuffSize += serializeKeysGetPackSize(&gridP->y.keys, context);

  return packBuffSize;
}

int
gridGetPackSizeArrays(grid_t *gridP, void *context)
{
  int packBuffSize = 0, ui32PackSize = serializeGetSize(1, CDI_DATATYPE_UINT32, context);

  if (gridP->reducedPoints)
  {
    xassert(gridP->reducedPointsSize);
    packBuffSize += serializeGetSize(gridP->reducedPointsSize, CDI_DATATYPE_INT, context) + ui32PackSize;
  }

  if (gridP->vtable->inqXValsPtr(gridP))
  {
    size_t count = grid_axis_size(gridP, GRID_AXIS_X);
    xassert(count && count <= INT_MAX);
    packBuffSize += serializeGetSize((int) count, CDI_DATATYPE_FLT64, context) + ui32PackSize;
  }

  if (gridP->vtable->inqYValsPtr(gridP))
  {
    size_t count = grid_axis_size(gridP, GRID_AXIS_Y);
    xassert(count && count <= INT_MAX);
    packBuffSize += serializeGetSize((int) count, CDI_DATATYPE_FLT64, context) + ui32PackSize;
  }

  if (gridP->vtable->inqAreaPtr(gridP))
  {
    size_t count = gridP->size;
    xassert(count && count <= INT_MAX);
    packBuffSize += serializeGetSize((int) count, CDI_DATATYPE_FLT64, context) + ui32PackSize;
  }

  if (gridP->x.bounds)
  {
    xassert(gridP->nvertex);
    size_t count = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_X);
    xassert(count && count <= INT_MAX);
    packBuffSize += serializeGetSize((int) count, CDI_DATATYPE_FLT64, context) + ui32PackSize;
  }

  if (gridP->y.bounds)
  {
    xassert(gridP->nvertex);
    size_t count = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_Y);
    xassert(count && count <= INT_MAX);
    packBuffSize += (serializeGetSize((int) count, CDI_DATATYPE_FLT64, context) + ui32PackSize);
  }

  if (gridP->mask)
  {
    size_t count = gridP->size;
    xassert(count && count <= INT_MAX);
    packBuffSize += serializeGetSize((int) count, CDI_DATATYPE_UCHAR, context) + ui32PackSize;
  }

  if (gridP->mask_gme)
  {
    size_t count = gridP->size;
    xassert(count && count <= INT_MAX);
    packBuffSize += serializeGetSize((int) count, CDI_DATATYPE_UCHAR, context) + ui32PackSize;
  }

  return packBuffSize;
}

int
gridUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id)
{
  int memberMask;
  grid_t *gridP
      = gridUnpackScalars(unpackBuffer, unpackBufferSize, unpackBufferPos, originNamespace, context, force_id, &memberMask);
  gridP->vtable->unpackArrays(gridP, memberMask, unpackBuffer, unpackBufferSize, unpackBufferPos, originNamespace, context);
  reshSetStatus(gridP->self, &gridOps, reshGetStatus(gridP->self, &gridOps) & ~RESH_SYNC_BIT);
  return gridP->self;
}

grid_t *
gridUnpackScalars(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id,
                  int *memberMaskP)
{
  grid_t *gridP;
  uint32_t d;
  int memberMask;
  {
    int intBuffer[gridNint];
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, intBuffer, gridNint, CDI_DATATYPE_INT, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);

    xassert(cdiCheckSum(CDI_DATATYPE_INT, gridNint, intBuffer) == d);
    int targetID = namespaceAdaptKey(intBuffer[0], originNamespace);
    gridP = gridNewEntry(force_id ? targetID : CDI_UNDEFID);

    xassert(!force_id || targetID == gridP->self);

    gridP->type = intBuffer[GRID_PACK_INT_IDX_TYPE];
    gridP->isCyclic = (signed char) intBuffer[GRID_PACK_INT_IDX_IS_CYCLIC];
    gridP->x.flag = (short) intBuffer[GRID_PACK_INT_IDX_X_FLAG];
    gridP->y.flag = (short) intBuffer[GRID_PACK_INT_IDX_Y_FLAG];
    gridP->gme.nd = intBuffer[GRID_PACK_INT_IDX_GME_ND];
    gridP->gme.ni = intBuffer[GRID_PACK_INT_IDX_GME_NI];
    gridP->gme.ni2 = intBuffer[GRID_PACK_INT_IDX_GME_NI2];
    gridP->gme.ni3 = intBuffer[GRID_PACK_INT_IDX_GME_NI3];
    gridP->trunc = intBuffer[GRID_PACK_INT_IDX_TRUNC];
    gridP->nvertex = intBuffer[GRID_PACK_INT_IDX_NVERTEX];
    gridP->reducedPointsSize = intBuffer[GRID_PACK_INT_IDX_REDUCED_POINTS_SIZE];
    /* FIXME: use int64_t for the following 3 */
    gridP->size = (size_t) intBuffer[GRID_PACK_INT_IDX_SIZE];
    gridP->x.size = (size_t) intBuffer[GRID_PACK_INT_IDX_X_SIZE];
    gridP->y.size = (size_t) intBuffer[GRID_PACK_INT_IDX_Y_SIZE];
    gridP->lcomplex = (bool) intBuffer[GRID_PACK_INT_IDX_LCOMPLEX];
    memberMask = intBuffer[GRID_PACK_INT_IDX_MEMBERMASK];
  }

  {
    double doubleBuffer[gridNdouble];
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, doubleBuffer, gridNdouble, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(d == cdiCheckSum(CDI_DATATYPE_FLT, gridNdouble, doubleBuffer));

    gridP->x.first = doubleBuffer[GRID_PACK_DBL_IDX_X_FIRST];
    gridP->y.first = doubleBuffer[GRID_PACK_DBL_IDX_Y_FIRST];
    gridP->x.last = doubleBuffer[GRID_PACK_DBL_IDX_X_LAST];
    gridP->y.last = doubleBuffer[GRID_PACK_DBL_IDX_Y_LAST];
    gridP->x.inc = doubleBuffer[GRID_PACK_DBL_IDX_X_INC];
    gridP->y.inc = doubleBuffer[GRID_PACK_DBL_IDX_Y_INC];
  }

  serializeKeysUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &gridP->keys, context);
  serializeKeysUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &gridP->x.keys, context);
  serializeKeysUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &gridP->y.keys, context);

  *memberMaskP = memberMask;
  return gridP;
}

void
gridUnpackArrays(grid_t *gridP, int memberMask, char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace,
                 void *context)
{
  UNUSED(originNamespace);
  uint32_t d;

  if (memberMask & gridHasReducedPointsFlag)
  {
    xassert(gridP->reducedPointsSize);
    gridP->reducedPoints = (int *) Malloc((size_t) gridP->reducedPointsSize * sizeof(int));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->reducedPoints, gridP->reducedPointsSize,
                    CDI_DATATYPE_INT, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_INT, gridP->reducedPointsSize, gridP->reducedPoints) == d);
  }

  if (memberMask & gridHasXValsFlag)
  {
    size_t size = grid_axis_size(gridP, GRID_AXIS_X);
    xassert(size <= INT_MAX);
    gridP->x.vals = (double *) Malloc(size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->x.vals, (int) size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->x.vals) == d);
  }

  if (memberMask & gridHasYValsFlag)
  {
    size_t size = grid_axis_size(gridP, GRID_AXIS_Y);
    xassert(size > 0 && size <= INT_MAX);
    gridP->y.vals = (double *) Malloc(size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->y.vals, (int) size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->y.vals) == d);
  }

  if (memberMask & gridHasAreaFlag)
  {
    size_t size = gridP->size;
    xassert(size > 0 && size <= INT_MAX);
    gridP->area = (double *) Malloc(size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->area, (int) size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->area) == d);
  }

  if (memberMask & gridHasXBoundsFlag)
  {
    size_t size = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_X);
    xassert(size > 0 && size <= INT_MAX);
    gridP->x.bounds = (double *) Malloc(size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->x.bounds, (int) size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->x.bounds) == d);
  }

  if (memberMask & gridHasYBoundsFlag)
  {
    size_t size = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_Y);
    xassert(size && size <= INT_MAX);
    gridP->y.bounds = (double *) Malloc(size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->y.bounds, (int) size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->y.bounds) == d);
  }

  if (memberMask & gridHasMaskFlag)
  {
    size_t size = gridP->size;
    xassert(size && size <= INT_MAX);
    gridP->mask = (mask_t *) Malloc(size * sizeof(mask_t));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->mask, (int) size, CDI_DATATYPE_UCHAR, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_UCHAR, (int) size, gridP->mask) == d);
  }

  if (memberMask & gridHasGMEMaskFlag)
  {
    size_t size = gridP->size;
    xassert(size && size <= INT_MAX);
    gridP->mask_gme = (mask_t *) Malloc(size * sizeof(mask_t));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, gridP->mask_gme, (int) size, CDI_DATATYPE_UCHAR, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_UCHAR, (int) size, gridP->mask_gme) == d);
  }
}

void
gridPack(void *voidP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  grid_t *gridP = (grid_t *) voidP;
  gridP->vtable->pack(gridP, packBuffer, packBufferSize, packBufferPos, context);
}

void
gridPackBase(grid_t *gridP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  int memberMask = gridP->vtable->packScalars(gridP, packBuffer, packBufferSize, packBufferPos, context);
  gridP->vtable->packArrays(gridP, memberMask, packBuffer, packBufferSize, packBufferPos, context);
}

int
gridPackScalars(grid_t *gridP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  uint32_t d;
  int memberMask;

  {
    int intBuffer[gridNint];

    intBuffer[GRID_PACK_INT_IDX_SELF] = gridP->self;
    intBuffer[GRID_PACK_INT_IDX_TYPE] = gridP->type;
    intBuffer[GRID_PACK_INT_IDX_IS_CYCLIC] = gridP->isCyclic;
    intBuffer[GRID_PACK_INT_IDX_X_FLAG] = gridP->x.flag;
    intBuffer[GRID_PACK_INT_IDX_Y_FLAG] = gridP->y.flag;
    intBuffer[GRID_PACK_INT_IDX_GME_ND] = gridP->gme.nd;
    intBuffer[GRID_PACK_INT_IDX_GME_NI] = gridP->gme.ni;
    intBuffer[GRID_PACK_INT_IDX_GME_NI2] = gridP->gme.ni2;
    intBuffer[GRID_PACK_INT_IDX_GME_NI3] = gridP->gme.ni3;
    intBuffer[GRID_PACK_INT_IDX_TRUNC] = gridP->trunc;
    intBuffer[GRID_PACK_INT_IDX_NVERTEX] = gridP->nvertex;
    intBuffer[GRID_PACK_INT_IDX_REDUCED_POINTS_SIZE] = gridP->reducedPointsSize;
    xassert(gridP->size <= INT_MAX && gridP->x.size <= INT_MAX && gridP->y.size <= INT_MAX);
    intBuffer[GRID_PACK_INT_IDX_SIZE] = (int) gridP->size;
    intBuffer[GRID_PACK_INT_IDX_X_SIZE] = (int) gridP->x.size;
    intBuffer[GRID_PACK_INT_IDX_Y_SIZE] = (int) gridP->y.size;
    intBuffer[GRID_PACK_INT_IDX_LCOMPLEX] = gridP->lcomplex;
    intBuffer[GRID_PACK_INT_IDX_MEMBERMASK] = memberMask = gridGetComponentFlags(gridP);

    serializePack(intBuffer, gridNint, CDI_DATATYPE_INT, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_INT, gridNint, intBuffer);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  {
    double doubleBuffer[gridNdouble];

    doubleBuffer[GRID_PACK_DBL_IDX_X_FIRST] = gridP->x.first;
    doubleBuffer[GRID_PACK_DBL_IDX_Y_FIRST] = gridP->y.first;
    doubleBuffer[GRID_PACK_DBL_IDX_X_LAST] = gridP->x.last;
    doubleBuffer[GRID_PACK_DBL_IDX_Y_LAST] = gridP->y.last;
    doubleBuffer[GRID_PACK_DBL_IDX_X_INC] = gridP->x.inc;
    doubleBuffer[GRID_PACK_DBL_IDX_Y_INC] = gridP->y.inc;

    serializePack(doubleBuffer, gridNdouble, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, gridNdouble, doubleBuffer);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  serializeKeysPack(&gridP->keys, packBuffer, packBufferSize, packBufferPos, context);
  serializeKeysPack(&gridP->x.keys, packBuffer, packBufferSize, packBufferPos, context);
  serializeKeysPack(&gridP->y.keys, packBuffer, packBufferSize, packBufferPos, context);

  return memberMask;
}

void
gridPackArrays(grid_t *gridP, int memberMask, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  uint32_t d;

  if (memberMask & gridHasReducedPointsFlag)
  {
    size_t size = (size_t) gridP->reducedPointsSize;
    xassert(size > 0 && size <= INT_MAX);
    serializePack(gridP->reducedPoints, (int) size, CDI_DATATYPE_INT, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_INT, (int) size, gridP->reducedPoints);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & gridHasXValsFlag)
  {
    size_t size = grid_axis_size(gridP, GRID_AXIS_X);
    xassert(size && size <= INT_MAX);
    const double *gridP_xvals = gridP->vtable->inqXValsPtr(gridP);
    serializePack(gridP_xvals, (int) size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP_xvals);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & gridHasYValsFlag)
  {
    size_t size = grid_axis_size(gridP, GRID_AXIS_Y);
    xassert(size && size <= INT_MAX);
    const double *gridP_yvals = gridP->vtable->inqYValsPtr(gridP);
    serializePack(gridP_yvals, (int) size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP_yvals);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & gridHasAreaFlag)
  {
    size_t size = gridP->size;
    xassert(size && size <= INT_MAX);
    serializePack(gridP->area, (int) size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->area);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & gridHasXBoundsFlag)
  {
    size_t size = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_X);
    xassert(size && size <= INT_MAX);
    serializePack(gridP->x.bounds, (int) size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->x.bounds);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & gridHasYBoundsFlag)
  {
    size_t size = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_Y);
    xassert(size && size <= INT_MAX);
    serializePack(gridP->y.bounds, (int) size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, (int) size, gridP->y.bounds);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & gridHasMaskFlag)
  {
    size_t size = gridP->size;
    xassert(size && size <= INT_MAX);
    serializePack(gridP->mask, (int) size, CDI_DATATYPE_UCHAR, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_UCHAR, (int) size, gridP->mask);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & gridHasGMEMaskFlag)
  {
    size_t size = gridP->size;
    xassert(size && size <= INT_MAX);
    serializePack(gridP->mask_gme, (int) size, CDI_DATATYPE_UCHAR, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_UCHAR, (int) size, gridP->mask_gme);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }
}
