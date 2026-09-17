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

#include <float.h> /* FLT_MAX */
#include "dmemory.h"
#include "vlist.h"
#include "tablepar.h"
#include "namespace.h"

static void
vlistvar_init_entry(var_t *vlistvars, int varID)
{
  var_t *varP = &vlistvars[varID];
  memset(varP, 0, sizeof(var_t));

  varP->xyz = 5;  // xyzStorVals[5] == 321
  varP->mvarID = varID;
  varP->fvarID = varID;
  varP->gridID = CDI_UNDEFID;
  varP->zaxisID = CDI_UNDEFID;
  varP->timetype = CDI_UNDEFID;
  varP->tsteptype = TSTEP_INSTANT;
  varP->datatype = CDI_UNDEFID;
  varP->instID = CDI_UNDEFID;
  varP->modelID = CDI_UNDEFID;
  varP->tableID = CDI_UNDEFID;
  varP->missval = CDI_Default_Missval;
  varP->validrange[0] = VALIDMISS;
  varP->validrange[1] = VALIDMISS;
  varP->comptype = CDI_COMPRESS_NONE;
  varP->complevel = 1;
  varP->keys.nalloc = MAX_KEYS;
  for (int i = 0; i < MAX_KEYS; ++i) { varP->keys.value[i].length = 0; }
  varP->atts.nalloc = MAX_ATTRIBUTES;
  varP->subtypeID = CDI_UNDEFID;
}

static int
vlistvarNewEntry(int vlistID)
{
  int varID = 0;
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  int vlistvarSize = vlistP->varsAllocated;
  var_t *vlistvars = vlistP->vars;
  // Look for a free slot in vlistvar. (Create the table the first time through).
  if (!vlistvarSize)
  {
    vlistvarSize = 2;
    vlistvars = (var_t *) Malloc((size_t) vlistvarSize * sizeof(var_t));
    for (int i = 0; i < vlistvarSize; ++i) { vlistvars[i].isUsed = false; }
  }
  else
  {
    while (varID < vlistvarSize && vlistvars[varID].isUsed) ++varID;
  }
  // If the table overflows, double its size.
  if (varID == vlistvarSize)
  {
    vlistvars = (var_t *) Realloc(vlistvars, (size_t) (vlistvarSize *= 2) * sizeof(var_t));
    for (int i = varID; i < vlistvarSize; ++i) { vlistvars[i].isUsed = false; }
  }

  vlistP->varsAllocated = vlistvarSize;
  vlistP->vars = vlistvars;

  vlistvar_init_entry(vlistvars, varID);
  vlistvars[varID].isUsed = true;

  return varID;
}

static inline var_t *
vlistP_get_varP(const char *caller, vlist_t *vlistP, int varID)
{
  if (varID < 0 || varID >= vlistP->nvars || !vlistP->vars[varID].isUsed) Errorc("varID %d undefined!", varID);
  return &vlistP->vars[varID];
}

var_t *
vlist_get_varP(const char *caller, int vlistID, int varID)
{
  return vlistP_get_varP(caller, vlist_to_pointer(vlistID), varID);
}

