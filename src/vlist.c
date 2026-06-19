#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>

#include "dmemory.h"
#include "vlist.h"
#include "zaxis.h"
#include "varscan.h"
#include "namespace.h"
#include "vlist_pack.h"
#include "vlist_var.h"

#ifdef HAVE_LIBGRIB_API
/* list of additional GRIB2 keywords which are read by the open process */
int cdiNAdditionalGRIBKeys = 0;
char *cdiAdditionalGRIBKeys[MAX_OPT_GRIB_ENTRIES];
#endif

static int VLIST_Debug = 0;

static void vlist_initialize(void);

#ifdef HAVE_LIBPTHREAD
#include <pthread.h>

static pthread_once_t _vlist_init_thread = PTHREAD_ONCE_INIT;

#define VLIST_INIT() pthread_once(&_vlist_init_thread, vlist_initialize)

#else

static bool vlistIsInitialized = false;

#define VLIST_INIT() \
  if (!vlistIsInitialized) vlist_initialize()
#endif

void *
stream_get_pointer(int streamID)
{
  return stream_to_pointer(streamID);
}

void *
stream_get_vlist_pointer(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);
  return vlist_to_pointer(streamP->vlistID);
}

static int
vlist_compare(vlist_t *a, vlist_t *b)
{
  int diff = (a->nvars != b->nvars) | (a->ngrids != b->ngrids) | (a->nzaxis != b->nzaxis) | (a->instID != b->instID)
             | (a->modelID != b->modelID) | (a->tableID != b->tableID) | (a->ntsteps != b->ntsteps)
             | (a->atts.nelems != b->atts.nelems);

  int nvars = a->nvars;
  for (int varID = 0; varID < nvars; ++varID) diff |= vlistVarCompare(a, varID, b, varID);

  size_t natts = a->atts.nelems;
  for (size_t attID = 0; attID < natts; ++attID) diff |= cdi_att_compare(&a->atts, &b->atts, (int) attID);

  return diff;
}

static void vlist_print(vlist_t *vlistP, FILE *fp);
static void vlist_delete(vlist_t *vlistP);

const resOps vlistOps = {
  (valCompareFunc) vlist_compare, (valDestroyFunc) vlist_delete, (valPrintFunc) vlist_print, vlistGetSizeP, vlistPackP, vlistTxCode
};

vlist_t *
vlist_to_pointer(int vlistID)
{
  VLIST_INIT();
  return (vlist_t *) reshGetVal(vlistID, &vlistOps);
}

static void
vlist_init_entry(vlist_t *vlistP)
{
  memset(vlistP, 0, sizeof(vlist_t));
  vlistP->self = CDI_UNDEFID;
  vlistP->taxisID = CDI_UNDEFID;
  vlistP->instID = CDI_Default_InstID;
  vlistP->modelID = CDI_Default_ModelID;
  vlistP->tableID = CDI_Default_TableID;
  vlistP->ntsteps = CDI_UNDEFID;
  vlistP->keys.nalloc = MAX_KEYS;
  for (int i = 0; i < MAX_KEYS; ++i) vlistP->keys.value[i].length = 0;
  vlistP->atts.nalloc = MAX_ATTRIBUTES;
  for (int i = 0; i < MAX_SUBTYPES_PS; ++i) vlistP->subtypeIDs[i] = CDI_UNDEFID;
}

vlist_t *
vlist_new_entry(cdiResH resH)
{
  vlist_t *vlistP = (vlist_t *) Malloc(sizeof(vlist_t));
  vlist_init_entry(vlistP);
  if (resH == CDI_UNDEFID) { vlistP->self = reshPut(vlistP, &vlistOps); }
  else
  {
    vlistP->self = resH;
    reshReplace(resH, vlistP, &vlistOps);
  }
  return vlistP;
}

static void
vlist_delete_entry(int vlistID)
{
  reshRemove(vlistID, &vlistOps);

  if (VLIST_Debug) Message("Removed idx %d from vlist list", vlistID);
}

static void
vlist_initialize(void)
{
  char *env = getenv("VLIST_DEBUG");
  if (env) VLIST_Debug = atoi(env);
#ifndef HAVE_LIBPTHREAD
  vlistIsInitialized = true;
#endif
}

static void
vlist_copy(vlist_t *vlistP2, vlist_t *vlistP1)
{
  int vlistID2 = vlistP2->self;
  int vlist2internal = vlistP2->internal;
  memcpy(vlistP2, vlistP1, sizeof(vlist_t));
  vlistP2->internal = vlist2internal;  // the question who's responsible to destroy the vlist is tied to its containing memory
                                       // region, so we retain this flag
  vlistP2->immutable = 0;              // this is a copy, so it's mutable, independent of whether the original is mutable or not
  vlistP2->keys.nelems = 0;
  vlistP2->atts.nelems = 0;
  vlistP2->self = vlistID2;
}

void
cdiVlistMakeInternal(int vlistID)
{
  vlist_to_pointer(vlistID)->internal = 1;
}

void
cdiVlistMakeImmutable(int vlistID)
{
  vlist_to_pointer(vlistID)->immutable = 1;
}

/*
@Function  vlistCreate
@Title     Create a variable list

@Prototype int vlistCreate(void)

@Example
Here is an example using @func{vlistCreate} to create a variable list
and add a variable with @func{vlistDefVar}.

@Source
#include "cdi.h"
   ...
int vlistID, varID;
   ...
vlistID = vlistCreate();
varID = vlistDefVar(vlistID, gridID, zaxisID, TIME_VARYING);
   ...
streamDefVlist(streamID, vlistID);
   ...
vlistDestroy(vlistID);
   ...
@EndSource
@EndFunction
*/
int
vlistCreate(void)
{
  cdiInitialize();

  VLIST_INIT();

  vlist_t *vlistP = vlist_new_entry(CDI_UNDEFID);
  if (CDI_Debug) Message("create vlistID = %d", vlistP->self);
  return vlistP->self;
}

