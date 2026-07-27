/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include <string.h>
#include <math.h>
#include <float.h>

#include "dmemory.h"

#include "cdi.h"
#include "cdi_cksum.h"
#include "cdi_int.h"
#include "cdi_uuid.h"
#include "resource_handle.h"
#include "resource_unpack.h"
#include "namespace.h"
#include "serialize.h"
#include "zaxis.h"
#include "zaxis_pack.h"

enum
{
  LevelUp = 1,
  LevelDown = 2
};

// clang-format off
static const struct ZaxisParam
{
  unsigned char positive;   // 1: up;  2: down
  const char *name;
  const char *longname;
  const char *stdname;
  const char *units;
}
ZaxistypeEntry[] = {
  { /*  0 */ 0, "sfc",               "surface",                     "",               ""},
  { /*  1 */ 0, "lev",               "generic",                     "",               ""},
  { /*  2 */ 2, "lev",               "hybrid",                      "",               "level"},
  { /*  3 */ 2, "lev",               "hybrid_half",                 "",               "level"},
  { /*  4 */ 2, "plev",              "pressure",                    "air_pressure",   "Pa"},
  { /*  5 */ 1, "height",            "height",                      "height",         "m"},
  { /*  6 */ 2, "depth",             "depth_below_sea",             "depth",          "m"},
  { /*  7 */ 2, "depth",             "depth_below_land",            "",               "cm"},
  { /*  8 */ 0, "lev",               "isentropic",                  "",               "K"},
  { /*  9 */ 0, "lev",               "trajectory",                  "",               ""},
  { /* 10 */ 1, "alt",               "height_above_mean_sea_level", "altitude",       "m"},
  { /* 11 */ 0, "lev",               "sigma",                       "",               "level"},
  { /* 12 */ 0, "lev",               "meansea",                     "",               "level"},
  { /* 13 */ 0, "toa",               "top_of_atmosphere",           "",               ""},
  { /* 14 */ 0, "seabottom",         "sea_bottom",                  "",               ""},
  { /* 15 */ 0, "atmosphere",        "atmosphere",                  "",               ""},
  { /* 16 */ 0, "cloudbase",         "cloud_base",                  "",               ""},
  { /* 17 */ 0, "cloudtop",          "cloud_top",                   "",               ""},
  { /* 18 */ 0, "isotherm0",         "isotherm_zero",               "",               ""},
  { /* 19 */ 0, "snow",              "snow",                        "",               ""},
  { /* 20 */ 0, "lakebottom",        "lake_bottom",                 "",               ""},
  { /* 21 */ 0, "sedimentbottom",    "sediment_bottom",             "",               ""},
  { /* 22 */ 0, "sedimentbottomta",  "sediment_bottom_ta",          "",               ""},
  { /* 23 */ 0, "sedimentbottomtw",  "sediment_bottom_tw",          "",               ""},
  { /* 24 */ 0, "mixlayer",          "mix_layer",                   "",               ""},
  { /* 25 */ 0, "height",            "generalized_height",          "height",         ""},
  { /* 26 */ 0, "character",         "area_type",                   "",               ""},
  { /* 27 */ 0, "tropopause",        "tropopause",                  "",               ""},
};
// clang-format on

enum
{
  CDI_NumZaxistype = ARRAY_SIZE(ZaxistypeEntry)
};

static int zaxis_compare(zaxis_t *z1, zaxis_t *z2);
static void zaxis_destroy(void *zaxisP);
static void zaxis_print(void *zaxisP, FILE *fp);

static const resOps zaxisOps
    = { (int (*)(void *, void *)) zaxis_compare, zaxis_destroy, zaxis_print, zaxisGetPackSize, zaxisPack, zaxisTxCode };

const resOps *
getZaxisOps(void)
{
  return &zaxisOps;
}

void
zaxisGetTypeDescription(int zaxisType, int *outPositive, const char **outName, const char **outLongName, const char **outStdName,
                        const char **outUnit)
{
  if (zaxisType < 0 || zaxisType >= CDI_NumZaxistype)
  {
    if (outPositive) *outPositive = 0;
    if (outName) *outName = NULL;
    if (outLongName) *outLongName = NULL;
    if (outStdName) *outStdName = NULL;
    if (outUnit) *outUnit = NULL;
  }
  else
  {
    const struct ZaxisParam *zaxisParam = &ZaxistypeEntry[zaxisType];
    if (outPositive) *outPositive = zaxisParam->positive;
    if (outName) *outName = zaxisParam->name;
    if (outLongName && zaxisType != ZAXIS_GENERIC) *outLongName = zaxisParam->longname;
    if (outStdName) *outStdName = zaxisParam->stdname;
    if (outUnit) *outUnit = zaxisParam->units;
  }
}

zaxis_t *
zaxis_to_pointer(int id)
{
  return (zaxis_t *) reshGetVal(id, &zaxisOps);
}

