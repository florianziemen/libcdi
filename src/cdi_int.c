#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>

#include "binary.h"
#include "cdf_config.h"
#include "cdf.h"
#include "cdi.h"
#include "cdi_int.h"
#include "dmemory.h"
#include "file.h"
#include "gribapi.h"

#ifdef HAVE_LIBCGRIBEX
#include "cgribex.h"
#endif

#ifdef HAVE_TENSORSTORE
#include "tstore.h"
#endif

#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
pthread_mutex_t CDI_IO_Mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

int CDI_Default_Calendar = CALENDAR_PROLEPTIC;

int CDI_Default_InstID = CDI_UNDEFID;
int CDI_Default_ModelID = CDI_UNDEFID;
int CDI_Default_TableID = CDI_UNDEFID;
// int cdiNcMissingValue  = CDI_UNDEFID;
int CDI_Netcdf_Chunksizehint = CDI_UNDEFID;
bool CDI_Split_Ltype105 = false;

bool CDI_Ignore_Att_Coordinates = false;
bool CDI_Coordinates_Lon_Lat = false;
bool CDI_Ignore_Valid_Range = false;
bool CDI_Shuffle = false;
int CDI_Skip_Records = 0;
const char *CDI_GRIB1_Template = NULL;
const char *CDI_GRIB2_Template = NULL;
int CDI_Convention = CDI_CONVENTION_ECHAM;
int CDI_Inventory_Mode = 1;
int CDI_Version_Info = 1;
int CDI_Query_Abort = 1;
int CDI_Convert_Cubesphere = 1;
bool CDI_Read_Cell_Center = true;
bool CDI_Read_Cell_Corners = true;
int CDI_CMOR_Mode = 0;
int CDI_Reduce_Dim = 0;
int CDI_Reduce_DimX = 0;
int CDI_Reduce_DimY = 0;
int CDI_Reduce_DimZ = 0;
int CDI_Reduce_DimT = 0;
bool CDI_Test = false;
size_t CDI_Netcdf_Hdr_Pad = 0UL;
bool CDI_Copy_ChunkSpec = false;
bool CDI_Remove_ChunkSpec = false;
bool CDI_Cache_Info = false;
bool CDI_Cache_NC4 = false;
bool CDI_Cache_NCZ = true;
long CDI_CacheSize_In = -1L;
long CDI_CacheSize_Out = -1L;
long CDI_FilterSize_Min = 0;
size_t CDI_CacheSize_Max = 0UL;
bool CDI_Netcdf_Lazy_Grid_Load = false;

int cdiPartabIntern = 1;

double CDI_Default_Missval = -9.E33;
double CDI_Grid_Missval = -9999.;

// clang-format off
static const char *FileTypes[] = {
  "UNKNOWN",
  "GRIB",
  "GRIB2",
  "NetCDF",
  "NetCDF2",
  "NetCDF4",
  "NetCDF4c",
  "NetCDF5",
  "SERVICE",
  "EXTRA",
  "IEG",
  "NCZarr",
  "Zarr2",
  "Zarr3",
  "HDF5",
};
// clang-format on

bool CDI_Debug = false;  // If set to 1, debugging

bool CDI_gribapi_debug = false;
bool CDI_gribapi_grib1 = false;
bool CDI_Lock_IO = false;
bool CDI_Threadsafe = false;
int cdiDefaultLeveltype = -1;
bool cdiDataUnreduced = false;
bool cdiSortName = false;
bool cdiHaveMissval = false;

static long
cdi_getenv_int(const char *envName)
{
  long envValue = -1;

  char *envString = getenv(envName);
  if (envString)
  {
    long fact = 1;
    int len = (int) strlen(envString);
    for (int i = 0; i < len; i++)
    {
      if (!isdigit((int) envString[i]))
      {
        switch (tolower((int) envString[i]))
        {
          case 'k': fact = 1024; break;
          case 'm': fact = 1048576; break;
          case 'g': fact = 1073741824; break;
          default:
            fact = 0;
            Warning("Invalid number string in %s: %s", envName, envString);
            Warning("%s must comprise only digits [0-9].", envName);
            break;
        }
        break;
      }
    }

    if (fact) { envValue = fact * atol(envString); }

    if (CDI_Debug) Message("set %s to %ld", envName, envValue);
  }

  return envValue;
}

static void
cdiPrintDefaults(void)
{
  fprintf(stderr,
          "default instID     :  %d\n"
          "default modelID    :  %d\n"
          "default tableID    :  %d\n"
          "default missval    :  %g\n",
          CDI_Default_InstID, CDI_Default_ModelID, CDI_Default_TableID, CDI_Default_Missval);
}

