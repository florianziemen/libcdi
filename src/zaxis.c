#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>
#include <math.h>
#include <float.h>

#include "dmemory.h"

#include "cdi.h"
#include "stream_int.h"
#include "pio_util.h"
#include "resource_handle.h"
#include "pio_rpc.h"
#include "varscan.h"


static struct {
  char *name;
  char *longname;
  char *units;
}
ZaxistypeEntry[] = {
  {"sfc",     "surface",                  ""},
  {"lev",     "generic",                  "level"},
  {"lev",     "hybrid",                   "level"},
  {"lev",     "hybrid_half",              "level"},
  {"lev",     "pressure",                 "Pa"},
  {"height",  "height",                   "m"},
  {"depth",   "depth_below_sea",          "m"},
  {"depth",   "depth_below_land",         "cm"},
  {"lev",     "isentropic",               "K"},
  {"lev",     "trajectory",               ""},
  {"alt",     "altitude",                 "m"},
  {"lev",     "sigma",                    "level"},
  {"lev",     "meansea",                  "level"},
};


#define  LevelDown  0
#define  LevelUp    1

typedef struct {
  char     name[256];
  char     longname[256];
  char     units[256];
  double  *vals;
  double  *lbounds;
  double  *ubounds;
  double  *weights;
  int      self;
  int      prec;
  int      type;
  int      ltype;    /* GRIB level type */
  int      size;
  int      direction;
  int      vctsize;
  double  *vct;
}
zaxis_t;

static int CDI_MaxZaxistype = sizeof(ZaxistypeEntry) / sizeof(ZaxistypeEntry[0]);

static int    zaxisCompareP ( void * zaxisptr1, void * zaxisptr2 );
static void   zaxisDestroyP ( void * zaxisptr );
static void   zaxisPrintP   ( void * zaxisptr, FILE * fp );
#ifdef USE_MPI
static int    zaxisGetSizeP ( void * zaxisptr, MPI_Comm comm );
static void   zaxisPackP    ( void * zaxisptr, void * buffer, int size,
                              int *pos, MPI_Comm comm);
static int    zaxisTxCode   ( void );
#endif

resOps zaxisOps = { zaxisCompareP, zaxisDestroyP, zaxisPrintP
#ifdef USE_MPI
                    , zaxisGetSizeP, zaxisPackP, zaxisTxCode
#endif
};

static int  ZAXIS_Debug = 0;   /* If set to 1, debugging */


static
void zaxisDefaultValue ( zaxis_t *zaxisptr )
{
  zaxisptr->self        = CDI_UNDEFID;
  zaxisptr->name[0]     = 0;
  zaxisptr->longname[0] = 0;
  zaxisptr->units[0]    = 0;
  zaxisptr->vals        = NULL;
  zaxisptr->ubounds     = NULL;
  zaxisptr->lbounds     = NULL;
  zaxisptr->weights     = NULL;
  zaxisptr->type        = CDI_UNDEFID;
  zaxisptr->ltype       = 0;
  zaxisptr->direction   = CDI_UNDEFID;
  zaxisptr->prec        = 0;
  zaxisptr->size        = 0;
  zaxisptr->vctsize     = 0;
  zaxisptr->vct         = NULL;
}


static
zaxis_t *zaxisNewEntry(void)
{
  zaxis_t *zaxisptr;

  zaxisptr = (zaxis_t *) xmalloc(sizeof(zaxis_t));

  zaxisDefaultValue ( zaxisptr );

  zaxisptr->self = reshPut (( void * ) zaxisptr, &zaxisOps );

  return (zaxisptr);
}


static
void zaxisInit (void)
{
  static int zaxisInitialized = 0;
  char *env;

  if ( zaxisInitialized ) return;

  zaxisInitialized = 1;

  env = getenv("ZAXIS_DEBUG");
  if ( env ) ZAXIS_Debug = atoi(env);
}


static
void zaxis_copy(zaxis_t *zaxisptr2, zaxis_t *zaxisptr1)
{
  int zaxisID2;

  zaxisID2 = zaxisptr2->self;
  memcpy(zaxisptr2, zaxisptr1, sizeof(zaxis_t));
  zaxisptr2->self = zaxisID2;
}

