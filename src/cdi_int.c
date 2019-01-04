#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include "binary.h"
#include "cdf.h"
#include "cdi.h"
#include "cdi_int.h"
#include "dmemory.h"
#include "file.h"
#include "gribapi.h"

#ifdef  HAVE_LIBCGRIBEX
#include "cgribex.h"
#endif

int cdiDefaultCalendar = CALENDAR_PROLEPTIC;

int cdiDefaultInstID   = CDI_UNDEFID;
int cdiDefaultModelID  = CDI_UNDEFID;
int cdiDefaultTableID  = CDI_UNDEFID;
//int cdiNcMissingValue  = CDI_UNDEFID;
int cdiNcChunksizehint = CDI_UNDEFID;
int cdiChunkType       = CDI_CHUNK_GRID;
int cdiSplitLtype105   = CDI_UNDEFID;

bool cdiIgnoreAttCoordinates = false;
bool cdiCoordinatesLonLat    = false;
bool cdiIgnoreValidRange     = false;
int cdiSkipRecords           = 0;
int CDI_convention           = CDI_CONVENTION_ECHAM;
int CDI_inventory_mode       = 1;
int CDO_version_info         = 1;
int CDI_read_cell_corners    = 1;
int CDI_cmor_mode            = 0;
int CDI_reduce_dim           = 0;
size_t CDI_netcdf_hdr_pad    = 0UL;
bool CDI_netcdf_lazy_grid_load = false;

char *cdiPartabPath   = NULL;
int   cdiPartabIntern = 1;

double CDI_default_missval = -9.E33;
double CDI_grid_missval    = -9999.;

static const char Filetypes[][9] = {
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
  "HDF5",
};

int CDI_Debug   = 0;    // If set to 1, debugging
int CDI_Recopt  = 0;

bool CDI_gribapi_debug  = false;
bool CDI_gribapi_grib1  = false;
int cdiDefaultLeveltype = -1;
int cdiDataUnreduced = 0;
int cdiSortName = 0;
int cdiSortParam = 0;
int cdiHaveMissval = 0;


static
long cdiGetenvInt(const char *envName)
{
  long envValue = -1;

  char *envString = getenv(envName);
  if ( envString )
    {
      long fact = 1;
      int len = (int) strlen(envString);
      for ( int loop = 0; loop < len; loop++ )
	{
	  if ( ! isdigit((int) envString[loop]) )
	    {
	      switch ( tolower((int) envString[loop]) )
		{
		case 'k':  fact = 1024;        break;
		case 'm':  fact = 1048576;     break;
		case 'g':  fact = 1073741824;  break;
		default:
		  fact = 0;
		  Message("Invalid number string in %s: %s", envName, envString);
		  Warning("%s must comprise only digits [0-9].",envName);
		  break;
		}
	      break;
	    }
	}

      if ( fact ) envValue = fact*atol(envString);

      if ( CDI_Debug ) Message("set %s to %ld", envName, envValue);
    }

  return envValue;
}

static
void cdiPrintDefaults(void)
{
  fprintf(stderr, "default instID     :  %d\n"
          "default modelID    :  %d\n"
          "default tableID    :  %d\n"
          "default missval    :  %g\n", cdiDefaultInstID,
          cdiDefaultModelID, cdiDefaultTableID, CDI_default_missval);
}

void cdiPrintVersion(void)
{
  fprintf(stderr, "     CDI library version : %s\n", cdiLibraryVersion());
#ifdef  HAVE_LIBCGRIBEX
  fprintf(stderr, " cgribex library version : %s\n", cgribexLibraryVersion());
#endif
#ifdef  HAVE_LIBGRIB_API
  fprintf(stderr, " ecCodes library version : %s\n", gribapiLibraryVersionString());
#endif
#ifdef  HAVE_LIBNETCDF
  fprintf(stderr, "  NetCDF library version : %s\n", cdfLibraryVersion());
#endif
#ifdef  HAVE_NC4HDF5
  fprintf(stderr, "    hdf5 library version : %s\n", hdfLibraryVersion());
#endif
#ifdef  HAVE_LIBSERVICE
  fprintf(stderr, "    exse library version : %s\n", srvLibraryVersion());
#endif
  fprintf(stderr, "    FILE library version : %s\n", fileLibraryVersion());
}

