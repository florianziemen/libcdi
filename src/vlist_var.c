#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "vlist.h"
#include "vlist_var.h"
#include "resource_handle.h"
#include "tablepar.h"
#include "namespace.h"
#include "serialize.h"
#include "error.h"


static
void vlistvarInitEntry(int vlistID, int varID)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].fvarID        = varID;
  vlistptr->vars[varID].mvarID        = varID;
  vlistptr->vars[varID].flag          = 0;
  vlistptr->vars[varID].param         = 0;
  vlistptr->vars[varID].datatype      = CDI_UNDEFID;
  vlistptr->vars[varID].timetype      = CDI_UNDEFID;
  vlistptr->vars[varID].tsteptype     = TSTEP_INSTANT;
  vlistptr->vars[varID].chunktype     = cdiChunkType;
  vlistptr->vars[varID].xyz           = 321;
  vlistptr->vars[varID].gridID        = CDI_UNDEFID;
  vlistptr->vars[varID].zaxisID       = CDI_UNDEFID;
  vlistptr->vars[varID].subtypeID     = CDI_UNDEFID;
  vlistptr->vars[varID].instID        = CDI_UNDEFID;
  vlistptr->vars[varID].modelID       = CDI_UNDEFID;
  vlistptr->vars[varID].tableID       = CDI_UNDEFID;
  vlistptr->vars[varID].missvalused   = false;
  vlistptr->vars[varID].missval       = CDI_default_missval;
  vlistptr->vars[varID].addoffset     = 0.0;
  vlistptr->vars[varID].scalefactor   = 1.0;
  vlistptr->vars[varID].extra         = NULL;
  vlistptr->vars[varID].levinfo       = NULL;
  vlistptr->vars[varID].comptype      = CDI_COMPRESS_NONE;
  vlistptr->vars[varID].complevel     = 1;
  vlistptr->vars[varID].keys.nalloc   = MAX_KEYS;
  vlistptr->vars[varID].keys.nelems   = 0;
  for ( int i = 0; i < MAX_KEYS; ++i )
    vlistptr->vars[varID].keys.value[i].length = 0;
  vlistptr->vars[varID].atts.nalloc   = MAX_ATTRIBUTES;
  vlistptr->vars[varID].atts.nelems   = 0;
  vlistptr->vars[varID].lvalidrange   = false;
  vlistptr->vars[varID].validrange[0] = VALIDMISS;
  vlistptr->vars[varID].validrange[1] = VALIDMISS;
  vlistptr->vars[varID].iorank        = CDI_UNDEFID;
  vlistptr->vars[varID].opt_grib_kvpair_size = 0;
  vlistptr->vars[varID].opt_grib_kvpair      = NULL;
  vlistptr->vars[varID].opt_grib_nentries    = 0;
}

static
int vlistvarNewEntry(int vlistID)
{
  int varID = 0;
  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  int vlistvarSize = vlistptr->varsAllocated;
  var_t *vlistvar = vlistptr->vars;
  // Look for a free slot in vlistvar. (Create the table the first time through).
  if ( ! vlistvarSize )
    {
      vlistvarSize = 2;
      vlistvar = (var_t *) Malloc((size_t)vlistvarSize * sizeof (var_t));
      for ( int i = 0; i < vlistvarSize; i++ ) vlistvar[i].isUsed = false;
    }
  else
    {
      while (varID < vlistvarSize && vlistvar[varID].isUsed)
        ++varID;
    }
  // If the table overflows, double its size.
  if ( varID == vlistvarSize )
    {
      vlistvar = (var_t *) Realloc(vlistvar, (size_t)(vlistvarSize *= 2) * sizeof(var_t));
      for ( int i = varID; i < vlistvarSize; i++ ) vlistvar[i].isUsed = false;
    }

  vlistptr->varsAllocated = vlistvarSize;
  vlistptr->vars          = vlistvar;

  vlistvarInitEntry(vlistID, varID);

  vlistptr->vars[varID].isUsed = true;

  return varID;
}

void vlistCheckVarID(const char *caller, int vlistID, int varID)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if ( vlistptr == NULL ) Errorc("vlist undefined!");

  if ( varID < 0 || varID >= vlistptr->nvars ) Errorc("varID %d undefined!", varID);

  if ( ! vlistptr->vars[varID].isUsed ) Errorc("varID %d undefined!", varID);
}


int vlistDefVarTiles(int vlistID, int gridID, int zaxisID, int timetype, int tilesetID)
{
  if ( CDI_Debug ) Message("gridID = %d  zaxisID = %d  timetype = %d", gridID, zaxisID, timetype);

  const int varID = vlistvarNewEntry(vlistID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  vlistptr->nvars++;
  vlistptr->vars[varID].gridID    = gridID;
  vlistptr->vars[varID].zaxisID   = zaxisID;
  vlistptr->vars[varID].timetype = timetype;
  vlistptr->vars[varID].subtypeID = tilesetID;

  if ( timetype < 0 )
    {
      Message("Unexpected time type %d, set to TIME_VARYING!", timetype);
      vlistptr->vars[varID].timetype = TIME_VARYING;
    }

  vlistAdd2GridIDs(vlistptr, gridID);
  vlistAdd2ZaxisIDs(vlistptr, zaxisID);
  vlistAdd2SubtypeIDs(vlistptr, tilesetID);

  vlistptr->vars[varID].param = cdiEncodeParam(-(varID + 1), 255, 255);
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);

  return varID;
}

