#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "vlist.h"


static void vlistvarInitEntry(int vlistID, int varID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].fvarID       = varID;
  vlistptr->vars[varID].flag         = 0;
  vlistptr->vars[varID].code         = 0;
  vlistptr->vars[varID].timeID       = CDI_UNDEFID;
  vlistptr->vars[varID].datatype     = CDI_UNDEFID;
  vlistptr->vars[varID].average      = 0;
  vlistptr->vars[varID].gridID       = CDI_UNDEFID;
  vlistptr->vars[varID].zaxisID      = CDI_UNDEFID;
  vlistptr->vars[varID].instID       = CDI_UNDEFID;
  vlistptr->vars[varID].modelID      = CDI_UNDEFID;
  vlistptr->vars[varID].tableID      = CDI_UNDEFID;
  vlistptr->vars[varID].missvalused  = FALSE;
  vlistptr->vars[varID].missval      = cdiDefaultMissval;
  vlistptr->vars[varID].addoffset    = 0.0;
  vlistptr->vars[varID].scalefactor  = 1.0;
  vlistptr->vars[varID].name         = NULL;
  vlistptr->vars[varID].longname     = NULL;
  vlistptr->vars[varID].stdname      = NULL;
  vlistptr->vars[varID].units        = NULL;
  vlistptr->vars[varID].nlevs        = 0;
  vlistptr->vars[varID].levinfo      = NULL;
}

