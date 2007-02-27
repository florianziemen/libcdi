#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>
#include <math.h>
#include <float.h>

#include "dmemory.h"

#include "cdi.h"
#include "stream_int.h"


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
  {"depth",   "depth below sea",          "m"},
  {"depth",   "depth below land",         "cm"},
  {"lev",     "isentropic",               "K"},
  {"lev",     "trajectory",               ""},
  {"alt",     "altitude",                 "m"},
  {"lev",     "sigma",                    "level"},
};

static int CDI_MaxZaxistype = sizeof(ZaxistypeEntry) / sizeof(ZaxistypeEntry[0]);


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
ZAXIS;

static int  ZAXIS_Debug = 0;   /* If set to 1, debugging */

static int _zaxis_max = MAX_ZAXIS;

static void zaxis_initialize(void);

static int _zaxis_init = FALSE;

#if  defined  (HAVE_LIBPTHREAD)
#  include <pthread.h>

static pthread_once_t _zaxis_init_thread = PTHREAD_ONCE_INIT;
static pthread_mutex_t _zaxis_mutex;

#  define ZAXIS_LOCK           pthread_mutex_lock(&_zaxis_mutex);
#  define ZAXIS_UNLOCK         pthread_mutex_unlock(&_zaxis_mutex);
#  define ZAXIS_INIT                               \
   if ( _zaxis_init == FALSE ) pthread_once(&_zaxis_init_thread, zaxis_initialize);

#else

#  define ZAXIS_LOCK
#  define ZAXIS_UNLOCK
#  define ZAXIS_INIT                               \
   if ( _zaxis_init == FALSE ) zaxis_initialize();

#endif


typedef struct _zaxisPtrToIdx {
  int idx;
  ZAXIS *ptr;
  struct _zaxisPtrToIdx *next;
} zaxisPtrToIdx;


static zaxisPtrToIdx *_zaxisList  = NULL;
static zaxisPtrToIdx *_zaxisAvail = NULL;


static void zaxis_list_new(void)
{
  static char func[] = "zaxis_list_new";

  assert(_zaxisList == NULL);

  _zaxisList = (zaxisPtrToIdx *) malloc(_zaxis_max*sizeof(zaxisPtrToIdx));
}


static void zaxis_list_delete(void)
{
  static char func[] = "zaxis_list_delete";

  if ( _zaxisList ) free(_zaxisList);
}


static void zaxis_init_pointer(void)
{
  int  i;
  
  for ( i = 0; i < _zaxis_max; i++ )
    {
      _zaxisList[i].next = _zaxisList + i + 1;
      _zaxisList[i].idx  = i;
      _zaxisList[i].ptr  = 0;
    }

  _zaxisList[_zaxis_max-1].next = 0;

  _zaxisAvail = _zaxisList;
}


ZAXIS *zaxis_to_pointer(int idx)
{
  static char func[] = "zaxis_to_pointer";
  ZAXIS *zaxisptr = NULL;

  ZAXIS_INIT

  if ( idx >= 0 && idx < _zaxis_max )
    {
      ZAXIS_LOCK

      zaxisptr = _zaxisList[idx].ptr;

      ZAXIS_UNLOCK
    }
  else
    Error(func, "zaxis index %d undefined!", idx);

  return (zaxisptr);
}


/* Create an index from a pointer */
static int zaxis_from_pointer(ZAXIS *ptr)
{
  static char func[] = "zaxis_from_pointer";
  int      idx = -1;
  zaxisPtrToIdx *newptr;

  if ( ptr )
    {
      ZAXIS_LOCK

      if ( _zaxisAvail )
	{
	  newptr       = _zaxisAvail;
	  _zaxisAvail  = _zaxisAvail->next;
	  newptr->next = 0;
	  idx	       = newptr->idx;
	  newptr->ptr  = ptr;
      
	  if ( ZAXIS_Debug )
	    Message(func, "Pointer %p has idx %d from zaxis list", ptr, idx);
	}
      else
	Warning(func, "Too many open zaxis (limit is %d)!", _zaxis_max);

      ZAXIS_UNLOCK
    }
  else
    Error(func, "Internal problem (pointer %p undefined)", ptr);

  return (idx);
}