/*
@Function  vlistDefVar
@Title     Define a Variable

@Prototype int vlistDefVar(int vlistID, int gridID, int zaxisID, int timetype)
@Parameter
    @Item  vlistID   Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  gridID    Grid ID, from a previous call to @fref{gridCreate}.
    @Item  zaxisID   Z-axis ID, from a previous call to @fref{zaxisCreate}.
    @Item  timetype  One of the set of predefined CDI timestep types.
                     The valid CDI timestep types are @func{TIME_CONSTANT} and @func{TIME_VARYING}.

@Description
The function @func{vlistDefVar} adds a new variable to vlistID.

@Result
@func{vlistDefVar} returns an identifier to the new variable.

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
int vlistDefVar(int vlistID, int gridID, int zaxisID, int timetype)
{
  // call "vlistDefVarTiles" with a trivial tile index:
  return vlistDefVarTiles(vlistID, gridID, zaxisID, timetype, CDI_UNDEFID);
}

void
cdiVlistCreateVarLevInfo(vlist_t *vlistptr, int varID)
{
  xassert(varID >= 0 && varID < vlistptr->nvars && vlistptr->vars[varID].levinfo == NULL);

  const int zaxisID = vlistptr->vars[varID].zaxisID;
  const size_t nlevs = (size_t)zaxisInqSize(zaxisID);

  vlistptr->vars[varID].levinfo = (levinfo_t*) Malloc(nlevs * sizeof(levinfo_t));

  for (size_t levID = 0; levID < nlevs; levID++ )
    vlistptr->vars[varID].levinfo[levID] = DEFAULT_LEVINFO((int)levID);
}

/*
@Function  vlistDefVarParam
@Title     Define the parameter number of a Variable

@Prototype void vlistDefVarParam(int vlistID, int varID, int param)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  param    Parameter number.

@Description
The function @func{vlistDefVarParam} defines the parameter number of a variable.

@EndFunction
*/
void vlistDefVarParam(int vlistID, int varID, int param)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].param != param)
    {
      vlistptr->vars[varID].param = param;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  vlistDefVarCode
@Title     Define the code number of a Variable

@Prototype void vlistDefVarCode(int vlistID, int varID, int code)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  code     Code number.

@Description
The function @func{vlistDefVarCode} defines the code number of a variable.

@EndFunction
*/
void vlistDefVarCode(int vlistID, int varID, int code)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  const int param = vlistptr->vars[varID].param;
  int pnum, pcat, pdis;
  cdiDecodeParam(param, &pnum, &pcat, &pdis);
  const int newParam = cdiEncodeParam(code, pcat, pdis);
  if (vlistptr->vars[varID].param != newParam)
    {
      vlistptr->vars[varID].param = newParam;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


void vlistInqVar(int vlistID, int varID, int *gridID, int *zaxisID, int *timetype)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  *gridID   = vlistptr->vars[varID].gridID;
  *zaxisID  = vlistptr->vars[varID].zaxisID;
  *timetype = vlistptr->vars[varID].timetype;
}

/*
@Function  vlistInqVarGrid
@Title     Get the Grid ID of a Variable

@Prototype int vlistInqVarGrid(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Description
The function @func{vlistInqVarGrid} returns the grid ID of a Variable.

@Result
@func{vlistInqVarGrid} returns the grid ID of the Variable.

@EndFunction
*/
int vlistInqVarGrid(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].gridID;
}

/*
@Function  vlistInqVarZaxis
@Title     Get the Zaxis ID of a Variable

@Prototype int vlistInqVarZaxis(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Description
The function @func{vlistInqVarZaxis} returns the zaxis ID of a variable.

@Result
@func{vlistInqVarZaxis} returns the zaxis ID of the variable.

@EndFunction
*/
int vlistInqVarZaxis(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].zaxisID;
}


/*
@Function  vlistInqVarSubtype
@Title     Get the Subtype ID of a Variable

@Description
The function @func{vlistInqVarSubtype} returns the subtype ID of a variable.

@Result
@func{vlistInqVarSubtype} returns the subtype ID of the variable.

@EndFunction
*/
int vlistInqVarSubtype(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].subtypeID;
}


/*
@Function  vlistInqVarParam
@Title     Get the parameter number of a Variable

@Prototype int vlistInqVarParam(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Description
The function @func{vlistInqVarParam} returns the parameter number of a variable.

@Result
@func{vlistInqVarParam} returns the parameter number of the variable.

@EndFunction
*/
int vlistInqVarParam(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].param;
}