static
void zaxisCheckPtr(const char *caller, int zaxisID, zaxis_t *zaxisptr)
{
  if ( zaxisptr == NULL )
    Errorc("zaxis %d undefined!", zaxisID);
}

#define  zaxis_check_ptr(zaxisID, zaxisptr)  zaxisCheckPtr(__func__, zaxisID, zaxisptr)

int zaxisSize(void)
{
  return reshCountType ( &zaxisOps );
}


/*
@Function  zaxisCreate
@Title     Create a vertical Z-axis

@Prototype int zaxisCreate(int zaxistype, int size)
@Parameter
    @Item  zaxistype  The type of the Z-axis, one of the set of predefined CDI Z-axis types.
                      The valid CDI Z-axis types are @func{ZAXIS_GENERIC}, @func{ZAXIS_SURFACE},
                      @func{ZAXIS_HYBRID}, @func{ZAXIS_SIGMA}, @func{ZAXIS_PRESSURE}, @func{ZAXIS_HEIGHT},
                      @func{ZAXIS_DEPTH_BELOW_SEA} and @func{ZAXIS_DEPTH_BELOW_LAND}.
    @Item  size       Number of levels

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
int zaxisCreate(int zaxistype, int size)
{
  int ilev;
  int zaxisID;
  double *vals;
  zaxis_t *zaxisptr;

  if ( CDI_Debug )
    Message("zaxistype: %d size: %d ", zaxistype, size);

  zaxisInit ();

  zaxisptr = zaxisNewEntry();

  zaxisID = zaxisptr->self;

  zaxisptr->type = zaxistype;
  zaxisptr->size = size;

  if ( zaxistype > CDI_MaxZaxistype )
    Error("Internal problem! zaxistype > CDI_MaxZaxistype");

  zaxisDefName(zaxisID, ZaxistypeEntry[zaxistype].name);
  zaxisDefLongname(zaxisID, ZaxistypeEntry[zaxistype].longname);
  zaxisDefUnits(zaxisID, ZaxistypeEntry[zaxistype].units);

  vals = (double *) malloc(size*sizeof(double));

  for ( ilev = 0; ilev < size; ilev++ )
    vals[ilev] = 0.0;

  zaxisptr->vals = vals;

  return (zaxisID);
}


void zaxisDestroyKernel ( zaxis_t * zaxisptr )
{
  int id;

  assert ( zaxisptr );

  id = zaxisptr->self;

  if ( zaxisptr->vals ) free ( zaxisptr->vals );

  free ( zaxisptr );

  reshRemove ( id, &zaxisOps );
}

/*
@Function  zaxisDestroy
@Title     Destroy a vertical Z-axis

@Prototype void zaxisDestroy(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}

@EndFunction
*/
void zaxisDestroy(int zaxisID)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxisDestroyKernel ( zaxisptr );
}


static
void zaxisDestroyP ( void * zaxisptr )
{
  zaxisDestroyKernel (( zaxis_t * ) zaxisptr );
}


char *zaxisNamePtr(int zaxistype)
{
  char *name;

  if ( zaxistype >= 0 && zaxistype < CDI_MaxZaxistype )
    name = ZaxistypeEntry[zaxistype].longname;
  else
    name = ZaxistypeEntry[ZAXIS_GENERIC].longname;

  return (name);
}


void zaxisName(int zaxistype, char *zaxisname)
{
  strcpy(zaxisname, zaxisNamePtr(zaxistype));
}

/*
@Function  zaxisDefName
@Title     Define the name of a Z-axis

@Prototype void zaxisDefName(int zaxisID, const char *name)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  name     Name of the Z-axis

@Description
The function @func{zaxisDefName} defines the name of a Z-axis.

@EndFunction
*/
void zaxisDefName(int zaxisID, const char *name)
{
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( name )
    strcpy(zaxisptr->name, name);
}

