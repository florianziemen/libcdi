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
LevelTypeEntry[] = {
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
};

static int CDI_MaxLeveltype = sizeof(LevelTypeEntry) / sizeof(LevelTypeEntry[0]);


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
  int      prec;
  int      type;
  int      size;
  int      direction;
  int      used;
  int      vctsize;
  double  *vct;
}
ZAXIS;

static ZAXIS *zaxisTable;
static int    zaxisTableSize = 0;


static void zaxisTableInitEntry (int zaxisID)
{
  zaxisTable[zaxisID].name[0]     = 0;
  zaxisTable[zaxisID].longname[0] = 0;
  zaxisTable[zaxisID].units[0]    = 0;
  zaxisTable[zaxisID].vals        = NULL;
  zaxisTable[zaxisID].ubounds     = NULL;
  zaxisTable[zaxisID].lbounds     = NULL;
  zaxisTable[zaxisID].weights     = NULL;
  zaxisTable[zaxisID].type        = CDI_UNDEFID;
  zaxisTable[zaxisID].direction   = CDI_UNDEFID;
  zaxisTable[zaxisID].prec        = 0;
  zaxisTable[zaxisID].size        = 0;
  zaxisTable[zaxisID].vctsize     = 0;
  zaxisTable[zaxisID].vct         = NULL;
}