static
void cdiPrintDatatypes(void)
{
#define XSTRING(x)	#x
#define STRING(x)	XSTRING(x)
  fprintf (stderr, "+-------------+-------+\n"
           "| types       | bytes |\n"
           "+-------------+-------+\n"
           "| void *      |   %3d |\n"
           "+-------------+-------+\n"
           "| char        |   %3d |\n"
           "+-------------+-------+\n"
           "| bool        |   %3d |\n"
           "| short       |   %3d |\n"
           "| int         |   %3d |\n"
           "| long        |   %3d |\n"
           "| long long   |   %3d |\n"
           "| size_t      |   %3d |\n"
           "| off_t       |   %3d |\n"
           "+-------------+-------+\n"
           "| float       |   %3d |\n"
           "| double      |   %3d |\n"
           "| long double |   %3d |\n"
           "+-------------+-------+\n\n"
           "+-------------+-----------+\n"
           "| INT32       | %-9s |\n"
           "| INT64       | %-9s |\n"
           "| FLT32       | %-9s |\n"
           "| FLT64       | %-9s |\n"
           "+-------------+-----------+\n"
           "\n  byte ordering is %s\n\n",
           (int) sizeof(void *), (int) sizeof(char), (int) sizeof(bool),
           (int) sizeof(short), (int) sizeof(int), (int) sizeof(long), (int) sizeof(long long),
           (int) sizeof(size_t), (int) sizeof(off_t),
           (int) sizeof(float), (int) sizeof(double), (int) sizeof(long double),
           STRING(INT32), STRING(INT64), STRING(FLT32), STRING(FLT64),
           ((HOST_ENDIANNESS == CDI_BIGENDIAN) ? "BIGENDIAN"
            : ((HOST_ENDIANNESS == CDI_LITTLEENDIAN) ? "LITTLEENDIAN"
               : "Unhandled endianness!")));
#undef STRING
#undef XSTRING
}


void cdiDebug(int level)
{
  unsigned ulevel = (unsigned) level;

  if ( ulevel == 1 || (ulevel &  2) ) CDI_Debug = 1;

  if ( CDI_Debug ) Message("debug level %d", level);

  if ( ulevel == 1 || (ulevel &  4) ) memDebug(1);

  if ( ulevel == 1 || (ulevel &  8) ) fileDebug(1);

  if ( ulevel == 1 || (ulevel & 16) )
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

  if ( CDI_Debug )
    {
      cdiPrintDefaults();
      cdiPrintDatatypes();
    }
}


int cdiHaveFiletype(int filetype)
{
  int status = 0;

  switch (filetype)
    {
#ifdef  HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV:  status = 1; break;
#endif
#ifdef  HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT:  status = 1; break;
#endif
#ifdef  HAVE_LIBIEG
    case CDI_FILETYPE_IEG:  status = 1; break;
#endif
#ifdef  HAVE_LIBGRIB
#if  defined  HAVE_LIBGRIB_API || defined  HAVE_LIBCGRIBEX
    case CDI_FILETYPE_GRB:  status = 1; break;
#endif
#ifdef  HAVE_LIBGRIB_API
    case CDI_FILETYPE_GRB2: status = 1; break;
#endif
#endif
#ifdef  HAVE_LIBNETCDF
    case CDI_FILETYPE_NC:   status = 1; break;
#ifdef  HAVE_NETCDF2
    case CDI_FILETYPE_NC2:  status = 1; break;
#endif
#ifdef  HAVE_NETCDF4
    case CDI_FILETYPE_NC4:  status = 1; break;
    case CDI_FILETYPE_NC4C: status = 1; break;
#endif
#ifdef  HAVE_NETCDF5
    case CDI_FILETYPE_NC5:  status = 1; break;
#endif
#endif
    default: status = 0; break;
    }

  return status;
}


void cdiDefTableID(int tableID)
{
  cdiDefaultTableID = tableID;
  int modelID = cdiDefaultModelID = tableInqModel(tableID);
  cdiDefaultInstID = modelInqInstitut(modelID);
}