/*
@Function  zaxisDefLongname
@Title     Define the longname of a Z-axis

@Prototype void zaxisDefLongname(int zaxisID, const char *longname)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  longname Longname of the Z-axis

@Description
The function @func{zaxisDefLongname} defines the longname of a Z-axis.

@EndFunction
*/
void zaxisDefLongname(int zaxisID, const char *longname)
{
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( longname )
    strcpy(zaxisptr->longname, longname);
}

/*
@Function  zaxisDefUnits
@Title     Define the units of a Z-axis

@Prototype void zaxisDefUnits(int zaxisID, const char *units)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  units    Units of the Z-axis

@Description
The function @func{zaxisDefUnits} defines the units of a Z-axis.

@EndFunction
*/
void zaxisDefUnits(int zaxisID, const char *units)
{
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( units )
    strcpy(zaxisptr->units, units);
}

/*
@Function  zaxisInqName
@Title     Get the name of a Z-axis

@Prototype void zaxisInqName(int zaxisID, char *name)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  name     Name of the Z-axis

@Description
The function @func{zaxisInqName} returns the name of a Z-axis.

@Result
@func{zaxisInqName} returns the name of the Z-axis to the parameter name.

@EndFunction
*/
void zaxisInqName(int zaxisID, char *name)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  strcpy(name, zaxisptr->name);
}

/*
@Function  zaxisInqLongname
@Title     Get the longname of a Z-axis

@Prototype void zaxisInqLongname(int zaxisID, char *longname)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  longname Longname of the Z-axis

@Description
The function @func{zaxisInqLongname} returns the longname of a Z-axis.

@Result
@func{zaxisInqLongname} returns the longname of the Z-axis to the parameter longname.

@EndFunction
*/
void zaxisInqLongname(int zaxisID, char *longname)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  strcpy(longname, zaxisptr->longname);
}

/*
@Function  zaxisInqUnits
@Title     Get the units of a Z-axis

@Prototype void zaxisInqUnits(int zaxisID, char *units)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  units    Units of the Z-axis

@Description
The function @func{zaxisInqUnits} returns the units of a Z-axis.

@Result
@func{zaxisInqUnits} returns the units of the Z-axis to the parameter units.

@EndFunction
*/
void zaxisInqUnits(int zaxisID, char *units)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  strcpy(units, zaxisptr->units);
}


void zaxisDefPrec(int zaxisID, int prec)
{
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  zaxisptr->prec = prec;
}


int zaxisInqPrec(int zaxisID)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  return (zaxisptr->prec);
}


void zaxisDefLtype(int zaxisID, int ltype)
{
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  zaxisptr->ltype = ltype;
}


int zaxisInqLtype(int zaxisID)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  return (zaxisptr->ltype);
}

/*
@Function  zaxisDefLevels
@Title     Define the levels of a Z-axis

@Prototype void zaxisDefLevels(int zaxisID, const double *levels)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  levels   All levels of the Z-axis

@Description
The function @func{zaxisDefLevels} defines the levels of a Z-axis.

@EndFunction
*/
void zaxisDefLevels(int zaxisID, const double *levels)
{
  int ilev;
  int size;
  double *vals;
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  size = zaxisptr->size;

  vals = zaxisptr->vals;

  for ( ilev = 0; ilev < size; ilev++ )
    vals[ilev] = levels[ilev];
}

/*
@Function  zaxisDefLevel
@Title     Define one level of a Z-axis

@Prototype void zaxisDefLevel(int zaxisID, int levelID, double level)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  levelID  Level identifier
    @Item  level    Level

@Description
The function @func{zaxisDefLevel} defines one level of a Z-axis.

@EndFunction
*/
void zaxisDefLevel(int zaxisID, int levelID, double level)
{
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( levelID >= 0 && levelID < zaxisptr->size )
    zaxisptr->vals[levelID] = level;
}

/*
@Function  zaxisInqLevel
@Title     Get one level of a Z-axis

@Prototype double zaxisInqLevel(int zaxisID, int levelID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  levelID  Level index (range: 0 to nlevel-1)

@Description
The function @func{zaxisInqLevel} returns one level of a Z-axis.

@Result
@func{zaxisInqLevel} returns the level of a Z-axis.
@EndFunction
*/
double zaxisInqLevel(int zaxisID, int levelID)
{
  double level = 0;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( levelID >= 0 && levelID < zaxisptr->size )
    level = zaxisptr->vals[levelID];

  return (level);
}