static int zaxisTableNewEntry (int zaxistype)
{
  static char func[] = "zaxisTableNewEntry";
  int zaxisID = 0;

  /*
    Look for a free slot in zaxisTable.
    (Create the table the first time through).
  */
  if ( ! zaxisTableSize )
    {
      int i;

      zaxisTableSize = 2;
      zaxisTable = (ZAXIS *) malloc(zaxisTableSize*sizeof(ZAXIS));
      if ( zaxisTable == NULL )
	{
          Message(func, "zaxisTableSize = %d", zaxisTableSize);
	  SysError(func, "Allocation of ZAXISTABLE failed");
	}

      for ( i = 0; i < zaxisTableSize; i++ )
	zaxisTableInitEntry(i);
    }
  else
    {
      while ( zaxisID < zaxisTableSize )
	{
	  if ( zaxisTable[zaxisID].type == CDI_UNDEFID ) break;
	  zaxisID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if ( zaxisID == zaxisTableSize )
    {
      int i;

      zaxisTableSize = 2*zaxisTableSize;
      zaxisTable = (ZAXIS *) realloc(zaxisTable, zaxisTableSize*sizeof(ZAXIS));
      if ( zaxisTable == NULL )
	{
          Message(func, "zaxisTableSize = %d", zaxisTableSize);
	  SysError(func, "Reallocation of ZAXISTABLE failed");
	}

      zaxisID = zaxisTableSize/2;

      for ( i = zaxisID; i < zaxisTableSize; i++ )
	zaxisTableInitEntry(i);
    }

  zaxisTable[zaxisID].type = zaxistype;

  return (zaxisID);
}


void zaxisCheckID(const char *func, int zaxisID)
{
  if ( zaxisID < 0 || zaxisID >= zaxisTableSize )
      Error(func, "zaxisID %d undefined!", zaxisID);

  if ( zaxisTable[zaxisID].type == CDI_UNDEFID )
    Error(func, "zaxisID %d type undefined!", zaxisID);
}


/*
@Function  zaxisNew
@Title     Create a new Z-axis

@Prototype int zaxisNew(int zaxistype, int size)
@Parameter
    @Item  zaxistype  The type of the Z-axis, one of the set of predefined CDI Z-axis types.
                      The valid CDI Z-axis types are {\tt ZAXIS_SURFACE}, {\tt ZAXIS_HYBRID},
                      {\tt ZAXIS_HYBRID_HALF}, {\tt ZAXIS_PRESSURE}, {\tt ZAXIS_HEIGHT},
                      {\tt ZAXIS_DEPTH_BELOW_SEA} and {\tt ZAXIS_DEPTH_BELOW_LAND}.
    @Item  size       Number of levels

@Description
The function {\tt zaxisNew} creates a new Z-axis.

@Result
{\tt zaxisNew} returns an identifier to the new Z-axis.

@EndFunction
*/
int zaxisNew(int zaxistype, int size)
{
  static char func[] = "zaxisNew";
  int ilev;
  int zaxisID;
  double *vals;

  if ( CDI_Debug )
    Message(func, "zaxistype: %d size: %d ", zaxistype, size);

  /* get next free level entry */
  zaxisID = zaxisTableNewEntry(zaxistype);

  zaxisTable[zaxisID].size = size;

  if ( zaxistype > CDI_MaxLeveltype )
    Error(func, "Internal problem! zaxistype > CDI_MaxLeveltype");

  zaxisDefName(zaxisID, LevelTypeEntry[zaxistype].name);
  zaxisDefLongname(zaxisID, LevelTypeEntry[zaxistype].longname);
  zaxisDefUnits(zaxisID, LevelTypeEntry[zaxistype].units);

  vals = (double *) malloc(size*sizeof(double));

  for ( ilev = 0; ilev < size; ilev++ )
    vals[ilev] = 0.0;

  zaxisTable[zaxisID].vals = vals;

  return (zaxisID);
}


char *zaxisNamePtr(int leveltype)
{
  char *name;

  if ( leveltype >= 0 && leveltype < CDI_MaxLeveltype )
    name = LevelTypeEntry[leveltype].longname;
  else
    name = LevelTypeEntry[ZAXIS_GENERIC].longname;

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
    @Item  zaxisID   Z-axis identifier
    @Item  name      Name of the Z-axis

@Description
The function {\tt zaxisDefName} defines the name of a Z-axis.

@EndFunction
*/
void zaxisDefName(int zaxisID, const char *name)
{
  if ( name )
    strcpy(zaxisTable[zaxisID].name, name);
}


/*
@Function  zaxisDefLongname
@Title     Define the longname of a Z-axis

@Prototype void zaxisDefLongname(int zaxisID, const char *longname)
@Parameter
    @Item  zaxisID   Z-axis identifier
    @Item  longname  Longname of the Z-axis

@Description
The function {\tt zaxisDefLongname} defines the longname of a Z-axis.

@EndFunction
*/
void zaxisDefLongname(int zaxisID, const char *longname)
{
  if ( longname )
    strcpy(zaxisTable[zaxisID].longname, longname);
}


/*
@Function  zaxisDefUnits
@Title     Define the units of a Z-axis

@Prototype void zaxisDefUnits(int zaxisID, const char *units)
@Parameter
    @Item  zaxisID   Z-axis identifier
    @Item  units     Units of the Z-axis

@Description
The function {\tt zaxisDefUnits} defines the units of a Z-axis.

@EndFunction
*/
void zaxisDefUnits(int zaxisID, const char *units)
{
  if ( units )
    strcpy(zaxisTable[zaxisID].units, units);
}


/*
@Function  zaxisInqName
@Title     Get the name of a Z-axis

@Prototype void zaxisInqName(int zaxisID, char *name)
@Parameter
    @Item  zaxisID     Z-axis identifier
    @Item  name        Name of the Z-axis

@Description
The function {\tt zaxisInqName} returns the name of a Z-axis.

@Result
{\tt zaxisInqName} returns the name of the Z-axis to the parameter name.

@EndFunction
*/
void zaxisInqName(int zaxisID, char *name)
{
  strcpy(name, zaxisTable[zaxisID].name);
}


/*
@Function  zaxisInqLongname
@Title     Get the longname of a Z-axis

@Prototype void zaxisInqLongname(int zaxisID, char *longname)
@Parameter
    @Item  zaxisID     Z-axis identifier
    @Item  longname    Longname of the Z-axis

@Description
The function {\tt zaxisInqLongname} returns the longname of a Z-axis.

@Result
{\tt zaxisInqLongname} returns the longname of the Z-axis to the parameter longname.

@EndFunction
*/
void zaxisInqLongname(int zaxisID, char *longname)
{
  strcpy(longname, zaxisTable[zaxisID].longname);
}


/*
@Function  zaxisInqUnits
@Title     Get the units of a Z-axis

@Prototype void zaxisInqUnits(int zaxisID, char *units)
@Parameter
    @Item  zaxisID     Z-axis identifier
    @Item  units       Units of the Z-axis

@Description
The function {\tt zaxisInqUnits} returns the units of a Z-axis.

@Result
{\tt zaxisInqUnits} returns the units of the Z-axis to the parameter units.

@EndFunction
*/
void zaxisInqUnits(int zaxisID, char *units)
{
  strcpy(units, zaxisTable[zaxisID].units);
}


void zaxisDefPrec(int zaxisID, int prec)
{
  static char func[] = "zaxisDefPrec";

  zaxisCheckID(func, zaxisID);

  zaxisTable[zaxisID].prec = prec;
}


int zaxisInqPrec(int zaxisID)
{
  static char func[] = "zaxisInqPrec";

  zaxisCheckID(func, zaxisID);

  return (zaxisTable[zaxisID].prec);
}


/*
@Function  zaxisDefLevels
@Title     Define the levels of a Z-axis

@Prototype void zaxisDefLevels(int zaxisID, double *levels)
@Parameter
    @Item  zaxisID   Z-axis identifier
    @Item  levels    All levels of the Z-axis

@Description
The function {\tt zaxisDefLevels} defines the levels of a Z-axis.

@EndFunction
*/
void zaxisDefLevels(int zaxisID, double *levels)
{
  static char func[] = "zaxisDefLevels";
  int ilev;
  int size;
  double *vals;

  zaxisCheckID(func, zaxisID);

  size = zaxisTable[zaxisID].size;

  vals = zaxisTable[zaxisID].vals;

  for ( ilev = 0; ilev < size; ilev++ )
    vals[ilev] = levels[ilev];
}


/*
@Function  zaxisDefLevel
@Title     Define one level of a Z-axis

@Prototype void zaxisDefLevel(int zaxisID, int levelID, double level)
@Parameter
    @Item  zaxisID   Z-axis identifier
    @Item  levelID   Level identifier
    @Item  level     Level

@Description
The function {\tt zaxisDefLevel} defines one level of a Z-axis.

@EndFunction
*/
void zaxisDefLevel(int zaxisID, int levelID, double level)
{
  static char func[] = "zaxisDefLevel";

  zaxisCheckID(func, zaxisID);

  if ( levelID >= 0 && levelID < zaxisTable[zaxisID].size )
    zaxisTable[zaxisID].vals[levelID] = level;
}


/*
@Function  zaxisInqLevel
@Title     Get one level of a Z-axis

@Prototype double zaxisInqLevel(int zaxisID, int levelID)
@Parameter
    @Item  zaxisID   Z-axis identifier
    @Item  levelID   Level index (range: 0 to nlevel-1)

@Description
The function {\tt zaxisInqLevel} returns one level of a Z-axis.

@Result
{\tt zaxisInqLevel} returns the level of a Z-axis.
@EndFunction
*/
double zaxisInqLevel(int zaxisID, int levelID)
{
  static char func[] = "zaxisInqLevel";
  double level = 0;

  zaxisCheckID(func, zaxisID);

  if ( levelID >= 0 && levelID < zaxisTable[zaxisID].size )
    level = zaxisTable[zaxisID].vals[levelID];

  return (level);
}


double zaxisInqLbound(int zaxisID, int index)
{
  static char func[] = "zaxisInqLbound";
  double level = 0;

  zaxisCheckID(func, zaxisID);

  if ( index >= 0 && index < zaxisTable[zaxisID].size )
    level = zaxisTable[zaxisID].lbounds[index];

  return (level);
}


double zaxisInqUbound(int zaxisID, int index)
{
  static char func[] = "zaxisInqUbound";
  double level = 0;

  zaxisCheckID(func, zaxisID);

  if ( index >= 0 && index < zaxisTable[zaxisID].size )
    level = zaxisTable[zaxisID].ubounds[index];

  return (level);
}


const double *zaxisInqLevelsPtr(int zaxisID)
{
  return ( zaxisTable[zaxisID].vals );
}


/*
@Function  zaxisInqLevels
@Title     Get all levels of a Z-axis

@Prototype void zaxisInqLevels(int zaxisID, double *levels)
@Parameter
    @Item  zaxisID   Z-axis identifier
    @Item  levels    Levels of the Z-axis
    
@Description
The function {\tt zaxisInqLevels} returns all levels of a Z-axis.

@Result
{\tt zaxisInqLevels} saves all levels to the parameter {\tt levels}.
@EndFunction
*/
void zaxisInqLevels(int zaxisID, double *levels)
{
  static char func[] = "zaxisInqLevels";
  int size;
  int i;

  zaxisCheckID(func, zaxisID);

  size = zaxisTable[zaxisID].size;
  for ( i = 0; i < size; i++ )
    levels[i] =  zaxisTable[zaxisID].vals[i];
}


int zaxisInqLbounds(int zaxisID, double *lbounds)
{
  static char func[] = "zaxisInqLbounds";
  int size = 0;
  int i;

  zaxisCheckID(func, zaxisID);

  if ( zaxisTable[zaxisID].lbounds )
    {
      size = zaxisTable[zaxisID].size;

      if ( lbounds )
	for ( i = 0; i < size; i++ )
	  lbounds[i] =  zaxisTable[zaxisID].lbounds[i];
    }

  return (size);
}


int zaxisInqUbounds(int zaxisID, double *ubounds)
{
  static char func[] = "zaxisInqUbounds";
  int size = 0;
  int i;

  zaxisCheckID(func, zaxisID);

  if ( zaxisTable[zaxisID].ubounds )
    {
      size = zaxisTable[zaxisID].size;

      if ( ubounds )
	for ( i = 0; i < size; i++ )
	  ubounds[i] =  zaxisTable[zaxisID].ubounds[i];
    }

  return (size);
}


int zaxisInqWeights(int zaxisID, double *weights)
{
  static char func[] = "zaxisInqWeights";
  int size = 0;
  int i;

  zaxisCheckID(func, zaxisID);

  if ( zaxisTable[zaxisID].weights )
    {
      size = zaxisTable[zaxisID].size;

      if ( weights )
	for ( i = 0; i < size; i++ )
	  weights[i] =  zaxisTable[zaxisID].weights[i];
    }

  return (size);
}


int zaxisInqLevelID(int zaxisID, double level)
{
  static char func[] = "zaxisInqLevelID";
  int size;
  int levelID = CDI_UNDEFID;
  int i;

  zaxisCheckID(func, zaxisID);

  size = zaxisTable[zaxisID].size;
  for ( i = 0; i < size; i++ )
    if ( fabs(level-zaxisTable[zaxisID].vals[i]) < DBL_EPSILON ) break;

  if ( i < size ) levelID = i;

  return (levelID);
}


/*
@Function  zaxisInqType
@Title     Get the type of a Z-axis

@Prototype int zaxisInqType(int zaxisID)
@Parameter
    @Item  zaxisID   Z-axis identifier

@Description
The function {\tt zaxisInqType} returns the type of a Z-axis.

@Result
{\tt zaxisInqType} returns the type of the Z-axis,
one of the set of predefined CDI Z-axis types.
The valid CDI Z-axis types are {\tt ZAXIS_SURFACE}, {\tt ZAXIS_HYBRID},
{\tt ZAXIS_HYBRID_HALF}, {\tt ZAXIS_PRESSURE}, {\tt ZAXIS_HEIGHT},
{\tt ZAXIS_DEPTH_BELOW_SEA} and {\tt ZAXIS_DEPTH_BELOW_LAND}.

@EndFunction
*/
int zaxisInqType(int zaxisID)
{
  static char func[] = "zaxisInqType";

  zaxisCheckID(func, zaxisID);

  return (zaxisTable[zaxisID].type);
}


/*
@Function  zaxisInqSize
@Title     Get the size of a Z-axis

@Prototype int zaxisInqSize(int zaxisID)
@Parameter
    @Item  zaxisID  Z-axis identifier

@Description
The function {\tt zaxisInqSize} returns the size of a Z-axis.

@Result
{\tt zaxisInqSize} returns the number of levels of a Z-axis.

@EndFunction
*/
int zaxisInqSize(int zaxisID)
{
  static char func[] = "zaxisInqSize";
  int size = 1;

  zaxisCheckID(func, zaxisID);

  size = zaxisTable[zaxisID].size;

  return (size);
}


void cdiCheckZaxis(int zaxisID)
{
  static char func[] = "cdiCheckZaxis";
  int size, i, found;
  
  zaxisCheckID(func, zaxisID);

  if ( zaxisInqType(zaxisID) == ZAXIS_GENERIC )
    {
      size = zaxisTable[zaxisID].size;
      if ( size > 1 )
	{
	  /* check direction */
	  if ( zaxisTable[zaxisID].direction == CDI_UNDEFID )
	    {
	      found = 0;
	      for ( i = 1; i < size; i++ )
		if ( zaxisTable[zaxisID].vals[i] > zaxisTable[zaxisID].vals[i-1] )
		  found++;
	      if ( found == size-1 )
		{
		  zaxisTable[zaxisID].direction = LevelUp;
		}
	      else
		{
		  found = 0;
		  for ( i = 1; i < size; i++ )
		    if ( zaxisTable[zaxisID].vals[i] < zaxisTable[zaxisID].vals[i-1] )
		      found++;
		  if ( found == size-1 )
		    {
		      zaxisTable[zaxisID].direction = LevelDown;
		    }
		}
	    }
	  /* check consistent */
	  if ( zaxisTable[zaxisID].direction == CDI_UNDEFID )
	    {
	      Warning(func, "direction undefined for zaxisID %d", zaxisID);
	    }
	}
    }
}


int zaxisSize(void)
{
  int zaxisID = 0;

  while ( zaxisID < zaxisTableSize )
    {
      if ( zaxisTable[zaxisID].type == CDI_UNDEFID ) break;
      zaxisID++;
    }

  return (zaxisID);
}


void zaxisDefVct(int zaxisID, int size, const double *vct)
{
  static char func[] = "zaxisDefVct";

  zaxisCheckID(func, zaxisID);

  if ( zaxisTable[zaxisID].vct == 0 )
    {
      zaxisTable[zaxisID].vctsize = size;
      zaxisTable[zaxisID].vct = (double *) malloc(size*sizeof(double));
      memcpy(zaxisTable[zaxisID].vct, vct, size*sizeof(double));
    }
  else
    if ( zaxisTable[zaxisID].vctsize != size )
      Warning(func, "VCT was allready defined");
}


int zaxisInqVctSize(int zaxisID)
{
  static char func[] = "zaxisInqVctSize";

  zaxisCheckID(func, zaxisID);

  return (zaxisTable[zaxisID].vctsize);
}


const double *zaxisInqVctPtr(int zaxisID)
{
  static char func[] = "zaxisInqVctPtr";

  zaxisCheckID(func, zaxisID);

  return (zaxisTable[zaxisID].vct);
}


void zaxisDefLbounds(int zaxisID, double *lbounds)
{
  static char func[] = "zaxisDefLbounds";
  size_t size;

  zaxisCheckID(func, zaxisID);

  size = zaxisTable[zaxisID].size;

  if ( zaxisTable[zaxisID].lbounds != NULL )
    Warning(func, "Lbounds allready defined for zaxisID = %d", zaxisID);

  zaxisTable[zaxisID].lbounds = (double *) malloc(size*sizeof(double));
  memcpy(zaxisTable[zaxisID].lbounds, lbounds, size*sizeof(double));
}


void zaxisDefUbounds(int zaxisID, double *ubounds)
{
  static char func[] = "zaxisDefUbounds";
  size_t size;

  zaxisCheckID(func, zaxisID);

  size = zaxisTable[zaxisID].size;

  if ( zaxisTable[zaxisID].ubounds != NULL )
    Warning(func, "Ubounds allready defined for zaxisID = %d", zaxisID);

  zaxisTable[zaxisID].ubounds = (double *) malloc(size*sizeof(double));
  memcpy(zaxisTable[zaxisID].ubounds, ubounds, size*sizeof(double));
}


void zaxisDefWeights(int zaxisID, double *weights)
{
  static char func[] = "zaxisDefWeights";
  size_t size;

  zaxisCheckID(func, zaxisID);

  size = zaxisTable[zaxisID].size;

  if ( zaxisTable[zaxisID].weights != NULL )
    Warning(func, "Weights allready defined for zaxisID = %d", zaxisID);

  zaxisTable[zaxisID].weights = (double *) malloc(size*sizeof(double));
  memcpy(zaxisTable[zaxisID].weights, weights, size*sizeof(double));
}


void zaxisChangeType(int zaxisID, int leveltype)
{
  static char func[] = "zaxisChangeType";

  zaxisCheckID(func, zaxisID);

  Message(func, "Change zaxis type from %s to %s\n",
	  zaxisNamePtr(zaxisTable[zaxisID].type),
	  zaxisNamePtr(leveltype));
  
  zaxisTable[zaxisID].type = leveltype;
}


void zaxisResize(int zaxisID, int size)
{
  static char func[] = "zaxisResize";

  zaxisCheckID(func, zaxisID);

  zaxisTable[zaxisID].size = size;

  if ( zaxisTable[zaxisID].vals )
    zaxisTable[zaxisID].vals =
      (double *) realloc(zaxisTable[zaxisID].vals, size*sizeof(double));
}


int zaxisDuplicate(int zaxisID)
{
  static char func[] = "zaxisDuplicate";
  int zaxisIDnew;
  int zaxistype, zaxissize;
  int size;

  zaxistype = zaxisInqType(zaxisID);
  zaxissize = zaxisInqSize(zaxisID);

  zaxisIDnew = zaxisNew(zaxistype, zaxissize);

  memcpy(&zaxisTable[zaxisIDnew], &zaxisTable[zaxisID], sizeof(ZAXIS));

  strcpy(zaxisTable[zaxisIDnew].name, zaxisTable[zaxisID].name);
  strcpy(zaxisTable[zaxisIDnew].longname, zaxisTable[zaxisID].longname);
  strcpy(zaxisTable[zaxisIDnew].units, zaxisTable[zaxisID].units);

  if ( zaxisTable[zaxisID].vals != NULL )
    {
      size = zaxissize;

      zaxisTable[zaxisIDnew].vals = (double *) malloc(size*sizeof(double));
      memcpy(zaxisTable[zaxisIDnew].vals, zaxisTable[zaxisID].vals, size*sizeof(double));
    }

  if ( zaxisTable[zaxisIDnew].lbounds )
    {
      size = zaxissize;

      zaxisTable[zaxisIDnew].lbounds = (double *) malloc(size*sizeof(double));
      memcpy(zaxisTable[zaxisIDnew].lbounds, zaxisTable[zaxisID].lbounds, size*sizeof(double));
    }

  if ( zaxisTable[zaxisIDnew].ubounds )
    {
      size = zaxissize;

      zaxisTable[zaxisIDnew].ubounds = (double *) malloc(size*sizeof(double));
      memcpy(zaxisTable[zaxisIDnew].ubounds, zaxisTable[zaxisID].ubounds, size*sizeof(double));
    }

  if ( zaxisTable[zaxisID].vct != NULL )
    {
      size = zaxisTable[zaxisID].vctsize;

      if ( size )
	{
	  zaxisTable[zaxisID].vctsize = size;
	  zaxisTable[zaxisIDnew].vct = (double *) malloc(size*sizeof(double));
	  memcpy(zaxisTable[zaxisIDnew].vct, zaxisTable[zaxisID].vct, size*sizeof(double));
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

  type    = zaxisInqType(zaxisID);
  nlevels = zaxisInqSize(zaxisID);

  nbyte0 = 0;
  fprintf(fp, "#\n");
  fprintf(fp, "# zaxisID %d\n", zaxisID);
  fprintf(fp, "#\n");
  fprintf(fp, "zaxistype : %s\n", zaxisNamePtr(type));
  fprintf(fp, "size      : %d\n", nlevels);
  if ( zaxisTable[zaxisID].name[0]     ) fprintf(fp, "name      : %s\n", zaxisTable[zaxisID].name);
  if ( zaxisTable[zaxisID].longname[0] ) fprintf(fp, "longname  : %s\n", zaxisTable[zaxisID].longname);
  if ( zaxisTable[zaxisID].units[0]    ) fprintf(fp, "units     : %s\n", zaxisTable[zaxisID].units);

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

  if ( zaxisTable[zaxisID].lbounds && zaxisTable[zaxisID].ubounds )
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