static void
zaxis_init(zaxis_t *zaxisP)
{
  zaxisP->self = CDI_UNDEFID;
  zaxisP->vals = NULL;
#ifndef USE_MPI
  zaxisP->cvals = NULL;
  zaxisP->clength = 0;
#endif
  zaxisP->ubounds = NULL;
  zaxisP->lbounds = NULL;
  zaxisP->weights = NULL;
  zaxisP->type = CDI_UNDEFID;
  zaxisP->positive = 0;
  zaxisP->scalar = 0;
  zaxisP->direction = 0;
  zaxisP->size = 0;
  zaxisP->vctsize = 0;
  zaxisP->vct = NULL;

  cdiInitKeys(&zaxisP->keys);
  zaxisP->atts.nalloc = MAX_ATTRIBUTES;
  zaxisP->atts.nelems = 0;

  cdiDefVarKeyInt(&zaxisP->keys, CDI_KEY_DATATYPE, CDI_DATATYPE_FLT64);
}

zaxis_t *
zaxisNewEntry(int id)
{
  zaxis_t *zaxisP = (zaxis_t *) Malloc(sizeof(zaxis_t));
  zaxis_init(zaxisP);

  if (id == CDI_UNDEFID) { zaxisP->self = reshPut(zaxisP, &zaxisOps); }
  else
  {
    zaxisP->self = id;
    reshReplace(id, zaxisP, &zaxisOps);
  }

  return zaxisP;
}

static void
zaxis_copy(zaxis_t *zaxisP2, zaxis_t *zaxisP1)
{
  int zaxisID2 = zaxisP2->self;
  memcpy(zaxisP2, zaxisP1, sizeof(zaxis_t));
  zaxisP2->self = zaxisID2;
  cdiInitKeys(&zaxisP2->keys);
  cdiCopyVarKeys(&zaxisP1->keys, &zaxisP2->keys);
}

unsigned
cdiZaxisCount(void)
{
  return reshCountType(&zaxisOps);
}

static int
zaxisCreate_(int zaxisType, int size, int id)
{
  zaxis_t *zaxisP = zaxisNewEntry(id);

  xassert(size >= 0);
  zaxisP->type = zaxisType;
  zaxisP->size = size;

  int zaxisID = zaxisP->self;

  if (zaxisType >= 0 && zaxisType < CDI_NumZaxistype)
  {
    const struct ZaxisParam *zaxisParam = &ZaxistypeEntry[zaxisType];
    cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_NAME, zaxisParam->name);
    if (zaxisType != ZAXIS_GENERIC) cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_LONGNAME, zaxisParam->longname);
    if (*zaxisParam->units) cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_UNITS, zaxisParam->units);

    const char *stdname = zaxisParam->stdname;
    if (*stdname) cdiDefVarKeyBytes(&zaxisP->keys, CDI_KEY_STDNAME, (const unsigned char *) stdname, (int) strlen(stdname) + 1);

    zaxisP->positive = zaxisParam->positive;
  }
  else
  {
    Error("Internal problem! zaxistype=%d out of range (min=0/max=%d)!", zaxisType, CDI_NumZaxistype - 1);
  }

  return zaxisID;
}

/*
@Function  zaxisCreate
@Title     Create a vertical Z-axis

@Prototype int zaxisCreate(int zaxistype, int size)
@Parameter
    @Item  zaxistype  The type of the Z-axis, one of the set of predefined CDI Z-axis types.
                      The valid CDI Z-axis types are @func{ZAXIS_GENERIC}, @func{ZAXIS_SURFACE},
                      @func{ZAXIS_HYBRID}, @func{ZAXIS_SIGMA}, @func{ZAXIS_PRESSURE}, @func{ZAXIS_HEIGHT},
                      @func{ZAXIS_ISENTROPIC}, @func{ZAXIS_ALTITUDE}, @func{ZAXIS_MEANSEA}, @func{ZAXIS_TOA},
                      @func{ZAXIS_SEA_BOTTOM}, @func{ZAXIS_ATMOSPHERE}, @func{ZAXIS_CLOUD_BASE},
                      @func{ZAXIS_CLOUD_TOP}, @func{ZAXIS_ISOTHERM_ZERO}, @func{ZAXIS_SNOW},
                      @func{ZAXIS_LAKE_BOTTOM}, @func{ZAXIS_SEDIMENT_BOTTOM}, @func{ZAXIS_SEDIMENT_BOTTOM_TA},
                      @func{ZAXIS_SEDIMENT_BOTTOM_TW}, @func{ZAXIS_MIX_LAYER},
                      @func{ZAXIS_DEPTH_BELOW_SEA} and @func{ZAXIS_DEPTH_BELOW_LAND}.
    @Item  size       Number of levels.

@Description
The function @func{zaxisCreate} creates a vertical Z-axis.

@Result
@func{zaxisCreate} returns an identifier to the Z-axis.

@Example
Here is an example using @func{zaxisCreate} to create a pressure level Z-axis:

@Source
#include "cdi.h"
   ...
#define  nlev    5
   ...
double levs[nlev] = {101300, 92500, 85000, 50000, 20000};
int zaxisID;
   ...
zaxisID = zaxisCreate(ZAXIS_PRESSURE, nlev);
zaxisDefLevels(zaxisID, levs);
   ...
@EndSource
@EndFunction
*/
int
zaxisCreate(int zaxistype, int size)
{
  if (CDI_Debug) Message("zaxistype: %d size: %d ", zaxistype, size);

  xassert(size);

  return zaxisCreate_(zaxistype, size, CDI_UNDEFID);
}