static void
vlist_delete(vlist_t *vlistP)
{
  int vlistID = vlistP->self;
  if (CDI_Debug) Message("call to vlist_delete, vlistID = %d", vlistID);

  cdiDeleteKeys(vlistID, CDI_GLOBAL);
  cdiDeleteAtts(vlistID, CDI_GLOBAL);

  int nvars = vlistP->nvars;
  var_t *vars = vlistP->vars;

  for (int varID = 0; varID < nvars; varID++)
  {
    var_t *var = &vars[varID];
    Free(var->levinfo);

    if (var->opt_grib_kvpair)
    {
      for (int i = 0; i < var->opt_grib_nentries; i++) { Free(var->opt_grib_kvpair[i].keyword); }
      Free(var->opt_grib_kvpair);
    }
    var->opt_grib_nentries = 0;
    var->opt_grib_kvpair_size = 0;
    var->opt_grib_kvpair = NULL;

    cdiDeleteKeys(vlistID, varID);
    cdiDeleteAtts(vlistID, varID);
  }

  Free(vars);
  Free(vlistP);
}

// destroy a vlist object, should always be called through namespace lookup
void
cdiVlistDestroy_(int vlistID, bool assertInternal)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  static const char warningTextUserByInternal[]
      = "Destroying a vlist object that is owned by the user (vlistID=%d).\n"
        "This is most likely because of a missing vlistDestroy() in the application code.\n"
        "If that's not the case, and you are absolutely certain about it, please report the bug.",
      warningTextInternalByUser[] = "Attempt to destroy an internal vlist object by the user (vlistID=%d).";
  static const char *const wText[2] = { warningTextUserByInternal, warningTextInternalByUser };
  if (vlistP->internal == assertInternal)
  {
    vlist_delete(vlistP);
    vlist_delete_entry(vlistID);
  }
  else
    Warning(wText[!assertInternal], vlistID);
}

/*
@Function  vlistDestroy
@Title     Destroy a variable list

@Prototype void vlistDestroy(int vlistID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.

@EndFunction
*/
void
vlistDestroy(int vlistID)
{
  void (*mycdiVlistDestroy_)(int, bool) = (void (*)(int, bool)) namespaceSwitchGet(NSSWITCH_VLIST_DESTROY_).func;
  mycdiVlistDestroy_(vlistID, false);
}

static void
var_copy_entries(var_t *var2, var_t *var1)
{
  var2->opt_grib_kvpair_size = 0;
  var2->opt_grib_kvpair = NULL;
  var2->opt_grib_nentries = 0;

  resize_opt_grib_entries(var2, var1->opt_grib_nentries);
  var2->opt_grib_nentries = var1->opt_grib_nentries;
  if ((var2->opt_grib_nentries > 0) && CDI_Debug) Message("copy %d optional GRIB keywords", var2->opt_grib_nentries);

  for (int i = 0; i < var1->opt_grib_nentries; i++)
  {
    if (CDI_Debug) Message("copy entry \"%s\" ...", var1->opt_grib_kvpair[i].keyword);
    var2->opt_grib_kvpair[i].keyword = NULL;
    if (var1->opt_grib_kvpair[i].keyword != NULL)
    {
      var2->opt_grib_kvpair[i] = var1->opt_grib_kvpair[i];
      var2->opt_grib_kvpair[i].keyword = strdup(var1->opt_grib_kvpair[i].keyword);
      var2->opt_grib_kvpair[i].update = true;
      if (CDI_Debug) Message("done.");
    }
    else
    {
      if (CDI_Debug) Message("not done.");
    }
  }
}

/*
@Function  vlistCopy
@Title     Copy a variable list

@Prototype void vlistCopy(int vlistID2, int vlistID1)
@Parameter
    @Item  vlistID1  Source variable list ID.
    @Item  vlistID2  Target variable list ID.

@Description
The function @func{vlistCopy} copies all entries from vlistID1 to vlistID2.

@EndFunction
*/
void
vlistCopy(int vlistID1, int vlistID2)
{
  vlist_t *vlistP1 = vlist_to_pointer(vlistID1);
  vlist_t *vlistP2 = vlist_to_pointer(vlistID2);
  if (CDI_Debug) Message("call to vlistCopy, vlistIDs %d -> %d", vlistID1, vlistID2);

  var_t *vars1 = vlistP1->vars;
  var_t *vars2 = vlistP2->vars;
  vlist_copy(vlistP2, vlistP1);

  vlistP2->keys.nelems = 0;
  vlistP2->atts.nelems = 0;
  cdiCopyKeys(vlistID1, CDI_GLOBAL, vlistID2, CDI_GLOBAL);
  cdiCopyAtts(vlistID1, CDI_GLOBAL, vlistID2, CDI_GLOBAL);

  if (!vars1) return;

  int nvars = vlistP1->nvars;
  // vlistP2->varsAllocated = nvars;

  size_t n = (size_t) vlistP2->varsAllocated;
  vars2 = (var_t *) Realloc(vars2, n * sizeof(var_t));
  memcpy(vars2, vars1, n * sizeof(var_t));
  vlistP2->vars = vars2;

  for (int varID = 0; varID < nvars; varID++)
  {
    var_t *var1 = &vars1[varID];
    var_t *var2 = &vars2[varID];
    var_copy_entries(var2, var1);
    var2->keys.nelems = 0;
    var2->atts.nelems = 0;
    cdiCopyKeys(vlistID1, varID, vlistID2, varID);
    cdiCopyAtts(vlistID1, varID, vlistID2, varID);

    if (var1->levinfo)
    {
      n = (size_t) zaxisInqSize(var1->zaxisID);
      var2->levinfo = (levinfo_t *) Malloc(n * sizeof(levinfo_t));
      memcpy(var2->levinfo, var1->levinfo, n * sizeof(levinfo_t));
    }
  }
}

/*
@Function  vlistDuplicate
@Title     Duplicate a variable list

@Prototype int vlistDuplicate(int vlistID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.

@Description
The function @func{vlistDuplicate} duplicates the variable list from vlistID1.

@Result
@func{vlistDuplicate} returns an identifier to the duplicated variable list.

@EndFunction
*/
int
vlistDuplicate(int vlistID)
{
  if (CDI_Debug) Message("call to vlistDuplicate");

  int vlistIDnew = vlistCreate();
  vlistCopy(vlistID, vlistIDnew);
  return vlistIDnew;
}

