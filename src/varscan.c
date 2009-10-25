#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>
#include <math.h>

#include "cdi.h"
#include "stream_int.h"
#include "dmemory.h"
#include "varscan.h"
#include "vlist.h"

#undef  UNDEFID
#define UNDEFID -1

static size_t Vctsize = 0;
static double *Vct = NULL;

typedef struct
{
  int      level1;
  int      level2;
  int      recID;
  int      lindex;
}
LEVELTABLE;

typedef struct
{
  int         code;
  int         prec;
  int         timave;
  int         timaccu;
  int         gridID;
  int         zaxistype;
  int         ltype;     /* GRIB level type */
  int         lbounds;
  int         zaxisID;
  int         nlevels;
  int         levelTableSize;
  LEVELTABLE *levelTable;
  int         instID;
  int         modelID;
  int         tableID;
  int         codetable;
  int         ztype;
  int         zlevel;
  char       *name;
  char       *longname;
  char       *units;
}
VARTABLE;


int vartableInit = 0;
VARTABLE *vartable;
static int varTablesize = 0;
int nvars = 0;


static
void codeInitEntry(int varID, int code)
{
  vartable[varID].code           = code;
  vartable[varID].prec           = 0;
  vartable[varID].timave         = 0;
  vartable[varID].timaccu        = 0;
  vartable[varID].gridID         = UNDEFID;
  vartable[varID].zaxistype      = 0;
  vartable[varID].ltype          = 0;
  vartable[varID].levelTable     = NULL;
  vartable[varID].levelTableSize = 0;
  vartable[varID].nlevels        = 0;
  vartable[varID].instID         = UNDEFID;
  vartable[varID].modelID        = UNDEFID;
  vartable[varID].tableID        = UNDEFID;
  vartable[varID].ztype          = COMPRESS_NONE;
  vartable[varID].zlevel         = 1;
  vartable[varID].name           = NULL;
  vartable[varID].longname       = NULL;
  vartable[varID].units          = NULL;
}


static
int varGetEntry(int code, int zaxistype, int codetable, int ltype)
{
  int varID;

  for ( varID = 0; varID < varTablesize; varID++ )
    {
      if ( vartable[varID].code      == code      &&
	   vartable[varID].zaxistype == zaxistype &&
	   vartable[varID].ltype     == ltype &&
	   vartable[varID].codetable == codetable )
	return (varID);
    }

  return (UNDEFID);
}


void varFree(void)
{
  static char func[] = "varFree";
  int varID;

  for ( varID = 0; varID < nvars; varID++ )
    {
      if ( vartable[varID].levelTable )
	free(vartable[varID].levelTable);

      if ( vartable[varID].name )     free(vartable[varID].name);
      if ( vartable[varID].longname ) free(vartable[varID].longname);
      if ( vartable[varID].units )    free(vartable[varID].units);
    }

  if ( vartable )
    free(vartable);

  vartable = NULL;
  varTablesize = 0;
  nvars = 0;

  if ( Vct )
    free(Vct);

  Vct = NULL;
  Vctsize = 0;
}