static int vlistvarNewEntry(int vlistID)
{
  static char func[] = "vlistvarNewEntry";
  int varID = 0;
  int vlistvarSize;
  VARINFO *vlistvar;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistvarSize = vlistptr->varsAllocated;
  vlistvar     = vlistptr->vars;
  /*
    Look for a free slot in vlistvar.
    (Create the table the first time through).
  */
  if ( ! vlistvarSize )
    {
      int i;

      vlistvarSize = 2;
      vlistvar = (VARINFO *) malloc(vlistvarSize*sizeof(VARINFO));
      if ( vlistvar == NULL )
	{
          Message(func, "vlistvarSize = %d", vlistvarSize);
	  SysError(func, "Allocation of VARINFO failed");
	}

      for ( i = 0; i < vlistvarSize; i++ )
	vlistvar[i].isUsed = FALSE;
    }
  else
    {
      while ( varID < vlistvarSize )
	{
	  if ( ! vlistvar[varID].isUsed ) break;
	  varID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if ( varID == vlistvarSize )
    {
      int i;

      vlistvarSize = 2*vlistvarSize;
      vlistvar = (VARINFO *) realloc(vlistvar, vlistvarSize*sizeof(VARINFO));
      if ( vlistvar == NULL )
	{
          Message(func, "vlistvarSize = %d", vlistvarSize);
	  SysError(func, "Reallocation of VARINFO failed");
	}
      varID = vlistvarSize/2;

      for ( i = varID; i < vlistvarSize; i++ )
	vlistvar[i].isUsed = FALSE;
    }

  vlistptr->varsAllocated = vlistvarSize;
  vlistptr->vars          = vlistvar;

  vlistvarInitEntry(vlistID, varID);

  vlistptr->vars[varID].isUsed = TRUE;

  return (varID);
}

static void vlistCheckVarID(char func[], int vlistID, int varID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  if ( vlistptr == NULL )
    Error(func, "vlist undefined!");

  if ( varID < 0 || varID >= vlistptr->nvars )
    Error(func, "varID %d undefined!", varID);

  if ( ! vlistptr->vars[varID].isUsed )
    Error(func, "varID %d undefined!", varID);
}


/*
@Function  vlistDefVar
@Title     Define a Variable

@Prototype int vlistDefVar(int vlistID, int gridID, int zaxisID, int timeID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}
    @Item  zaxisID  Z-axis ID, from a previous call to @fref{zaxisCreate}
    @Item  timeID   One of the set of predefined CDI time identifiers.
                    The valid CDI time identifiers are @func{TIME_CONSTANT} and @func{TIME_VARIABLE}.

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
varID = vlistDefVar(vlistID, gridID, zaxisID, TIME_VARIABLE);
   ...
streamDefVlist(streamID, vlistID);
   ...
vlistDestroy(vlistID);
   ...
@EndSource
@EndFunction
*/
int vlistDefVar(int vlistID, int gridID, int zaxisID, int timeID)
{
  static char func[] = "vlistDefVar";
  int varID;
  int nlevs;
  int levID;
  int index;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  if ( CDI_Debug )
    Message(func, "gridID = %d  zaxisID = %d  timeID = %d", gridID, zaxisID, timeID);

  varID = vlistvarNewEntry(vlistID);

  vlistptr->nvars++;

  vlistptr->vars[varID].gridID  = gridID;
  vlistptr->vars[varID].zaxisID = zaxisID;
  vlistptr->vars[varID].timeID  = timeID;

  if ( timeID != TIME_VARIABLE && timeID != TIME_CONSTANT )
    {
      Message(func, "unexpected timeID %d. Set to TIME_VARIABLE", timeID);
      vlistptr->vars[varID].timeID = TIME_VARIABLE;	  
    }

  nlevs = zaxisInqSize(zaxisID);

  vlistptr->vars[varID].levinfo = (LEVINFO *) malloc(nlevs*sizeof(LEVINFO));

  for ( levID = 0; levID < nlevs; levID++ )
    {
      vlistptr->vars[varID].levinfo[levID].flag     = 0;
      vlistptr->vars[varID].levinfo[levID].index    = -1;
      vlistptr->vars[varID].levinfo[levID].flevelID = levID;
    }

  vlistptr->vars[varID].nlevs = nlevs;

  for ( index = 0; index < vlistptr->ngrids; index++ )
    if ( gridID == vlistptr->gridIDs[index] ) break;

  if ( index == vlistptr->ngrids )
    {
      if ( vlistptr->ngrids + 1 >= MAX_GRIDS )
	Error(func, "Maximum of %d grids reached", MAX_GRIDS);

      vlistptr->gridIDs[vlistptr->ngrids] = gridID;
      vlistptr->ngrids++;
    }

  for ( index = 0; index < vlistptr->nzaxis; index++ )
    if ( zaxisID == vlistptr->zaxisIDs[index] ) break;

  if ( index == vlistptr->nzaxis )
    {
      if ( vlistptr->nzaxis + 1 >= MAX_ZAXIS )
	Error(func, "Maximum of %d zaxis reached", MAX_ZAXIS);

      vlistptr->zaxisIDs[vlistptr->nzaxis] = zaxisID;
      vlistptr->nzaxis++;
    }

  vlistptr->vars[varID].code = -(varID + 1);

  return (varID);
}


/*
@Function  vlistDefVarCode
@Title     Define the code number of a Variable

@Prototype void vlistDefVarCode(int vlistID, int varID, int code)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  code     Code number

@Description
The function @func{vlistDefVarCode} defines the code number of a variable.

@EndFunction
*/
void vlistDefVarCode(int vlistID, int varID, int code)
{
  static char func[] = "vlistDefVarCode";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  vlistptr->vars[varID].code = code;
}


void vlistInqVar(int vlistID, int varID, int *gridID, int *zaxisID, int *timeID)
{
  static char func[] = "vlistInqVar";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  *gridID  = vlistptr->vars[varID].gridID;
  *zaxisID = vlistptr->vars[varID].zaxisID;
  *timeID  = vlistptr->vars[varID].timeID;

  return;
}


/*
@Function  vlistInqVarGrid
@Title     Get the Grid ID of a Variable

@Prototype int vlistInqVarGrid(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier

@Description
The function @func{vlistInqVarGrid} returns the grid ID of a variable.

@Result
@func{vlistInqVarGrid} returns the grid ID of the variable.

@EndFunction
*/
int vlistInqVarGrid(int vlistID, int varID)
{
  static char func[] = "vlistInqVarGrid";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].gridID);
}


/*
@Function  vlistInqVarZaxis
@Title     Get the Zaxis ID of a Variable

@Prototype int vlistInqVarZaxis(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier

@Description
The function @func{vlistInqVarZaxis} returns the zaxis ID of a variable.

@Result
@func{vlistInqVarZaxis} returns the zaxis ID of the variable.

@EndFunction
*/
int vlistInqVarZaxis(int vlistID, int varID)
{
  static char func[] = "vlistInqVarZaxis";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].zaxisID);
}