void
vlistClearFlag(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  for (int varID = 0; varID < vlistP->nvars; varID++)
  {
    vlistP->vars[varID].flag = false;
    if (vlistP->vars[varID].levinfo)
    {
      int nlevs = zaxisInqSize(vlistP->vars[varID].zaxisID);
      for (int levID = 0; levID < nlevs; levID++) vlistP->vars[varID].levinfo[levID].flag = false;
    }
  }
}

struct vgzSearchState
{
  int resIDValue;
  int zaxistype;
  int nlevels;
  const double *levels;
  const double *lbounds;
  const double *ubounds;
};

static enum cdiApplyRet
vgzZAxisSearch(int id, void *res, void *data)
{
  struct vgzSearchState *state = (struct vgzSearchState *) data;
  (void) res;
  if (zaxis_compare(id, state->zaxistype, state->nlevels, state->levels, state->lbounds, state->ubounds, NULL, NULL, 0, -1)
      == false)
  {
    state->resIDValue = id;
    return CDI_APPLY_STOP;
  }

  return CDI_APPLY_GO_ON;
}

static int
vlist_generate_zaxis(int vlistID, int zaxistype, int nlevels, const double *levels, const double *lbounds, const double *ubounds,
                     int vctsize, const double *vct, const char **cvals, size_t clen)
{
  int zaxisID = CDI_UNDEFID;
  bool zaxisdefined = false;
  bool zaxisglobdefined = false;
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  int nzaxis = vlistP->nzaxis;

  bool hasBounds = (lbounds && ubounds);

  for (int index = 0; index < nzaxis; ++index)
  {
    zaxisID = vlistP->zaxisIDs[index];

    if (zaxis_compare(zaxisID, zaxistype, nlevels, levels, lbounds, ubounds, NULL, NULL, 0, -1) == false)
    {
      zaxisdefined = true;
      break;
    }
  }

  if (!zaxisdefined)
  {
    struct vgzSearchState query;
    query.zaxistype = zaxistype;
    query.nlevels = nlevels;
    query.levels = levels;
    query.lbounds = lbounds;
    query.ubounds = ubounds;

    if ((zaxisglobdefined = (cdiResHFilterApply(getZaxisOps(), vgzZAxisSearch, &query) == CDI_APPLY_STOP)))
      zaxisID = query.resIDValue;
  }

  if (!zaxisdefined)
  {
    if (!zaxisglobdefined)
    {
      zaxisID = zaxisCreate(zaxistype, nlevels);
      zaxisDefLevels(zaxisID, levels);

      if (zaxistype == ZAXIS_CHAR) zaxisDefCvals(zaxisID, cvals, (int) clen);

      if (hasBounds)
      {
        zaxisDefLbounds(zaxisID, lbounds);
        zaxisDefUbounds(zaxisID, ubounds);
      }

      if (zaxistype == ZAXIS_HYBRID && vctsize > 0) zaxisDefVct(zaxisID, vctsize, vct);
    }

    nzaxis = vlistP->nzaxis;
    vlistP->zaxisIDs[nzaxis] = zaxisID;
    vlistP->nzaxis++;
  }

  return zaxisID;
}

static void
delete_chunks(int vlistID, int varID)
{
  static const int chunkKeys[4]
      = { CDI_KEY_CHUNKSIZE_DIMX, CDI_KEY_CHUNKSIZE_DIMY, CDI_KEY_CHUNKSIZE_DIMZ, CDI_KEY_CHUNKSIZE_DIMT };
  for (int i = 0; i < 4; ++i)
  {
    int chunkSize = 0;
    cdiInqKeyInt(vlistID, varID, chunkKeys[i], &chunkSize);
    if (chunkSize != 0) cdiDeleteKey(vlistID, varID, chunkKeys[i]);
  }

  int chunkSize = 0;
  cdiInqKeyInt(vlistID, varID, CDI_KEY_CHUNKSIZE, &chunkSize);
  if (chunkSize > 0) cdiDeleteKey(vlistID, varID, CDI_KEY_CHUNKSIZE);
}

