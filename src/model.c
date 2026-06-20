/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include <limits.h>
#include <stdlib.h>

#include "cdi.h"
#include "cdi_int.h"
#include "model.h"
#include "namespace.h"
#include "resource_handle.h"
#include "resource_unpack.h"
#include "serialize.h"

typedef struct
{
  int self;
  int instID;
  int modelgribID;
  char *name;
} model_t;

static int modelCompareP(void *modelP1, void *modelP2);
static void modelDestroyP(void *modelP);
static void modelPrintP(void *modelP, FILE *fp);
static int modelGetSizeP(void *modelP, void *context);
static void modelPackP(void *modelP, void *buff, int size, int *position, void *context);
static int modelTxCode(void *modelP);

static const resOps modelOps = { modelCompareP, modelDestroyP, modelPrintP, modelGetSizeP, modelPackP, modelTxCode };

static void
modelDefaultValue(model_t *modelP)
{
  modelP->self = CDI_UNDEFID;
  modelP->instID = CDI_UNDEFID;
  modelP->modelgribID = CDI_UNDEFID;
  modelP->name = NULL;
}

static model_t *
modelNewEntry(cdiResH resH, int instID, int modelgribID, const char *name)
{
  model_t *modelP = (model_t *) malloc(sizeof(model_t));
  modelDefaultValue(modelP);
  if (resH == CDI_UNDEFID)
    modelP->self = reshPut(modelP, &modelOps);
  else
  {
    modelP->self = resH;
    reshReplace(resH, modelP, &modelOps);
  }
  modelP->instID = instID;
  modelP->modelgribID = modelgribID;
  if (name && *name) modelP->name = strdup(name);

  return (modelP);
}

void
modelDefaultEntries(void)
{
  enum
  {
    nDefModels = 10
  };
  cdiResH resH[nDefModels];

  int instID = institutInq(0, 0, "ECMWF", NULL);
  /* (void)    modelDef(instID, 131, "ERA15"); */
  /* (void)    modelDef(instID, 199, "ERA40"); */

  instID = institutInq(98, 232, "MPIMET", NULL);
  resH[0] = modelDef(instID, 64, "ECHAM5.4");
  resH[1] = modelDef(instID, 63, "ECHAM5.3");
  resH[2] = modelDef(instID, 62, "ECHAM5.2");
  resH[3] = modelDef(instID, 61, "ECHAM5.1");

  instID = institutInq(98, 255, "MPIMET", NULL);
  resH[4] = modelDef(instID, 60, "ECHAM5.0");
  resH[5] = modelDef(instID, 50, "ECHAM4");
  resH[6] = modelDef(instID, 110, "MPIOM1");

  instID = institutInq(0, 0, "DWD", NULL);
  resH[7] = modelDef(instID, 149, "GME");

  instID = institutInq(0, 0, "MCH", NULL);
  resH[8] = modelDef(instID, 255, "COSMO");

  instID = institutInq(0, 1, "NCEP", NULL);
  resH[9] = modelDef(instID, 80, "T62L28MRF");

  /* pre-defined models are not synchronized */
  for (int i = 0; i < nDefModels; i++) reshSetStatus(resH[i], &modelOps, RESH_IN_USE);
}

struct modelLoc
{
  const char *name;
  int instID, modelgribID, resID;
};

static enum cdiApplyRet
findModelByID(int resID, void *res, void *data)
{
  model_t *modelP = (model_t *) res;
  struct modelLoc *ret = (struct modelLoc *) data;
  int instID = ret->instID, modelgribID = ret->modelgribID;
  if (modelP->instID == instID && modelP->modelgribID == modelgribID)
  {
    ret->resID = resID;
    return CDI_APPLY_STOP;
  }
  else
    return CDI_APPLY_GO_ON;
}

static enum cdiApplyRet
findModelByName(int resID, void *res, void *data)
{
  model_t *modelP = (model_t *) res;
  struct modelLoc *ret = (struct modelLoc *) data;
  int instID = ret->instID, modelgribID = ret->modelgribID;
  const char *name = ret->name;
  if ((instID == -1 || modelP->instID == instID) && (modelgribID == 0 || modelP->modelgribID == modelgribID) && modelP->name)
  {
    const char *p = name, *q = modelP->name;
    while (*p != '\0' && *p == *q) ++p, ++q;
    if (*p == '\0' || *q == '\0')
    {
      ret->resID = resID;
      return CDI_APPLY_STOP;
    }
  }
  return CDI_APPLY_GO_ON;
}

int
modelInq(int instID, int modelgribID, const char *name)
{
  struct modelLoc searchState = { .name = name, .instID = instID, .modelgribID = modelgribID, .resID = CDI_UNDEFID };
  if (name && *name)
    cdiResHFilterApply(&modelOps, findModelByName, &searchState);
  else
    cdiResHFilterApply(&modelOps, findModelByID, &searchState);
  return searchState.resID;
}