double zaxisInqLbound(int zaxisID, int index)
{
  double level = 0;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( zaxisptr->lbounds )
    if ( index >= 0 && index < zaxisptr->size )
      level = zaxisptr->lbounds[index];

  return (level);
}


double zaxisInqUbound(int zaxisID, int index)
{
  double level = 0;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( zaxisptr->ubounds )
    if ( index >= 0 && index < zaxisptr->size )
      level = zaxisptr->ubounds[index];

  return (level);
}


const double *zaxisInqLevelsPtr(int zaxisID)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  return ( zaxisptr->vals );
}

/*
@Function  zaxisInqLevels
@Title     Get all levels of a Z-axis

@Prototype void zaxisInqLevels(int zaxisID, double *levels)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  levels   Levels of the Z-axis

@Description
The function @func{zaxisInqLevels} returns all levels of a Z-axis.

@Result
@func{zaxisInqLevels} saves all levels to the parameter @func{levels}.
@EndFunction
*/
void zaxisInqLevels(int zaxisID, double *levels)
{
  int size;
  int i;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  size = zaxisptr->size;
  for ( i = 0; i < size; i++ )
    levels[i] =  zaxisptr->vals[i];
}


int zaxisInqLbounds(int zaxisID, double *lbounds)
{
  int size = 0;
  int i;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( zaxisptr->lbounds )
    {
      size = zaxisptr->size;

      if ( lbounds )
        for ( i = 0; i < size; i++ )
          lbounds[i] =  zaxisptr->lbounds[i];
    }

  return (size);
}


int zaxisInqUbounds(int zaxisID, double *ubounds)
{
  int size = 0;
  int i;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( zaxisptr->ubounds )
    {
      size = zaxisptr->size;

      if ( ubounds )
        for ( i = 0; i < size; i++ )
          ubounds[i] =  zaxisptr->ubounds[i];
    }

  return (size);
}


int zaxisInqWeights(int zaxisID, double *weights)
{
  int size = 0;
  int i;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( zaxisptr->weights )
    {
      size = zaxisptr->size;

      if ( weights )
        for ( i = 0; i < size; i++ )
          weights[i] =  zaxisptr->weights[i];
    }

  return (size);
}


int zaxisInqLevelID(int zaxisID, double level)
{
  int size;
  int levelID = CDI_UNDEFID;
  int i;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  size = zaxisptr->size;
  for ( i = 0; i < size; i++ )
    if ( fabs(level-zaxisptr->vals[i]) < DBL_EPSILON ) break;

  if ( i < size ) levelID = i;

  return (levelID);
}

/*
@Function  zaxisInqType
@Title     Get the type of a Z-axis

@Prototype int zaxisInqType(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}

@Description
The function @func{zaxisInqType} returns the type of a Z-axis.

@Result
@func{zaxisInqType} returns the type of the Z-axis,
one of the set of predefined CDI Z-axis types.
The valid CDI Z-axis types are @func{ZAXIS_GENERIC}, @func{ZAXIS_SURFACE},
@func{ZAXIS_HYBRID}, @func{ZAXIS_SIGMA}, @func{ZAXIS_PRESSURE}, @func{ZAXIS_HEIGHT},
@func{ZAXIS_DEPTH_BELOW_SEA} and @func{ZAXIS_DEPTH_BELOW_LAND}.

@EndFunction
*/
int zaxisInqType(int zaxisID)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  return (zaxisptr->type);
}

/*
@Function  zaxisInqSize
@Title     Get the size of a Z-axis

@Prototype int zaxisInqSize(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}

@Description
The function @func{zaxisInqSize} returns the size of a Z-axis.

@Result
@func{zaxisInqSize} returns the number of levels of a Z-axis.

@EndFunction
*/
int zaxisInqSize(int zaxisID)
{
  int size = 1;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );
  zaxis_check_ptr(zaxisID, zaxisptr);

  size = zaxisptr->size;

  return (size);
}


