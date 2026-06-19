#include "taxis.h"
#include "taxis_pack.h"
#include "namespace.h"
#include "resource_unpack.h"
#include "serialize.h"

int
taxisTxCode(void *taxisP)
{
  (void) taxisP;
  return TAXIS;
}

enum
{
  TAXIS_PACK_INT_SELF,
  TAXIS_PACK_INT_TYPE,
  TAXIS_PACK_INT_VDATE,
  TAXIS_PACK_INT_VTIME,
  TAXIS_PACK_INT_RDATE,
  TAXIS_PACK_INT_RTIME,
  TAXIS_PACK_INT_FDATE,
  TAXIS_PACK_INT_FTIME,
  TAXIS_PACK_INT_CALENDAR,
  TAXIS_PACK_INT_UNIT,
  TAXIS_PACK_INT_FC_UNIT,
  TAXIS_PACK_INT_NUMAVG,
  TAXIS_PACK_INT_CLIMATOLOGY,
  TAXIS_PACK_INT_HAS_BOUNDS,
  TAXIS_PACK_INT_VDATE_LB,
  TAXIS_PACK_INT_VDATE_UB,
  TAXIS_PACK_INT_VTIME_LB,
  TAXIS_PACK_INT_VTIME_UB,
  TAXIS_PACK_INT_NAMELEN,
  TAXIS_PACK_INT_LNAMELEN,
  TAXIS_PACK_INT_UNITSLEN,
  taxisNint
};

enum
{
  TAXIS_PACK_FC_PERIOD,
  taxisNdouble
};

int
taxisGetPackSize(void *p, void *context)
{
  taxis_t *taxisP = (taxis_t *) p;
  int packBufferSize = serializeGetSize(taxisNint, CDI_DATATYPE_INT, context)
                       + serializeGetSize(taxisNdouble, CDI_DATATYPE_FLT64, context)
                       + (taxisP->name ? serializeGetSize((int) strlen(taxisP->name), CDI_DATATYPE_TXT, context) : 0)
                       + (taxisP->longname ? serializeGetSize((int) strlen(taxisP->longname), CDI_DATATYPE_TXT, context) : 0)
                       + (taxisP->units ? serializeGetSize((int) strlen(taxisP->units), CDI_DATATYPE_TXT, context) : 0)
                       + serializeGetSize(1, CDI_DATATYPE_UINT32, context);
  packBufferSize += serializeKeysGetPackSize(&taxisP->keys, context);
  return packBufferSize;
}

int
taxisUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id)
{
#define adaptKey(key) (namespaceAdaptKey((key), originNamespace))
  taxis_t *taxisP;
  int intBuffer[taxisNint];
  double dblBuffer[taxisNdouble];
  uint32_t d;

  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, intBuffer, taxisNint, CDI_DATATYPE_INT, context);
  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, dblBuffer, taxisNdouble, CDI_DATATYPE_FLT64, context);
  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);

  xassert(cdiCheckSum(CDI_DATATYPE_INT, taxisNint, intBuffer) == d);

  cdiResH targetID = force_id ? adaptKey(intBuffer[TAXIS_PACK_INT_SELF]) : CDI_UNDEFID;
  taxisP = taxisNewEntry(targetID);

  xassert(!force_id || targetID == taxisP->self);

  taxisP->type = intBuffer[TAXIS_PACK_INT_TYPE];
  taxisP->vDateTime.date = cdiDate_set(intBuffer[TAXIS_PACK_INT_VDATE]);
  taxisP->vDateTime.time = cdiTime_set(intBuffer[TAXIS_PACK_INT_VTIME]);
  taxisP->rDateTime.date = cdiDate_set(intBuffer[TAXIS_PACK_INT_RDATE]);
  taxisP->rDateTime.time = cdiTime_set(intBuffer[TAXIS_PACK_INT_RTIME]);
  taxisP->fDateTime.date = cdiDate_set(intBuffer[TAXIS_PACK_INT_FDATE]);
  taxisP->fDateTime.time = cdiTime_set(intBuffer[TAXIS_PACK_INT_FTIME]);
  taxisP->calendar = intBuffer[TAXIS_PACK_INT_CALENDAR];
  taxisP->unit = intBuffer[TAXIS_PACK_INT_UNIT];
  taxisP->fc_unit = intBuffer[TAXIS_PACK_INT_FC_UNIT];
  taxisP->numavg = intBuffer[TAXIS_PACK_INT_NUMAVG];
  taxisP->climatology = intBuffer[TAXIS_PACK_INT_CLIMATOLOGY];
  taxisP->hasBounds = (bool) intBuffer[TAXIS_PACK_INT_HAS_BOUNDS];
  taxisP->vDateTime_lb.date = cdiDate_set(intBuffer[TAXIS_PACK_INT_VDATE_LB]);
  taxisP->vDateTime_lb.time = cdiTime_set(intBuffer[TAXIS_PACK_INT_VDATE_UB]);
  taxisP->vDateTime_ub.date = cdiDate_set(intBuffer[TAXIS_PACK_INT_VTIME_LB]);
  taxisP->vDateTime_ub.time = cdiTime_set(intBuffer[TAXIS_PACK_INT_VTIME_UB]);
  taxisP->fc_period = dblBuffer[TAXIS_PACK_FC_PERIOD];
  if (intBuffer[TAXIS_PACK_INT_NAMELEN])
  {
    int len = intBuffer[TAXIS_PACK_INT_NAMELEN];
    char *name = new_refcount_string((size_t) len);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, name, len, CDI_DATATYPE_TXT, context);
    name[len] = '\0';
    taxisP->name = name;
  }
  if (intBuffer[TAXIS_PACK_INT_LNAMELEN])
  {
    int len = intBuffer[TAXIS_PACK_INT_LNAMELEN];
    char *longname = new_refcount_string((size_t) len);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, longname, len, CDI_DATATYPE_TXT, context);
    longname[len] = '\0';
    taxisP->longname = longname;
  }
  if (intBuffer[TAXIS_PACK_INT_UNITSLEN])
  {
    int len = intBuffer[TAXIS_PACK_INT_UNITSLEN];
    char *units = new_refcount_string((size_t) len);
    serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, units, len, CDI_DATATYPE_TXT, context);
    units[len] = '\0';
    taxisP->units = units;
  }

  serializeKeysUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &taxisP->keys, context);

  reshSetStatus(taxisP->self, &taxisOps, reshGetStatus(taxisP->self, &taxisOps) & ~RESH_SYNC_BIT);