int
vlistDefVarTiles(int vlistID, int gridID, int zaxisID, int timetype, int tilesetID)
{
  if (CDI_Debug) Message("gridID = %d  zaxisID = %d  timetype = %d", gridID, zaxisID, timetype);

  int varID = vlistvarNewEntry(vlistID);

  vlist_t *vlistP = vlist_to_pointer(vlistID);
  vlistP->nvars++;
  var_t *varP = &vlistP->vars[varID];
  varP->gridID = gridID;
  varP->zaxisID = zaxisID;
  varP->timetype = timetype;
  varP->subtypeID = tilesetID;

  if (timetype < 0)
  {
    Warning("Unexpected time type %d, set to TIME_VARYING!", timetype);
    varP->timetype = TIME_VARYING;
  }

  // Compatibility for release 1.8.3
  if (timetype > 1 && timetype < 15)
  {
    varP->timetype = TIME_VARYING;
    varP->tsteptype = timetype;
    static bool printInfo = true;
    if (printInfo)
    {
      printInfo = false;
      fprintf(stdout, "CDI info: The vlistDefVar() function was called with an invalid value for the timetype parameter.\n");
      fprintf(stdout, "CDI info:    This may be an obsolete form of using the vlistDefVar() function.\n");
      fprintf(stdout, "CDI info:    The correct form is:\n");
      fprintf(stdout, "CDI info:       varID = vlistDefVar(vlistID, gridID, zaxisID, timetype)\n");
      fprintf(stdout, "CDI info:       vlistDefVarTsteptype(vlistID, varID, tsteptype)\n");
      fprintf(stdout, "CDI info:          with: timetype = TIME_CONSTANT | TIME_VARYING\n");
      fprintf(stdout, "CDI info:                tsteptype = TSTEP_AVG .... TSTEP_SUM\n");
    }
  }

  vlistAdd2GridIDs(vlistP, gridID);
  vlistAdd2ZaxisIDs(vlistP, zaxisID);
  vlistAdd2SubtypeIDs(vlistP, tilesetID);

  varP->param = cdiEncodeParam(-(varID + 1), 255, 255);
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
int
vlistDefVar(int vlistID, int gridID, int zaxisID, int timetype)
{
  // call "vlistDefVarTiles" with a trivial tile index:
  return vlistDefVarTiles(vlistID, gridID, zaxisID, timetype, CDI_UNDEFID);
}

void
cdiVlistCreateVarLevInfo(vlist_t *vlistP, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlistP, varID);
  xassert(varID >= 0 && varID < vlistP->nvars && varP->levinfo == NULL);

  int zaxisID = varP->zaxisID;
  int nlevs = zaxisInqSize(zaxisID);
  varP->levinfo = (levinfo_t *) Malloc(nlevs * sizeof(levinfo_t));
  for (int levID = 0; levID < nlevs; ++levID) { varP->levinfo[levID] = DEFAULT_LEVINFO(levID); }
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
void
vlistDefVarParam(int vlistID, int varID, int param)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  if (varP->param != param)
  {
    varP->param = param;
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
void
vlistDefVarCode(int vlistID, int varID, int code)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  int pnum, pcat, pdis;
  cdiDecodeParam(varP->param, &pnum, &pcat, &pdis);
  int newParam = cdiEncodeParam(code, pcat, pdis);
  if (varP->param != newParam)
  {
    varP->param = newParam;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

void
vlistInqVar(int vlistID, int varID, int *gridID, int *zaxisID, int *timetype)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  *gridID = varP->gridID;
  *zaxisID = varP->zaxisID;
  *timetype = varP->timetype;
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
int
vlistInqVarGrid(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->gridID;
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
int
vlistInqVarZaxis(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->zaxisID;
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
int
vlistInqVarSubtype(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->subtypeID;
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
int
vlistInqVarParam(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->param;
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
int
vlistInqVarCode(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  int pdis, pcat, pnum;
  cdiDecodeParam(varP->param, &pnum, &pcat, &pdis);
  int code = pnum;
  if (pdis != 255) code = -varID - 1;  // GRIB2 Parameter

  int tableID = varP->tableID;
  if (code < 0 && tableID != -1)
  {
    char name[CDI_MAX_NAME];
    int length = CDI_MAX_NAME;
    (void) cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length);

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
void
vlistInqVarName(int vlistID, int varID, char *name)
{
  int length = CDI_MAX_NAME;
  if (CDI_NOERR != cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length))
  {
    var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

    int pdis, pcat, pnum;
    cdiDecodeParam(varP->param, &pnum, &pcat, &pdis);
    if (pdis == 255)
    {
      int code = pnum;
      int tableID = varP->tableID;
      tableInqEntry(tableID, code, -1, name, NULL, NULL);
      if (!name[0]) snprintf(name, 8, "var%d", code);
    }
    else
    {
      snprintf(name, CDI_MAX_NAME, "param%d.%d.%d", pnum, pcat, pdis);
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
char *
vlistCopyVarName(int vlistID, int varID)
{
  // If a name is set in the variable description, use that.
  char name[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length);
  if (name[0]) return strdup(name);

  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  // Otherwise we check if we should use the table of parameter descriptions.
  int discipline, category, number;
  cdiDecodeParam(varP->param, &number, &category, &discipline);
  char *result = NULL;
  if (discipline == 255)
  {
    tableInqEntry(varP->tableID, number, -1, name, NULL, NULL);
    if (name[0]) { result = strdup(name); }
    else
    {
      // No luck, fall back to outputting a name of the format "var<num>".
      size_t len = 16;  // 3 + 3 * sizeof(int) * CHAR_BIT / 8 + 2;
      result = (char *) Malloc(len);
      snprintf(result, len, "var%d", number);
    }
  }
  else
  {
    size_t len = 64;  // 5 + 2 + 3 * (3 * sizeof(int) * CHAR_BIT + 1) + 1;
    result = (char *) Malloc(len);
    snprintf(result, len, "param%d.%d.%d", number, category, discipline);
  }
  // Finally, we fall back to outputting a name of the format "param<num>.<cat>.<dis>".
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
void
vlistInqVarLongname(int vlistID, int varID, char *longname)
{
  int length = CDI_MAX_NAME;
  if (CDI_NOERR != cdiInqKeyString(vlistID, varID, CDI_KEY_LONGNAME, longname, &length))
  {
    var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

    int pdis, pcat, pnum;
    cdiDecodeParam(varP->param, &pnum, &pcat, &pdis);
    if (pdis == 255) tableInqEntry(varP->tableID, pnum, -1, NULL, longname, NULL);
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
void
vlistInqVarStdname(int vlistID, int varID, char *stdname)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(vlistID, varID, CDI_KEY_STDNAME, stdname, &length);
}

// obsolete function
int
vlistInqVarTypeOfGeneratingProcess(int vlistID, int varID)
{
  static bool printInfo = true;
  if (printInfo) printInfo = cdiObsoleteInfo(__func__, "cdiInqKeyInt");
  int typeOfGeneratingProcess = 0;
  cdiInqKeyInt(vlistID, varID, CDI_KEY_TYPEOFGENERATINGPROCESS, &typeOfGeneratingProcess);
  return typeOfGeneratingProcess;
}

// obsolete function
void
vlistDefVarTypeOfGeneratingProcess(int vlistID, int varID, int typeOfGeneratingProcess)
{
  static bool printInfo = true;
  if (printInfo) printInfo = cdiObsoleteInfo(__func__, "cdiDefKeyInt");
  cdiDefKeyInt(vlistID, varID, CDI_KEY_TYPEOFGENERATINGPROCESS, typeOfGeneratingProcess);
}

// obsolete function
void
vlistDefVarProductDefinitionTemplate(int vlistID, int varID, int productDefinitionTemplate)
{
  static bool printInfo = true;
  if (printInfo) printInfo = cdiObsoleteInfo(__func__, "cdiDefKeyInt");
  cdiDefKeyInt(vlistID, varID, CDI_KEY_PRODUCTDEFINITIONTEMPLATE, productDefinitionTemplate);
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
void
vlistInqVarUnits(int vlistID, int varID, char *units)
{
  int length = CDI_MAX_NAME;
  if (CDI_NOERR != cdiInqKeyString(vlistID, varID, CDI_KEY_UNITS, units, &length))
  {
    var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

    int pdis, pcat, pnum;
    cdiDecodeParam(varP->param, &pnum, &pcat, &pdis);
    if (pdis == 255) tableInqEntry(varP->tableID, pnum, -1, NULL, NULL, units);
  }
}

// used in MPIOM !
int
vlistInqVarID(int vlistID, int code)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  for (int varID = 0; varID < vlistP->nvars; ++varID)
  {
    int param = vlistP->vars[varID].param;
    int pdis, pcat, pnum;
    cdiDecodeParam(param, &pnum, &pcat, &pdis);
    if (pnum == code) return varID;
  }

  return CDI_UNDEFID;
}

SizeType
vlistInqVarSize(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return gridInqSize(varP->gridID) * (SizeType) zaxisInqSize(varP->zaxisID);
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
int
vlistInqVarDatatype(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->datatype;
}

int
vlistInqVarNumber(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return (varP->datatype == CDI_DATATYPE_CPX32 || varP->datatype == CDI_DATATYPE_CPX64) ? CDI_COMP : CDI_REAL;
}

static inline bool
check_range(double value, double lowerBound, double upperBound)
{
  return (value >= lowerBound && value <= upperBound);
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
void
vlistDefVarDatatype(int vlistID, int varID, int datatype)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (varP->datatype == datatype) return;

  varP->datatype = datatype;

  if (!varP->missvalused)
  {
    double missval = varP->missval;
    bool missvalIsDefault = dbl_is_equal(missval, CDI_Default_Missval);
    if (missvalIsDefault)
    {
      // clang-format off
      switch (datatype)
      {
        case CDI_DATATYPE_INT8:   varP->missval = -SCHAR_MAX; break;
        case CDI_DATATYPE_UINT8:  varP->missval =  UCHAR_MAX; break;
        case CDI_DATATYPE_INT16:  varP->missval = -SHRT_MAX;  break;
        case CDI_DATATYPE_UINT16: varP->missval =  USHRT_MAX; break;
        case CDI_DATATYPE_INT32:  varP->missval = -INT_MAX;   break;
        case CDI_DATATYPE_UINT32: varP->missval =  UINT_MAX;  break;
        case CDI_DATATYPE_FLT32:  varP->missval =  (float) CDI_Default_Missval;  break;
      }
      // clang-format on
    }
    else
    {
      // clang-format off
      switch (datatype)
      {
        case CDI_DATATYPE_INT8:   varP->missval = check_range(missval, -SCHAR_MAX, SCHAR_MAX) ? missval : -SCHAR_MAX; break;
        case CDI_DATATYPE_UINT8:  varP->missval = check_range(missval,          0, UCHAR_MAX) ? missval :  UCHAR_MAX; break;
        case CDI_DATATYPE_INT16:  varP->missval = check_range(missval,  -SHRT_MAX,  SHRT_MAX) ? missval : -SHRT_MAX;  break;
        case CDI_DATATYPE_UINT16: varP->missval = check_range(missval,          0, USHRT_MAX) ? missval :  USHRT_MAX; break;
        case CDI_DATATYPE_INT32:  varP->missval = check_range(missval,   -INT_MAX,   INT_MAX) ? missval : -INT_MAX;   break;
        case CDI_DATATYPE_UINT32: varP->missval = check_range(missval,          0,  UINT_MAX) ? missval :  UINT_MAX;  break;
        case CDI_DATATYPE_FLT32:  varP->missval = check_range(missval,   -FLT_MAX,   FLT_MAX) ? missval :  CDI_Default_Missval;  break;
      }
      // clang-format on
    }
  }
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

void
vlistDefVarInstitut(int vlistID, int varID, int instID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  if (varP->instID != instID)
  {
    varP->instID = instID;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqVarInstitut(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->instID;
}

void
vlistDefVarModel(int vlistID, int varID, int modelID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  if (varP->modelID != modelID)
  {
    varP->modelID = modelID;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqVarModel(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->modelID;
}

void
vlistDefVarTable(int vlistID, int varID, int tableID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (varP->tableID != tableID)
  {
    varP->tableID = tableID;
    int tablenum = tableInqNum(tableID);
    int pnum, pcat, pdis;
    cdiDecodeParam(varP->param, &pnum, &pcat, &pdis);
    varP->param = cdiEncodeParam(pnum, tablenum, pdis);
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqVarTable(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->tableID;
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
void
vlistDefVarName(int vlistID, int varID, const char *name)
{
  (void) cdiDefKeyString(vlistID, varID, CDI_KEY_NAME, name);
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
void
vlistDefVarLongname(int vlistID, int varID, const char *longname)
{
  if (longname) (void) cdiDefKeyString(vlistID, varID, CDI_KEY_LONGNAME, longname);
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
void
vlistDefVarStdname(int vlistID, int varID, const char *stdname)
{
  if (stdname) (void) cdiDefKeyString(vlistID, varID, CDI_KEY_STDNAME, stdname);
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
void
vlistDefVarUnits(int vlistID, int varID, const char *units)
{
  if (units) (void) cdiDefKeyString(vlistID, varID, CDI_KEY_UNITS, units);
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
double
vlistInqVarMissval(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->missval;
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
void
vlistDefVarMissval(int vlistID, int varID, double missval)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  varP->missval = missval;
  varP->missvalused = true;
  cdiDefKeyFloat(vlistID, varID, CDI_KEY_MISSVAL, missval);
}

int
vlistInqVarValidrange(int vlistID, int varID, double *validrange)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (validrange != NULL && varP->lvalidrange)
  {
    validrange[0] = varP->validrange[0];
    validrange[1] = varP->validrange[1];
  }

  return (int) varP->lvalidrange;
}

void
vlistDefVarValidrange(int vlistID, int varID, const double *validrange)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  varP->validrange[0] = validrange[0];
  varP->validrange[1] = validrange[1];
  varP->lvalidrange = true;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

void
vlistDefVarTimetype(int vlistID, int varID, int timetype)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  if (varP->timetype != timetype)
  {
    varP->timetype = timetype;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqVarTimetype(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->timetype;
}

void
vlistDefVarTsteptype(int vlistID, int varID, int tsteptype)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  if (varP->tsteptype != tsteptype)
  {
    varP->tsteptype = tsteptype;
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
int
vlistInqVarTsteptype(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->tsteptype;
}

int
vlistInqVarMissvalUsed(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return (int) varP->missvalused;
}

void
vlistDefFlag(int vlistID, int varID, int levID, int flag)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  var_t *varP = vlistP_get_varP(__func__, vlistP, varID);

  levinfo_t li = DEFAULT_LEVINFO(levID);
  if (varP->levinfo == NULL)
  {
    if (flag == li.flag) return;
    cdiVlistCreateVarLevInfo(vlistP, varID);
  }

  varP->levinfo[levID].flag = flag;
  varP->flag = 0;

  int nlevs = zaxisInqSize(varP->zaxisID);
  for (int levelID = 0; levelID < nlevs; ++levelID)
  {
    if (varP->levinfo[levelID].flag)
    {
      varP->flag = 1;
      break;
    }
  }

  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

int
vlist_inq_flag(vlist_t *vlistP, int varID, int levelID)
{
  var_t *varP = vlistP_get_varP(__func__, vlistP, varID);

  if (varP->levinfo) return varP->levinfo[levelID].flag;

  levinfo_t li = DEFAULT_LEVINFO(levelID);
  return li.flag;
}

int
pvlistInqFlag(void *vlistP, int varID, int levelID)
{
  return vlist_inq_flag((vlist_t *) vlistP, varID, levelID);
}

int
vlistInqFlag(int vlistID, int varID, int levelID)
{
  return vlist_inq_flag(vlist_to_pointer(vlistID), varID, levelID);
}

int
vlistFindVar(int vlistID, int fvarID)
{
  int varID;
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  for (varID = 0; varID < vlistP->nvars; ++varID)
  {
    if (vlistP->vars[varID].fvarID == fvarID) break;
  }

  if (varID == vlistP->nvars)
  {
    varID = -1;
    Warning("varID not found for fvarID %d in vlistID %d!", fvarID, vlistID);
  }

  return varID;
}

int
vlistFindLevel(int vlistID, int fvarID, int flevelID)
{
  int levelID = -1;
  int varID = vlistFindVar(vlistID, fvarID);

  if (varID != -1)
  {
    var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
    int nlevs = zaxisInqSize(varP->zaxisID);
    for (levelID = 0; levelID < nlevs; ++levelID)
    {
      if (varP->levinfo[levelID].flevelID == flevelID) break;
    }

    if (levelID == nlevs)
    {
      levelID = -1;
      Warning("levelID not found for fvarID %d and levelID %d in vlistID %d!", fvarID, flevelID, vlistID);
    }
  }

  return levelID;
}

int
vlistMergedVar(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->mvarID;
}

int
vlistMergedLevel(int vlistID, int varID, int levelID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (varP->levinfo) return varP->levinfo[levelID].mlevelID;

  levinfo_t li = DEFAULT_LEVINFO(levelID);
  return li.mlevelID;
}

void
vlistDefIndex(int vlistID, int varID, int levelID, int index)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  var_t *varP = vlistP_get_varP(__func__, vlistP, varID);

  if (!varP->levinfo)
  {
    if (index == DEFAULT_LEVINFO(levelID).index) return;
    cdiVlistCreateVarLevInfo(vlistP, varID);
  }

  varP->levinfo[levelID].index = index;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

int
vlistInqIndex(int vlistID, int varID, int levelID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (varP->levinfo) return varP->levinfo[levelID].index;

  levinfo_t li = DEFAULT_LEVINFO(levelID);
  return li.index;
}

void
vlistChangeVarZaxis(int vlistID, int varID, int zaxisID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  var_t *varP = vlistP_get_varP(__func__, vlistP, varID);

  int nlevs1 = zaxisInqSize(varP->zaxisID);
  int nlevs2 = zaxisInqSize(zaxisID);

  if (nlevs1 != nlevs2) Error("Number of levels must not change!");

  int nvars = vlistP->nvars;
  int found = 0;
  int oldZaxisID = varP->zaxisID;
  for (int i = 0; i < varID && !found; ++i) { found = (vlistP->vars[i].zaxisID == oldZaxisID); }
  for (int i = varID + 1; i < nvars && !found; ++i) { found = (vlistP->vars[i].zaxisID == oldZaxisID); }

  if (found)
  {
    int nzaxis = vlistP->nzaxis;
    for (int i = 0; i < nzaxis; ++i)
      if (vlistP->zaxisIDs[i] == oldZaxisID) { vlistP->zaxisIDs[i] = zaxisID; }
  }
  else
    vlistAdd2ZaxisIDs(vlistP, zaxisID);

  varP->zaxisID = zaxisID;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

void
vlistChangeVarGrid(int vlistID, int varID, int gridID)
{
  vlist_t *vlistP = vlist_to_pointer(vlistID);
  var_t *varP = vlistP_get_varP(__func__, vlistP, varID);

  int nvars = vlistP->nvars;
  int index;
  for (index = 0; index < nvars; index++)
    if (index != varID)
      if (vlistP->vars[index].gridID == vlistP->vars[varID].gridID) break;

  if (index == nvars)
  {
    for (index = 0; index < vlistP->ngrids; index++)
      if (vlistP->gridIDs[index] == vlistP->vars[varID].gridID) { vlistP->gridIDs[index] = gridID; }
  }
  else
    vlistAdd2GridIDs(vlistP, gridID);

  varP->gridID = gridID;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

void
vlistDefVarCompType(int vlistID, int varID, int comptype)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (varP->comptype != comptype)
  {
    varP->comptype = comptype;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqVarCompType(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->comptype;
}

void
vlistDefVarCompLevel(int vlistID, int varID, int complevel)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (varP->complevel != complevel)
  {
    varP->complevel = complevel;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqVarCompLevel(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->complevel;
}

void
vlistDefVarNSB(int vlistID, int varID, int nsb)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  if (varP->nsb != nsb)
  {
    varP->nsb = nsb;
    reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
  }
}

int
vlistInqVarNSB(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return varP->nsb;
}

static int
vlistEncodeXyz(const int dimorder[3])
{
  return (short) (dimorder[0] * 100 + dimorder[1] * 10 + dimorder[2]);
}

static void
vlistDecodeXyz(int xyz, int outDimorder[3])
{
  outDimorder[0] = xyz / 100;
  xyz = xyz % 100;
  outDimorder[1] = xyz / 10;
  outDimorder[2] = xyz % 10;
}

static const short xyzStorVals[] = { 123, 132, 213, 231, 312, 321 };
enum
{
  numXYZStorVals = ARRAY_SIZE(xyzStorVals)
};

void
vlistDefVarXYZ(int vlistID, int varID, int xyz)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);

  if (xyz == 3) { xyz = 321; }

  // check xyz dimension order
  {
    int dimorder[3];
    vlistDecodeXyz(xyz, dimorder);
    int dimx = 0, dimy = 0, dimz = 0;
    for (int id = 0; id < 3; ++id)
    {
      switch (dimorder[id])
      {
        case 1: dimx++; break;
        case 2: dimy++; break;
        case 3: dimz++; break;
        default: dimorder[id] = 0; break;  // Ensure that we assign a valid dimension to this position.
      }
    }
    if (dimz > 1 || dimy > 1 || dimx > 1) { xyz = 321; }  // ZYX
    else
    {
      // clang-format off
      if (dimz == 0) for (int id = 0; id < 3; ++id) if (dimorder[id] == 0) { dimorder[id] = 3; break; }
      if (dimy == 0) for (int id = 0; id < 3; ++id) if (dimorder[id] == 0) { dimorder[id] = 2; break; }
      if (dimx == 0) for (int id = 0; id < 3; ++id) if (dimorder[id] == 0) { dimorder[id] = 1; break; }
      // clang-format on
      xyz = vlistEncodeXyz(dimorder);
    }
  }

  assert(xyz == 123 || xyz == 312 || xyz == 231 || xyz == 321 || xyz == 132 || xyz == 213);

  for (size_t i = 0; i < numXYZStorVals; ++i)
    if (xyz == xyzStorVals[i])
    {
      xyz = (int) i;
      break;
    }
  varP->xyz = (signed char) xyz;
  reshSetStatus(vlistID, &vlistOps, RESH_DESYNC_IN_USE);
}

void
vlistInqVarDimorder(int vlistID, int varID, int outDimorder[3])
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  vlistDecodeXyz(xyzStorVals[varP->xyz], outDimorder);
}

int
vlistInqVarXYZ(int vlistID, int varID)
{
  var_t *varP = vlistP_get_varP(__func__, vlist_to_pointer(vlistID), varID);
  return xyzStorVals[varP->xyz];
}

int
vlistVarCompare(vlist_t *a, int varIDA, vlist_t *b, int varIDB)
{
  xassert(a && b && varIDA >= 0 && varIDA < a->nvars && varIDB >= 0 && varIDB < b->nvars);
  var_t *pva = a->vars + varIDA, *pvb = b->vars + varIDB;
#define FCMP(f) ((pva->f) != (pvb->f))
#define FCMPFLT(f) (is_not_equal((pva->f), (pvb->f)))
// #define FCMPSTR(fs) ((pva->fs) != (pvb->fs) && strcmp((pva->fs), (pvb->fs)))
#define FCMP2(f) (namespaceResHDecode(pva->f).idx != namespaceResHDecode(pvb->f).idx)
  int diff = (int) FCMP(fvarID) | FCMP(mvarID) | FCMP(flag) | FCMP(param) | FCMP(datatype) | FCMP(timetype) | FCMP(tsteptype)
             | FCMP(xyz) | FCMP2(gridID) | FCMP2(zaxisID) | FCMP2(instID) | FCMP2(modelID) | FCMP2(tableID) | FCMP(missvalused)
             | FCMPFLT(missval) | FCMP(comptype) | FCMP(complevel) | FCMP(lvalidrange) | FCMPFLT(validrange[0])
             | FCMPFLT(validrange[1]);
#undef FCMP
#undef FCMPFLT
// #undef FCMPSTR
#undef FCMP2
  if ((diff |= ((pva->levinfo == NULL) ^ (pvb->levinfo == NULL)))) return 1;

  if (pva->levinfo)
  {
    int zaxisID = pva->zaxisID;
    size_t nlevs = (size_t) zaxisInqSize(zaxisID);
    diff |= (memcmp(pva->levinfo, pvb->levinfo, sizeof(levinfo_t) * nlevs) != 0);
    if (diff) return 1;
  }

  int natts = a->vars[varIDA].atts.nelems;
  if (natts != b->vars[varIDB].atts.nelems) return 1;
  for (int attID = 0; attID < natts; ++attID) diff |= cdi_att_compare(&a->vars[varIDA].atts, &b->vars[varIDB].atts, attID);

  int nkeys = a->vars[varIDA].keys.nelems;
  if (nkeys != b->vars[varIDB].keys.nelems) return 1;
  for (int keyID = 0; keyID < nkeys; ++keyID) diff |= cdi_key_compare(&a->vars[varIDA].keys, &b->vars[varIDB].keys, keyID);

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