#ifdef HAVE_LIBFDB5
#include <fdb5/fdb5_config.h>
#endif

void
cdiPrintVersion(void)
{
  fprintf(stdout, "     CDI library version : %s\n", cdiLibraryVersion());
#ifdef HAVE_LIBCGRIBEX
  fprintf(stdout, " cgribex library version : %s\n", cgribexLibraryVersion());
#endif
#ifdef HAVE_LIBGRIB_API
  fprintf(stdout, " ecCodes library version : %s\n", gribapiLibraryVersionString());
#endif
#ifdef HAVE_LIBNETCDF
  fprintf(stdout, "  NetCDF library version : %s\n", cdfLibraryVersion());
#endif
#ifdef HAVE_LIBSERVICE
  fprintf(stdout, "    exse library version : %s\n", srvLibraryVersion());
#endif
  fprintf(stdout, "    FILE library version : %s\n", fileLibraryVersion());
#ifdef HAVE_LIBFDB5
  fprintf(stdout, "    FDB5 library version : %s\n", fdb5_version());
#endif
#ifdef HAVE_TENSORSTORE
  fprintf(stdout, "     TensorStore version : %s\n", ts_lib_version());
#endif
}

static void
cdiPrintDatatypes(void)
{
#define XSTRING(x) #x
#define STRING(x) XSTRING(x)

  fprintf(stderr,
          "+-------------+--------+\n"
          "| types       |  bytes |\n"
          "+-------------+--------+\n"
          "| void *      |    %3d |\n"
          "+-------------+--------+\n"
          "| char        |    %3d |\n"
          "+-------------+--------+\n"
          "| bool        |    %3d |\n"
          "| short       |    %3d |\n"
          "| int         |    %3d |\n"
          "| long        |    %3d |\n"
          "| long long   |    %3d |\n"
          "| size_t      |    %3d |\n"
          "| off_t       |    %3d |\n"
          "+-------------+--------+\n"
          "| float       |    %3d |\n"
          "| double      |    %3d |\n"
          "| long double |    %3d |\n"
          "+-------------+--------+\n",
          (int) sizeof(void *), (int) sizeof(char), (int) sizeof(bool), (int) sizeof(short), (int) sizeof(int), (int) sizeof(long),
          (int) sizeof(long long), (int) sizeof(size_t), (int) sizeof(off_t), (int) sizeof(float), (int) sizeof(double),
          (int) sizeof(long double));

  fprintf(stderr,
          "| SizeType    | %-6s |\n"
          "+-------------+--------+\n",
          STRING(CDI_SIZE_TYPE));

  fprintf(stderr, "\n  byte ordering is %s\n\n",
          ((HOST_ENDIANNESS == CDI_BIGENDIAN)
               ? "BIGENDIAN"
               : ((HOST_ENDIANNESS == CDI_LITTLEENDIAN) ? "LITTLEENDIAN" : "Unhandled endianness!")));

#undef STRING
#undef XSTRING
}

void
cdiDebug(int level)
{
  unsigned ulevel = (level == 1) ? (1U << 16) : (unsigned) level;

  if (ulevel & 2) CDI_Debug = true;
  if (ulevel & 4) memDebug(1);
  if (ulevel & 8) fileDebug(1);

  if (ulevel & 16)
  {
#ifdef HAVE_LIBCGRIBEX
    gribSetDebug(1);
#endif
#ifdef HAVE_LIBNETCDF
    cdfDebug(1);
#endif
#ifdef HAVE_LIBSERVICE
    srvDebug(1);
#endif
#ifdef HAVE_LIBEXTRA
    extDebug(1);
#endif
#ifdef HAVE_LIBIEG
    iegDebug(1);
#endif
  }

  if (CDI_Debug)
  {
    cdiPrintDefaults();
    cdiPrintDatatypes();
  }
}