void cdiCheckZaxis(int zaxisID)
{
  int size, i, found;
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( zaxisInqType(zaxisID) == ZAXIS_GENERIC )
    {
      size = zaxisptr->size;
      if ( size > 1 )
        {
          /* check direction */
          if ( zaxisptr->direction == CDI_UNDEFID )
            {
              found = 0;
              for ( i = 1; i < size; i++ )
                if ( zaxisptr->vals[i] > zaxisptr->vals[i-1] )
                  found++;
              if ( found == size-1 )
                {
                  zaxisptr->direction = LevelUp;
                }
              else
                {
                  found = 0;
                  for ( i = 1; i < size; i++ )
                    if ( zaxisptr->vals[i] < zaxisptr->vals[i-1] )
                      found++;
                  if ( found == size-1 )
                    {
                      zaxisptr->direction = LevelDown;
                    }
                }
            }
          /* check consistent */
          if ( zaxisptr->direction == CDI_UNDEFID )
            {
              Warning("direction undefined for zaxisID %d", zaxisID);
            }
        }
    }
}


void zaxisDefVct(int zaxisID, int size, const double *vct)
{
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  if ( zaxisptr->vct == 0 )
    {
      zaxisptr->vctsize = size;
      zaxisptr->vct = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptr->vct, vct, size*sizeof(double));
    }
  else
    if ( zaxisptr->vctsize != size )
      Warning("VCT was already defined");
}


int zaxisInqVctSize(int zaxisID)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  return (zaxisptr->vctsize);
}


const double *zaxisInqVctPtr(int zaxisID)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  return (zaxisptr->vct);
}


void zaxisDefLbounds(int zaxisID, const double *lbounds)
{
  size_t size;
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  size = zaxisptr->size;

  if ( CDI_Debug )
    if ( zaxisptr->lbounds != NULL )
      Warning("Lower bounds already defined for zaxisID = %d", zaxisID);

  if ( zaxisptr->lbounds == NULL )
    zaxisptr->lbounds = (double *) malloc(size*sizeof(double));

  memcpy(zaxisptr->lbounds, lbounds, size*sizeof(double));
}


void zaxisDefUbounds(int zaxisID, const double *ubounds)
{
  size_t size;
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  size = zaxisptr->size;

  if ( CDI_Debug )
    if ( zaxisptr->ubounds != NULL )
      Warning("Upper bounds already defined for zaxisID = %d", zaxisID);

  if ( zaxisptr->ubounds == NULL )
    zaxisptr->ubounds = (double *) malloc(size*sizeof(double));

  memcpy(zaxisptr->ubounds, ubounds, size*sizeof(double));
}


void zaxisDefWeights(int zaxisID, const double *weights)
{
  size_t size;
  zaxis_t *zaxisptr;

  if ( reshGetStatus ( zaxisID, &zaxisOps ) == CLOSED )
    {
      xwarning ( "Operation not executed." );
      return;
    }

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  size = zaxisptr->size;

  if ( CDI_Debug )
    if ( zaxisptr->weights != NULL )
      Warning("Weights already defined for zaxisID = %d", zaxisID);

  if ( zaxisptr->weights == NULL )
    zaxisptr->weights = (double *) malloc(size*sizeof(double));

  memcpy(zaxisptr->weights, weights, size*sizeof(double));
}


void zaxisChangeType(int zaxisID, int zaxistype)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  zaxisptr->type = zaxistype;
}


void zaxisResize(int zaxisID, int size)
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  zaxisptr->size = size;

  if ( zaxisptr->vals )
    zaxisptr->vals = (double *) realloc(zaxisptr->vals, size*sizeof(double));
}