/*
@Function  vlistInqVarCode
@Title     Get the Code number of a Variable

@Prototype int vlistInqVarCode(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Description
The function @func{vlistInqVarCode} returns the code number of a variable.

@Result
@func{vlistInqVarCode} returns the code number of the variable.

@EndFunction
*/
int vlistInqVarCode(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  const int param = vlistptr->vars[varID].param;
  int pdis, pcat, pnum;
  cdiDecodeParam(param, &pnum, &pcat, &pdis);
  int code = pnum;
  if ( pdis != 255 ) code = -varID-1; // GRIB2 Parameter

  int tableID = vlistptr->vars[varID].tableID;
  if (code < 0 && tableID != -1)
    {
      char name[CDI_MAX_NAME];
      int length = CDI_MAX_NAME;
      (void)cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length);

      if (name[0]) tableInqParCode(tableID, name, &code);
    }

  return code;
}

/*
@Function  vlistInqVarName
@Title     Get the name of a Variable

@Prototype void vlistInqVarName(int vlistID, int varID, char *name)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.
    @Item  name     Returned variable name. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{vlistInqVarName} returns the name of a variable.

@Result
@func{vlistInqVarName} returns the name of the variable to the parameter name if available,
otherwise the result is an empty string.

@EndFunction
*/
void vlistInqVarName(int vlistID, int varID, char *name)
{
  int length = CDI_MAX_NAME;
  (void)cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length);

  if (!name[0])
    {
      vlistCheckVarID(__func__, vlistID, varID);

      vlist_t *vlistptr = vlist_to_pointer(vlistID);

      int param = vlistptr->vars[varID].param;
      int pdis, pcat, pnum;
      cdiDecodeParam(param, &pnum, &pcat, &pdis);
      if ( pdis == 255 )
	{
	  int code = pnum;
	  int tableID = vlistptr->vars[varID].tableID;
          tableInqEntry(tableID, code, -1, name, NULL, NULL);
	  if (!name[0]) sprintf(name, "var%d", code);
	}
      else
	{
	  sprintf(name, "param%d.%d.%d", pnum, pcat, pdis);
	}
    }
}

/*
@Function vlistCopyVarName
@Tatle    Get the name of a Variable in a safe way

@Prototype char* vlistCopyVarName(int vlistId, int varId)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Return A pointer to a malloc'ed string. Must be cleaned up with Free().

@Description
This is the buffer overflow immune version of vlistInqVarName().
The memory for the returned string is allocated to fit the string via Malloc().

@EndFunction
*/
char *vlistCopyVarName(int vlistID, int varID)
{
  // If a name is set in the variable description, use that.
  char name[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  (void)cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length);
  if (name[0]) return strdup(name);

  vlistCheckVarID(__func__, vlistID, varID);
  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  // Otherwise we check if we should use the table of parameter descriptions.
  int param = vlistptr->vars[varID].param;
  int discipline, category, number;
  cdiDecodeParam(param, &number, &category, &discipline);
  char *result = NULL;
  if (discipline == 255)
    {
      int tableID = vlistptr->vars[varID].tableID;
      tableInqEntry(tableID, number, -1, name, NULL, NULL);
      if ( name[0] )
        result = strdup(name);
      else
        {
          //No luck, fall back to outputting a name of the format "var<num>".
          result = (char *) Malloc(3 + 3 * sizeof (int) * CHAR_BIT / 8 + 2);
          sprintf(result, "var%d", number);
        }
    }
  else
    {
      result = (char *) Malloc(5 + 2 + 3 * (3 * sizeof (int) * CHAR_BIT + 1) + 1);
      sprintf(result, "param%d.%d.%d", number, category, discipline);
    }
  //Finally, we fall back to outputting a name of the format "param<num>.<cat>.<dis>".
  return result;
}

/*
@Function  vlistInqVarLongname
@Title     Get the longname of a Variable

@Prototype void vlistInqVarLongname(int vlistID, int varID, char *longname)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.
    @Item  longname Long name of the variable. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{vlistInqVarLongname} returns the longname of a variable if available,
otherwise the result is an empty string.

@Result
@func{vlistInqVarLongname} returns the longname of the variable to the parameter longname.

@EndFunction
*/
void vlistInqVarLongname(int vlistID, int varID, char *longname)
{
  int length = CDI_MAX_NAME;
  (void)cdiInqKeyString(vlistID, varID, CDI_KEY_LONGNAME, longname, &length);

  if (!longname[0])
    {
      vlistCheckVarID(__func__, vlistID, varID);

      vlist_t *vlistptr = vlist_to_pointer(vlistID);

      int param = vlistptr->vars[varID].param;
      int pdis, pcat, pnum;
      cdiDecodeParam(param, &pnum, &pcat, &pdis);
      if ( pdis == 255 )
	{
	  int code = pnum;
          int tableID = vlistptr->vars[varID].tableID;
          tableInqEntry(tableID, code, -1, NULL, longname, NULL);
	}
    }
}