/*
@Function  vlistCopyFlag
@Title     Copy some entries of a variable list

@Prototype void vlistCopyFlag(int vlistID2, int vlistID1)
@Parameter
    @Item  vlistID2  Target variable list ID.
    @Item  vlistID1  Source variable list ID.

@Description
The function @func{vlistCopyFlag} copies all entries with a flag from vlistID1 to vlistID2.

@EndFunction
*/
void
vlistCopyFlag(int vlistID2, int vlistID1)
{
  vlist_t *vlistP1 = vlist_to_pointer(vlistID1);
  vlist_t *vlistP2 = vlist_to_pointer(vlistID2);
  var_t *vars1 = vlistP1->vars;

  vlist_copy(vlistP2, vlistP1);

  vlistP2->keys.nelems = 0;
  cdiCopyKeys(vlistID1, CDI_GLOBAL, vlistID2, CDI_GLOBAL);
  vlistP2->atts.nelems = 0;
  cdiCopyAtts(vlistID1, CDI_GLOBAL, vlistID2, CDI_GLOBAL);

  if (vars1)
  {
    vlistP2->ngrids = 0;
    vlistP2->nzaxis = 0;

    int nvars = vlistP1->nvars;
    int nvars2 = 0;
    for (int varID = 0; varID < nvars; varID++) nvars2 += vars1[varID].flag;

    vlistP2->nvars = nvars2;
    vlistP2->varsAllocated = nvars2;
    var_t *vars2 = (nvars2 > 0) ? (var_t *) Malloc((size_t) nvars2 * sizeof(var_t)) : NULL;

    vlistP2->vars = vars2;

    int varID2 = 0;
    for (int varID = 0; varID < nvars; varID++)
      if (vars1[varID].flag)
      {
        vlistP2->vars[varID2].flag = false;
        int zaxisID = vars1[varID].zaxisID;
        int gridID = vars1[varID].gridID;
        int subtypeID = vars1[varID].subtypeID;

        memcpy(&vars2[varID2], &vars1[varID], sizeof(var_t));

        vars1[varID].fvarID = varID2;
        vars2[varID2].fvarID = varID;

        vars2[varID2].mvarID = varID2;

        var_copy_entries(&vars2[varID2], &vars1[varID]);
        vlistP2->vars[varID2].keys.nelems = 0;
        cdiCopyKeys(vlistID1, varID, vlistID2, varID2);

        vlistP2->vars[varID2].atts.nelems = 0;
        cdiCopyAtts(vlistID1, varID, vlistID2, varID2);

        int nlevs = zaxisInqSize(vars1[varID].zaxisID);
        int nlevs2 = 0;
        if (vars1[varID].levinfo)
          for (int levID = 0; levID < nlevs; levID++) nlevs2 += vars1[varID].levinfo[levID].flag;

        vars2[varID2].levinfo = (levinfo_t *) Malloc((size_t) nlevs2 * sizeof(levinfo_t));

        if (nlevs != nlevs2)
        {
          int nvct = 0;
          double *levels = NULL;
          double *lbounds = NULL, *ubounds = NULL;
          const double *vct = NULL;

          if (!vars1[varID].levinfo) cdiVlistCreateVarLevInfo(vlistP1, varID);

          zaxisID = vars1[varID].zaxisID;
          int zaxisType = zaxisInqType(zaxisID);

          int levID2 = 0;
          for (int levID = 0; levID < nlevs; levID++)
            if (vars1[varID].levinfo[levID].flag)
            {
              vars1[varID].levinfo[levID].flevelID = levID2;
              vars1[varID].levinfo[levID].mlevelID = levID2;
            }

          if (zaxisInqLevels(zaxisID, NULL))
          {
            levels = (double *) Malloc((size_t) nlevs2 * sizeof(double));
            levID2 = 0;
            for (int levID = 0; levID < nlevs; ++levID)
              if (vars1[varID].levinfo[levID].flag) levels[levID2++] = zaxisInqLevel(zaxisID, levID);
          }

          if (zaxisType == ZAXIS_HYBRID)
          {
            nvct = zaxisInqVctSize(zaxisID);
            vct = zaxisInqVctPtr(zaxisID);
          }

          size_t clen2 = 0;
          char **cvals2 = NULL;
#ifndef USE_MPI
          if (zaxisType == ZAXIS_CHAR)
          {
            char **cvals1 = zaxisInqCValsPtr(zaxisID);
            size_t clen1 = (size_t) zaxisInqCLen(zaxisID);
            for (int levID = 0; levID < nlevs; ++levID)
              if (vars1[varID].levinfo[levID].flag)
              {
                size_t testlen = clen1;
                while (cvals1[levID][testlen] == ' ') testlen--;
                if (clen2 < testlen) clen2 = testlen;
              }
            cvals2 = (char **) Malloc((size_t) nlevs2 * sizeof(char *));
            levID2 = 0;

            for (int levID = 0; levID < nlevs; ++levID)
              if (vars1[varID].levinfo[levID].flag)
              {
                cvals2[levID2] = (char *) Malloc((size_t) (clen2) * sizeof(char));
                memcpy(cvals2[levID2], cvals1[levID], clen2 * sizeof(char));
                levID2++;
              }
          }
#endif

          if (zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL))
          {
            lbounds = (double *) Malloc(2 * (size_t) nlevs2 * sizeof(double));
            ubounds = lbounds + nlevs2;

            double *lbounds1 = (double *) Malloc(2 * (size_t) nlevs * sizeof(double)), *ubounds1 = lbounds1 + nlevs;
            zaxisInqLbounds(zaxisID, lbounds1);
            zaxisInqUbounds(zaxisID, ubounds1);

            levID2 = 0;
            for (int levID = 0; levID < nlevs; ++levID)
              if (vars1[varID].levinfo[levID].flag)
              {
                lbounds[levID2] = lbounds1[levID];
                ubounds[levID2] = ubounds1[levID];
                levID2++;
              }

            Free(lbounds1);
          }

          int zaxisID2 = vlist_generate_zaxis(vlistID2, zaxisType, nlevs2, levels, lbounds, ubounds, nvct, vct,
                                              (const char **) cvals2, clen2);
          Free(levels);
          Free(lbounds);
          if (cvals2)
          {
            for (int levID = 0; levID < nlevs2; ++levID) Free(cvals2[levID]);
            Free(cvals2);
          }

          char ctemp[CDI_MAX_NAME];
          int length = CDI_MAX_NAME;
          cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_NAME, ctemp, &length);
          cdiDefKeyString(zaxisID2, CDI_GLOBAL, CDI_KEY_NAME, ctemp);
          length = CDI_MAX_NAME;
          cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_LONGNAME, ctemp, &length);
          cdiDefKeyString(zaxisID2, CDI_GLOBAL, CDI_KEY_LONGNAME, ctemp);
          length = CDI_MAX_NAME;
          cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_UNITS, ctemp, &length);
          cdiDefKeyString(zaxisID2, CDI_GLOBAL, CDI_KEY_UNITS, ctemp);

          zaxisDefDatatype(zaxisID2, zaxisInqDatatype(zaxisID));
          zaxisDefPositive(zaxisID2, zaxisInqPositive(zaxisID));

          if (zaxisType == ZAXIS_CHAR)
          {
            char dimname[CDI_MAX_NAME + 3];
            length = sizeof(dimname);
            cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_DIMNAME, dimname, &length);
            if (dimname[0] == 0)
            {
              memcpy(dimname, "area_type", 10);
              dimname[10] = 0;
            }
            cdiDefKeyString(zaxisID2, CDI_GLOBAL, CDI_KEY_DIMNAME, dimname);
          }

          if (zaxisType == ZAXIS_GENERIC) cdiCopyKey(zaxisID, CDI_GLOBAL, CDI_KEY_TYPEOFFIRSTFIXEDSURFACE, zaxisID2);

          cdiCopyAtts(zaxisID, CDI_GLOBAL, zaxisID2, CDI_GLOBAL);

          zaxisID = zaxisID2;
          vars2[varID2].zaxisID = zaxisID2;
          delete_chunks(vlistID2, varID2);
        }

        for (int levID = 0; levID < nlevs2; levID++)
        {
          vars2[varID2].levinfo[levID].flag = false;
          vars2[varID2].levinfo[levID].index = -1;
        }

        int levID2 = 0;
        for (int levID = 0; levID < nlevs; levID++)
          if (vars1[varID].levinfo[levID].flag)
          {
            vars2[varID2].levinfo[levID2].flevelID = levID;
            vars2[varID2].levinfo[levID2].mlevelID = levID2;
            levID2++;
          }

        vlistAdd2GridIDs(vlistP2, gridID);
        vlistAdd2ZaxisIDs(vlistP2, zaxisID);
        vlistAdd2SubtypeIDs(vlistP2, subtypeID);

        varID2++;
      }
  }
}