#undef adaptKey

  return taxisP->self;
}

void
taxisPack(void *voidP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  taxis_t *taxisP = (taxis_t *) voidP;
  int nameLen, lnameLen, unitsLen;
  uint32_t d;

  {
    int intBuffer[taxisNint];
    intBuffer[TAXIS_PACK_INT_SELF] = taxisP->self;
    intBuffer[TAXIS_PACK_INT_TYPE] = taxisP->type;
    intBuffer[TAXIS_PACK_INT_VDATE] = (int) cdiDate_get(taxisP->vDateTime.date);
    intBuffer[TAXIS_PACK_INT_VTIME] = cdiTime_get(taxisP->vDateTime.time);
    intBuffer[TAXIS_PACK_INT_RDATE] = (int) cdiDate_get(taxisP->rDateTime.date);
    intBuffer[TAXIS_PACK_INT_RTIME] = cdiTime_get(taxisP->rDateTime.time);
    intBuffer[TAXIS_PACK_INT_FDATE] = (int) cdiDate_get(taxisP->fDateTime.date);
    intBuffer[TAXIS_PACK_INT_FTIME] = cdiTime_get(taxisP->fDateTime.time);
    intBuffer[TAXIS_PACK_INT_CALENDAR] = taxisP->calendar;
    intBuffer[TAXIS_PACK_INT_UNIT] = taxisP->unit;
    intBuffer[TAXIS_PACK_INT_FC_UNIT] = taxisP->fc_unit;
    intBuffer[TAXIS_PACK_INT_NUMAVG] = taxisP->numavg;
    intBuffer[TAXIS_PACK_INT_CLIMATOLOGY] = taxisP->climatology;
    intBuffer[TAXIS_PACK_INT_HAS_BOUNDS] = taxisP->hasBounds;
    intBuffer[TAXIS_PACK_INT_VDATE_LB] = (int) cdiDate_get(taxisP->vDateTime_lb.date);
    intBuffer[TAXIS_PACK_INT_VDATE_UB] = cdiTime_get(taxisP->vDateTime_lb.time);
    intBuffer[TAXIS_PACK_INT_VTIME_LB] = (int) cdiDate_get(taxisP->vDateTime_ub.date);
    intBuffer[TAXIS_PACK_INT_VTIME_UB] = cdiTime_get(taxisP->vDateTime_ub.time);
    intBuffer[TAXIS_PACK_INT_NAMELEN] = nameLen = taxisP->name ? (int) strlen(taxisP->name) : 0;
    intBuffer[TAXIS_PACK_INT_LNAMELEN] = lnameLen = taxisP->longname ? (int) strlen(taxisP->longname) : 0;
    intBuffer[TAXIS_PACK_INT_UNITSLEN] = unitsLen = taxisP->units ? (int) strlen(taxisP->units) : 0;
    serializePack(intBuffer, taxisNint, CDI_DATATYPE_INT, packBuffer, packBufferSize, packBufferPos, context);
    d = cdiCheckSum(CDI_DATATYPE_INT, taxisNint, intBuffer);
  }

  {
    double dblBuffer[taxisNdouble];
    dblBuffer[TAXIS_PACK_FC_PERIOD] = taxisP->fc_period;
    serializePack(dblBuffer, taxisNdouble, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
  }
  serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  if (taxisP->name) serializePack(taxisP->name, nameLen, CDI_DATATYPE_TXT, packBuffer, packBufferSize, packBufferPos, context);
  if (taxisP->longname)
    serializePack(taxisP->longname, lnameLen, CDI_DATATYPE_TXT, packBuffer, packBufferSize, packBufferPos, context);
  if (taxisP->units) serializePack(taxisP->units, unitsLen, CDI_DATATYPE_TXT, packBuffer, packBufferSize, packBufferPos, context);

  serializeKeysPack(&taxisP->keys, packBuffer, packBufferSize, packBufferPos, context);
}