static void
zaxisDestroyKernel(zaxis_t *zaxisP)
{
  xassert(zaxisP);

  Free(zaxisP->vals);
#ifndef USE_MPI
  if (zaxisP->cvals)
  {
    for (int i = 0; i < zaxisP->size; i++) Free(zaxisP->cvals[i]);
    Free((void *) zaxisP->cvals);
  }
#endif
  Free(zaxisP->lbounds);
  Free(zaxisP->ubounds);
  Free(zaxisP->weights);
  Free(zaxisP->vct);

  int zaxisID = zaxisP->self;
  cdiDeleteKeys(zaxisID, CDI_GLOBAL);
  cdiDeleteAtts(zaxisID, CDI_GLOBAL);

  Free(zaxisP);
}

/*
@Function  zaxisDestroy
@Title     Destroy a vertical Z-axis

@Prototype void zaxisDestroy(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}.

@EndFunction
*/
void
zaxisDestroy(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  zaxisDestroyKernel(zaxisP);
  reshRemove(zaxisID, &zaxisOps);
}

static void
zaxis_destroy(void *zaxisP)
{
  zaxisDestroyKernel((zaxis_t *) zaxisP);
}

const char *
zaxisNamePtr(int zaxisType)
{
  const char *name = (zaxisType >= 0 && zaxisType < CDI_NumZaxistype) ? ZaxistypeEntry[zaxisType].longname
                                                                      : ZaxistypeEntry[ZAXIS_GENERIC].longname;
  return name;
}

// obsolete function
void
zaxisDefLtype(int zaxisID, int ltype)
{
  static bool printInfo = true;
  if (printInfo) printInfo = cdiObsoleteInfo(__func__, "cdiDefKeyInt");

  (void) cdiDefKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_TYPEOFFIRSTFIXEDSURFACE, ltype);
}

/*
@Function  zaxisDefName
@Title     Define the name of a Z-axis

@Prototype void zaxisDefName(int zaxisID, const char *name)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  name     Name of the Z-axis.

@Description
The function @func{zaxisDefName} defines the name of a Z-axis.

@EndFunction
*/
void
zaxisDefName(int zaxisID, const char *name)
{
  (void) cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_NAME, name);
}

/*
@Function  zaxisDefLongname
@Title     Define the longname of a Z-axis

@Prototype void zaxisDefLongname(int zaxisID, const char *longname)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  longname Longname of the Z-axis.

@Description
The function @func{zaxisDefLongname} defines the longname of a Z-axis.

@EndFunction
*/
void
zaxisDefLongname(int zaxisID, const char *longname)
{
  (void) cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_LONGNAME, longname);
}

/*
@Function  zaxisDefUnits
@Title     Define the units of a Z-axis

@Prototype void zaxisDefUnits(int zaxisID, const char *units)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  units    Units of the Z-axis.

@Description
The function @func{zaxisDefUnits} defines the units of a Z-axis.

@EndFunction
*/
void
zaxisDefUnits(int zaxisID, const char *units)
{
  (void) cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_UNITS, units);
}

/*
@Function  zaxisInqName
@Title     Get the name of a Z-axis

@Prototype void zaxisInqName(int zaxisID, char *name)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.
    @Item  name     Name of the Z-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{zaxisInqName} returns the name of a Z-axis.

@Result
@func{zaxisInqName} returns the name of the Z-axis to the parameter name.

@EndFunction
*/
void
zaxisInqName(int zaxisID, char *name)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_NAME, name, &length);
}

const char *
zaxisInqNamePtr(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return cdiInqVarKeyString(&zaxisP->keys, CDI_KEY_NAME);
}

/*
@Function  zaxisInqLongname
@Title     Get the longname of a Z-axis

@Prototype void zaxisInqLongname(int zaxisID, char *longname)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.
    @Item  longname Longname of the Z-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{zaxisInqLongname} returns the longname of a Z-axis.

@Result
@func{zaxisInqLongname} returns the longname of the Z-axis to the parameter longname.

@EndFunction
*/
void
zaxisInqLongname(int zaxisID, char *longname)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_LONGNAME, longname, &length);
}