/*
@Function  vlistCat
@Title     Concatenate two variable lists

@Prototype void vlistCat(int vlistID2, int vlistID1)
@Parameter
    @Item  vlistID2  Target variable list ID.
    @Item  vlistID1  Source variable list ID.

@Description
Concatenate the variable list vlistID1 at the end of vlistID2.

@EndFunction
*/
void
vlistCat(int vlistID2, int vlistID1)
{
  vlist_t *vlistP1 = vlist_to_pointer(vlistID1);
  vlist_t *vlistP2 = vlist_to_pointer(vlistID2);
  var_t *vars1 = vlistP1->vars;
  var_t *vars2 = vlistP2->vars;
  int nvars1 = vlistP1->nvars;
  int nvars2 = vlistP2->nvars;
  int nvars = nvars1 + nvars2;
  vlistP2->nvars = nvars;

  if (nvars > vlistP2->varsAllocated)
  {
    vlistP2->varsAllocated = nvars;
    vars2 = (var_t *) Realloc(vars2, (size_t) nvars * sizeof(var_t));
    vlistP2->vars = vars2;
  }
  memcpy(vars2 + nvars2, vars1, (size_t) nvars1 * sizeof(var_t));

  for (int varID = 0; varID < nvars1; varID++)
  {
    int varID2 = varID + nvars2;
    vars1[varID].fvarID = varID2;
    vars2[varID2].fvarID = varID;

    vars1[varID].mvarID = varID2;
    vars2[varID2].mvarID = varID;

    if (vars1[varID].param < 0)
    {
      int pnum, pcat, pdis;
      cdiDecodeParam(vars1[varID].param, &pnum, &pcat, &pdis);
      pnum = -(varID2 + 1);
      vars2[varID2].param = cdiEncodeParam(pnum, pcat, pdis);
    }

    var_copy_entries(&vars2[varID2], &vars1[varID]);
    vars2[varID2].keys.nelems = 0;
    cdiCopyKeys(vlistID1, varID, vlistID2, varID2);

    if (vars1[varID].levinfo)
    {
      size_t nlevs = (size_t) zaxisInqSize(vars1[varID].zaxisID);
      vars2[varID2].levinfo = (levinfo_t *) Malloc(nlevs * sizeof(levinfo_t));
      memcpy(vars2[varID2].levinfo, vars1[varID].levinfo, nlevs * sizeof(levinfo_t));
    }

    vars2[varID2].atts.nelems = 0;
    cdiCopyAtts(vlistID1, varID, vlistID2, varID2);

    vlistAdd2GridIDs(vlistP2, vars1[varID].gridID);
    vlistAdd2ZaxisIDs(vlistP2, vars1[varID].zaxisID);
    vlistAdd2SubtypeIDs(vlistP2, vars1[varID].subtypeID);
  }
}