/*
@Function  vlistInqVarStdname
@Title     Get the standard name of a Variable

@Prototype void vlistInqVarStdname(int vlistID, int varID, char *stdname)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.
    @Item  stdname  Standard name of the variable. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{vlistInqVarStdname} returns the standard name of a variable if available,
otherwise the result is an empty string.

@Result
@func{vlistInqVarStdname} returns the standard name of the variable to the parameter stdname.

@EndFunction
*/
void vlistInqVarStdname(int vlistID, int varID, char *stdname)
{
  int length = CDI_MAX_NAME;
  (void)cdiInqKeyString(vlistID, varID, CDI_KEY_STDNAME, stdname, &length);
}

/*
@Function  vlistInqVarUnits
@Title     Get the units of a Variable

@Prototype void vlistInqVarUnits(int vlistID, int varID, char *units)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.
    @Item  units    Units of the variable. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{vlistInqVarUnits} returns the units of a variable if available,
otherwise the result is an empty string.

@Result
@func{vlistInqVarUnits} returns the units of the variable to the parameter units.

@EndFunction
*/
void vlistInqVarUnits(int vlistID, int varID, char *units)
{
  int length = CDI_MAX_NAME;
  (void)cdiInqKeyString(vlistID, varID, CDI_KEY_UNITS, units, &length);

  if (!units[0])
    {
      vlistCheckVarID(__func__, vlistID, varID);

      vlist_t *vlistptr = vlist_to_pointer(vlistID);

      int param = vlistptr->vars[varID].param;
      int pdis, pcat, pnum;
      cdiDecodeParam(param, &pnum, &pcat, &pdis);
      if ( pdis == 255 )
	{
	  int code = pnum;
	  int tableID = vlistptr->vars[varID].tableID;
          tableInqEntry(tableID, code, -1, NULL, NULL, units);
	}
    }
}

// used in MPIOM !
int vlistInqVarID(int vlistID, int code)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  for ( int varID = 0; varID < vlistptr->nvars; varID++ )
    {
      const int param = vlistptr->vars[varID].param;
      int pdis, pcat, pnum;
      cdiDecodeParam(param, &pnum, &pcat, &pdis);
      if ( pnum == code ) return varID;
    }

  return CDI_UNDEFID;
}


size_t vlistInqVarSize(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  int zaxisID, gridID, timetype;
  vlistInqVar(vlistID, varID, &gridID, &zaxisID, &timetype);

  const size_t nlevs = (size_t)zaxisInqSize(zaxisID);
  const size_t gridsize = gridInqSize(gridID);

  return gridsize*nlevs;
}

/*
@Function  vlistInqVarDatatype
@Title     Get the data type of a Variable

@Prototype int vlistInqVarDatatype(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Description
The function @func{vlistInqVarDatatype} returns the data type of a variable.

@Result
@func{vlistInqVarDatatype} returns an identifier to the data type of the variable.
The valid CDI data types are @func{CDI_DATATYPE_PACK8}, @func{CDI_DATATYPE_PACK16}, @func{CDI_DATATYPE_PACK24},
@func{CDI_DATATYPE_FLT32}, @func{CDI_DATATYPE_FLT64}, @func{CDI_DATATYPE_INT8}, @func{CDI_DATATYPE_INT16} and
@func{CDI_DATATYPE_INT32}.

@EndFunction
*/
int vlistInqVarDatatype(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].datatype;
}


int vlistInqVarNumber(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  int number = CDI_REAL;
  if ( vlistptr->vars[varID].datatype == CDI_DATATYPE_CPX32 ||
       vlistptr->vars[varID].datatype == CDI_DATATYPE_CPX64 )
    number = CDI_COMP;

  return number;
}

/*
@Function  vlistDefVarDatatype
@Title     Define the data type of a Variable

@Prototype void vlistDefVarDatatype(int vlistID, int varID, int datatype)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  datatype The data type identifier.
                    The valid CDI data types are @func{CDI_DATATYPE_PACK8}, @func{CDI_DATATYPE_PACK16},
                    @func{CDI_DATATYPE_PACK24}, @func{CDI_DATATYPE_FLT32}, @func{CDI_DATATYPE_FLT64},
                    @func{CDI_DATATYPE_INT8}, @func{CDI_DATATYPE_INT16} and @func{CDI_DATATYPE_INT32}.

@Description
The function @func{vlistDefVarDatatype} defines the data type of a variable.

@EndFunction
*/
void vlistDefVarDatatype(int vlistID, int varID, int datatype)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  if (vlistptr->vars[varID].datatype != datatype)
    {
      vlistptr->vars[varID].datatype = datatype;

      if ( !vlistptr->vars[varID].missvalused )
        switch (datatype)
          {
          case CDI_DATATYPE_INT8:   vlistptr->vars[varID].missval = -SCHAR_MAX; break;
          case CDI_DATATYPE_UINT8:  vlistptr->vars[varID].missval =  UCHAR_MAX; break;
          case CDI_DATATYPE_INT16:  vlistptr->vars[varID].missval = -SHRT_MAX;  break;
          case CDI_DATATYPE_UINT16: vlistptr->vars[varID].missval =  USHRT_MAX; break;
          case CDI_DATATYPE_INT32:  vlistptr->vars[varID].missval = -INT_MAX;   break;
          case CDI_DATATYPE_UINT32: vlistptr->vars[varID].missval =  UINT_MAX;  break;
          case CDI_DATATYPE_FLT32:  vlistptr->vars[varID].missval =  (float) CDI_default_missval;  break;
          }
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


void vlistDefVarInstitut(int vlistID, int varID, int instID)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].instID != instID)
    {
      vlistptr->vars[varID].instID = instID;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarInstitut(int vlistID, int varID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].instID;
}