/*
@Function  zaxisInqUnits
@Title     Get the units of a Z-axis

@Prototype void zaxisInqUnits(int zaxisID, char *units)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.
    @Item  units    Units of the Z-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{zaxisInqUnits} returns the units of a Z-axis.

@Result
@func{zaxisInqUnits} returns the units of the Z-axis to the parameter units.

@EndFunction
*/
void
zaxisInqUnits(int zaxisID, char *units)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_UNITS, units, &length);
}

void
zaxisInqStdname(int zaxisID, char *stdname)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_STDNAME, stdname, &length);
}

void
zaxisDefDatatype(int zaxisID, int datatype)
{
  cdiDefKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_DATATYPE, datatype);
}

int
zaxisInqDatatype(int zaxisID)
{
  int datatype = 0;
  cdiInqKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);
  return datatype;
}

void
zaxisDefPositive(int zaxisID, int positive)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  if (zaxisP->positive != (unsigned) positive)
  {
    zaxisP->positive = (unsigned) positive;
    reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
  }
}

int
zaxisInqPositive(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return (int) zaxisP->positive;
}

void
zaxisDefScalar(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  zaxisP->scalar = 1;
  reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
}

int
zaxisInqScalar(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return zaxisP->scalar;
}

/*
@Function  zaxisDefLevels
@Title     Define the levels of a Z-axis

@Prototype void zaxisDefLevels(int zaxisID, const double *levels)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  levels   All levels of the Z-axis.

@Description
The function @func{zaxisDefLevels} defines the levels of a Z-axis.

@EndFunction
*/
void
zaxisDefLevels(int zaxisID, const double *levels)
{
  if (levels)
  {
    zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
    size_t size = (size_t) zaxisP->size;
    xassert(size);

    if (zaxisP->vals == NULL && size) { zaxisP->vals = (double *) Malloc(size * sizeof(double)); }
    memcpy(zaxisP->vals, levels, size * sizeof(double));

    reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
  }
}

void
zaxisDefCvals(int zaxisID, const char **cvals, int clen)
{
#ifndef USE_MPI
  if (cvals && clen)
  {
    zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
    size_t size = (size_t) zaxisP->size;
    xassert(size);

    zaxisP->clength = clen;
    if (size) { zaxisP->cvals = (char **) Malloc(size * sizeof(char *)); }

    for (size_t ilev = 0; ilev < size; ++ilev)
    {
      zaxisP->cvals[ilev] = (char *) Malloc(clen);
      memcpy(zaxisP->cvals[ilev], cvals[ilev], (size_t) clen);
    }
    reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
  }
#else
  (void) zaxisID;
  (void) cvals;
  (void) clen;
  Error("This function was disabled!");
#endif
}

/*
@Function  zaxisDefLevel
@Title     Define one level of a Z-axis

@Prototype void zaxisDefLevel(int zaxisID, int levelID, double level)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  levelID  Level identifier.
    @Item  level    Level.

@Description
The function @func{zaxisDefLevel} defines one level of a Z-axis.

@EndFunction
*/
void
zaxisDefLevel(int zaxisID, int levelID, double level)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  int size = zaxisP->size;
  xassert(size);
  xassert(levelID >= 0 && levelID < size);

  if (zaxisP->vals == NULL && size) { zaxisP->vals = (double *) Malloc((size_t) size * sizeof(double)); }
  if (levelID >= 0 && levelID < size) { zaxisP->vals[levelID] = level; }

  reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
}

void
zaxisDefNlevRef(int zaxisID, int nlev)
{
  cdiDefKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_NLEV, nlev);
}

int
zaxisInqNlevRef(int zaxisID)
{
  int nlev = 0;
  cdiInqKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_NLEV, &nlev);
  return nlev;
}

/*
@Function  zaxisDefNumber
@Title     Define the reference number for a generalized Z-axis

@Prototype void zaxisDefNumber(int zaxisID, int number)
@Parameter
    @Item  zaxisID     Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  number      Reference number for a generalized Z-axis.

@Description
The function @func{zaxisDefNumber} defines the reference number for a generalized Z-axis.

@EndFunction
*/
void
zaxisDefNumber(int zaxisID, int number)
{
  cdiDefKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_NUMBEROFVGRIDUSED, number);
}

/*
@Function  zaxisInqNumber
@Title     Get the reference number to a generalized Z-axis

@Prototype int zaxisInqNumber(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.

@Description
The function @func{zaxisInqNumber} returns the reference number to a generalized Z-axis.

@Result
@func{zaxisInqNumber} returns the reference number to a generalized Z-axis.
@EndFunction
*/
int
zaxisInqNumber(int zaxisID)
{
  int referenceNumber = 0;
  cdiInqKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_NUMBEROFVGRIDUSED, &referenceNumber);
  return referenceNumber;
}