int vlistInqVarTime(int vlistID, int varID)
{
  static char func[] = "vlistInqVarTime";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].timeID);
}


/*
@Function  vlistInqVarCode
@Title     Get the Code number of a Variable

@Prototype int vlistInqVarCode(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier

@Description
The function @func{vlistInqVarCode} returns the code number of a variable.

@Result
@func{vlistInqVarCode} returns the code number of the variable.

@EndFunction
*/
int vlistInqVarCode(int vlistID, int varID)
{
  static char func[] = "vlistInqVarCode";
  VLIST *vlistptr;
  int code;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  code = vlistptr->vars[varID].code;

  if ( code < 0 && vlistptr->vars[varID].tableID != -1 && vlistptr->vars[varID].name != NULL )
    {
      tableInqParCode(vlistptr->vars[varID].tableID, vlistptr->vars[varID].name, &code);
    }

  return (code);
}


char *vlistInqVarNamePtr(int vlistID, int varID)
{
  static char func[] = "vlistInqVarNamePtr";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].name);
}

char *vlistInqVarLongnamePtr(int vlistID, int varID)
{
  static char func[] = "vlistInqVarLongnamePtr";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].longname);
}

char *vlistInqVarStdnamePtr(int vlistID, int varID)
{
  static char func[] = "vlistInqVarStdnamePtr";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].stdname);
}

char *vlistInqVarUnitsPtr(int vlistID, int varID)
{
  static char func[] = "vlistInqVarUnitsPtr";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].units);
}


/*
@Function  vlistInqVarName
@Title     Get the name of a Variable

@Prototype void vlistInqVarName(int vlistID, int varID, char *name)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  name     Variable name

@Description
The function @func{vlistInqVarName} returns the name of a variable.

@Result
@func{vlistInqVarName} returns the name of the variable to the parameter name.

@EndFunction
*/
void vlistInqVarName(int vlistID, int varID, char *name)
{
  static char func[] = "vlistInqVarName";
  int code, tableID;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].name == NULL )
    {
      code    = vlistptr->vars[varID].code;
      tableID = vlistptr->vars[varID].tableID;
      if ( tableInqParName(tableID, code, name) != 0 )
	sprintf(name, "var%d", code);
    }  
  else
    strcpy(name, vlistptr->vars[varID].name);

  return;
}


/*
@Function  vlistInqVarLongname
@Title     Get the longname of a Variable

@Prototype void vlistInqVarLongname(int vlistID, int varID, char *longname)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  longname Variable description

@Description
The function @func{vlistInqVarLongname} returns the longname of a variable.

@Result
@func{vlistInqVaeLongname} returns the longname of the variable to the parameter longname.

@EndFunction
*/
void vlistInqVarLongname(int vlistID, int varID, char *longname)
{
  static char func[] = "vlistInqVarLongname";
  int code, tableID;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].longname == NULL )
    {
      code    = vlistptr->vars[varID].code;
      tableID = vlistptr->vars[varID].tableID;
      if ( tableInqParLongname(tableID, code, longname) != 0 )
	longname[0] = '\0';
    }  
  else
    strcpy(longname, vlistptr->vars[varID].longname);

  return;
}


void vlistInqVarStdname(int vlistID, int varID, char *stdname)
{
  static char func[] = "vlistInqVarStdname";
  /*  int code, tableID; */
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].stdname == NULL )
    {
      /*
      code    = vlistptr->vars[varID].code;
      tableID = vlistptr->vars[varID].tableID;
      if ( tableInqParLongname(tableID, code, longname) != 0 )
      */
	stdname[0] = '\0';
    }  
  else
    strcpy(stdname, vlistptr->vars[varID].stdname);

  return;
}


/*
@Function  vlistInqVarUnits
@Title     Get the units of a Variable

@Prototype void vlistInqVarUnits(int vlistID, int varID, char *units)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  units    Variable units

@Description
The function @func{vlistInqVarUnits} returns the units of a variable.

@Result
@func{vlistInqVarUnits} returns the units of the variable to the parameter units.

@EndFunction
*/
void vlistInqVarUnits(int vlistID, int varID, char *units)
{
  static char func[] = "vlistInqVarUnits";
  int code, tableID;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].units == NULL )
    {
      code    = vlistptr->vars[varID].code;
      tableID = vlistptr->vars[varID].tableID;
      if ( tableInqParUnits(tableID, code, units) != 0 )
	units[0] = '\0';
    }  
  else
    strcpy(units, vlistptr->vars[varID].units);

  return;
}