int
cdiHaveFiletype(int filetype)
{
  switch (filetype)
  {
#ifdef HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV: return 1;
#endif
#ifdef HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT: return 1;
#endif
#ifdef HAVE_LIBIEG
    case CDI_FILETYPE_IEG: return 1;
#endif
#ifdef HAVE_LIBGRIB
#if defined HAVE_LIBGRIB_API || defined HAVE_LIBCGRIBEX
    case CDI_FILETYPE_GRB: return 1;
#endif
#ifdef HAVE_LIBGRIB_API
    case CDI_FILETYPE_GRB2: return 1;
#endif
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NC: return 1;
#ifdef HAVE_NETCDF2
    case CDI_FILETYPE_NC2: return 1;
#endif
    case CDI_FILETYPE_NC4: return 1;
    case CDI_FILETYPE_NC4C: return 1;
#ifdef HAVE_NCZARR
    case CDI_FILETYPE_NCZARR: return 1;
#endif
#ifdef HAVE_NETCDF5
    case CDI_FILETYPE_NC5: return 1;
#endif
#endif
#ifdef HAVE_TENSORSTORE
    case CDI_FILETYPE_TSZARR2: return 1;
    case CDI_FILETYPE_TSZARR3: return 1;
#endif
  }

  return 0;
}

void
cdiDefTableID(int tableID)
{
  CDI_Default_TableID = tableID;
  int modelID = CDI_Default_ModelID = tableInqModel(tableID);
  CDI_Default_InstID = modelInqInstitut(modelID);
}

void
cdiSetEccodesGrib1(bool value)
{
#ifndef HAVE_LIBGRIB_API
  if (value)
  {
    Warning("ecCodes support not compiled in, used CGRIBEX to decode/encode GRIB1 records!");
    value = false;
  }
#endif
  CDI_gribapi_grib1 = value;
}

static int
calendar_from_name(const char *name)
{
  // clang-format off
	if (strStartsWith(name, "standard"))  return CALENDAR_STANDARD;
	if (strStartsWith(name, "gregorian")) return CALENDAR_GREGORIAN;
	if (strStartsWith(name, "prolepc"))   return CALENDAR_PROLEPTIC;
	if (strStartsWith(name, "360"))       return CALENDAR_360DAYS;
	if (strStartsWith(name, "365"))       return CALENDAR_365DAYS;
	if (strStartsWith(name, "366"))       return CALENDAR_366DAYS;
	if (strStartsWith(name, "none") )     return CALENDAR_NONE;
  // clang-format on
  return CDI_UNDEFID;
}

void
cdi_finalize(void)
{
  /*
  int streamIDs[32] = {};
  int numIDs = (int) ARRAY_SIZE(streamIDs);
  reshGetResHListOfType(numIDs, streamIDs, &streamOps);
  for (int i = 0; i < numIDs; ++i)
  {
    if (streamIDs[i] > 0) streamClose(streamIDs[i]);
  }
  */
}