/*
@Function  zaxisDefUUID
@Title     Define the UUID for a genralized Z-axis

@Prototype void zaxisDefUUID(int zaxisID, const char *uuid)
@Parameter
    @Item  zaxisID     Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  uuid        UUID for a generalized Z-axis.

@Description
The function @func{zaxisDefUUID} defines the UUID for a generalized  Z-axis.

@EndFunction
*/
void
zaxisDefUUID(int zaxisID, const unsigned char uuid[CDI_UUID_SIZE])
{
  cdiDefKeyBytes(zaxisID, CDI_GLOBAL, CDI_KEY_UUID, uuid, CDI_UUID_SIZE);

  reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
}

/*
@Function  zaxisInqUUID
@Title     Get the uuid to a generalized Z-axis

@Prototype void zaxisInqUUID(int zaxisID, char *uuid)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.
    @Item uuid A user supplied buffer of at least 16 bytes.

@Description
The function @func{zaxisInqUUID} returns the UUID to a generalized Z-axis.

@Result
@func{zaxisInqUUID} returns the UUID to a generalized Z-axis to the parameter uuid.
@EndFunction
*/
void
zaxisInqUUID(int zaxisID, unsigned char uuid[CDI_UUID_SIZE])
{
  memset(uuid, 0, CDI_UUID_SIZE);
  int length = CDI_UUID_SIZE;
  cdiInqKeyBytes(zaxisID, CDI_GLOBAL, CDI_KEY_UUID, uuid, &length);
}

/*
@Function  zaxisInqLevel
@Title     Get one level of a Z-axis

@Prototype double zaxisInqLevel(int zaxisID, int levelID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.
    @Item  levelID  Level index (range: 0 to nlevel-1).

@Description
The function @func{zaxisInqLevel} returns one level of a Z-axis.

@Result
@func{zaxisInqLevel} returns the level of a Z-axis.
@EndFunction
*/
double
zaxisInqLevel(int zaxisID, int levelID)
{
  double level = 0;
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->vals && levelID >= 0 && levelID < zaxisP->size) level = zaxisP->vals[levelID];

  return level;
}

double
zaxisInqLbound(int zaxisID, int levelID)
{
  double level = 0;
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->lbounds && levelID >= 0 && levelID < zaxisP->size) level = zaxisP->lbounds[levelID];

  return level;
}

double
zaxisInqUbound(int zaxisID, int levelID)
{
  double level = 0;
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->ubounds && levelID >= 0 && levelID < zaxisP->size) level = zaxisP->ubounds[levelID];

  return level;
}

const double *
zaxisInqLevelsPtr(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return zaxisP->vals;
}

#ifndef USE_MPI
char **
zaxisInqCValsPtr(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return zaxisP->cvals;
}
#endif

/*
@Function  zaxisInqLevels
@Title     Get all levels of a Z-axis

@Prototype void zaxisInqLevels(int zaxisID, double *levels)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.
    @Item  levels   Pointer to the location into which the levels are read.
                    The caller must allocate space for the returned values.

@Description
The function @func{zaxisInqLevels} returns all levels of a Z-axis.

@Result
@func{zaxisInqLevels} saves all levels to the parameter @func{levels}.
@EndFunction
*/
int
zaxisInqLevels(int zaxisID, double *levels)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  int size = 0;
  if (zaxisP->vals)
  {
    size = zaxisP->size;
    if (levels) memcpy(levels, zaxisP->vals, size * sizeof(double));
  }

  return size;
}

int
zaxisInqCLen(int zaxisID)
{
  int clen = 0;
#ifndef USE_MPI
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->cvals && zaxisP->clength) clen = zaxisP->clength;
#else
  (void) zaxisID;
#endif

  return clen;
}

int
zaxisInqCVals(int zaxisID, char ***clevels)
{
  int size = 0;
#ifndef USE_MPI
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->cvals)
  {
    size = zaxisP->size;
    size_t clen = zaxisP->clength;
    if (size && clen)
    {
      (*clevels) = (char **) Malloc(size * sizeof(char *));
      for (int i = 0; i < size; i++)
      {
        (*clevels)[i] = (char *) Malloc(clen * sizeof(char));
        memcpy((*clevels)[i], zaxisP->cvals[i], clen * sizeof(char));
      }
    }
  }
#else
  (void) zaxisID;
  (void) clevels;
#endif

  return size;
}

int
zaxisInqLbounds(int zaxisID, double *lbounds)
{
  int size = 0;
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->lbounds)
  {
    size = zaxisP->size;
    if (lbounds) memcpy(lbounds, zaxisP->lbounds, size * sizeof(double));
  }

  return size;
}

int
zaxisInqUbounds(int zaxisID, double *ubounds)
{
  int size = 0;
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->ubounds)
  {
    size = zaxisP->size;
    if (ubounds) memcpy(ubounds, zaxisP->ubounds, size * sizeof(double));
  }

  return size;
}