int vlistInqVarID(int vlistID, int code)
{
  int varID;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  for ( varID = 0; varID < vlistptr->nvars; varID++ )
    if ( vlistptr->vars[varID].code == code ) break;

  if ( varID == vlistptr->nvars )
    {
      /*
      Warning(func, "varID not found for code %d\n", code);
      */
      varID = CDI_UNDEFID;
    }

  return (varID);
}

int vlistInqVarSize(int vlistID, int varID)
{
  static char func[] = "vlistInqVarSize";
  int size;
  int zaxisID, gridID;
  int nlevs, gridsize;
  int timeID;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  vlistInqVar(vlistID, varID, &gridID, &zaxisID, &timeID);

  nlevs = zaxisInqSize(zaxisID);

  gridsize = gridInqSize(gridID);

  size = gridsize*nlevs;

  return (size);
}


/*
@Function  vlistInqVarDatatype
@Title     Get the data type of a Variable

@Prototype int vlistInqVarDatatype(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier

@Description
The function @func{vlistInqVarDatatype} returns the data type of a variable.

@Result
@func{vlistInqVarDatatype} returns an identifier to the data type of the variable.
The valid CDI data types are @func{DATATYPE_PACK8}, @func{DATATYPE_PACK16}, @func{DATATYPE_PACK24},
@func{DATATYPE_FLT32} and @func{DATATYPE_FLT64}.

@EndFunction
*/
int vlistInqVarDatatype(int vlistID, int varID)
{
  static char func[] = "vlistInqVarDatatype";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].datatype);
}


/*
@Function  vlistDefVarDatatype
@Title     Define the data type of a Variable

@Prototype void vlistDefVarDatatype(int vlistID, int varID, int datatype)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  datatype The data type identifier.
                    The valid CDI data types are @func{DATATYPE_PACK8}, @func{DATATYPE_PACK16},
                    @func{DATATYPE_PACK24}, @func{DATATYPE_FLT32} and @func{DATATYPE_FLT64}.


@Description
The function @func{vlistDefVarDatatype} defines the data type of a variable.

@EndFunction
*/
void vlistDefVarDatatype(int vlistID, int varID, int datatype)
{
  static char func[] = "vlistDefVarDatatype";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  vlistptr->vars[varID].datatype = datatype;  
}

void vlistDefVarInstitut(int vlistID, int varID, int instID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].instID = instID;
}

int vlistInqVarInstitut(int vlistID, int varID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].instID);
}

void vlistDefVarModel(int vlistID, int varID, int modelID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].modelID = modelID;
}

int vlistInqVarModel(int vlistID, int varID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].modelID);
}

void vlistDefVarTable(int vlistID, int varID, int tableID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].tableID = tableID;
}

int vlistInqVarTable(int vlistID, int varID)
{
  int tableID;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  tableID = vlistptr->vars[varID].tableID;

  return (tableID);
}


/*
@Function  vlistDefVarName
@Title     Define the name of a Variable

@Prototype void vlistDefVarName(int vlistID, int varID, const char *name)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  name     Name of the variable

@Description
The function @func{vlistDefVarName} defines the name of a variable.

@EndFunction
*/
void vlistDefVarName(int vlistID, int varID, const char *name)
{
  static char func[] = "vlistDefVarName";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].name ) free(vlistptr->vars[varID].name);

  if ( name ) vlistptr->vars[varID].name = strdupx(name);
}


/*
@Function  vlistDefVarLongname
@Title     Define the long name of a Variable

@Prototype void vlistDefVarLongname(int vlistID, int varID, const char *longname)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  longname Long name of the variable

@Description
The function @func{vlistDefVarLongname} defines the long name of a variable.

@EndFunction
*/
void vlistDefVarLongname(int vlistID, int varID, const char *longname)
{
  static char func[] = "vlistDefVarLongname";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].longname ) free(vlistptr->vars[varID].longname);

  if ( longname ) vlistptr->vars[varID].longname = strdupx(longname);
}