/*
@Function  vlistMerge
@Title     Merge two variable lists

@Prototype void vlistMerge(int vlistID2, int vlistID1)
@Parameter
    @Item  vlistID2  Target variable list ID.
    @Item  vlistID1  Source variable list ID.

@Description
Merge the variable list vlistID1 to the variable list vlistID2.

@EndFunction
*/
void
vlistMerge(int vlistID2, int vlistID1)
{
  int varID = 0;
  vlist_t *vlistP1 = vlist_to_pointer(vlistID1);
  vlist_t *vlistP2 = vlist_to_pointer(vlistID2);
  var_t *vars1 = vlistP1->vars;
  var_t *vars2 = vlistP2->vars;
  int nvars1 = vlistP1->nvars;
  int nvars2 = vlistP2->nvars;

  if (nvars1 == nvars2)
  {
    char name1[CDI_MAX_NAME], name2[CDI_MAX_NAME];
    for (varID = 0; varID < nvars2; varID++)
    {
      SizeType ngp1 = gridInqSize(vars1[varID].gridID);
      SizeType ngp2 = gridInqSize(vars2[varID].gridID);
      if (ngp1 != ngp2) break;

      int length = CDI_MAX_NAME;
      (void) cdiInqKeyString(vlistID1, varID, CDI_KEY_NAME, name1, &length);
      length = CDI_MAX_NAME;
      (void) cdiInqKeyString(vlistID2, varID, CDI_KEY_NAME, name2, &length);

      if (*name1 && *name2)
      {
        if (!str_is_equal(name1, name2)) break;
      }
      else
      {
        if (vars1[varID].param != vars2[varID].param) break;
      }
    }
  }

  if (varID == nvars2) /* same variables in vlistID1 and vlistID2 */
  {
    for (varID = 0; varID < nvars2; varID++)
    {
      vars1[varID].fvarID = varID;
      vars2[varID].fvarID = varID;

      vars1[varID].mvarID = varID;
      vars2[varID].mvarID = varID;

      int nlevs1 = zaxisInqSize(vars1[varID].zaxisID);
      int nlevs2 = zaxisInqSize(vars2[varID].zaxisID);

      int nlevs = nlevs1 + nlevs2;

      // fprintf(stderr, "var %d %d %d %d %d\n", varID, nlevs1, nlevs2, nlevs, sizeof(levinfo_t));
      if (vars1[varID].levinfo)
      {
        vars2[varID].levinfo = (levinfo_t *) Realloc(vars2[varID].levinfo, (size_t) nlevs * sizeof(levinfo_t));
        memcpy(vars2[varID].levinfo + nlevs2, vars1[varID].levinfo, (size_t) nlevs1 * sizeof(levinfo_t));
      }
      else
        cdiVlistCreateVarLevInfo(vlistP1, varID);

      for (int levID = 0; levID < nlevs1; levID++) vars1[varID].levinfo[levID].mlevelID = nlevs2 + levID;
    }

    bool *lvar = (bool *) Calloc((size_t) nvars2, sizeof(bool));

    for (varID = 0; varID < nvars2; varID++)
    {
      if (lvar[varID] == true) continue;

      int zaxisID1 = vars1[varID].zaxisID;
      int zaxisID2 = vars2[varID].zaxisID;
      int nlevs1 = zaxisInqSize(zaxisID1);
      int nlevs2 = zaxisInqSize(zaxisID2);
      // fprintf(stderr, "zaxis %d %d %d %d\n", zaxisID1, zaxisID2, nlevs1, nlevs2);

      int nlevs = nlevs1 + nlevs2;

      int zaxisID = zaxisDuplicate(zaxisID2);
      zaxisResize(zaxisID, nlevs);

      if (zaxisInqLevels(zaxisID1, NULL))
      {
        double *levels = (double *) Malloc((size_t) nlevs1 * sizeof(double));
        zaxisInqLevels(zaxisID1, levels);
        /*
          for (int levID = 0; levID < nlevs1; levID++)
            fprintf(stderr, "%d %d %d %d %d %g\n", varID, levID, nlevs1, nlevs2, vars2[varID].nlevs, levels[levID]);
        */
        for (int levID = 0; levID < nlevs1; levID++) zaxisDefLevel(zaxisID, nlevs2 + levID, levels[levID]);

        Free(levels);
      }

      for (int index = 0; index < vlistP2->nzaxis; index++)
        if (vlistP2->zaxisIDs[index] == zaxisID2) vlistP2->zaxisIDs[index] = zaxisID;

      for (int varID2 = 0; varID2 < nvars2; varID2++)
        if (lvar[varID2] == false && vars2[varID2].zaxisID == zaxisID2)
        {
          vars2[varID2].zaxisID = zaxisID;
          lvar[varID2] = true;
        }
    }

    Free(lvar);
  }
  else
  {
    vlistCat(vlistID2, vlistID1);
  }
}

/*
@Function  vlistNvars
@Title     Number of variables in a variable list

@Prototype int vlistNvars(int vlistID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.

@Description
The function @func{vlistNvars} returns the number of variables in the variable list vlistID.

@Result
@func{vlistNvars} returns the number of variables in a variable list.

@EndFunction
*/
int
vlistNvars(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return vlistP->nvars;
}

int
vlistNumFields(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int nrecs = 0;
  for (int varID = 0; varID < vlistP->nvars; varID++) nrecs += zaxisInqSize(vlistP->vars[varID].zaxisID);

  return nrecs;
}

int
vlistNumber(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int datatype = vlistP->vars[0].datatype;
  int number = (datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64) ? CDI_COMP : CDI_REAL;

  for (int varID = 1; varID < vlistP->nvars; varID++)
  {
    datatype = vlistP->vars[varID].datatype;
    int number2 = (datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64) ? CDI_COMP : CDI_REAL;
    if (number2 != number)
    {
      number = CDI_BOTH;
      break;
    }
  }

  return number;
}

/*
@Function  vlistNumGrids
@Title     Number of grids in a variable list

@Prototype int vlistNumGrids(int vlistID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.

@Description
The function @func{vlistNumGrids} returns the number of grids in the variable list vlistID.

@Result
@func{vlistNumGrids} returns the number of grids in a variable list.

@EndFunction
*/
int
vlistNumGrids(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return vlistP->ngrids;
}

int
vlistNgrids(int vlistID)
{
  return vlistNumGrids(vlistID);
}

/*
@Function  vlistNumZaxis
@Title     Number of zaxis in a variable list

@Prototype int vlistNumZaxis(int vlistID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.

@Description
The function @func{vlistNumZaxis} returns the number of zaxis in the variable list vlistID.

@Result
@func{vlistNumZaxis} returns the number of zaxis in a variable list.

@EndFunction
*/
int
vlistNumZaxis(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return vlistP->nzaxis;
}

int
vlistNzaxis(int vlistID)
{
  return vlistNumZaxis(vlistID);
}

int
vlistNsubtypes(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return vlistP->nsubtypes;
}