void
cdi_initialize(void)
{
  static bool Init_CDI = false;
  if (!Init_CDI)
  {
    Init_CDI = true;

    atexit(cdi_finalize);

#ifdef HAVE_LIBCGRIBEX
    gribFixZSE(1);    // 1: Fix ZeroShiftError of simple packed spherical harmonics
    gribSetConst(1);  // 1: Don't pack constant fields on regular grids
#endif
#ifdef HAVE_LIBGRIB_API
    grib_multi_support_off(NULL);
#endif

    // clang-format off
#define GETENV_STR(name, var)  do { char *v = getenv(name); if (v) var = v; } while(0)
#define GETENV_BOOL(name, var) do { char *v = getenv(name); if (v) var = (v[0] == 't' || v[0] == 'T' || atoi(v) != 0); } while(0)
#define GETENV_FLT(name, var)  do { char *v = getenv(name); if (v) var = atof(v); } while(0)
#define GETENV_INT(name, var)  do { char *v = getenv(name); if (v) var = atoi(v); } while(0)
#define GETENV_SIZE(name, var)  do { long v = cdi_getenv_int(name); if (v >= 0) var = v; } while(0)
    // clang-format on

    GETENV_STR("CDI_GRIB1_TEMPLATE", CDI_GRIB1_Template);
    GETENV_STR("CDI_GRIB2_TEMPLATE", CDI_GRIB2_Template);

    GETENV_BOOL("CDI_DEBUG", CDI_Debug);
    GETENV_BOOL("CDI_GRIBAPI_DEBUG", CDI_gribapi_debug);
    GETENV_BOOL("CDI_ECCODES_DEBUG", CDI_gribapi_debug);
    GETENV_BOOL("CDI_LOCK_IO", CDI_Lock_IO);
    GETENV_BOOL("CDI_READ_CELL_CENTER", CDI_Read_Cell_Center);
    GETENV_BOOL("CDI_READ_CELL_CORNERS", CDI_Read_Cell_Corners);
    GETENV_BOOL("CDI_REGULARGRID", cdiDataUnreduced);
    GETENV_BOOL("CDI_SORTNAME", cdiSortName);
    GETENV_BOOL("CDI_HAVE_MISSVAL", cdiHaveMissval);
    GETENV_BOOL("CDI_COPY_CHUNKSPEC", CDI_Copy_ChunkSpec);
    GETENV_BOOL("CDI_REMOVE_CHUNKSPEC", CDI_Remove_ChunkSpec);
    GETENV_BOOL("CDI_CACHE_INFO", CDI_Cache_Info);
    GETENV_BOOL("CDI_CACHE_NC4", CDI_Cache_NC4);
    GETENV_BOOL("CDI_CACHE_NCZ", CDI_Cache_NCZ);
    GETENV_BOOL("CDI_CACHE_NC", CDI_Cache_NC4);
    GETENV_BOOL("CDI_CACHE_NC", CDI_Cache_NCZ);
    GETENV_BOOL("CDI_SHUFFLE", CDI_Shuffle);
    GETENV_BOOL("CDI_TEST", CDI_Test);
    GETENV_BOOL("SPLIT_LTYPE_105", CDI_Split_Ltype105);
    GETENV_BOOL("IGNORE_ATT_COORDINATES", CDI_Ignore_Att_Coordinates);
    GETENV_BOOL("CDI_COORDINATES_LONLAT", CDI_Coordinates_Lon_Lat);
    GETENV_BOOL("IGNORE_VALID_RANGE", CDI_Ignore_Valid_Range);

    GETENV_FLT("CDI_MISSVAL", CDI_Default_Missval);

    GETENV_INT("CDI_LEVELTYPE", cdiDefaultLeveltype);
    GETENV_INT("NC_CHUNKSIZEHINT", CDI_Netcdf_Chunksizehint);
    GETENV_INT("CDI_SKIP_RECORDS", CDI_Skip_Records);
    if (CDI_Skip_Records < 0) CDI_Skip_Records = 0;
    int CDI_Eccodes_grib1 = -1;
    GETENV_INT("CDI_ECCODES_GRIB1", CDI_Eccodes_grib1);
    if (CDI_Eccodes_grib1 >= 0) { cdiSetEccodesGrib1((bool) CDI_Eccodes_grib1); }

    GETENV_SIZE("CDI_NETCDF_HDR_PAD", CDI_Netcdf_Hdr_Pad);
    GETENV_SIZE("CDI_CACHESIZE_IN", CDI_CacheSize_In);
    GETENV_SIZE("CDI_CACHESIZE_OUT", CDI_CacheSize_Out);
    GETENV_SIZE("CDI_CACHESIZE_MAX", CDI_CacheSize_Max);
    GETENV_SIZE("CDI_FILTERSIZE_MIN", CDI_FilterSize_Min);

    char *envstr;
    envstr = getenv("CDI_CONVENTION");
    if (envstr)
    {
      if (str_is_equal(envstr, "CF") || str_is_equal(envstr, "cf"))
      {
        CDI_Convention = CDI_CONVENTION_CF;
        if (CDI_Debug) Message("CDI convention was set to CF!");
      }
    }

    envstr = getenv("CDI_INVENTORY_MODE");
    if (envstr)
    {
      if (strStartsWith(envstr, "time"))
      {
        CDI_Inventory_Mode = 2;
        if (CDI_Debug) Message("Inventory mode was set to timestep!");
      }
    }

    envstr = getenv("CDI_QUERY_ABORT");
    if (envstr)
    {
      int ival = atoi(envstr);
      if (ival == 0 || ival == 1)
      {
        CDI_Query_Abort = ival;
        if (CDI_Debug) Message("CDI_Query_Abort = %s", envstr);
      }
    }

    envstr = getenv("CDI_VERSION_INFO");
    if (envstr)
    {
      int ival = atoi(envstr);
      if (ival == 0 || ival == 1)
      {
        CDI_Version_Info = ival;
        if (CDI_Debug) Message("CDI_Version_Info = %s", envstr);
      }
    }

    envstr = getenv("CDI_CONVERT_CUBESPHERE");
    if (envstr)
    {
      int ival = atoi(envstr);
      if (ival == 0 || ival == 1)
      {
        CDI_Convert_Cubesphere = ival;
        if (CDI_Debug) Message("CDI_Convert_Cubesphere = %s", envstr);
      }
    }

    envstr = getenv("CDI_CALENDAR");
    if (envstr)
    {
      int calendar = calendar_from_name(envstr);
      if (calendar != CDI_UNDEFID) { CDI_Default_Calendar = calendar; }
      if (CDI_Debug) Message("Default calendar set to %s!", envstr);
    }
#ifdef HAVE_LIBCGRIBEX
    gribSetCalendar(CDI_Default_Calendar);
#endif

    envstr = getenv("PARTAB_INTERN");
    if (envstr) { cdiPartabIntern = atoi(envstr); }
  }
}