static void zaxis_init_entry(ZAXIS *zaxisptr)
{
  zaxisptr->self        = zaxis_from_pointer(zaxisptr);

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


static ZAXIS *zaxis_new_entry(void)
{
  static char func[] = "zaxis_new_entry";
  ZAXIS *zaxisptr;

  zaxisptr = (ZAXIS *) malloc(sizeof(ZAXIS));

  if ( zaxisptr ) zaxis_init_entry(zaxisptr);

  return (zaxisptr);
}


static void zaxis_delete_entry(ZAXIS *zaxisptr)
{
  static char func[] = "zaxis_delete_entry";
  int idx;

  idx = zaxisptr->self;

  ZAXIS_LOCK

  free(zaxisptr);

  _zaxisList[idx].next = _zaxisAvail;
  _zaxisList[idx].ptr  = 0;
  _zaxisAvail          = &_zaxisList[idx];

  ZAXIS_UNLOCK

  if ( ZAXIS_Debug )
    Message(func, "Removed idx %d from zaxis list", idx);
}


static void zaxis_initialize(void)
{
  char *env;

#if  defined  (HAVE_LIBPTHREAD)
  /* initialize global API mutex lock */
  pthread_mutex_init(&_zaxis_mutex, NULL);
#endif

  env = getenv("ZAXIS_DEBUG");
  if ( env ) ZAXIS_Debug = atoi(env);

  zaxis_list_new();
  atexit(zaxis_list_delete);

  ZAXIS_LOCK

  zaxis_init_pointer();

  ZAXIS_UNLOCK

  _zaxis_init = TRUE;
}


static void zaxis_copy(ZAXIS *zaxisptr2, ZAXIS *zaxisptr1)
{
  int zaxisID2;

  zaxisID2 = zaxisptr2->self;
  memcpy(zaxisptr2, zaxisptr1, sizeof(ZAXIS));
  zaxisptr2->self = zaxisID2;
}


static void zaxis_check_ptr(const char *func, ZAXIS *zaxisptr)
{
  if ( zaxisptr == NULL )
    Error(func, "zaxis undefined!");
}


int zaxisSize(void)
{
  int zaxissize = 0;
  int i;
  
  ZAXIS_INIT

  ZAXIS_LOCK

  for ( i = 0; i < _zaxis_max; i++ )
    if ( _zaxisList[i].ptr ) zaxissize++;

  ZAXIS_UNLOCK

  return (zaxissize);
}


/*
@Function  zaxisCreate
@Title     Create a vertical Z-axis

@Prototype int zaxisCreate(int zaxistype, int size)
@Parameter
    @Item  zaxistype  The type of the Z-axis, one of the set of predefined CDI Z-axis types.
                      The valid CDI Z-axis types are @func{ZAXIS_GENERIC}, @func{ZAXIS_SURFACE},
                      @func{ZAXIS_HYBRID}, @func{ZAXIS_PRESSURE}, @func{ZAXIS_HEIGHT},
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
#define  NLEV    5
   ...
double levs[NLEV] = {101300, 92500, 85000, 50000, 20000};
int zaxisID;
   ...
zaxisID = zaxisCreate(ZAXIS_PRESSURE, NLEV);
zaxisDefLevels(zaxisID, levs);
   ...
@EndSource
@EndFunction
*/
int zaxisCreate(int zaxistype, int size)
{
  static char func[] = "zaxisCreate";
  int ilev;
  int zaxisID;
  double *vals;
  ZAXIS *zaxisptr;

  if ( CDI_Debug )
    Message(func, "zaxistype: %d size: %d ", zaxistype, size);

  ZAXIS_INIT

  zaxisptr = zaxis_new_entry();
  if ( ! zaxisptr ) Error(func, "No memory");

  zaxisID = zaxisptr->self;

  zaxisptr->type = zaxistype;
  zaxisptr->size = size;

  if ( zaxistype > CDI_MaxZaxistype )
    Error(func, "Internal problem! zaxistype > CDI_MaxZaxistype");

  zaxisDefName(zaxisID, ZaxistypeEntry[zaxistype].name);
  zaxisDefLongname(zaxisID, ZaxistypeEntry[zaxistype].longname);
  zaxisDefUnits(zaxisID, ZaxistypeEntry[zaxistype].units);

  vals = (double *) malloc(size*sizeof(double));

  for ( ilev = 0; ilev < size; ilev++ )
    vals[ilev] = 0.0;

  zaxisptr->vals = vals;

  return (zaxisID);
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
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);
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
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

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
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

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
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

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
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

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
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

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
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  strcpy(units, zaxisptr->units);
}