int
zaxisInqWeights(int zaxisID, double *weights)
{
  int size = 0;
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->weights)
  {
    size = zaxisP->size;
    if (weights) memcpy(weights, zaxisP->weights, size * sizeof(double));
  }

  return size;
}

int
zaxisInqLevelID(int zaxisID, double level)
{
  int levelID = CDI_UNDEFID;
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  if (zaxisP->vals)
  {
    int size = zaxisP->size;
    for (int i = 0; i < size; i++)
      if (fabs(level - zaxisP->vals[i]) < DBL_EPSILON)
      {
        levelID = i;
        break;
      }
  }

  return levelID;
}

/*
@Function  zaxisInqType
@Title     Get the type of a Z-axis

@Prototype int zaxisInqType(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.

@Description
The function @func{zaxisInqType} returns the type of a Z-axis.

@Result
@func{zaxisInqType} returns the type of the Z-axis,
one of the set of predefined CDI Z-axis types.
The valid CDI Z-axis types are @func{ZAXIS_GENERIC}, @func{ZAXIS_SURFACE},
@func{ZAXIS_HYBRID}, @func{ZAXIS_SIGMA}, @func{ZAXIS_PRESSURE}, @func{ZAXIS_HEIGHT},
@func{ZAXIS_ISENTROPIC}, @func{ZAXIS_ALTITUDE}, @func{ZAXIS_MEANSEA}, @func{ZAXIS_TOA},
@func{ZAXIS_SEA_BOTTOM}, @func{ZAXIS_ATMOSPHERE}, @func{ZAXIS_CLOUD_BASE},
@func{ZAXIS_CLOUD_TOP}, @func{ZAXIS_ISOTHERM_ZERO}, @func{ZAXIS_SNOW},
@func{ZAXIS_LAKE_BOTTOM}, @func{ZAXIS_SEDIMENT_BOTTOM}, @func{ZAXIS_SEDIMENT_BOTTOM_TA},
@func{ZAXIS_SEDIMENT_BOTTOM_TW}, @func{ZAXIS_MIX_LAYER},
@func{ZAXIS_DEPTH_BELOW_SEA} and @func{ZAXIS_DEPTH_BELOW_LAND}.

@EndFunction
*/
int
zaxisInqType(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return zaxisP->type;
}

/*
@Function  zaxisInqSize
@Title     Get the size of a Z-axis

@Prototype int zaxisInqSize(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate} or @fref{vlistInqVarZaxis}.

@Description
The function @func{zaxisInqSize} returns the size of a Z-axis.

@Result
@func{zaxisInqSize} returns the number of levels of a Z-axis.

@EndFunction
*/
int
zaxisInqSize(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return zaxisP->size;
}

void
cdiCheckZaxis(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  if (zaxisP->type == ZAXIS_GENERIC && zaxisP->vals)
  {
    int size = zaxisP->size;
    if (size > 1)
    {
      /* check direction */
      if (!zaxisP->direction)
      {
        int ups = 0, downs = 0;
        for (int i = 1; i < size; i++)
        {
          ups += (zaxisP->vals[i] > zaxisP->vals[i - 1]);
          downs += (zaxisP->vals[i] < zaxisP->vals[i - 1]);
        }
        if (ups == size - 1) { zaxisP->direction = LevelUp; }
        else if (downs == size - 1) { zaxisP->direction = LevelDown; }
        else /* !zaxisP->direction */
        {
          Warning("Direction undefined for zaxisID %d", zaxisID);
        }
      }
    }
  }
}

void
zaxisDefVct(int zaxisID, int size, const double *vct)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  if (zaxisP->vct == NULL || zaxisP->vctsize != size)
  {
    zaxisP->vctsize = size;
    zaxisP->vct = (double *) Realloc(zaxisP->vct, (size_t) size * sizeof(double));
  }

  if (vct) memcpy(zaxisP->vct, vct, (size_t) size * sizeof(double));
  reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
}

void
zaxisInqVct(int zaxisID, double *vct)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  memcpy(vct, zaxisP->vct, (size_t) zaxisP->vctsize * sizeof(double));
}

int
zaxisInqVctSize(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return zaxisP->vctsize;
}

const double *
zaxisInqVctPtr(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  return zaxisP->vct;
}

void
zaxisDefLbounds(int zaxisID, const double *lbounds)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  size_t size = (size_t) zaxisP->size;

  if (CDI_Debug)
    if (zaxisP->lbounds) Warning("Lower bounds already defined for zaxisID = %d", zaxisID);

  if (zaxisP->lbounds == NULL) zaxisP->lbounds = (double *) Malloc(size * sizeof(double));

  if (lbounds) memcpy(zaxisP->lbounds, lbounds, size * sizeof(double));
  reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
}

