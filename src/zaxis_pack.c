#include "zaxis.h"
#include "zaxis_pack.h"
#include "namespace.h"
#include "resource_unpack.h"
#include "serialize.h"

int
zaxisTxCode(void *zaxisP)
{
  (void) zaxisP;
  return ZAXIS;
}

enum
{
  ZAXIS_PACK_INT_SELF,
  ZAXIS_PACK_INT_TYPE,
  ZAXIS_PACK_INT_SIZE,
  ZAXIS_PACK_INT_DIRECTION,
  ZAXIS_PACK_INT_VCTSIZE,
  ZAXIS_PACK_INT_MEMBERMASK,
  zaxisNint
};

enum
{
  vals = 1 << 0,
  lbounds = 1 << 1,
  ubounds = 1 << 2,
  weights = 1 << 3,
  vct = 1 << 4,
};

static int
zaxisGetMemberMask(zaxis_t *zaxisP)
{
  int memberMask = 0;

  if (zaxisP->vals) memberMask |= vals;
  if (zaxisP->lbounds) memberMask |= lbounds;
  if (zaxisP->ubounds) memberMask |= ubounds;
  if (zaxisP->weights) memberMask |= weights;
  if (zaxisP->vct) memberMask |= vct;

  return memberMask;
}

int
zaxisGetPackSize(void *voidP, void *context)
{
  zaxis_t *zaxisP = (zaxis_t *) voidP;
  int packBufferSize = serializeGetSize(zaxisNint, CDI_DATATYPE_INT, context) + serializeGetSize(1, CDI_DATATYPE_UINT32, context);

  if (zaxisP->vals || zaxisP->lbounds || zaxisP->ubounds || zaxisP->weights) xassert(zaxisP->size);

  if (zaxisP->vals)
    packBufferSize
        += serializeGetSize(zaxisP->size, CDI_DATATYPE_FLT64, context) + serializeGetSize(1, CDI_DATATYPE_UINT32, context);

  if (zaxisP->lbounds)
    packBufferSize
        += serializeGetSize(zaxisP->size, CDI_DATATYPE_FLT64, context) + serializeGetSize(1, CDI_DATATYPE_UINT32, context);

  if (zaxisP->ubounds)
    packBufferSize
        += serializeGetSize(zaxisP->size, CDI_DATATYPE_FLT64, context) + serializeGetSize(1, CDI_DATATYPE_UINT32, context);

  if (zaxisP->weights)
    packBufferSize
        += serializeGetSize(zaxisP->size, CDI_DATATYPE_FLT64, context) + serializeGetSize(1, CDI_DATATYPE_UINT32, context);

  if (zaxisP->vct)
  {
    xassert(zaxisP->vctsize);
    packBufferSize
        += serializeGetSize(zaxisP->vctsize, CDI_DATATYPE_FLT64, context) + serializeGetSize(1, CDI_DATATYPE_UINT32, context);
  }

  packBufferSize += serializeKeysGetPackSize(&zaxisP->keys, context);

  packBufferSize += serializeGetSize(1, CDI_DATATYPE_UINT, context);

  return packBufferSize;
}