int levelNewEntry(int varID, int level1, int level2)
{
  static char func[] = "levelNewEntry";
  int levelID = 0;
  int levelTableSize;
  LEVELTABLE *levelTable;

  levelTableSize = vartable[varID].levelTableSize;
  levelTable     = vartable[varID].levelTable;

  /*
    Look for a free slot in levelTable.
    (Create the table the first time through).
  */
  if ( ! levelTableSize )
    {
      int i;

      levelTableSize = 2;
      levelTable = (LEVELTABLE *) malloc(levelTableSize*sizeof(LEVELTABLE));
      if( levelTable == NULL )
	{
          Message(func, "levelTableSize = %d", levelTableSize);
	  SysError(func, "Allocation of LEVELTABLE failed");
	}

      for( i = 0; i < levelTableSize; i++ )
	levelTable[i].recID = UNDEFID;
    }
  else
    {
      while( levelID < levelTableSize )
	{
	  if ( levelTable[levelID].recID == UNDEFID ) break;
	  levelID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if( levelID == levelTableSize )
    {
      int i;

      levelTableSize = 2*levelTableSize;
      levelTable = (LEVELTABLE *) realloc(levelTable, levelTableSize*sizeof(LEVELTABLE));
      if( levelTable == NULL )
	{
          Message(func, "levelTableSize = %d", levelTableSize);
	  SysError(func, "Reallocation of LEVELTABLE failed");
	}
      levelID = levelTableSize/2;

      for( i = levelID; i < levelTableSize; i++ )
	levelTable[i].recID = UNDEFID;
    }

  levelTable[levelID].level1 = level1;
  levelTable[levelID].level2 = level2;
  levelTable[levelID].lindex = levelID;

  vartable[varID].nlevels = levelID+1;
  vartable[varID].levelTableSize = levelTableSize;
  vartable[varID].levelTable = levelTable;

  return (levelID);
}

#define  UNDEF_CODE  -4711

int codeNewEntry (int code)
{
  static char func[] = "codeNewEntry";
  int varID = 0;

  /*
    Look for a free slot in vartable.
    (Create the table the first time through).
  */
  if ( ! varTablesize )
    {
      int i;

      varTablesize = 2;
      vartable = (VARTABLE *) malloc(varTablesize*sizeof(VARTABLE));
      if( vartable == NULL )
	{
          Message(func, "varTablesize = %d", varTablesize);
	  SysError(func, "Allocation of VARTABLE failed");
	}

      for( i = 0; i < varTablesize; i++ )
	vartable[i].code = UNDEF_CODE;
    }
  else
    {
      while( varID < varTablesize )
	{
	  if ( vartable[varID].code == UNDEF_CODE ) break;
	  varID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if ( varID == varTablesize )
    {
      int i;

      varTablesize = 2*varTablesize;
      vartable = (VARTABLE *) realloc(vartable, varTablesize*sizeof(VARTABLE));
      if( vartable == NULL )
	{
          Message(func, "varTablesize = %d", varTablesize);
	  SysError(func, "Reallocation of VARTABLE failed");
	}
      varID = varTablesize/2;

      for( i = varID; i < varTablesize; i++ )
	vartable[i].code = UNDEF_CODE;
    }

  codeInitEntry(varID, code);

  return (varID);
}


void varAddRecord(int recID, int code, int gridID, int zaxistype, int lbounds,
		  int level1, int level2, int prec, int *pvarID, int *plevelID,
		  int numavg, int codetable, int ltype,
		  const char *name, const char *longname, const char *units)
{
  static char func[] = "varAddRecord";
  int varID = UNDEFID;
  int levelID = -1;

  if ( ! (cdiSplitLtype105 == 1 && zaxistype == ZAXIS_HEIGHT) )
    varID = varGetEntry(code, zaxistype, codetable, ltype);

  if ( varID == UNDEFID )
    {
      nvars++;
      varID = codeNewEntry(code);
      if ( prec > vartable[varID].prec ) vartable[varID].prec = prec;
      vartable[varID].gridID    = gridID;
      vartable[varID].zaxistype = zaxistype;
      vartable[varID].ltype     = ltype;
      vartable[varID].lbounds   = lbounds;
      if ( numavg ) vartable[varID].timave = 1;
      vartable[varID].codetable = codetable;

      if ( name )     if ( name[0] )     vartable[varID].name     = strdup(name);
      if ( longname ) if ( longname[0] ) vartable[varID].longname = strdup(longname);
      if ( units )    if ( units[0] )    vartable[varID].units    = strdup(units);
    }
  else
    {
      if ( vartable[varID].gridID != gridID )
	{
	  Message(func, "code = %d gridID = %d", code, gridID);
	  Error(func, "horizontal grid must not change for same code");
	}
      if ( vartable[varID].zaxistype != zaxistype )
	{
	  Message(func, "code = %d zaxistype = %d", code, zaxistype);
	  Error(func, "zaxistype must not change for same code");
	}
    }

  levelID = levelNewEntry(varID, level1, level2);
  vartable[varID].levelTable[levelID].recID = recID;

  if ( CDI_Debug )
    Message(func, "varID = %d  levelID = %d", varID, levelID);

  *pvarID   = varID;
  *plevelID = levelID;
}


int dblcmp(const void *s1, const void *s2)
{
  int cmp = 0;

  if      ( *((double *) s1) < *((double *) s2) ) cmp = -1;
  else if ( *((double *) s1) > *((double *) s2) ) cmp =  1;

  return (cmp);
}


int cmpLevelTable(const void *s1, const void *s2)
{
  int cmp = 0;
  LEVELTABLE *x = (LEVELTABLE *) s1;
  LEVELTABLE *y = (LEVELTABLE *) s2;
  /*
  printf("%g %g  %d %d\n", x->leve11, y->level1, x, y);
  */
  if      ( x->level1 < y->level1 ) cmp = -1;
  else if ( x->level1 > y->level1 ) cmp =  1;

  return (cmp);
}


void cdiGenVars(int streamID)
{
  static char func[] = "cdiGenVars";
  int varID, gridID, zaxisID, levelID;
  int instID, modelID, tableID;
  int code, nlevels, zaxistype, lindex, ltype;
  int prec;
  int timave, timaccu;
  int lbounds;
  int ztype;
  char name[256], longname[256], units[256];
  double *dlevels = NULL;
  double *dlevels1 = NULL;
  double *dlevels2 = NULL;
  int vlistID;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID =  streamInqVlist(streamID);

  for ( varID = 0; varID < nvars; varID++ )
    {
      gridID    = vartable[varID].gridID;
      code      = vartable[varID].code;
      nlevels   = vartable[varID].nlevels;
      ltype     = vartable[varID].ltype;
      zaxistype = vartable[varID].zaxistype;
      if ( ltype == 0 && zaxistype == ZAXIS_GENERIC && cdiDefaultLeveltype != -1 )
	zaxistype = cdiDefaultLeveltype;
      lbounds   = vartable[varID].lbounds;
      prec      = vartable[varID].prec;
      instID    = vartable[varID].instID;
      modelID   = vartable[varID].modelID;
      tableID   = vartable[varID].tableID;
      timave    = vartable[varID].timave;
      timaccu   = vartable[varID].timaccu;
      ztype     = vartable[varID].ztype;

      zaxisID = UNDEFID;

      if ( ltype == 0 && zaxistype == ZAXIS_GENERIC && nlevels == 1 && ! (fabs(vartable[varID].levelTable[0].level1)>0) )
	zaxistype = ZAXIS_SURFACE;

      dlevels = (double *) malloc(nlevels*sizeof(double));

      if ( lbounds && zaxistype != ZAXIS_HYBRID && zaxistype != ZAXIS_HYBRID_HALF )
	for ( levelID = 0; levelID < nlevels; levelID++ )
	  dlevels[levelID] = (vartable[varID].levelTable[levelID].level1 +
	                      vartable[varID].levelTable[levelID].level2)/2;
      else
	for ( levelID = 0; levelID < nlevels; levelID++ )
	  dlevels[levelID] = vartable[varID].levelTable[levelID].level1;

      if ( nlevels > 1 )
	{
	  int linc = FALSE, ldec = FALSE;
	  /* check increasing of levels */
	  for ( levelID = 1; levelID < nlevels; levelID++ )
	    if ( dlevels[levelID] < dlevels[levelID-1] ) break;

	  if ( levelID == nlevels ) linc = TRUE;

	  if ( linc == FALSE )
	    {
	      /* check decreasing of levels */
	      for ( levelID = 1; levelID < nlevels; levelID++ )
		if ( dlevels[levelID] > dlevels[levelID-1] ) break;

	      if ( levelID == nlevels ) ldec = TRUE;

	      if ( ldec == FALSE ||
		   zaxistype == ZAXIS_HYBRID ||
		   zaxistype == ZAXIS_DEPTH_BELOW_LAND )
		{
		  /*
		  qsort(dlevels, nlevels, sizeof(double), dblcmp);
		  */
		  qsort(vartable[varID].levelTable, nlevels, 
			sizeof(LEVELTABLE), cmpLevelTable);

		  if ( lbounds && zaxistype != ZAXIS_HYBRID && zaxistype != ZAXIS_HYBRID_HALF )
		    for ( levelID = 0; levelID < nlevels; levelID++ )
		      dlevels[levelID] = (vartable[varID].levelTable[levelID].level1 +
					  vartable[varID].levelTable[levelID].level2)/2.;
		  else
		    for ( levelID = 0; levelID < nlevels; levelID++ )
		      dlevels[levelID] = vartable[varID].levelTable[levelID].level1;
		}
	    }
	}

      if ( lbounds )
	{
	  dlevels1 = (double *) malloc(nlevels*sizeof(double));
	  for ( levelID = 0; levelID < nlevels; levelID++ )
	    dlevels1[levelID] = vartable[varID].levelTable[levelID].level1;
	  dlevels2 = (double *) malloc(nlevels*sizeof(double));
	  for ( levelID = 0; levelID < nlevels; levelID++ )
	    dlevels2[levelID] = vartable[varID].levelTable[levelID].level2;
	}

      zaxisID = varDefZaxis(vlistID, zaxistype, nlevels, dlevels, lbounds, dlevels1, dlevels2,
			    Vctsize, Vct, NULL, NULL, NULL, 0, 0, ltype);

      if ( lbounds ) free(dlevels1);
      if ( lbounds ) free(dlevels2);
      free(dlevels);

      varID = streamNewVar(streamID, gridID, zaxisID);
      varID = vlistDefVar(vlistID, gridID, zaxisID, TIME_VARIABLE);

      vlistDefVarCode(vlistID, varID, code);
      vlistDefVarDatatype(vlistID, varID, prec);
      vlistDefVarTimave(vlistID, varID, timave);
      vlistDefVarTimaccu(vlistID, varID, timaccu);
      vlistDefVarZtype(vlistID, varID, ztype);

      if ( vartable[varID].name )     vlistDefVarName(vlistID, varID, vartable[varID].name);
      if ( vartable[varID].longname ) vlistDefVarLongname(vlistID, varID, vartable[varID].longname);
      if ( vartable[varID].units )    vlistDefVarUnits(vlistID, varID, vartable[varID].units);

      if ( cdiDefaultTableID != UNDEFID )
	{
	  if ( tableInqParNamePtr(cdiDefaultTableID, code) )
	    {
	      if ( tableID != UNDEFID )
		{
		  strcpy(name, tableInqParNamePtr(cdiDefaultTableID, code));
		  vlistDefVarName(vlistID, varID, name);
		  if ( tableInqParLongnamePtr(cdiDefaultTableID, code) )
		    {
		      strcpy(longname, tableInqParLongnamePtr(cdiDefaultTableID, code));
		      vlistDefVarLongname(vlistID, varID, longname);
		    }
		  if ( tableInqParUnitsPtr(cdiDefaultTableID, code) )
		    {
		      strcpy(units, tableInqParUnitsPtr(cdiDefaultTableID, code));
		      vlistDefVarUnits(vlistID, varID, units);
		    }
		}
	      else
		tableID = cdiDefaultTableID;
	    }
	  if ( cdiDefaultModelID != UNDEFID ) modelID = cdiDefaultModelID;
	  if ( cdiDefaultInstID  != UNDEFID )  instID = cdiDefaultInstID;
	}

      if ( instID  != UNDEFID ) vlistDefVarInstitut(vlistID, varID, instID);
      if ( modelID != UNDEFID ) vlistDefVarModel(vlistID, varID, modelID);
      if ( tableID != UNDEFID ) vlistDefVarTable(vlistID, varID, tableID);
    }

  for ( varID = 0; varID < nvars; varID++ )
    {
      nlevels   = vartable[varID].nlevels;
      /*
      for ( levelID = 0; levelID < nlevels; levelID++ )
	{
	  lindex = vartable[varID].levelTable[levelID].lindex;
	  printf("%d %d %d %d %d\n", varID, levelID, 
		 vartable[varID].levelTable[levelID].lindex,
		 vartable[varID].levelTable[levelID].recID,
		 vartable[varID].levelTable[levelID].level1);
	}
      */
      for ( levelID = 0; levelID < nlevels; levelID++ )
	{
	  streamptr->vars[varID].level[levelID] =
	    vartable[varID].levelTable[levelID].recID;
	  for ( lindex = 0; lindex < nlevels; lindex++ )
	    if ( levelID == vartable[varID].levelTable[lindex].lindex ) break;

	  if ( lindex == nlevels )
	    Error(func, "Internal problem! lindex not found.");

	  streamptr->vars[varID].lindex[levelID] = lindex;
	}
    }

  varFree();
}


void varDefVCT(size_t vctsize, double *vctptr)
{
  static char func[] = "varDefVCT";

  if ( Vct == NULL && vctptr != NULL && vctsize > 0 )
    {
      Vctsize = vctsize;
      Vct = (double *) malloc(vctsize*sizeof(double));
      memcpy(Vct, vctptr, vctsize*sizeof(double)); 
    }
}


int varDefGrid(int vlistID, grid_t grid, int mode)
{
  /*
    mode: 0 search in vlist and grid table
          1 search in grid table
   */
  static char func[] = "varDefGrid";
  int gridglobdefined = 0;
  int griddefined;
  int ngrids;
  int gridID = UNDEFID;
  int index;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  griddefined = 0;
  ngrids = vlistptr->ngrids;

  if ( mode == 0 )
    for ( index = 0; index < ngrids; index++ )
      {
	gridID = vlistptr->gridIDs[index];
	if ( gridID == UNDEFID )
	  Error(func, "undefined gridID %d", gridID);

	if ( gridCompare(gridID, grid) == 0 )
	  {
	    griddefined = 1;
	    break;
	  }
      }

  if ( ! griddefined )
    {
      ngrids = gridSize();
      for ( gridID = 0; gridID < ngrids; gridID++ )
	{
	  if ( gridCompare(gridID, grid) == 0 )
	    {
	      gridglobdefined = 1;
	      break;
	    }
	}

      ngrids = vlistptr->ngrids;
      if ( mode == 1 )
	for ( index = 0; index < ngrids; index++ )
	  if ( vlistptr->gridIDs[index] == gridID )
	    {
	      gridglobdefined = FALSE;
	      break;
	    }
    }

  if ( ! griddefined )
    {
      if ( ! gridglobdefined ) gridID = gridGenerate(grid);
      ngrids = vlistptr->ngrids;
      vlistptr->gridIDs[ngrids] = gridID;
      vlistptr->ngrids++;
    }

  return (gridID);
}


int zaxisCompare(int zaxisID, int zaxistype, int nlevels, int lbounds, double *levels, char *longname, char *units, int ltype)
{
  int differ = 1;
  int levelID;
  int zlbounds = 0;
  int ltype_is_equal = FALSE;

  if ( ltype == zaxisInqLtype(zaxisID) ) ltype_is_equal = TRUE;

  if ( ltype_is_equal && (zaxistype == zaxisInqType(zaxisID) || zaxistype == ZAXIS_GENERIC) )
    {
      if ( zaxisInqLbounds(zaxisID, NULL) > 0 ) zlbounds = 1;
      if ( nlevels == zaxisInqSize(zaxisID) && zlbounds == lbounds )
	{
	  const double *dlevels;
	  char zlongname[256];
	  char zunits[256];
	  
	  dlevels = zaxisInqLevelsPtr(zaxisID);
	  for ( levelID = 0; levelID < nlevels; levelID++ )
	    {
	      if ( fabs(dlevels[levelID] - levels[levelID]) > 0 )
		break;
	    }

	  if ( levelID == nlevels ) differ = 0;

	  if ( ! differ )
	    {
	      zaxisInqLongname(zaxisID, zlongname);
	      zaxisInqUnits(zaxisID, zunits);
	      if ( longname && zlongname[0] )
		{
		  if ( strcmp(longname, zlongname) != 0 ) differ = 1;
		}
	      if ( units && zunits[0] )
		{
		  if ( strcmp(units, zunits) != 0 ) differ = 1;
		}
	    }
	}
    }

  return (differ);
}


int varDefZaxis(int vlistID, int zaxistype, int nlevels, double *levels, int lbounds, 
		double *levels1, double *levels2, int vctsize, double *vct, char *name, 
		char *longname, char *units, int prec, int mode, int ltype)
{
  /*
    mode: 0 search in vlist and zaxis table
          1 search in zaxis table
   */
  static char func[] = "varDefZaxis";
  int zaxisdefined;
  int nzaxis;
  int zaxisID = UNDEFID;
  int index;
  int zaxisglobdefined = 0;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  zaxisdefined = 0;
  nzaxis = vlistptr->nzaxis;

  if ( mode == 0 )
    for ( index = 0; index < nzaxis; index++ )
      {
	zaxisID = vlistptr->zaxisIDs[index];

	if ( zaxisCompare(zaxisID, zaxistype, nlevels, lbounds, levels, longname, units, ltype) == 0 )
	  {
	    zaxisdefined = 1;
	    break;
	  }
      }

  if ( ! zaxisdefined )
    {
      nzaxis = zaxisSize();
      for ( zaxisID = 0; zaxisID < nzaxis; zaxisID++ )
	if ( zaxisCompare(zaxisID, zaxistype, nlevels, lbounds, levels, longname, units, ltype) == 0 )
	  {
	    zaxisglobdefined = 1;
	    break;
	  }

      nzaxis = vlistptr->nzaxis;
      if ( mode == 1 )
	for ( index = 0; index < nzaxis; index++ )
	  if ( vlistptr->zaxisIDs[index] == zaxisID )
	    {
	      zaxisglobdefined = FALSE;
	      break;
	    }
    }

  if ( ! zaxisdefined )
    {
      if ( ! zaxisglobdefined )
	{
	  zaxisID = zaxisCreate(zaxistype, nlevels);
	  zaxisDefLevels(zaxisID, levels);
	  if ( lbounds )
	    {
	      zaxisDefLbounds(zaxisID, levels1);
	      zaxisDefUbounds(zaxisID, levels2);
	    }

	  if ( zaxistype == ZAXIS_HYBRID || zaxistype == ZAXIS_HYBRID_HALF )
	    {
	      /* if ( vctsize > 0 && vctsize >= 2*(nlevels+1)) */
	      /* if ( vctsize > 0 && vctsize >= 2*(nlevels)) */
	      if ( vctsize > 0 )
		zaxisDefVct(zaxisID, vctsize, vct);
	      else
		Warning(func, "VCT missing");
	    }

	  zaxisDefName(zaxisID, name);
	  zaxisDefLongname(zaxisID, longname);
	  zaxisDefUnits(zaxisID, units);
	  zaxisDefPrec(zaxisID, prec);
	  zaxisDefLtype(zaxisID, ltype);
	}

      nzaxis = vlistptr->nzaxis;
      vlistptr->zaxisIDs[nzaxis] = zaxisID;
      vlistptr->nzaxis++;
    }

  return (zaxisID);
}


void varDefZtype(int varID, int ztype)
{
  vartable[varID].ztype = ztype;
}


void varDefZlevel(int varID, int zlevel)
{
  vartable[varID].zlevel = zlevel;
}


int varInqInst(int varID)
{
  return (vartable[varID].instID);
}


void varDefInst(int varID, int instID)
{
  vartable[varID].instID = instID;
}


int varInqModel(int varID)
{
  return (vartable[varID].modelID);
}


void varDefModel(int varID, int modelID)
{
  vartable[varID].modelID = modelID;
}


int varInqTable(int varID)
{
  return (vartable[varID].tableID);
}


void varDefTable(int varID, int tableID)
{
  vartable[varID].tableID = tableID;
}
