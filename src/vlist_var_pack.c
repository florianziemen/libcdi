#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "vlist.h"
#include "vlist_var.h"
#include "namespace.h"
#include "serialize.h"

enum {
  VLISTVAR_PACK_INT_IDX_FLAG,
  VLISTVAR_PACK_INT_IDX_GRIDID,
  VLISTVAR_PACK_INT_IDX_ZAXISID,
  VLISTVAR_PACK_INT_IDX_TIMETYPE,
  VLISTVAR_PACK_INT_IDX_DATATYPE,
  VLISTVAR_PACK_INT_IDX_PARAM,
  VLISTVAR_PACK_INT_IDX_INSTID,
  VLISTVAR_PACK_INT_IDX_MODELID,
  VLISTVAR_PACK_INT_IDX_TABLEID,
  VLISTVAR_PACK_INT_IDX_TIMAVE,
  VLISTVAR_PACK_INT_IDX_MISSVALUSED,
  VLISTVAR_PACK_INT_IDX_COMPTYPE,
  VLISTVAR_PACK_INT_IDX_COMPLEVEL,
  VLISTVAR_PACK_INT_IDX_NLEVS,
  VLISTVAR_PACK_INT_IDX_IORANK,
  VLISTVAR_PACK_INT_IDX_EXTRALEN,
  vlistvarNint
};

enum {
  vlistvar_ndbls = 3,
};

int vlistVarGetPackSize(vlist_t *p, int varID, void *context)
{
  var_t *var = p->vars + varID;
  int varsize = serializeGetSize(vlistvarNint, CDI_DATATYPE_INT, context)
    + serializeGetSize(vlistvar_ndbls, CDI_DATATYPE_FLT64, context);
  if (var->extra)
    varsize += serializeGetSize((int)strlen(var->extra), CDI_DATATYPE_TXT, context);
  varsize += serializeGetSize(4 * zaxisInqSize(var->zaxisID), CDI_DATATYPE_INT, context);

  varsize += serializeKeysGetPackSize(&var->keys, context);

  varsize += cdiAttsGetSize(p, varID, context);

  return varsize;
}

void vlistVarPack(vlist_t *p, int varID, char * buf, int size, int *position, void *context)
{
  double dtempbuf[vlistvar_ndbls];
  var_t *var = p->vars + varID;
  int tempbuf[vlistvarNint], extralen;

  tempbuf[VLISTVAR_PACK_INT_IDX_FLAG] = var->flag;
  tempbuf[VLISTVAR_PACK_INT_IDX_GRIDID] = var->gridID;
  tempbuf[VLISTVAR_PACK_INT_IDX_ZAXISID] = var->zaxisID;
  tempbuf[VLISTVAR_PACK_INT_IDX_TIMETYPE] = var->timetype;
  tempbuf[VLISTVAR_PACK_INT_IDX_DATATYPE] = var->datatype;
  tempbuf[VLISTVAR_PACK_INT_IDX_PARAM] = var->param;
  tempbuf[VLISTVAR_PACK_INT_IDX_INSTID] = var->instID;
  tempbuf[VLISTVAR_PACK_INT_IDX_MODELID] = var->modelID;
  tempbuf[VLISTVAR_PACK_INT_IDX_TABLEID] = var->tableID;
  tempbuf[VLISTVAR_PACK_INT_IDX_TIMAVE] = var->timave;
  tempbuf[VLISTVAR_PACK_INT_IDX_MISSVALUSED] = (int)var->missvalused;
  tempbuf[VLISTVAR_PACK_INT_IDX_COMPTYPE] = var->comptype;
  tempbuf[VLISTVAR_PACK_INT_IDX_COMPLEVEL] = var->complevel;
  int nlevs = var->levinfo ? zaxisInqSize(var->zaxisID) : 0;
  tempbuf[VLISTVAR_PACK_INT_IDX_NLEVS] = nlevs;
  tempbuf[VLISTVAR_PACK_INT_IDX_IORANK] = var->iorank;
  tempbuf[VLISTVAR_PACK_INT_IDX_EXTRALEN] = extralen = var->extra?(int)strlen(var->extra):0;
  dtempbuf[0] = var->missval;
  dtempbuf[1] = var->scalefactor;
  dtempbuf[2] = var->addoffset;
  serializePack(tempbuf, vlistvarNint, CDI_DATATYPE_INT, buf, size, position, context);
  serializePack(dtempbuf, vlistvar_ndbls, CDI_DATATYPE_FLT64,buf, size, position, context);
  if (extralen)
    serializePack(var->extra, extralen, CDI_DATATYPE_TXT, buf, size, position, context);
  if (nlevs)
    {
      int *levbuf = (int*) malloc(nlevs*sizeof(int));
      for (int levID = 0; levID < nlevs; ++levID) levbuf[levID] = var->levinfo[levID].flag;
      serializePack(levbuf, nlevs, CDI_DATATYPE_INT, buf, size, position, context);
      for (int levID = 0; levID < nlevs; ++levID) levbuf[levID] = var->levinfo[levID].index;
      serializePack(levbuf, nlevs, CDI_DATATYPE_INT, buf, size, position, context);
      for (int levID = 0; levID < nlevs; ++levID) levbuf[levID] = var->levinfo[levID].mlevelID;
      serializePack(levbuf, nlevs, CDI_DATATYPE_INT, buf, size, position, context);
      for (int levID = 0; levID < nlevs; ++levID) levbuf[levID] = var->levinfo[levID].flevelID;
      free(levbuf);
    }

  serializeKeysPack(&var->keys, buf, size, position, context);

  cdiAttsPack(p, varID, buf, size, position, context);
}