int zaxisDuplicate(int zaxisID)
{
  int zaxisIDnew;
  int zaxistype, zaxissize;
  int size;
  zaxis_t *zaxisptr, *zaxisptrnew;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxis_check_ptr(zaxisID, zaxisptr);

  zaxistype = zaxisInqType(zaxisID);
  zaxissize = zaxisInqSize(zaxisID);

  zaxisIDnew = zaxisCreate(zaxistype, zaxissize);
  zaxisptrnew = ( zaxis_t * ) reshGetVal ( zaxisIDnew, &zaxisOps );

  zaxis_copy(zaxisptrnew, zaxisptr);

  strcpy(zaxisptrnew->name, zaxisptr->name);
  strcpy(zaxisptrnew->longname, zaxisptr->longname);
  strcpy(zaxisptrnew->units, zaxisptr->units);

  if ( zaxisptr->vals != NULL )
    {
      size = zaxissize;

      zaxisptrnew->vals = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptrnew->vals, zaxisptr->vals, size*sizeof(double));
    }

  if ( zaxisptr->lbounds )
    {
      size = zaxissize;

      zaxisptrnew->lbounds = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptrnew->lbounds, zaxisptr->lbounds, size*sizeof(double));
    }

  if ( zaxisptr->ubounds )
    {
      size = zaxissize;

      zaxisptrnew->ubounds = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptrnew->ubounds, zaxisptr->ubounds, size*sizeof(double));
    }

  if ( zaxisptr->vct != NULL )
    {
      size = zaxisptr->vctsize;

      if ( size )
        {
          zaxisptrnew->vctsize = size;
          zaxisptrnew->vct = (double *) malloc(size*sizeof(double));
          memcpy(zaxisptrnew->vct, zaxisptr->vct, size*sizeof(double));
        }
    }

  return (zaxisIDnew);
}


void zaxisPrintKernel ( zaxis_t * zaxisptr, FILE * fp )
{
  int type;
  int nlevels, levelID;
  int nbyte0, nbyte;
  double level;

  assert ( zaxisptr );

  type    = zaxisptr->type;
  nlevels = zaxisptr->size;

  nbyte0 = 0;
  fprintf(fp, "#\n");
  fprintf(fp, "# zaxisID %d\n", zaxisptr->self );
  fprintf(fp, "#\n");
  fprintf(fp, "zaxistype = %s\n", zaxisNamePtr(type));
  fprintf(fp, "size      = %d\n", nlevels);
  if ( zaxisptr->name[0]     ) fprintf(fp, "name      = %s\n", zaxisptr->name);
  if ( zaxisptr->longname[0] ) fprintf(fp, "longname  = %s\n", zaxisptr->longname);
  if ( zaxisptr->units[0]    ) fprintf(fp, "units     = %s\n", zaxisptr->units);

  nbyte0 = fprintf(fp, "levels    = ");
  nbyte = nbyte0;
  for ( levelID = 0; levelID < nlevels; levelID++ )
    {
      if ( nbyte > 80 )
        {
          fprintf(fp, "\n");
          fprintf(fp, "%*s", nbyte0, "");
          nbyte = nbyte0;
        }
      level = zaxisInqLevel(zaxisptr->self, levelID);
      nbyte += fprintf(fp, "%.9g ", level);
    }
  fprintf(fp, "\n");

  if ( zaxisptr->lbounds && zaxisptr->ubounds )
    {
      double level1, level2;
      nbyte = nbyte0;
      nbyte0 = fprintf(fp, "bounds    = ");
      for ( levelID = 0; levelID < nlevels; levelID++ )
        {
          if ( nbyte > 80 )
            {
              fprintf(fp, "\n");
              fprintf(fp, "%*s", nbyte0, "");
              nbyte = nbyte0;
            }
          level1 = zaxisInqLbound(zaxisptr->self, levelID);
          level2 = zaxisInqUbound(zaxisptr->self, levelID);
          nbyte += fprintf(fp, "%.9g-%.9g ", level1, level2);
        }
      fprintf(fp, "\n");
    }

  if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
    {
      int i;
      int vctsize;
      const double *vct;

      vctsize = zaxisptr->vctsize;
      vct     = zaxisptr->vct;
      fprintf(fp, "vctsize   = %d\n", vctsize);
      if ( vctsize )
        {
          nbyte0 = fprintf(fp, "vct       = ");
          nbyte = nbyte0;
          for ( i = 0; i < vctsize; i++ )
            {
              if ( nbyte > 70 || i == vctsize/2 )
                {
                  fprintf(fp, "\n%*s", nbyte0, "");
                  nbyte = nbyte0;
                }
              nbyte += fprintf(fp, "%.9g ", vct[i]);
            }
          fprintf(fp, "\n");
          /*
          nbyte0 = fprintf(fp, "vct_b     = ");
          nbyte  = nbyte0;
          for ( i = 0; i < vctsize/2; i++ )
            {
              if ( nbyte > 70 )
                {
                  fprintf(fp, "\n%*s", nbyte0, "");
                  nbyte = nbyte0;
                }
              nbyte += fprintf(fp, "%.9g ", vct[vctsize/2+i]);
            }
          fprintf(fp, "\n");
          */
        }
    }
}