void zaxisDefPrec(int zaxisID, int prec)
{
  static char func[] = "zaxisDefPrec";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  zaxisptr->prec = prec;
}


int zaxisInqPrec(int zaxisID)
{
  static char func[] = "zaxisInqPrec";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  return (zaxisptr->prec);
}


void zaxisDefLtype(int zaxisID, int ltype)
{
  static char func[] = "zaxisDefLtype";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  zaxisptr->ltype = ltype;
}


int zaxisInqLtype(int zaxisID)
{
  static char func[] = "zaxisInqLtype";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  return (zaxisptr->ltype);
}


/*
@Function  zaxisDefLevels
@Title     Define the levels of a Z-axis

@Prototype void zaxisDefLevels(int zaxisID, double *levels)
@Parameter
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  levels   All levels of the Z-axis

@Description
The function @func{zaxisDefLevels} defines the levels of a Z-axis.

@EndFunction
*/
void zaxisDefLevels(int zaxisID, double *levels)
{
  static char func[] = "zaxisDefLevels";
  int ilev;
  int size;
  double *vals;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

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
  static char func[] = "zaxisDefLevel";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

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
  static char func[] = "zaxisInqLevel";
  double level = 0;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  if ( levelID >= 0 && levelID < zaxisptr->size )
    level = zaxisptr->vals[levelID];

  return (level);
}


double zaxisInqLbound(int zaxisID, int index)
{
  static char func[] = "zaxisInqLbound";
  double level = 0;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  if ( index >= 0 && index < zaxisptr->size )
    level = zaxisptr->lbounds[index];

  return (level);
}


double zaxisInqUbound(int zaxisID, int index)
{
  static char func[] = "zaxisInqUbound";
  double level = 0;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  if ( index >= 0 && index < zaxisptr->size )
    level = zaxisptr->ubounds[index];

  return (level);
}


const double *zaxisInqLevelsPtr(int zaxisID)
{
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

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
  static char func[] = "zaxisInqLevels";
  int size;
  int i;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  size = zaxisptr->size;
  for ( i = 0; i < size; i++ )
    levels[i] =  zaxisptr->vals[i];
}


int zaxisInqLbounds(int zaxisID, double *lbounds)
{
  static char func[] = "zaxisInqLbounds";
  int size = 0;
  int i;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

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
  static char func[] = "zaxisInqUbounds";
  int size = 0;
  int i;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

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
  static char func[] = "zaxisInqWeights";
  int size = 0;
  int i;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

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
  static char func[] = "zaxisInqLevelID";
  int size;
  int levelID = CDI_UNDEFID;
  int i;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

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
@func{ZAXIS_HYBRID}, @func{ZAXIS_PRESSURE}, @func{ZAXIS_HEIGHT},
@func{ZAXIS_DEPTH_BELOW_SEA} and @func{ZAXIS_DEPTH_BELOW_LAND}.

@EndFunction
*/
int zaxisInqType(int zaxisID)
{
  static char func[] = "zaxisInqType";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

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
  static char func[] = "zaxisInqSize";
  int size = 1;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  size = zaxisptr->size;

  return (size);
}


void cdiCheckZaxis(int zaxisID)
{
  static char func[] = "cdiCheckZaxis";
  int size, i, found;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);
  
  zaxis_check_ptr(func, zaxisptr);

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
	      Warning(func, "direction undefined for zaxisID %d", zaxisID);
	    }
	}
    }
}