void vlistDefVarStdname(int vlistID, int varID, const char *stdname)
{
  static char func[] = "vlistDefVarStdname";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].stdname ) free(vlistptr->vars[varID].stdname);

  if ( stdname ) vlistptr->vars[varID].stdname = strdupx(stdname);
}


/*
@Function  vlistDefVarUnits
@Title     Define the units of a Variable

@Prototype void vlistDefVarUnits(int vlistID, int varID, const char *units)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  units    Units of the variable

@Description
The function @func{vlistDefVarUnits} defines the units of a variable.

@EndFunction
*/
void vlistDefVarUnits(int vlistID, int varID, const char *units)
{
  static char func[] = "vlistDefVarUnits";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  if ( vlistptr->vars[varID].units ) free(vlistptr->vars[varID].units);

  if ( units ) vlistptr->vars[varID].units = strdupx(units);
}


/*
@Function  vlistInqVarMissval
@Title     Get the missing value of a Variable

@Prototype double vlistInqVarMissval(int vlistID, int varID)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier

@Description
The function @func{vlistInqVarMissval} returns the missing value of a variable.

@Result
@func{vlistInqVarMissval} returns the missing value of the variable.

@EndFunction
*/
double vlistInqVarMissval(int vlistID, int varID)
{
  static char func[] = "vlistInqVarMissval";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].missval);
}


double vlistInqVarScalefactor(int vlistID, int varID)
{
  static char func[] = "vlistInqVarScalefactor";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].scalefactor);
}

double vlistInqVarAddoffset(int vlistID, int varID)
{
  static char func[] = "vlistInqVarAddoffset";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  return (vlistptr->vars[varID].addoffset);
}


/*
@Function  vlistDefVarMissval
@Title     Define the missing value of a Variable

@Prototype void vlistDefVarMissval(int vlistID, int varID, double missval)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier
    @Item  missval  Missing value

@Description
The function @func{vlistDefVarMissval} defines the missing value of a variable.

@EndFunction
*/
void vlistDefVarMissval(int vlistID, int varID, double missval)
{
  static char func[] = "vlistDefVarMissval";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  vlistptr->vars[varID].missval = missval;
  vlistptr->vars[varID].missvalused = TRUE;
}

void vlistDefVarScalefactor(int vlistID, int varID, double scalefactor)
{
  static char func[] = "vlistDefVarScalefactor";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  vlistptr->vars[varID].scalefactor = scalefactor;
}

void vlistDefVarAddoffset(int vlistID, int varID, double addoffset)
{
  static char func[] = "vlistDefVarAddoffset";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  vlistptr->vars[varID].addoffset = addoffset;
}

void vlistDefVarAverage(int vlistID, int varID, int average)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].average = average;
}

int vlistInqVarAverage(int vlistID, int varID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].average);
}

void vlistDestroyVarName(int vlistID, int varID)
{
  static char func[] = "vlistDestroyVarName";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  if ( vlistptr->vars[varID].name )
    {
      free(vlistptr->vars[varID].name);
      vlistptr->vars[varID].name = NULL;
    }
}

void vlistDestroyVarLongname(int vlistID, int varID)
{
  static char func[] = "vlistDestroyVarLongname";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  if ( vlistptr->vars[varID].longname )
    {
      free(vlistptr->vars[varID].longname);
      vlistptr->vars[varID].longname = NULL;
    }
}

void vlistDestroyVarStdname(int vlistID, int varID)
{
  static char func[] = "vlistDestroyVarStdname";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  if ( vlistptr->vars[varID].stdname )
    {
      free(vlistptr->vars[varID].stdname);
      vlistptr->vars[varID].stdname = NULL;
    }
}

void vlistDestroyVarUnits(int vlistID, int varID)
{
  static char func[] = "vlistDestroyVarUnits";
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  if ( vlistptr->vars[varID].units )
    {
      free(vlistptr->vars[varID].units);
      vlistptr->vars[varID].units = NULL;
    }
}


void vlistDefVarTime(int vlistID, int varID, int timeID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].timeID = timeID;
}


int vlistInqVarMissvalUsed(int vlistID, int varID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].missvalused);
}


void vlistDefFlag(int vlistID, int varID, int levID, int flag)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].flag = flag;
  vlistptr->vars[varID].levinfo[levID].flag = flag;
}