void vlistDefVarModel(int vlistID, int varID, int modelID)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].modelID != modelID)
    {
      vlistptr->vars[varID].modelID = modelID;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarModel(int vlistID, int varID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].modelID;
}


void vlistDefVarTable(int vlistID, int varID, int tableID)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  if (vlistptr->vars[varID].tableID != tableID)
    {
      vlistptr->vars[varID].tableID = tableID;
      const int tablenum = tableInqNum(tableID);

      const int param = vlistptr->vars[varID].param;
      int pnum, pcat, pdis;
      cdiDecodeParam(param, &pnum, &pcat, &pdis);
      vlistptr->vars[varID].param = cdiEncodeParam(pnum, tablenum, pdis);
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarTable(int vlistID, int varID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].tableID;
}

/*
@Function  vlistDefVarName
@Title     Define the name of a Variable

@Prototype void vlistDefVarName(int vlistID, int varID, const char *name)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  name     Name of the variable.

@Description
The function @func{vlistDefVarName} defines the name of a variable.

@EndFunction
*/
void vlistDefVarName(int vlistID, int varID, const char *name)
{
  (void)cdiDefKeyString(vlistID, varID, CDI_KEY_NAME, name);
}

/*
@Function  vlistDefVarLongname
@Title     Define the long name of a Variable

@Prototype void vlistDefVarLongname(int vlistID, int varID, const char *longname)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  longname Long name of the variable.

@Description
The function @func{vlistDefVarLongname} defines the long name of a variable.

@EndFunction
*/
void vlistDefVarLongname(int vlistID, int varID, const char *longname)
{
  if (longname) (void)cdiDefKeyString(vlistID, varID, CDI_KEY_LONGNAME, longname);
}

/*
@Function  vlistDefVarStdname
@Title     Define the standard name of a Variable

@Prototype void vlistDefVarStdname(int vlistID, int varID, const char *stdname)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  stdname  Standard name of the variable.

@Description
The function @func{vlistDefVarStdname} defines the standard name of a variable.

@EndFunction
*/
void vlistDefVarStdname(int vlistID, int varID, const char *stdname)
{
  if (stdname) (void)cdiDefKeyString(vlistID, varID, CDI_KEY_STDNAME, stdname);
}

/*
@Function  vlistDefVarUnits
@Title     Define the units of a Variable

@Prototype void vlistDefVarUnits(int vlistID, int varID, const char *units)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  units    Units of the variable.

@Description
The function @func{vlistDefVarUnits} defines the units of a variable.

@EndFunction
*/
void vlistDefVarUnits(int vlistID, int varID, const char *units)
{
  if (units) (void)cdiDefKeyString(vlistID, varID, CDI_KEY_UNITS, units);
}

/*
@Function  vlistInqVarMissval
@Title     Get the missing value of a Variable

@Prototype double vlistInqVarMissval(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Description
The function @func{vlistInqVarMissval} returns the missing value of a variable.

@Result
@func{vlistInqVarMissval} returns the missing value of the variable.

@EndFunction
*/
double vlistInqVarMissval(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].missval;
}

/*
@Function  vlistDefVarMissval
@Title     Define the missing value of a Variable

@Prototype void vlistDefVarMissval(int vlistID, int varID, double missval)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  missval  Missing value.

@Description
The function @func{vlistDefVarMissval} defines the missing value of a variable.

@EndFunction
*/
void vlistDefVarMissval(int vlistID, int varID, double missval)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  vlistptr->vars[varID].missval = missval;
  vlistptr->vars[varID].missvalused = true;
}