void
zaxisDefUbounds(int zaxisID, const double *ubounds)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  size_t size = (size_t) zaxisP->size;

  if (CDI_Debug)
    if (zaxisP->ubounds) Warning("Upper bounds already defined for zaxisID = %d", zaxisID);

  if (zaxisP->ubounds == NULL) zaxisP->ubounds = (double *) Malloc(size * sizeof(double));

  if (ubounds) memcpy(zaxisP->ubounds, ubounds, size * sizeof(double));
  reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
}

void
zaxisDefWeights(int zaxisID, const double *weights)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  size_t size = (size_t) zaxisP->size;

  if (CDI_Debug)
    if (zaxisP->weights != NULL) Warning("Weights already defined for zaxisID = %d", zaxisID);

  if (zaxisP->weights == NULL) zaxisP->weights = (double *) Malloc(size * sizeof(double));

  memcpy(zaxisP->weights, weights, size * sizeof(double));
  reshSetStatus(zaxisID, &zaxisOps, RESH_DESYNC_IN_USE);
}

void
zaxisChangeType(int zaxisID, int zaxistype)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);
  zaxisP->type = zaxistype;
}

void
zaxisResize(int zaxisID, int size)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  xassert(size >= 0);

  zaxisP->size = size;

  if (zaxisP->vals) zaxisP->vals = (double *) Realloc(zaxisP->vals, (size_t) size * sizeof(double));
}

static inline void
zaxisCopyKeyStr(zaxis_t *zaxisP1, zaxis_t *zaxisP2, int key)
{
  const cdi_key_t *keyp = find_key(&zaxisP1->keys, key);
  if (keyp && keyp->type == KeyBytes) cdiDefVarKeyBytes(&zaxisP2->keys, key, (const unsigned char *) keyp->v.s, (int) keyp->length);
}

static inline double *
copy_double_array(const double *src, size_t count)
{
  double *dst = (double *) Malloc(count * sizeof(double));
  memcpy(dst, src, count * sizeof(double));
  return dst;
}

int
zaxisDuplicate(int zaxisID)
{
  zaxis_t *zaxisP = zaxis_to_pointer(zaxisID);

  int zaxisIDnew = zaxisCreate(zaxisP->type, zaxisP->size);
  zaxis_t *zaxisPnew = zaxis_to_pointer(zaxisIDnew);

  zaxis_copy(zaxisPnew, zaxisP);

  zaxisCopyKeyStr(zaxisP, zaxisPnew, CDI_KEY_NAME);
  zaxisCopyKeyStr(zaxisP, zaxisPnew, CDI_KEY_LONGNAME);
  zaxisCopyKeyStr(zaxisP, zaxisPnew, CDI_KEY_UNITS);

  if (zaxisP->vals) { zaxisPnew->vals = copy_double_array(zaxisP->vals, (size_t) zaxisP->size); }
  if (zaxisP->lbounds) { zaxisPnew->lbounds = copy_double_array(zaxisP->lbounds, (size_t) zaxisP->size); }
  if (zaxisP->ubounds) zaxisPnew->ubounds = copy_double_array(zaxisP->ubounds, (size_t) zaxisP->size);
  if (zaxisP->vct && zaxisP->vctsize)
  {
    zaxisPnew->vctsize = zaxisP->vctsize;
    zaxisPnew->vct = copy_double_array(zaxisP->vct, (size_t) zaxisP->vctsize);
  }

  zaxisPnew->atts.nelems = 0;
  cdiCopyAtts(zaxisID, CDI_GLOBAL, zaxisIDnew, CDI_GLOBAL);

  return zaxisIDnew;
}

static void
zaxisPrintKernel(zaxis_t *zaxisP, FILE *fp)
{
  xassert(zaxisP);

  int zaxisID = zaxisP->self;
  int datatype = CDI_UNDEFID;
  cdiInqKeyInt(zaxisID, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);

  int type = zaxisP->type;
  int nlevels = zaxisP->size;

  int dig = (datatype == CDI_DATATYPE_FLT64) ? 15 : 7;

  fprintf(fp, "zaxistype = %s\n", zaxisNamePtr(type));
  fprintf(fp, "size      = %d\n", nlevels);
  if (nlevels == 1)
  {
    const bool zscalar = (bool) zaxisP->scalar;
    if (zscalar) fprintf(fp, "scalar    = true\n");
  }

  const char *string = cdiInqVarKeyString(&zaxisP->keys, CDI_KEY_NAME);
  if (string[0]) fprintf(fp, "name      = %s\n", string);
  string = cdiInqVarKeyString(&zaxisP->keys, CDI_KEY_LONGNAME);
  if (string[0]) fprintf(fp, "longname  = %s\n", string);
  string = cdiInqVarKeyString(&zaxisP->keys, CDI_KEY_UNITS);
  if (string[0]) fprintf(fp, "units     = %s\n", string);

  if (zaxisP->vals) { printDblsPrefixAutoBrk(fp, dig, "levels    = ", nlevels, zaxisP->vals); }
  if (zaxisP->lbounds) { printDblsPrefixAutoBrk(fp, dig, "lbounds   = ", nlevels, zaxisP->lbounds); }
  if (zaxisP->ubounds) { printDblsPrefixAutoBrk(fp, dig, "ubounds   = ", nlevels, zaxisP->ubounds); }

  if (type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF)
  {
    int vctsize = zaxisP->vctsize;
    const double *vct = zaxisP->vct;
    fprintf(fp, "vctsize   = %d\n", vctsize);
    if (vctsize)
    {
      int nbyte0 = fprintf(fp, "vct       = ");
      int nbyte = nbyte0;
      for (int i = 0; i < vctsize; i++)
      {
        if (nbyte > 70 || i == vctsize / 2)
        {
          fprintf(fp, "\n%*s", nbyte0, "");
          nbyte = nbyte0;
        }
        nbyte += fprintf(fp, "%.15g ", vct[i]);
      }
      fprintf(fp, "\n");
    }
  }
}