void
vlistDefNtsteps(int vlistID, int nts)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  if (vlistP->ntsteps != nts)
  {
    vlistP->ntsteps = nts;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

// This function is used in CDO!
int
vlistNtsteps(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return (int) vlistP->ntsteps;
}

static void
vlist_print(vlist_t *vlistP, FILE *fp)
{
  int vlistID = vlistP->self;
  fprintf(fp, "#\n# vlistID %d\n#\n", vlistID);

  int nvars = vlistP->nvars;

  fprintf(fp,
          "nvars    : %d\n"
          "ngrids   : %d\n"
          "nzaxis   : %d\n"
          "nsubtypes: %d\n"
          "taxisID  : %d\n"
          "instID   : %d\n"
          "modelID  : %d\n"
          "tableID  : %d\n",
          nvars, vlistP->ngrids, vlistP->nzaxis, vlistP->nsubtypes, vlistP->taxisID, vlistP->instID, vlistP->modelID,
          vlistP->tableID);

  if (nvars > 0)
  {
    fprintf(fp, " varID param    gridID zaxisID stypeID tsteptype flag name     longname         units\n");
    for (int varID = 0; varID < nvars; varID++)
    {
      var_t *var = &vlistP->vars[varID];
      int param = var->param;
      int gridID = var->gridID;
      int zaxisID = var->zaxisID;
      int subtypeID = var->subtypeID;
      int tsteptype = var->tsteptype;
      char name[CDI_MAX_NAME], longname[CDI_MAX_NAME], units[CDI_MAX_NAME];
      int length = CDI_MAX_NAME;
      (void) cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length);
      length = CDI_MAX_NAME;
      (void) cdiInqKeyString(vlistID, varID, CDI_KEY_LONGNAME, longname, &length);
      length = CDI_MAX_NAME;
      (void) cdiInqKeyString(vlistID, varID, CDI_KEY_UNITS, units, &length);
      int flag = var->flag;

      char paramstr[32];
      cdiParamToString(param, paramstr, sizeof(paramstr));
      fprintf(fp, "%6d %-8s %6d  %6d  %6d  %6d  %5d %-8s %s [%s]\n", varID, paramstr, gridID, zaxisID, subtypeID, tsteptype, flag,
              name, longname, units);
    }

    fputs("\n"
          " varID  levID fvarID flevID mvarID mlevID  index  dtype  flag  level\n",
          fp);
    for (int varID = 0; varID < nvars; varID++)
    {
      var_t *var = &vlistP->vars[varID];
      int zaxisID = var->zaxisID;
      int nlevs = zaxisInqSize(zaxisID);
      int fvarID = var->fvarID;
      int mvarID = var->mvarID;
      int dtype = var->datatype;
      for (int levID = 0; levID < nlevs; levID++)
      {
        levinfo_t li = (var->levinfo) ? var->levinfo[levID] : DEFAULT_LEVINFO(levID);
        int flevID = li.flevelID;
        int mlevID = li.mlevelID;
        int index = li.index;
        int flag = li.flag;

        double level = zaxisInqLevels(zaxisID, NULL) ? zaxisInqLevel(zaxisID, levID) : levID + 1;
        fprintf(fp, "%6d %6d %6d %6d %6d %6d %6d %6d %5d  %.9g\n", varID, levID, fvarID, flevID, mvarID, mlevID, index, dtype, flag,
                level);
      }
    }

    fputs("\n"
          " varID  size\n",
          fp);
    for (int varID = 0; varID < nvars; varID++)
    {
      var_t *var = &vlistP->vars[varID];
      fprintf(fp, "%3d %8zu\n", varID, (size_t) zaxisInqSize(var->zaxisID) * gridInqSize(var->gridID));
    }
  }
}

void
vlistPrint(int vlistID)
{
  if (vlistID == CDI_UNDEFID) return;
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  vlist_print(vlistP, stdout);
}