/*
@Function  vlistDefVarExtra
@Title     Define extra information of a Variable

@Prototype void vlistDefVarExtra(int vlistID, int varID, const char *extra)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.
    @Item  varID    Variable identifier.
    @Item  extra    Extra information.

@Description
The function @func{vlistDefVarExtra} defines the extra information of a variable.

@EndFunction
*/
void vlistDefVarExtra(int vlistID, int varID, const char *extra)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  if ( extra )
    {
      if ( vlistptr->vars[varID].extra )
	{
	  Free(vlistptr->vars[varID].extra);
	  vlistptr->vars[varID].extra = NULL;
	}

      vlistptr->vars[varID].extra = strdupx(extra);
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  vlistInqVarExtra
@Title     Get extra information of a Variable

@Prototype void vlistInqVarExtra(int vlistID, int varID, char *extra)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.
    @Item  extra    Returned variable extra information. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{vlistInqVarExtra} returns the extra information of a variable.

@Result
@func{vlistInqVarExtra} returns the extra information of the variable to the parameter extra if available,
otherwise the result is an empty string.

@EndFunction
*/
void vlistInqVarExtra(int vlistID, int varID, char *extra)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if ( vlistptr->vars[varID].extra == NULL )
    sprintf(extra, "-");
  else
    strcpy(extra, vlistptr->vars[varID].extra);
}


int vlistInqVarValidrange(int vlistID, int varID, double *validrange)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  if ( validrange != NULL && vlistptr->vars[varID].lvalidrange )
    {
      validrange[0] = vlistptr->vars[varID].validrange[0];
      validrange[1] = vlistptr->vars[varID].validrange[1];
    }

  return (int)vlistptr->vars[varID].lvalidrange;
}


void vlistDefVarValidrange(int vlistID, int varID, const double *validrange)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].validrange[0] = validrange[0];
  vlistptr->vars[varID].validrange[1] = validrange[1];
  vlistptr->vars[varID].lvalidrange = true;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}


double vlistInqVarScalefactor(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].scalefactor;
}


double vlistInqVarAddoffset(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].addoffset;
}


void vlistDefVarScalefactor(int vlistID, int varID, double scalefactor)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if ( IS_NOT_EQUAL(vlistptr->vars[varID].scalefactor, scalefactor) )
    {
      vlistptr->vars[varID].scalefactor = scalefactor;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


void vlistDefVarAddoffset(int vlistID, int varID, double addoffset)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if ( IS_NOT_EQUAL(vlistptr->vars[varID].addoffset, addoffset))
    {
      vlistptr->vars[varID].addoffset = addoffset;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


void vlistDefVarTimetype(int vlistID, int varID, int timetype)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].timetype != timetype)
    {
      vlistptr->vars[varID].timetype = timetype;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarTimetype(int vlistID, int varID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].timetype;
}


void vlistDefVarTsteptype(int vlistID, int varID, int tsteptype)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].tsteptype != tsteptype)
    {
      vlistptr->vars[varID].tsteptype = tsteptype;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  vlistInqVarTsteptype
@Title     Get the timestep type of a Variable

@Prototype int vlistInqVarTsteptype(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier.

@Description
The function @func{vlistInqVarTsteptype} returns the timestep type of a Variable.

@Result
@func{vlistInqVarTsteptype} returns the timestep type of the Variable,
one of the set of predefined CDI timestep types.
The valid CDI timestep types are @func{TSTEP_INSTANT},
@func{TSTEP_ACCUM}, @func{TSTEP_AVG}, @func{TSTEP_MAX}, @func{TSTEP_MIN} and @func{TSTEP_SD}.

@EndFunction
*/
int vlistInqVarTsteptype(int vlistID, int varID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].tsteptype;
}


int vlistInqVarMissvalUsed(int vlistID, int varID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return (int)vlistptr->vars[varID].missvalused;
}


void vlistDefFlag(int vlistID, int varID, int levID, int flag)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  levinfo_t li = DEFAULT_LEVINFO(levID);
  if (vlistptr->vars[varID].levinfo)
    ;
  else if (flag != li.flag)
    cdiVlistCreateVarLevInfo(vlistptr, varID);
  else
    return;

  vlistptr->vars[varID].levinfo[levID].flag = flag;

  vlistptr->vars[varID].flag = 0;

  int nlevs = zaxisInqSize(vlistptr->vars[varID].zaxisID);
  for ( int levelID = 0; levelID < nlevs; levelID++ )
    {
      if ( vlistptr->vars[varID].levinfo[levelID].flag )
        {
          vlistptr->vars[varID].flag = 1;
          break;
        }
    }

  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}


int vlistInqFlag(int vlistID, int varID, int levID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  if (vlistptr->vars[varID].levinfo)
    return vlistptr->vars[varID].levinfo[levID].flag;
  else
    {
      levinfo_t li = DEFAULT_LEVINFO(levID);
      return li.flag;
    }
}


int vlistFindVar(int vlistID, int fvarID)
{
  int varID;
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  for ( varID = 0; varID < vlistptr->nvars; varID++ )
    {
      if ( vlistptr->vars[varID].fvarID == fvarID ) break;
    }

  if ( varID == vlistptr->nvars )
    {
      varID = -1;
      Message("varID not found for fvarID %d in vlistID %d!", fvarID, vlistID);
    }

  return varID;
}