int vlistInqFlag(int vlistID, int varID, int levID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].levinfo[levID].flag);
}


int vlistFindVar(int vlistID, int fvarID)
{
  static char func[] = "vlistFindVar";
  int varID;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  for ( varID = 0; varID < vlistptr->nvars; varID++ )
    {
      if ( vlistptr->vars[varID].fvarID == fvarID ) break;
    }

  if ( varID == vlistptr->nvars )
    {
      varID = -1;
      Message(func, "varID not found for fvarID %d in vlistID %d!", fvarID, vlistID);
    }

  return (varID);  
}


int vlistFindLevel(int vlistID, int fvarID, int flevelID)
{
  static char func[] = "vlistFindLevel";
  int varID;
  int levelID = -1;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  varID = vlistFindVar(vlistID, fvarID);

  if ( varID != -1 )
    {
      for ( levelID = 0; levelID < vlistptr->vars[varID].nlevs; levelID++ )
	{
	  if ( vlistptr->vars[varID].levinfo[levelID].flevelID == flevelID ) break;
	}

      if ( levelID == vlistptr->vars[varID].nlevs )
	{
	  levelID = -1;
	  Message(func, "levelID not found for fvarID %d and levelID %d in vlistID %d!",
		  fvarID, flevelID, vlistID);
	}
    }

  return (levelID);  
}


int vlistFlagVar(int vlistID, int varID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].fvarID);  
}


int vlistFlagLevel(int vlistID, int varID, int levelID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].levinfo[levelID].flevelID);  
}


void vlistDefIndex(int vlistID, int varID, int levelID, int index)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistptr->vars[varID].levinfo[levelID].index = index;  
}


int vlistInqIndex(int vlistID, int varID, int levelID)
{
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  return (vlistptr->vars[varID].levinfo[levelID].index);  
}


void vlistChangeVarZaxis(int vlistID, int varID, int zaxisID)
{
  static char func[] = "vlistChangeVarZaxis";
  int nlevs1, nlevs2;
  int nvars, index;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  nlevs1 = zaxisInqSize(vlistptr->vars[varID].zaxisID);
  nlevs2 = zaxisInqSize(zaxisID);

  if ( nlevs1 != nlevs2 ) Error(func, "Number of levels must not change!");

  nvars = vlistptr->nvars;
  for ( index = 0; index < nvars; index++ )
    if ( index != varID )
      if ( vlistptr->vars[index].zaxisID == vlistptr->vars[varID].zaxisID ) break;

  if ( index == nvars )
    {
      for ( index = 0; index < vlistptr->nzaxis; index++ )
	if ( vlistptr->zaxisIDs[index] == vlistptr->vars[varID].zaxisID )
	  vlistptr->zaxisIDs[index] = zaxisID;
    }
  else
    {
      for ( index = 0; index < vlistptr->nzaxis; index++ )
	if ( vlistptr->zaxisIDs[index] == zaxisID ) break;

      if ( index == vlistptr->nzaxis )
	{
	  if ( vlistptr->nzaxis + 1 >= MAX_ZAXIS )
	    Error(func, "Maximum of %d zaxis reached", MAX_ZAXIS);

	  vlistptr->zaxisIDs[vlistptr->nzaxis] = zaxisID;
	  vlistptr->nzaxis++;
	}
    }
  
  vlistptr->vars[varID].zaxisID = zaxisID;
}


void vlistChangeVarGrid(int vlistID, int varID, int gridID)
{
  static char func[] = "vlistChangeVarGrid";
  int nvars, index;
  VLIST *vlistptr;

  vlistptr = vlist_to_pointer(vlistID);

  vlistCheckVarID(func, vlistID, varID);

  nvars = vlistptr->nvars;
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
    {
      for ( index = 0; index < vlistptr->ngrids; index++ )
	if ( vlistptr->gridIDs[index] == gridID ) break;

      if ( index == vlistptr->ngrids )
	{
	  if ( vlistptr->ngrids + 1 >= MAX_GRIDS )
	    Error(func, "Maximum of %d grids reached", MAX_GRIDS);

	  vlistptr->gridIDs[vlistptr->ngrids] = gridID;
	  vlistptr->ngrids++;
	}
    }
  
  vlistptr->vars[varID].gridID = gridID;
}