static inline int
imax(int a, int b)
{
  return (a >= b) ? a : b;
}

void vlistVarUnpack(int vlistID, char * buf, int size, int *position,
		    int originNamespace, void *context)
{
  double dtempbuf[vlistvar_ndbls];
  int tempbuf[vlistvarNint];
  char *varname = NULL;
  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  serializeUnpack(buf, size, position, tempbuf, vlistvarNint, CDI_DATATYPE_INT, context);
  serializeUnpack(buf, size, position, dtempbuf, vlistvar_ndbls, CDI_DATATYPE_FLT64, context);

  /* ------------------------------------------- */
  /* NOTE: Tile sets  currently not supported!!! */
  /* ------------------------------------------- */

  int newvar = vlistDefVar ( vlistID,
			 namespaceAdaptKey ( tempbuf[VLISTVAR_PACK_INT_IDX_GRIDID], originNamespace ),
			 namespaceAdaptKey ( tempbuf[VLISTVAR_PACK_INT_IDX_ZAXISID], originNamespace ),
			 tempbuf[VLISTVAR_PACK_INT_IDX_TIMETYPE]);
  if (tempbuf[VLISTVAR_PACK_INT_IDX_EXTRALEN])
    varname = (char *)Malloc((size_t)tempbuf[VLISTVAR_PACK_INT_IDX_EXTRALEN] + 1);
  if (tempbuf[VLISTVAR_PACK_INT_IDX_EXTRALEN])
    {
      serializeUnpack(buf, size, position,
                      varname, tempbuf[VLISTVAR_PACK_INT_IDX_EXTRALEN], CDI_DATATYPE_TXT, context);
      varname[tempbuf[VLISTVAR_PACK_INT_IDX_EXTRALEN]] = '\0';
      vlistDefVarExtra(vlistID, newvar, varname);
    }
  Free(varname);
  vlistDefVarDatatype(vlistID, newvar, tempbuf[VLISTVAR_PACK_INT_IDX_DATATYPE]);
  vlistDefVarInstitut ( vlistID, newvar,
			namespaceAdaptKey ( tempbuf[VLISTVAR_PACK_INT_IDX_INSTID], originNamespace ));
  vlistDefVarModel ( vlistID, newvar,
		     namespaceAdaptKey ( tempbuf[VLISTVAR_PACK_INT_IDX_MODELID], originNamespace ));
  vlistDefVarTable(vlistID, newvar, tempbuf[VLISTVAR_PACK_INT_IDX_TABLEID]);
  // FIXME: changing the table might change the param code
  vlistDefVarParam(vlistID, newvar, tempbuf[VLISTVAR_PACK_INT_IDX_PARAM]);
  vlistDefVarTimave(vlistID, newvar, tempbuf[VLISTVAR_PACK_INT_IDX_TIMAVE]);
  if (tempbuf[VLISTVAR_PACK_INT_IDX_MISSVALUSED])
    vlistDefVarMissval(vlistID, newvar, dtempbuf[0]);
  vlistDefVarScalefactor(vlistID, newvar, dtempbuf[1]);
  vlistDefVarAddoffset(vlistID, newvar, dtempbuf[2]);
  vlistDefVarCompType(vlistID, newvar, tempbuf[VLISTVAR_PACK_INT_IDX_COMPTYPE]);
  vlistDefVarCompLevel(vlistID, newvar, tempbuf[VLISTVAR_PACK_INT_IDX_COMPLEVEL]);
  const int nlevs = tempbuf[VLISTVAR_PACK_INT_IDX_NLEVS];
  var_t *var = vlistptr->vars + newvar;
  if (nlevs)
    {
      int i, flagSetLev = 0;
      cdiVlistCreateVarLevInfo(vlistptr, newvar);

      int *levbuf = (int*) malloc(nlevs*sizeof(int));
      serializeUnpack(buf, size, position, levbuf, nlevs, CDI_DATATYPE_INT, context);
      for (i = 0; i < nlevs; ++i) vlistDefFlag(vlistID, newvar, i, levbuf[i]);
      for (i = 0; i < nlevs; ++i) if (levbuf[i] == tempbuf[0]) flagSetLev = i;
      vlistDefFlag(vlistID, newvar, flagSetLev, levbuf[flagSetLev]);
      serializeUnpack(buf, size, position, levbuf, nlevs, CDI_DATATYPE_INT, context);
      for (i = 0; i < nlevs; ++i) vlistDefIndex(vlistID, newvar, i, levbuf[i]);
      serializeUnpack(buf, size, position, levbuf, nlevs, CDI_DATATYPE_INT, context);
      for (i = 0; i < nlevs; ++i) var->levinfo[i].mlevelID = levbuf[i];
      serializeUnpack(buf, size, position, levbuf, nlevs, CDI_DATATYPE_INT, context);
      for (i = 0; i < nlevs; ++i) var->levinfo[i].flevelID = levbuf[i];
      free(levbuf);
    }
  vlistDefVarIOrank(vlistID, newvar, tempbuf[VLISTVAR_PACK_INT_IDX_IORANK]);

  serializeKeysUnpack(buf, size, position, &var->keys, context);

  cdiAttsUnpack(vlistID, newvar, buf, size, position, context);
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