int vlistFindLevel(int vlistID, int fvarID, int flevelID)
{
  int levelID = -1;
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  const int varID = vlistFindVar(vlistID, fvarID);

  if ( varID != -1 )
    {
      const int nlevs = zaxisInqSize(vlistptr->vars[varID].zaxisID);
      for ( levelID = 0; levelID < nlevs; levelID++ )
	{
	  if ( vlistptr->vars[varID].levinfo[levelID].flevelID == flevelID ) break;
	}

      if ( levelID == nlevs )
	{
	  levelID = -1;
	  Message("levelID not found for fvarID %d and levelID %d in vlistID %d!",
		  fvarID, flevelID, vlistID);
	}
    }

  return levelID;
}


int vlistMergedVar(int vlistID, int varID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].mvarID;
}


int vlistMergedLevel(int vlistID, int varID, int levelID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  if (vlistptr->vars[varID].levinfo)
    return vlistptr->vars[varID].levinfo[levelID].mlevelID;
  else
    {
      levinfo_t li = DEFAULT_LEVINFO(levelID);
      return li.mlevelID;
    }
}


void vlistDefIndex(int vlistID, int varID, int levelID, int index)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  levinfo_t li = DEFAULT_LEVINFO(levelID);
  if (vlistptr->vars[varID].levinfo)
    ;
  else if (index != li.index)
    cdiVlistCreateVarLevInfo(vlistptr, varID);
  else
    return;

  vlistptr->vars[varID].levinfo[levelID].index = index;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}


int vlistInqIndex(int vlistID, int varID, int levelID)
{
  const vlist_t *vlistptr = vlist_to_pointer(vlistID);

  if (vlistptr->vars[varID].levinfo)
    return vlistptr->vars[varID].levinfo[levelID].index;
  else
    {
      levinfo_t li = DEFAULT_LEVINFO(levelID);
      return li.index;
    }
}


void vlistChangeVarZaxis(int vlistID, int varID, int zaxisID)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(__func__, vlistID, varID);

  const int nlevs1 = zaxisInqSize(vlistptr->vars[varID].zaxisID);
  const int nlevs2 = zaxisInqSize(zaxisID);

  if ( nlevs1 != nlevs2 ) Error("Number of levels must not change!");

  const int nvars = vlistptr->nvars;
  int found = 0;
  const int oldZaxisID = vlistptr->vars[varID].zaxisID;
  for ( int i = 0; i < varID; ++i)
    found |= (vlistptr->vars[i].zaxisID == oldZaxisID);
  for ( int i = varID + 1; i < nvars; ++i)
    found |= (vlistptr->vars[i].zaxisID == oldZaxisID);

  if (found)
    {
      const int nzaxis = vlistptr->nzaxis;
      for (int i = 0; i < nzaxis; ++i)
	if (vlistptr->zaxisIDs[i] == oldZaxisID )
	  vlistptr->zaxisIDs[i] = zaxisID;
    }
  else
    vlistAdd2ZaxisIDs(vlistptr, zaxisID);

  vlistptr->vars[varID].zaxisID = zaxisID;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}


void vlistChangeVarGrid(int vlistID, int varID, int gridID)
{
  vlist_t *vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(__func__, vlistID, varID);

  const int nvars = vlistptr->nvars;
  int index;
  for ( index = 0; index < nvars; index++ )
    if ( index != varID )
      if ( vlistptr->vars[index].gridID == vlistptr->vars[varID].gridID ) break;

  if ( index == nvars )
    {
      for ( index = 0; index < vlistptr->ngrids; index++ )
	if ( vlistptr->gridIDs[index] == vlistptr->vars[varID].gridID )
	  vlistptr->gridIDs[index] = gridID;
    }
  else
    vlistAdd2GridIDs(vlistptr, gridID);

  vlistptr->vars[varID].gridID = gridID;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}


void vlistDefVarCompType(int vlistID, int varID, int comptype)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].comptype != comptype)
    {
      vlistptr->vars[varID].comptype = comptype;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarCompType(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].comptype;
}


void vlistDefVarCompLevel(int vlistID, int varID, int complevel)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].complevel != complevel)
    {
      vlistptr->vars[varID].complevel = complevel;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarCompLevel(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].complevel;
}


void  vlistDefVarChunkType(int vlistID, int varID, int chunktype)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].chunktype != chunktype)
    {
      vlistptr->vars[varID].chunktype = chunktype;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarChunkType(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].chunktype;
}

static
int vlistEncodeXyz(int (*dimorder)[3])
{
  return (*dimorder)[0]*100 + (*dimorder)[1]*10 + (*dimorder)[2];
}


static
void vlistDecodeXyz(int xyz, int (*outDimorder)[3])
{
  (*outDimorder)[0] = xyz/100, xyz -= (*outDimorder)[0]*100;
  (*outDimorder)[1] = xyz/10, xyz -= (*outDimorder)[1]*10;
  (*outDimorder)[2] = xyz;
}