const char *
strfiletype(int filetype)
{
  return (filetype > 0 && filetype < (int) ARRAY_SIZE(FileTypes)) ? FileTypes[filetype] : FileTypes[0];
}

void
cdiDefGlobal(const char *string, int value)
{
  // clang-format off
  if      (str_is_equal(string, "REGULARGRID"))           { cdiDataUnreduced = (bool) value; }
  else if (str_is_equal(string, "LOCKIO"))                { CDI_Lock_IO = (bool) value; }
  else if (str_is_equal(string, "THREADSAFE"))            { CDI_Threadsafe = (bool) value; }
  else if (str_is_equal(string, "ECCODES_DEBUG"))         { CDI_gribapi_debug = (bool) value; }
  else if (str_is_equal(string, "ECCODES_GRIB1"))         { cdiSetEccodesGrib1((bool) value); }
  else if (str_is_equal(string, "SORTNAME"))              { cdiSortName = (bool) value; }
  else if (str_is_equal(string, "HAVE_MISSVAL"))          { cdiHaveMissval = (bool) value; }
  else if (str_is_equal(string, "NC_CHUNKSIZEHINT"))      { CDI_Netcdf_Chunksizehint = value; }
  else if (str_is_equal(string, "READ_CELL_CENTER"))      { CDI_Read_Cell_Center = value; }
  else if (str_is_equal(string, "READ_CELL_CORNERS"))     { CDI_Read_Cell_Corners = value; }
  else if (str_is_equal(string, "CMOR_MODE"))             { CDI_CMOR_Mode = value; }
  else if (str_is_equal(string, "REDUCE_DIMX"))           { CDI_Reduce_DimX = value; }
  else if (str_is_equal(string, "REDUCE_DIMY"))           { CDI_Reduce_DimY = value; }
  else if (str_is_equal(string, "REDUCE_DIMZ"))           { CDI_Reduce_DimZ = value; }
  else if (str_is_equal(string, "REDUCE_DIMT"))           { CDI_Reduce_DimT = value; }
  else if (str_is_equal(string, "REDUCE_DIM"))            { CDI_Reduce_Dim = value; }
  else if (str_is_equal(string, "NETCDF_HDR_PAD"))        { CDI_Netcdf_Hdr_Pad = (size_t) value; }
  else if (str_is_equal(string, "NETCDF_LAZY_GRID_LOAD")) { CDI_Netcdf_Lazy_Grid_Load = (bool) value; }
  else if (str_is_equal(string, "COPY_CHUNKSPEC"))        { CDI_Copy_ChunkSpec = (bool) value; }
  else if (str_is_equal(string, "REMOVE_CHUNKSPEC"))      { CDI_Remove_ChunkSpec = (bool) value; }
  else Warning("Unsupported global key: %s", string);
  // clang-format on
}

void
cdiDefMissval(double missval)
{
  cdi_initialize();

  CDI_Default_Missval = missval;
}

double
cdiInqMissval(void)
{
  cdi_initialize();

  return CDI_Default_Missval;
}

bool
cdiFiletypeIsExse(int filetype)
{
  return (filetype == CDI_FILETYPE_SRV || filetype == CDI_FILETYPE_EXT || filetype == CDI_FILETYPE_IEG);
}

int
cdiBaseFiletype(int filetype)
{
  switch (filetype)
  {
    case CDI_FILETYPE_GRB:
    case CDI_FILETYPE_GRB2: return CDI_FILETYPE_GRIB;
    case CDI_FILETYPE_NC:
    case CDI_FILETYPE_NC2:
    case CDI_FILETYPE_NC4:
    case CDI_FILETYPE_NC4C:
    case CDI_FILETYPE_NCZARR:
    case CDI_FILETYPE_NC5: return CDI_FILETYPE_NETCDF;
    case CDI_FILETYPE_TSZARR2:
    case CDI_FILETYPE_TSZARR3: return CDI_FILETYPE_TSZARR;
  }

  return filetype;
}

void
stream_def_accesstype(stream_t *s, int type)
{
  if (s->accesstype == CDI_UNDEFID) { s->accesstype = type; }
  else if (s->accesstype != type)
    Error("Changing access type from %s not allowed!", s->accesstype == TYPE_REC ? "REC to VAR" : "VAR to REC");
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