int
zaxisUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id)
{
  int intBuffer[zaxisNint], memberMask;
  uint32_t d;

  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, intBuffer, zaxisNint, CDI_DATATYPE_INT, context);
  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);

  xassert(cdiCheckSum(CDI_DATATYPE_INT, zaxisNint, intBuffer) == d);

  zaxis_t *zaxisP = zaxisNewEntry(force_id ? namespaceAdaptKey(intBuffer[ZAXIS_PACK_INT_SELF], originNamespace) : CDI_UNDEFID);

  zaxisP->type = intBuffer[ZAXIS_PACK_INT_TYPE];
  zaxisP->size = intBuffer[ZAXIS_PACK_INT_SIZE];
  zaxisP->direction = intBuffer[ZAXIS_PACK_INT_DIRECTION];
  zaxisP->vctsize = intBuffer[ZAXIS_PACK_INT_VCTSIZE];
  memberMask = intBuffer[ZAXIS_PACK_INT_MEMBERMASK];

  if (memberMask & vals)
  {
    int size = zaxisP->size;
    xassert(size >= 0);

    zaxisP->vals = (double *) Malloc((size_t) size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, zaxisP->vals, size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, size, zaxisP->vals) == d);
  }

  if (memberMask & lbounds)
  {
    int size = zaxisP->size;
    xassert(size >= 0);

    zaxisP->lbounds = (double *) Malloc((size_t) size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, zaxisP->lbounds, size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, size, zaxisP->lbounds) == d);
  }

  if (memberMask & ubounds)
  {
    int size = zaxisP->size;
    xassert(size >= 0);

    zaxisP->ubounds = (double *) Malloc((size_t) size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, zaxisP->ubounds, size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, size, zaxisP->ubounds) == d);
  }

  if (memberMask & weights)
  {
    int size = zaxisP->size;
    xassert(size >= 0);

    zaxisP->weights = (double *) Malloc((size_t) size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, zaxisP->weights, size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT, size, zaxisP->weights) == d);
  }

  if (memberMask & vct)
  {
    int size = zaxisP->vctsize;
    xassert(size >= 0);

    zaxisP->vct = (double *) Malloc((size_t) size * sizeof(double));
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, zaxisP->vct, size, CDI_DATATYPE_FLT64, context);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
    xassert(cdiCheckSum(CDI_DATATYPE_FLT64, size, zaxisP->vct) == d);
  }

  serializeKeysUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &zaxisP->keys, context);

  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &zaxisP->positive, 1, CDI_DATATYPE_UINT, context);

  const resOps *zaxisOps = getZaxisOps();
  reshSetStatus(zaxisP->self, zaxisOps, reshGetStatus(zaxisP->self, zaxisOps) & ~RESH_SYNC_BIT);
  return zaxisP->self;
}

void
zaxisPack(void *voidP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  zaxis_t *zaxisP = (zaxis_t *) voidP;
  int intBuffer[zaxisNint];
  int memberMask;
  uint32_t d;

  intBuffer[ZAXIS_PACK_INT_SELF] = zaxisP->self;
  intBuffer[ZAXIS_PACK_INT_TYPE] = zaxisP->type;
  intBuffer[ZAXIS_PACK_INT_SIZE] = zaxisP->size;
  intBuffer[ZAXIS_PACK_INT_DIRECTION] = zaxisP->direction;
  intBuffer[ZAXIS_PACK_INT_VCTSIZE] = zaxisP->vctsize;
  intBuffer[ZAXIS_PACK_INT_MEMBERMASK] = memberMask = zaxisGetMemberMask(zaxisP);

  serializePack(intBuffer, zaxisNint, CDI_DATATYPE_INT, packBuffer, packBufferSize, packBufferPos, context);
  d = cdiCheckSum(CDI_DATATYPE_INT, zaxisNint, intBuffer);
  serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);

  if (memberMask & vals)
  {
    xassert(zaxisP->size);
    serializePack(zaxisP->vals, zaxisP->size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, zaxisP->size, zaxisP->vals);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & lbounds)
  {
    xassert(zaxisP->size);
    serializePack(zaxisP->lbounds, zaxisP->size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, zaxisP->size, zaxisP->lbounds);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & ubounds)
  {
    xassert(zaxisP->size);

    serializePack(zaxisP->ubounds, zaxisP->size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, zaxisP->size, zaxisP->ubounds);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & weights)
  {
    xassert(zaxisP->size);

    serializePack(zaxisP->weights, zaxisP->size, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT, zaxisP->size, zaxisP->weights);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  if (memberMask & vct)
  {
    xassert(zaxisP->vctsize);

    serializePack(zaxisP->vct, zaxisP->vctsize, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_FLT64, zaxisP->vctsize, zaxisP->vct);
    serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  }

  serializeKeysPack(&zaxisP->keys, packBuffer, packBufferSize, packBufferPos, context);

  serializePack(&zaxisP->positive, 1, CDI_DATATYPE_UINT, packBuffer, packBufferSize, packBufferPos, context);
}
