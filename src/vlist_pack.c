#include "cdi.h"
#include "namespace.h"
#include "vlist_var.h"
#include "resource_unpack.h"
#include "serialize.h"

enum
{
  VLIST_PACK_INT_SELF,
  VLIST_PACK_INT_NVARS,
  VLIST_PACK_INT_TAXISID,
  VLIST_PACK_INT_TABLEID,
  VLIST_PACK_INT_INSTID,
  VLIST_PACK_INT_MODELID,
  vlistNints,
};

int
vlistTxCode(void *vlistP)
{
  (void) vlistP;
  return VLIST;
}

int
vlistGetSizeP(void *vlistP, void *context)
{
  vlist_t *p = (vlist_t *) vlistP;
  int txsize = serializeGetSize(vlistNints, CDI_DATATYPE_INT, context);
  txsize += serializeGetSize(1, CDI_DATATYPE_LONG, context);
  txsize += cdiAttsGetSize(p, CDI_GLOBAL, context);
  for (int varID = 0; varID < p->nvars; varID++) txsize += vlistVarGetPackSize(p, varID, context);
  return txsize;
}

void
vlistPackP(void *vlistP, void *buf, int size, int *position, void *context)
{
  int tempbuf[vlistNints];
  vlist_t *p = (vlist_t *) vlistP;
  tempbuf[VLIST_PACK_INT_SELF] = p->self;
  tempbuf[VLIST_PACK_INT_NVARS] = p->nvars;
  tempbuf[VLIST_PACK_INT_TAXISID] = p->taxisID;
  tempbuf[VLIST_PACK_INT_TABLEID] = p->tableID;
  tempbuf[VLIST_PACK_INT_INSTID] = p->instID;
  tempbuf[VLIST_PACK_INT_MODELID] = p->modelID;
  serializePack(tempbuf, vlistNints, CDI_DATATYPE_INT, buf, size, position, context);
  serializePack(&p->ntsteps, 1, CDI_DATATYPE_LONG, buf, size, position, context);

  cdiAttsPack(p, CDI_GLOBAL, buf, size, position, context);
  for (int varID = 0; varID < p->nvars; varID++) { vlistVarPack(p, varID, (char *) buf, size, position, context); }
}

int
vlistUnpack(char *buf, int size, int *position, int originNamespace, void *context, int force_id)
{
#define adaptKey(key) (namespaceAdaptKey((key), originNamespace))
  int tempbuf[vlistNints];
  serializeUnpack(buf, size, position, tempbuf, vlistNints, CDI_DATATYPE_INT, context);
  int nvars = tempbuf[VLIST_PACK_INT_NVARS];
  int targetID = force_id ? adaptKey(tempbuf[VLIST_PACK_INT_SELF]) : CDI_UNDEFID;
  vlist_t *p = vlist_new_entry(targetID);
  xassert(!force_id || p->self == targetID);
  if (!force_id) targetID = p->self;
  cdiVlistMakeInternal(p->self);
  p->taxisID = adaptKey(tempbuf[VLIST_PACK_INT_TAXISID]);
  p->tableID = tempbuf[VLIST_PACK_INT_TABLEID];
  p->instID = adaptKey(tempbuf[VLIST_PACK_INT_INSTID]);
  p->modelID = adaptKey(tempbuf[VLIST_PACK_INT_MODELID]);
  serializeUnpack(buf, size, position, &p->ntsteps, 1, CDI_DATATYPE_LONG, context);
  cdiAttsUnpack(targetID, CDI_GLOBAL, buf, size, position, context);
  for (int varID = 0; varID < nvars; varID++) vlistVarUnpack(targetID, buf, size, position, originNamespace, context);
  reshSetStatus(targetID, &vlistOps, reshGetStatus(targetID, &vlistOps) & ~RESH_SYNC_BIT);
#undef adaptKey
  return targetID;
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