static void
zaxis_print(void *voidptr, FILE *fp)
{
  zaxis_t *zaxisP = (zaxis_t *) voidptr;

  xassert(zaxisP);

  zaxisPrintKernel(zaxisP, fp);
}

static int
zaxis_compare(zaxis_t *z1, zaxis_t *z2)
{
  enum
  {
    differ = 1
  };
  xassert(z1 && z2);

  if (z1->type != z2->type) return differ;
  if (z1->direction != z2->direction) return differ;
  if (z1->size != z2->size) return differ;
  if (z1->vctsize != z2->vctsize) return differ;
  if (z1->positive != z2->positive) return differ;
  if (cdiInqVarKeyInt(&z1->keys, CDI_KEY_DATATYPE) != cdiInqVarKeyInt(&z2->keys, CDI_KEY_DATATYPE)) return differ;
  if (cdiInqVarKeyInt(&z1->keys, CDI_KEY_TYPEOFFIRSTFIXEDSURFACE) != cdiInqVarKeyInt(&z2->keys, CDI_KEY_TYPEOFFIRSTFIXEDSURFACE))
    return differ;

  int diff = 0;

  int size = z1->size;
  int anyPresent = 0;
  int present = (z1->vals != NULL);
  diff |= (present ^ (z2->vals != NULL));
  anyPresent |= present;
  if (!diff && present)
  {
    const double *p = z1->vals, *q = z2->vals;
    for (int i = 0; i < size; i++) { diff |= is_not_equal(p[i], q[i]); }
  }

  present = (z1->lbounds != NULL);
  diff |= (present ^ (z2->lbounds != NULL));
  anyPresent |= present;
  if (!diff && present)
  {
    const double *p = z1->lbounds, *q = z2->lbounds;
    for (int i = 0; i < size; i++) { diff |= is_not_equal(p[i], q[i]); }
  }

  present = (z1->ubounds != NULL);
  diff |= (present ^ (z2->ubounds != NULL));
  anyPresent |= present;
  if (!diff && present)
  {
    const double *p = z1->ubounds, *q = z2->ubounds;
    for (int i = 0; i < size; ++i) { diff |= is_not_equal(p[i], q[i]); }
  }

  present = (z1->weights != NULL);
  diff |= (present ^ (z2->weights != NULL));
  anyPresent |= present;
  if (!diff && present)
  {
    const double *p = z1->weights, *q = z2->weights;
    for (int i = 0; i < size; ++i) { diff |= is_not_equal(p[i], q[i]); }
  }

  present = (z1->vct != NULL);
  diff |= (present ^ (z2->vct != NULL));
  if (!diff && present)
  {
    int vctsize = z1->vctsize;
    xassert(vctsize);
    const double *p = z1->vct, *q = z2->vct;
    for (int i = 0; i < vctsize; ++i) { diff |= is_not_equal(p[i], q[i]); }
  }

  if (anyPresent) xassert(size);

  diff |= strcmp(cdiInqVarKeyString(&z1->keys, CDI_KEY_NAME), cdiInqVarKeyString(&z2->keys, CDI_KEY_NAME))
          | strcmp(cdiInqVarKeyString(&z1->keys, CDI_KEY_LONGNAME), cdiInqVarKeyString(&z2->keys, CDI_KEY_LONGNAME))
          | strcmp(cdiInqVarKeyString(&z1->keys, CDI_KEY_STDNAME), cdiInqVarKeyString(&z2->keys, CDI_KEY_STDNAME))
          | strcmp(cdiInqVarKeyString(&z1->keys, CDI_KEY_UNITS), cdiInqVarKeyString(&z2->keys, CDI_KEY_UNITS));

  return diff != 0;
}

void
cdiZaxisGetIndexList(unsigned nzaxis, int *zaxisResHs)
{
  reshGetResHListOfType(nzaxis, zaxisResHs, &zaxisOps);
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