static
void cdiSetChunk(const char *chunkAlgo)
{
  int algo = -1;

  if      ( strcmp("auto",  chunkAlgo) == 0 ) algo = CDI_CHUNK_AUTO;
  else if ( strcmp("grid",  chunkAlgo) == 0 ) algo = CDI_CHUNK_GRID;
  else if ( strcmp("lines", chunkAlgo) == 0 ) algo = CDI_CHUNK_LINES;
  else
    Warning("Invalid environment variable CDI_CHUNK_ALGO: %s", chunkAlgo);

  if ( algo != -1 )
    {
      cdiChunkType = algo;
      if ( CDI_Debug ) Message("set ChunkAlgo to %s", chunkAlgo);
    }
}


void cdiSetEccodesGrib1(bool value)
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


void cdiInitialize(void)
{
  static bool Init_CDI = false;

  if ( ! Init_CDI )
    {
      Init_CDI = true;
      char *envstr;
      long value;

#ifdef  HAVE_LIBCGRIBEX
      gribFixZSE(1);   // 1: Fix ZeroShiftError of simple packed spherical harmonics
      gribSetConst(1); // 1: Don't pack constant fields on regular grids
#endif

      value = cdiGetenvInt("CDI_DEBUG");
      if ( value >= 0 ) CDI_Debug = (int) value;

      value = cdiGetenvInt("CDI_GRIBAPI_DEBUG");
      if ( value >= 0 ) CDI_gribapi_debug = (bool) value;

      value = cdiGetenvInt("CDI_ECCODES_DEBUG");
      if ( value >= 0 ) CDI_gribapi_debug = (bool) value;

      value = cdiGetenvInt("CDI_ECCODES_GRIB1");
      if ( value >= 0 ) cdiSetEccodesGrib1((bool) value);

      value = cdiGetenvInt("CDI_READ_CELL_CORNERS");
      if ( value >= 0 ) CDI_read_cell_corners = (int) value;

      value = cdiGetenvInt("CDI_RECOPT");
      if ( value >= 0 ) CDI_Recopt = (int) value;

      value = cdiGetenvInt("CDI_REGULARGRID");
      if ( value >= 0 ) cdiDataUnreduced = (int) value;

      value = cdiGetenvInt("CDI_SORTNAME");
      if ( value >= 0 ) cdiSortName = (int) value;

      value = cdiGetenvInt("CDI_SORTPARAM");
      if ( value >= 0 ) cdiSortParam = (int) value;

      value = cdiGetenvInt("CDI_HAVE_MISSVAL");
      if ( value >= 0 ) cdiHaveMissval = (int) value;

      value = cdiGetenvInt("CDI_LEVELTYPE");
      if ( value >= 0 ) cdiDefaultLeveltype = (int) value;

      value = cdiGetenvInt("CDI_NETCDF_HDR_PAD");
      if ( value >= 0 ) CDI_netcdf_hdr_pad = (size_t) value;

      envstr = getenv("CDI_MISSVAL");
      if ( envstr ) CDI_default_missval = atof(envstr);
      /*
      envstr = getenv("NC_MISSING_VALUE");
      if ( envstr ) cdiNcMissingValue = atoi(envstr);
      */
      envstr = getenv("NC_CHUNKSIZEHINT");
      if ( envstr ) cdiNcChunksizehint = atoi(envstr);

      envstr = getenv("CDI_CHUNK_ALGO");
      if ( envstr ) cdiSetChunk(envstr);

      envstr = getenv("SPLIT_LTYPE_105");
      if ( envstr ) cdiSplitLtype105 = atoi(envstr);

      envstr = getenv("IGNORE_ATT_COORDINATES");
      if ( envstr ) cdiIgnoreAttCoordinates = atoi(envstr) > 0;

      envstr = getenv("CDI_COORDINATES_LONLAT");
      if ( envstr ) cdiCoordinatesLonLat = atoi(envstr) > 0;

      envstr = getenv("IGNORE_VALID_RANGE");
      if ( envstr ) cdiIgnoreValidRange = atoi(envstr) > 0;

      envstr = getenv("CDI_SKIP_RECORDS");
      if ( envstr )
	{
	  cdiSkipRecords = atoi(envstr);
	  cdiSkipRecords = cdiSkipRecords > 0 ? cdiSkipRecords : 0;
	}

      envstr = getenv("CDI_CONVENTION");
      if ( envstr )
	{
	  if ( strcmp(envstr, "CF") == 0 || strcmp(envstr, "cf") == 0 )
	    {
	      CDI_convention = CDI_CONVENTION_CF;
	      if ( CDI_Debug )
		Message("CDI convention was set to CF!");
	    }
	}

      envstr = getenv("CDI_INVENTORY_MODE");
      if ( envstr )
	{
	  if ( strncmp(envstr, "time", 4) == 0 )
	    {
	      CDI_inventory_mode = 2;
	      if ( CDI_Debug )
		Message("Inventory mode was set to timestep!");
	    }
	}

      envstr = getenv("CDI_VERSION_INFO");
      if ( envstr )
        {
          int ival = atoi(envstr);
          if ( ival == 0 || ival == 1 )
            {
              CDO_version_info = ival;
              if ( CDI_Debug )
                Message("CDO_version_info = %s", envstr);
            }
        }

      envstr = getenv("CDI_CALENDAR");
      if ( envstr )
	{
          // clang-format off
	  if      (strncmp(envstr, "standard", 8)  == 0) cdiDefaultCalendar = CALENDAR_STANDARD;
	  else if (strncmp(envstr, "gregorian", 9) == 0) cdiDefaultCalendar = CALENDAR_GREGORIAN;
	  else if (strncmp(envstr, "proleptic", 9) == 0) cdiDefaultCalendar = CALENDAR_PROLEPTIC;
	  else if (strncmp(envstr, "360days", 7)   == 0) cdiDefaultCalendar = CALENDAR_360DAYS;
	  else if (strncmp(envstr, "365days", 7)   == 0) cdiDefaultCalendar = CALENDAR_365DAYS;
	  else if (strncmp(envstr, "366days", 7)   == 0) cdiDefaultCalendar = CALENDAR_366DAYS;
	  else if (strncmp(envstr, "none", 4)      == 0) cdiDefaultCalendar = CALENDAR_NONE;
          // clang-format on
	  if ( CDI_Debug ) Message("Default calendar set to %s!", envstr);
	}
#ifdef  HAVE_LIBCGRIBEX
      gribSetCalendar(cdiDefaultCalendar);
#endif

      envstr = getenv("PARTAB_INTERN");
      if ( envstr ) cdiPartabIntern = atoi(envstr);

      envstr = getenv("PARTAB_PATH");
      if ( envstr ) cdiPartabPath = strdup(envstr);
    }
}