/*
@Function  vlistDefTaxis
@Title     Define the time axis

@Prototype void vlistDefTaxis(int vlistID, int taxisID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}.

@Description
The function @func{vlistDefTaxis} defines the time axis of a variable list.

@EndFunction
*/
void
vlistDefTaxis(int vlistID, int taxisID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  if (vlistP->taxisID != taxisID)
  {
    // FIXME: This code seems to leak a taxis_t object if `vlistP->taxisID` was valid before the call to vlistDefTaxis.
    vlistP->taxisID = taxisID;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

/*
@Function  vlistInqTaxis
@Title     Get the time axis

@Prototype int vlistInqTaxis(int vlistID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.

@Description
The function @func{vlistInqTaxis} returns the time axis of a variable list.

@Result
@func{vlistInqTaxis} returns an identifier to the time axis.

@EndFunction
*/
int
vlistInqTaxis(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return vlistP->taxisID;
}

void
vlistDefTable(int vlistID, int tableID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  if (vlistP->tableID != tableID)
  {
    vlistP->tableID = tableID;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqTable(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return vlistP->tableID;
}

void
vlistDefInstitut(int vlistID, int instID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  if (vlistP->instID != instID)
  {
    vlistP->instID = instID;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqInstitut(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  int instID = vlistP->instID;
  if (instID == CDI_UNDEFID)
  {
    instID = vlistInqVarInstitut(vlistID, 0);
    for (int varID = 1; varID < vlistP->nvars; varID++)
      if (instID != vlistInqVarInstitut(vlistID, varID))
      {
        instID = CDI_UNDEFID;
        break;
      }
    vlistDefInstitut(vlistID, instID);
  }

  return instID;
}

void
vlistDefModel(int vlistID, int modelID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  if (vlistP->modelID != modelID)
  {
    vlistP->modelID = modelID;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqModel(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  int modelID = vlistP->modelID;
  if (modelID == CDI_UNDEFID)
  {
    modelID = vlistInqVarModel(vlistID, 0);
    for (int varID = 1; varID < vlistP->nvars; varID++)
      if (modelID != vlistInqVarModel(vlistID, varID))
      {
        modelID = CDI_UNDEFID;
        break;
      }

    vlistDefModel(vlistID, modelID);
  }

  return modelID;
}

SizeType
vlistGridsizeMax(int vlistID)
{
  SizeType gridsizemax = 0;
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  for (int index = 0; index < vlistP->ngrids; index++)
  {
    int gridID = vlistP->gridIDs[index];
    SizeType gridsize = gridInqSize(gridID);
    if (gridsize > gridsizemax) gridsizemax = gridsize;
  }

  return gridsizemax;
}

int
vlistGrid(int vlistID, int index)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  return (index < vlistP->ngrids && index >= 0) ? vlistP->gridIDs[index] : CDI_UNDEFID;
}

int
vlistGridIndex(int vlistID, int gridID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  int index;
  for (index = 0; index < vlistP->ngrids; index++)
    if (gridID == vlistP->gridIDs[index]) break;
  if (index == vlistP->ngrids) index = -1;

  return index;
}

void
vlistChangeGridIndex(int vlistID, int index, int gridID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int gridIDold = vlistP->gridIDs[index];
  if (gridIDold != gridID)
  {
    vlistP->gridIDs[index] = gridID;

    int nvars = vlistP->nvars;
    for (int varID = 0; varID < nvars; varID++)
      if (vlistP->vars[varID].gridID == gridIDold)
      {
        vlistP->vars[varID].gridID = gridID;
        delete_chunks(vlistID, varID);
        if (gridInqXsize(gridIDold) == 0 && gridInqXsize(gridID) > 0 && vlistInqVarXYZ(vlistID, varID) != 0)
          vlistDefVarXYZ(vlistID, varID, 0);
      }
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

void
vlistChangeGrid(int vlistID, int gridID1, int gridID2)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  if (gridID1 != gridID2)
  {
    int ngrids = vlistP->ngrids;
    for (int index = 0; index < ngrids; index++)
    {
      if (vlistP->gridIDs[index] == gridID1)
      {
        vlistP->gridIDs[index] = gridID2;
        break;
      }
    }
    int nvars = vlistP->nvars;
    for (int varID = 0; varID < nvars; varID++)
      if (vlistP->vars[varID].gridID == gridID1)
      {
        vlistP->vars[varID].gridID = gridID2;
        delete_chunks(vlistID, varID);
      }
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistZaxis(int vlistID, int index)
{
  int zaxisID = CDI_UNDEFID;
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  if (index < vlistP->nzaxis && index >= 0) zaxisID = vlistP->zaxisIDs[index];

  return zaxisID;
}

int
vlistZaxisIndex(int vlistID, int zaxisID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int index;
  for (index = 0; index < vlistP->nzaxis; index++)
    if (zaxisID == vlistP->zaxisIDs[index]) break;

  if (index == vlistP->nzaxis) index = -1;

  return index;
}

void
vlistChangeZaxisIndex(int vlistID, int index, int zaxisID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int zaxisIDold = vlistP->zaxisIDs[index];
  if (zaxisIDold != zaxisID)
  {
    vlistP->zaxisIDs[index] = zaxisID;

    int nlevs = zaxisInqSize(zaxisID), nlevsOld = zaxisInqSize(zaxisIDold);
    int nvars = vlistP->nvars;
    for (int varID = 0; varID < nvars; varID++)
      if (vlistP->vars[varID].zaxisID == zaxisIDold)
      {
        vlistP->vars[varID].zaxisID = zaxisID;
        delete_chunks(vlistID, varID);
        if (vlistP->vars[varID].levinfo && nlevs != nlevsOld)
        {
          vlistP->vars[varID].levinfo = (levinfo_t *) Realloc(vlistP->vars[varID].levinfo, (size_t) nlevs * sizeof(levinfo_t));

          for (int levID = 0; levID < nlevs; levID++) vlistP->vars[varID].levinfo[levID] = DEFAULT_LEVINFO(levID);
        }
      }
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

void
vlistChangeZaxis(int vlistID, int zaxisID1, int zaxisID2)
{
  int nlevs1 = zaxisInqSize(zaxisID1), nlevs2 = zaxisInqSize(zaxisID2);
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int nzaxis = vlistP->nzaxis;
  for (int index = 0; index < nzaxis; index++)
  {
    if (vlistP->zaxisIDs[index] == zaxisID1)
    {
      vlistP->zaxisIDs[index] = zaxisID2;
      break;
    }
  }

  int nvars = vlistP->nvars;
  for (int varID = 0; varID < nvars; varID++)
    if (vlistP->vars[varID].zaxisID == zaxisID1)
    {
      vlistP->vars[varID].zaxisID = zaxisID2;
      delete_chunks(vlistID, varID);

      if (vlistP->vars[varID].levinfo && nlevs2 != nlevs1)
      {
        vlistP->vars[varID].levinfo = (levinfo_t *) Realloc(vlistP->vars[varID].levinfo, (size_t) nlevs2 * sizeof(levinfo_t));

        for (int levID = 0; levID < nlevs2; levID++) vlistP->vars[varID].levinfo[levID] = DEFAULT_LEVINFO(levID);
      }
    }
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

int
vlistSubtype(int vlistID, int index)
{
  int subtypeID = CDI_UNDEFID;
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  if (index < vlistP->nsubtypes && index >= 0) subtypeID = vlistP->subtypeIDs[index];

  return subtypeID;
}

int
vlistSubtypeIndex(int vlistID, int subtypeID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int index;
  for (index = vlistP->nsubtypes; index--;)
    if (subtypeID == vlistP->subtypeIDs[index]) break;

  return index;
}

bool
vlistHasTime(int vlistID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  if ((CDI_Reduce_Dim || CDI_Reduce_DimT) && vlistP->ntsteps == 1) return false;

  int nvars = vlistP->nvars;
  var_t *vars = vlistP->vars;
  for (int varID = 0; varID < nvars; varID++)
    if (vars[varID].timetype != TIME_CONSTANT) { return true; }

  return false;
}

void
vlist_check_contents(int vlistID)
{
  int nzaxis = vlistNumZaxis(vlistID);
  for (int index = 0; index < nzaxis; index++)
  {
    int zaxisID = vlistZaxis(vlistID, index);
    if (zaxisInqType(zaxisID) == ZAXIS_GENERIC) cdiCheckZaxis(zaxisID);
  }
}

/* Resizes and initializes opt_grib_kvpair data structure. */
void
resize_opt_grib_entries(var_t *var, int nentries)
{
  if (var->opt_grib_kvpair_size >= nentries) return;  // nothing to do; array is still large enough

  if (CDI_Debug) Message("resize data structure, %d -> %d", var->opt_grib_kvpair_size, nentries);

  int new_size = (2 * var->opt_grib_kvpair_size) > nentries ? (2 * var->opt_grib_kvpair_size) : nentries;
  opt_key_val_pair_t *tmp = (opt_key_val_pair_t *) Malloc((size_t) new_size * sizeof(opt_key_val_pair_t));
  for (int i = 0; i < var->opt_grib_kvpair_size; ++i) { tmp[i] = var->opt_grib_kvpair[i]; }
  for (int i = var->opt_grib_kvpair_size; i < new_size; ++i)
  {
    tmp[i].int_val = 0;
    tmp[i].dbl_val = 0;
    tmp[i].int_arr = 0;
    tmp[i].dbl_arr = 0;
    tmp[i].update = false;
    tmp[i].keyword = NULL;
  }  // for
  var->opt_grib_kvpair_size = new_size;
  Free(var->opt_grib_kvpair);
  var->opt_grib_kvpair = tmp;
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