void zaxisDefVct(int zaxisID, int size, const double *vct)
{
  static char func[] = "zaxisDefVct";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  if ( zaxisptr->vct == 0 )
    {
      zaxisptr->vctsize = size;
      zaxisptr->vct = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptr->vct, vct, size*sizeof(double));
    }
  else
    if ( zaxisptr->vctsize != size )
      Warning(func, "VCT was allready defined");
}


int zaxisInqVctSize(int zaxisID)
{
  static char func[] = "zaxisInqVctSize";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  return (zaxisptr->vctsize);
}


const double *zaxisInqVctPtr(int zaxisID)
{
  static char func[] = "zaxisInqVctPtr";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  return (zaxisptr->vct);
}


void zaxisDefLbounds(int zaxisID, double *lbounds)
{
  static char func[] = "zaxisDefLbounds";
  size_t size;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  size = zaxisptr->size;

  if ( zaxisptr->lbounds != NULL )
    Warning(func, "Lbounds allready defined for zaxisID = %d", zaxisID);

  zaxisptr->lbounds = (double *) malloc(size*sizeof(double));
  memcpy(zaxisptr->lbounds, lbounds, size*sizeof(double));
}


void zaxisDefUbounds(int zaxisID, double *ubounds)
{
  static char func[] = "zaxisDefUbounds";
  size_t size;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  size = zaxisptr->size;

  if ( zaxisptr->ubounds != NULL )
    Warning(func, "Ubounds allready defined for zaxisID = %d", zaxisID);

  zaxisptr->ubounds = (double *) malloc(size*sizeof(double));
  memcpy(zaxisptr->ubounds, ubounds, size*sizeof(double));
}


void zaxisDefWeights(int zaxisID, double *weights)
{
  static char func[] = "zaxisDefWeights";
  size_t size;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  size = zaxisptr->size;

  if ( zaxisptr->weights != NULL )
    Warning(func, "Weights allready defined for zaxisID = %d", zaxisID);

  zaxisptr->weights = (double *) malloc(size*sizeof(double));
  memcpy(zaxisptr->weights, weights, size*sizeof(double));
}


void zaxisChangeType(int zaxisID, int zaxistype)
{
  static char func[] = "zaxisChangeType";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  Message(func, "Change zaxis type from %s to %s\n",
	  zaxisNamePtr(zaxisptr->type),
	  zaxisNamePtr(zaxistype));
  
  zaxisptr->type = zaxistype;
}


void zaxisResize(int zaxisID, int size)
{
  static char func[] = "zaxisResize";
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxis_check_ptr(func, zaxisptr);

  zaxisptr->size = size;

  if ( zaxisptr->vals )
    zaxisptr->vals = (double *) realloc(zaxisptr->vals, size*sizeof(double));
}


int zaxisDuplicate(int zaxisID)
{
  static char func[] = "zaxisDuplicate";
  int zaxisIDnew;
  int zaxistype, zaxissize;
  int size;
  ZAXIS *zaxisptr, *zaxisptrnew;

  zaxisptr = zaxis_to_pointer(zaxisID);

  zaxistype = zaxisInqType(zaxisID);
  zaxissize = zaxisInqSize(zaxisID);

  zaxisIDnew = zaxisCreate(zaxistype, zaxissize);
  zaxisptrnew = zaxis_to_pointer(zaxisIDnew);

  zaxis_copy(zaxisptrnew, zaxisptr);

  strcpy(zaxisptr->name, zaxisptr->name);
  strcpy(zaxisptr->longname, zaxisptr->longname);
  strcpy(zaxisptr->units, zaxisptr->units);

  if ( zaxisptr->vals != NULL )
    {
      size = zaxissize;

      zaxisptr->vals = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptr->vals, zaxisptr->vals, size*sizeof(double));
    }

  if ( zaxisptr->lbounds )
    {
      size = zaxissize;

      zaxisptr->lbounds = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptr->lbounds, zaxisptr->lbounds, size*sizeof(double));
    }

  if ( zaxisptr->ubounds )
    {
      size = zaxissize;

      zaxisptr->ubounds = (double *) malloc(size*sizeof(double));
      memcpy(zaxisptr->ubounds, zaxisptr->ubounds, size*sizeof(double));
    }

  if ( zaxisptr->vct != NULL )
    {
      size = zaxisptr->vctsize;

      if ( size )
	{
	  zaxisptr->vctsize = size;
	  zaxisptr->vct = (double *) malloc(size*sizeof(double));
	  memcpy(zaxisptr->vct, zaxisptr->vct, size*sizeof(double));
	}
    }

  return (zaxisIDnew);
}