const char *strfiletype(int filetype)
{
  int size = (int) (sizeof(Filetypes)/sizeof(char *));
  return (filetype > 0 && filetype < size) ? Filetypes[filetype] : Filetypes[0];
}


void cdiDefGlobal(const char *string, int value)
{
  // clang-format off
  if      (strcmp(string, "REGULARGRID")           == 0) cdiDataUnreduced = value;
  else if (strcmp(string, "ECCODES_DEBUG")         == 0) CDI_gribapi_debug = (bool) value;
  else if (strcmp(string, "ECCODES_GRIB1")         == 0) cdiSetEccodesGrib1((bool) value);
  else if (strcmp(string, "SORTNAME")              == 0) cdiSortName = value;
  else if (strcmp(string, "SORTPARAM")             == 0) cdiSortParam = value;
  else if (strcmp(string, "HAVE_MISSVAL")          == 0) cdiHaveMissval = value;
  else if (strcmp(string, "NC_CHUNKSIZEHINT")      == 0) cdiNcChunksizehint = value;
  else if (strcmp(string, "READ_CELL_CORNERS")     == 0) CDI_read_cell_corners = value;
  else if (strcmp(string, "CMOR_MODE")             == 0) CDI_cmor_mode = value;
  else if (strcmp(string, "REDUCE_DIM")            == 0) CDI_reduce_dim = value;
  else if (strcmp(string, "NETCDF_HDR_PAD")        == 0) CDI_netcdf_hdr_pad = (size_t) value;
  else if (strcmp(string, "NETCDF_LAZY_GRID_LOAD") == 0) CDI_netcdf_lazy_grid_load = (bool) value;
  else Warning("Unsupported global key: %s", string);
  // clang-format on
}


void cdiDefMissval(double missval)
{
  cdiInitialize();

  CDI_default_missval = missval;
}


double cdiInqMissval(void)
{
  cdiInitialize();

  return CDI_default_missval;
}


double cdiInqGridMissval(void)
{
  cdiInitialize();

  return CDI_default_missval;
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