void zaxisPrint ( int zaxisID )
{
  zaxis_t *zaxisptr;

  zaxisptr = ( zaxis_t * ) reshGetVal ( zaxisID, &zaxisOps );

  zaxisPrintKernel ( zaxisptr, stdout );
}


static
void zaxisPrintP ( void * zaxisptr, FILE * fp )
{
  zaxisPrintKernel (( zaxis_t * ) zaxisptr, fp );
}


static
int  zaxisCompareP ( void * zaxisptr1, void * zaxisptr2 )
{
  zaxis_t * z1 = ( zaxis_t * ) zaxisptr1 ;
  zaxis_t * z2 = ( zaxis_t * ) zaxisptr2 ;
  static int differ = -1;
  static int equal  =  0;

  assert ( z1 );
  assert ( z2 );

  //zaxisptr->self        = CDI_UNDEFID;
  //zaxisptr->name[0]     = 0;
  //zaxisptr->longname[0] = 0;
  //zaxisptr->units[0]    = 0;
  //zaxisptr->vals        = NULL;
  //zaxisptr->ubounds     = NULL;
  //zaxisptr->lbounds     = NULL;
  //zaxisptr->weights     = NULL;
  if ( z1->type      != z2->type )      return differ;
  if ( z1->ltype     != z2->ltype )     return differ;
  if ( z1->direction != z2->direction ) return differ;
  if ( z1->prec      != z2->prec )      return differ;
  if ( z1->size      != z2->size )      return differ;
  if ( z1->vctsize   != z2->vctsize )   return differ;
  //zaxisptr->vct         = NULL;
  
  /*
    return zaxisCompare ( z1->self,
    z2->type, z2->size, z2->size > 0 ? 1 : 0,
    z2->vals, z2->longname, z2->units, z2->ltype );
  */
  return equal;
}


#ifdef USE_MPI
static int
zaxisTxCode ( void )
{
  return ZAXIS;
}

static
int zaxisGetSizeP ( void * zaxisptr, MPI_Comm comm )
{
  zaxis_t * zaxisP = ( zaxis_t * ) zaxisptr;
  int packBufferSize = 0, size;

  xmpi ( MPI_Pack_size ( 2, MPI_INT, comm, &size ));
  packBufferSize += size;

  xmpi ( MPI_Pack_size ( zaxisP->size, MPI_DOUBLE, comm, &size ));
  packBufferSize += size;

  return packBufferSize;
}


static
void zaxisPackP ( void * zaxisptr, void * packBuffer, int packBufferSize,
                  int * packBufferPos, MPI_Comm comm )
{
  zaxis_t * zaxisP = ( zaxis_t * ) zaxisptr;

  xmpi ( MPI_Pack ( &zaxisP->type, 1, MPI_INT, 
		    packBuffer, packBufferSize, packBufferPos, comm ));
  xmpi ( MPI_Pack ( &zaxisP->size, 1, MPI_INT, 
		    packBuffer, packBufferSize, packBufferPos, comm ));
  xmpi ( MPI_Pack ( zaxisP->vals, zaxisP->size, MPI_DOUBLE, 
		    packBuffer, packBufferSize, packBufferPos, comm ));
}

#endif

void zaxisGetIndexArray ( int nzaxis, int * zaxisIndexArray )
{
  reshGetKeysArrayOfType ( nzaxis, zaxisIndexArray, &zaxisOps );
}