void zaxisPrint(int zaxisID)
{
  FILE *fp = stdout;
  int type;
  int nlevels, levelID;
  int nbyte0, nbyte;
  double level;
  ZAXIS *zaxisptr;

  zaxisptr = zaxis_to_pointer(zaxisID);

  type    = zaxisInqType(zaxisID);
  nlevels = zaxisInqSize(zaxisID);

  nbyte0 = 0;
  fprintf(fp, "#\n");
  fprintf(fp, "# zaxisID %d\n", zaxisID);
  fprintf(fp, "#\n");
  fprintf(fp, "zaxistype : %s\n", zaxisNamePtr(type));
  fprintf(fp, "size      : %d\n", nlevels);
  if ( zaxisptr->name[0]     ) fprintf(fp, "name      : %s\n", zaxisptr->name);
  if ( zaxisptr->longname[0] ) fprintf(fp, "longname  : %s\n", zaxisptr->longname);
  if ( zaxisptr->units[0]    ) fprintf(fp, "units     : %s\n", zaxisptr->units);

  nbyte0 = fprintf(fp, "levels    : ");
  nbyte = nbyte0;
  for ( levelID = 0; levelID < nlevels; levelID++ )
    {
      if ( nbyte > 80 )
	{
	  fprintf(stdout, "\n");
	  fprintf(stdout, "%*s", nbyte0, "");
	  nbyte = nbyte0;
	}
      level = zaxisInqLevel(zaxisID, levelID);
      nbyte += fprintf(stdout, "%.9g ", level);
    }
  fprintf(stdout, "\n");

  if ( zaxisptr->lbounds && zaxisptr->ubounds )
    {
      double level1, level2;
      nbyte = nbyte0;
      nbyte0 = fprintf(stdout, "%32s : ", "bounds");
      for ( levelID = 0; levelID < nlevels; levelID++ )
	{
	  if ( nbyte > 80 )
	    {
	      fprintf(stdout, "\n");
	      fprintf(stdout, "%*s", nbyte0, "");
	      nbyte = nbyte0;
	    }
	  level1 = zaxisInqLbound(zaxisID, levelID);
	  level2 = zaxisInqUbound(zaxisID, levelID);
	  nbyte += fprintf(stdout, "%.9g-%.9g ", level1, level2);
	}
      fprintf(stdout, "\n");
    }

  if ( type == ZAXIS_HYBRID )
    {
      int i;
      int vctsize;
      const double *vct;

      vctsize = zaxisInqVctSize(zaxisID);
      vct     = zaxisInqVctPtr(zaxisID);
      fprintf(stdout, "vctsize   : %d\n", vctsize);
      if ( vctsize )
	{
	  nbyte0 = fprintf(stdout, "vct       : ");
	  nbyte = nbyte0;
	  for ( i = 0; i < vctsize; i++ )
	    {
	      if ( nbyte > 70 )
		{
		  fprintf(stdout, "\n%*s", nbyte0, "");
		  nbyte = nbyte0;
		}
	      nbyte += fprintf(stdout, "%.9g ", vct[i]);
	    }
	  fprintf(stdout, "\n");
	  /*
	  nbyte0 = fprintf(stdout, "vct_b     : ");
	  nbyte  = nbyte0;
	  for ( i = 0; i < vctsize/2; i++ )
	    {
	      if ( nbyte > 70 )
		{
		  fprintf(stdout, "\n%*s", nbyte0, "");
		  nbyte = nbyte0;
		}
	      nbyte += fprintf(stdout, "%.9g ", vct[vctsize/2+i]);
	    }
	  fprintf(stdout, "\n");
	  */
	}
    }
}