int
modelDef(int instID, int modelgribID, const char *name)
{
  model_t *modelP = modelNewEntry(CDI_UNDEFID, instID, modelgribID, name);
  return modelP->self;
}

int
modelInqInstitut(int modelID)
{
  model_t *modelP = NULL;
  if (modelID != CDI_UNDEFID) { modelP = (model_t *) reshGetVal(modelID, &modelOps); }
  return modelP ? modelP->instID : CDI_UNDEFID;
}

int
modelInqGribID(int modelID)
{
  model_t *modelP = NULL;
  if (modelID != CDI_UNDEFID) { modelP = (model_t *) reshGetVal(modelID, &modelOps); }
  return modelP ? modelP->modelgribID : CDI_UNDEFID;
}

const char *
modelInqNamePtr(int modelID)
{
  model_t *modelP = NULL;
  if (modelID != CDI_UNDEFID) { modelP = (model_t *) reshGetVal(modelID, &modelOps); }
  return modelP ? modelP->name : NULL;
}

static int
modelCompareP(void *modelP1, void *modelP2)
{
  model_t *model1 = (model_t *) modelP1, *model2 = (model_t *) modelP2;
  if (model1->modelgribID != model2->modelgribID) return 1;
  if (namespaceResHDecode(model1->instID).idx != namespaceResHDecode(model2->instID).idx) return 1;
  return !str_is_equal(model1->name, model2->name);
}

void
modelDestroyP(void *modelP)
{
  model_t *mp = (model_t *) modelP;
  free(mp->name);
  free(mp);
}

void
modelPrintP(void *modelP, FILE *fp)
{
  model_t *mp = (model_t *) modelP;
  fprintf(fp,
          "#\n"
          "# modelID %d\n"
          "#\n"
          "self          = %d\n"
          "instID        = %d\n"
          "modelgribID   = %d\n"
          "name          = %s\n",
          mp->self, mp->self, mp->instID, mp->modelgribID, mp->name ? mp->name : "NN");
}

static int
modelTxCode(void *modelP)
{
  (void) modelP;
  return MODEL;
}

enum
{
  MODEL_PACK_INT_SELF,
  MODEL_PACK_INT_INSTID,
  MODEL_PACK_INT_MODELGRIBID,
  MODEL_PACK_INT_NAMELEN,
  modelNints,
};

static int
modelGetSizeP(void *modelP, void *context)
{
  model_t *p = (model_t *) modelP;
  size_t txsize = (size_t) serializeGetSize(modelNints, CDI_DATATYPE_INT, context)
                  + (size_t) serializeGetSize(p->name ? (int) strlen(p->name) : 0, CDI_DATATYPE_TXT, context);
  xassert(txsize <= INT_MAX);
  return (int) txsize;
}

static void
modelPackP(void *modelP, void *buf, int size, int *position, void *context)
{
  model_t *p = (model_t *) modelP;
  int tempbuf[modelNints];
  tempbuf[MODEL_PACK_INT_SELF] = p->self;
  tempbuf[MODEL_PACK_INT_INSTID] = p->instID;
  tempbuf[MODEL_PACK_INT_MODELGRIBID] = p->modelgribID;
  tempbuf[MODEL_PACK_INT_NAMELEN] = p->name ? (int) strlen(p->name) : 0;
  serializePack(tempbuf, modelNints, CDI_DATATYPE_INT, buf, size, position, context);
  if (p->name) serializePack(p->name, tempbuf[MODEL_PACK_INT_NAMELEN], CDI_DATATYPE_TXT, buf, size, position, context);
}

int
modelUnpack(void *buf, int size, int *position, int originNamespace, void *context, int force_id)
{
#define adaptKey(key) (namespaceAdaptKey((key), originNamespace))
  int tempbuf[modelNints];
  char *name;
  serializeUnpack(buf, size, position, tempbuf, modelNints, CDI_DATATYPE_INT, context);
  if (tempbuf[MODEL_PACK_INT_NAMELEN] != 0)
  {
    size_t len = (size_t) tempbuf[MODEL_PACK_INT_NAMELEN];
    name = (char *) malloc(len + 1);
    serializeUnpack(buf, size, position, name, tempbuf[MODEL_PACK_INT_NAMELEN], CDI_DATATYPE_TXT, context);
    name[len] = '\0';
  }
  else
  {
    name = (char *) "";
  }
  int targetID = adaptKey(tempbuf[MODEL_PACK_INT_SELF]);
  model_t *mp = modelNewEntry(force_id ? targetID : CDI_UNDEFID, adaptKey(tempbuf[MODEL_PACK_INT_INSTID]),
                              tempbuf[MODEL_PACK_INT_MODELGRIBID], name);
  if (tempbuf[MODEL_PACK_INT_NAMELEN] != 0) free(name);
  xassert(!force_id || (mp->self == adaptKey(tempbuf[0])));
  reshSetStatus(mp->self, &modelOps, reshGetStatus(mp->self, &modelOps) & ~RESH_SYNC_BIT);
#undef adaptKey
  return mp->self;
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