void vlistDefVarXYZ(int vlistID, int varID, int xyz)
{
  vlistCheckVarID(__func__, vlistID, varID);

  if ( xyz == 3 ) xyz = 321;

  /* check xyz dimension order */
  {
    int dimorder[3];
    vlistDecodeXyz(xyz, &dimorder);
    int dimx = 0, dimy = 0, dimz = 0;
    for ( int id = 0; id < 3; ++id )
      {
        switch ( dimorder[id] )
          {
            case 1: dimx++; break;
            case 2: dimy++; break;
            case 3: dimz++; break;
            default: dimorder[id] = 0; break;    //Ensure that we assign a valid dimension to this position.
          }
     }
    if ( dimz > 1 || dimy > 1 || dimx > 1 ) xyz = 321; // ZYX
    else
      {
        if ( dimz == 0 ) for ( int id = 0; id < 3; ++id ) if ( dimorder[id] == 0 ) {dimorder[id] = 3; break;}
        if ( dimy == 0 ) for ( int id = 0; id < 3; ++id ) if ( dimorder[id] == 0 ) {dimorder[id] = 2; break;}
        if ( dimx == 0 ) for ( int id = 0; id < 3; ++id ) if ( dimorder[id] == 0 ) {dimorder[id] = 1; break;}
        xyz = vlistEncodeXyz(&dimorder);
      }
  }

  assert(xyz == 123 || xyz == 312 || xyz == 231 || xyz == 321 || xyz == 132 || xyz == 213);

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  vlistptr->vars[varID].xyz = xyz;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}


void vlistInqVarDimorder(int vlistID, int varID, int (*outDimorder)[3])
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  vlistDecodeXyz(vlistptr->vars[varID].xyz, outDimorder);
}


int vlistInqVarXYZ(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].xyz;
}


void vlistDefVarIOrank(int vlistID, int varID, int iorank)
{
  vlistCheckVarID ( __func__, vlistID, varID );

  vlist_t *vlistptr = vlist_to_pointer(vlistID);
  if (vlistptr->vars[varID].iorank != iorank)
    {
      vlistptr->vars[varID].iorank = iorank;
      reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
    }
}


int vlistInqVarIOrank(int vlistID, int varID)
{
  vlistCheckVarID(__func__, vlistID, varID);

  const vlist_t *vlistptr = vlist_to_pointer(vlistID);
  return vlistptr->vars[varID].iorank;
}


int vlistVarCompare(vlist_t *a, int varIDA, vlist_t *b, int varIDB)
{
  xassert(a && b
          && varIDA >= 0 && varIDA < a->nvars
          && varIDB >= 0 && varIDB < b->nvars);
  var_t *pva = a->vars + varIDA, *pvb = b->vars + varIDB;
#define FCMP(f) ((pva->f) != (pvb->f))
#define FCMPFLT(f) (IS_NOT_EQUAL((pva->f), (pvb->f)))
#define FCMPSTR(fs) ((pva->fs) != (pvb->fs) && strcmp((pva->fs), (pvb->fs)))
#define FCMP2(f) (namespaceResHDecode(pva->f).idx       \
                  != namespaceResHDecode(pvb->f).idx)
  int diff = FCMP(fvarID) | FCMP(mvarID) | FCMP(flag) | FCMP(param)
    | FCMP(datatype) | FCMP(timetype) | FCMP(tsteptype)
    | FCMP(chunktype) | FCMP(xyz) | FCMP2(gridID) | FCMP2(zaxisID)
    | FCMP2(instID) | FCMP2(modelID) | FCMP2(tableID) | FCMP(missvalused)
    | FCMPFLT(missval) | FCMPFLT(addoffset) | FCMPFLT(scalefactor)
    | FCMPSTR(extra)
    | FCMP(comptype) | FCMP(complevel) | FCMP(lvalidrange)
    | FCMPFLT(validrange[0]) | FCMPFLT(validrange[1]);
#undef FCMP
#undef FCMPFLT
#undef FCMPSTR
#undef FCMP2
  if ((diff |= ((pva->levinfo == NULL) ^ (pvb->levinfo == NULL)))) return 1;

  if (pva->levinfo)
    {
      const int zaxisID = pva->zaxisID;
      const size_t nlevs = (size_t)zaxisInqSize(zaxisID);
      diff |= (memcmp(pva->levinfo, pvb->levinfo, sizeof (levinfo_t) * nlevs) != 0);
      if (diff) return 1;
    }

  const size_t natts = a->vars[varIDA].atts.nelems;
  if (natts != b->vars[varIDB].atts.nelems) return 1;
  for (size_t attID = 0; attID < natts; ++attID)
    diff |= cdi_att_compare(&a->vars[varIDA].atts, &b->vars[varIDB].atts, (int)attID);

  const size_t nkeys = a->vars[varIDA].keys.nelems;
  if (nkeys != b->vars[varIDB].keys.nelems) return 1;
  for (size_t keyID = 0; keyID < nkeys; ++keyID)
    diff |= cdi_key_compare(&a->vars[varIDA].keys, &b->vars[varIDB].keys, (int)keyID);

  return diff;
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
