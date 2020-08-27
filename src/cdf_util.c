#include <string.h>
#include <ctype.h>

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "cdf_util.h"
#include "error.h"


char *strToLower(char *str)
{
  if (str)
    for (size_t i = 0; str[i]; ++i)
      str[i] = (char)tolower((int)str[i]);

  return str;
}


bool strStartsWith(const char *vstr, const char *cstr)
{
  bool is_equal = false;
  if (vstr && cstr)
    {
      size_t clen = strlen(cstr);
      size_t vlen = strlen(vstr);
      if (clen <= vlen) is_equal = (memcmp(vstr, cstr, clen) == 0);
    }
  return is_equal;
}

int get_timeunit(size_t len, const char *ptu)
{
  int timeunit = -1;

  while ( isspace(*ptu) && len ) { ptu++; len--; }

  // clang-format off
  if ( len > 2 )
    {
      if      ( strStartsWith(ptu, "sec") )            timeunit = TUNIT_SECOND;
      else if ( strStartsWith(ptu, "minute") )         timeunit = TUNIT_MINUTE;
      else if ( strStartsWith(ptu, "hour") )           timeunit = TUNIT_HOUR;
      else if ( strStartsWith(ptu, "day") )            timeunit = TUNIT_DAY;
      else if ( strStartsWith(ptu, "month") )          timeunit = TUNIT_MONTH;
      else if ( strStartsWith(ptu, "calendar_month") ) timeunit = TUNIT_MONTH;
      else if ( strStartsWith(ptu, "year") )           timeunit = TUNIT_YEAR;
    }
  else if ( len == 1 && ptu[0] == 's' )  timeunit = TUNIT_SECOND;
  // clang-format on

  return timeunit;
}


bool is_time_units(const char *timeunits)
{
  while ( isspace(*timeunits) ) timeunits++; 

  bool status = strStartsWith(timeunits, "sec")
             || strStartsWith(timeunits, "minute")
             || strStartsWith(timeunits, "hour")
             || strStartsWith(timeunits, "day")
             || strStartsWith(timeunits, "month")
             || strStartsWith(timeunits, "calendar_month")
             || strStartsWith(timeunits, "year");

  return status;
}


bool is_timeaxis_units(const char *timeunits)
{
  bool status = false;

  size_t len = strlen(timeunits);
  char *tu = (char *) Malloc((len+1)*sizeof(char));
  memcpy(tu, timeunits, (len+1) * sizeof(char));
  char *ptu = tu;

  for ( size_t i = 0; i < len; i++ ) ptu[i] = (char)tolower((int)ptu[i]);

  int timeunit = get_timeunit(len, ptu);
  if ( timeunit != -1 )
    {
      while ( ! isspace(*ptu) && *ptu != 0 ) ptu++;
      if ( *ptu )
        {
          while ( isspace(*ptu) ) ptu++;

          int timetype = strStartsWith(ptu, "as") ? TAXIS_ABSOLUTE :
                         strStartsWith(ptu, "since") ? TAXIS_RELATIVE : -1;

          status = timetype != -1;
        }
    }

  Free(tu);

  return status;
}


bool is_height_units(const char *units)
{
  const int u0 = units[0];

  bool status
    = (u0=='m' && (!units[1] || strncmp(units, "meter", 5) == 0))
    || (!units[2] && units[1]=='m' && (u0=='c' || u0=='d' || u0=='k'))
    || (strncmp(units, "decimeter", 9) == 0)
    || (strncmp(units, "centimeter", 10) == 0)
    || (strncmp(units, "millimeter", 10) == 0)
    || (strncmp(units, "kilometer", 9) == 0);

  return status;
}


bool is_pressure_units(const char *units)
{
  bool status = false;

  if ( strncmp(units, "millibar", 8) == 0 ||
       strncmp(units, "mb", 2)       == 0 ||
       strncmp(units, "hectopas", 8) == 0 ||
       strncmp(units, "hPa", 3)      == 0 ||
       strncmp(units, "Pa", 2)       == 0 )
    {
      status = true;
    }

  return status;
}


bool is_DBL_axis(/*const char *units,*/ const char *longname)
{
  bool status = false;

  if ( strcmp(longname, "depth below land")         == 0 ||
       strcmp(longname, "depth_below_land")         == 0 ||
       strcmp(longname, "levels below the surface") == 0 )
    {
      /*
      if ( strcmp(ncvars[ncvarid].units, "cm") == 0 ||
           strcmp(ncvars[ncvarid].units, "dm") == 0 ||
           strcmp(ncvars[ncvarid].units, "m")  == 0 )
      */
        status = true;
    }

  return status;
}


bool is_depth_axis(const char *stdname, const char *longname)
{
  bool status = false;

  if ( strcmp(stdname, "depth") == 0 )
    {
      status = true;
    }
  else if ( strcmp(longname, "depth_below_sea") == 0 ||
            strcmp(longname, "depth below sea") == 0 )
    {
      status = true;
    }

  return status;
}


bool is_height_axis(const char *stdname, const char *longname)
{
  bool status = false;

  if ( strcmp(stdname, "height") == 0 )
    {
      status = true;
    }
  else if ( strcmp(longname, "height") == 0 ||
            strcmp(longname, "height above the surface") == 0 )
    {
      status = true;
    }

  return status;
}

bool is_altitude_axis(const char *stdname, const char *longname)
{
  bool status = false;

  if ( strcmp(stdname, "altitude") == 0 )
    {
      status = true;
    }
  else if ( strcmp(longname, "altitude") == 0 )
    {
      status = true;
    }

  return status;
}


bool is_lon_axis(const char *units, const char *stdname)
{
  bool status = false;
  char lc_units[16];

  memcpy(lc_units, units, 15);
  lc_units[15] = 0;
  strToLower(lc_units);

  if ( (strStartsWith(lc_units, "degree") || strStartsWith(lc_units, "radian")) &&
       (strStartsWith(stdname, "grid_longitude") || strStartsWith(stdname, "longitude")) )
    {
      status = true;
    }
  else if ( strStartsWith(lc_units, "degree")
            && !strStartsWith(stdname, "grid_latitude")
            && !strStartsWith(stdname, "latitude") )
    {
      int ioff = 6;
      if ( lc_units[ioff] == 's' ) ioff++;
      if ( lc_units[ioff] == ' ' ) ioff++;
      if ( lc_units[ioff] == '_' ) ioff++;
      if ( lc_units[ioff] == 'e' ) status = true;
    }

  return status;
}


bool is_lat_axis(const char *units, const char *stdname)
{
  bool status = false;
  char lc_units[16];

  memcpy(lc_units, units, 15);
  lc_units[15] = 0;
  strToLower(lc_units);

  if ( (strStartsWith(lc_units, "degree") || strStartsWith(lc_units, "radian")) &&
        (strStartsWith(stdname, "grid_latitude") || strStartsWith(stdname, "latitude")) )
    {
      status = true;
    }
  else if ( strStartsWith(lc_units, "degree")
            && !strStartsWith(stdname, "grid_longitude")
            && !strStartsWith(stdname, "longitude") )
    {
      int ioff = 6;
      if ( lc_units[ioff] == 's' ) ioff++;
      if ( lc_units[ioff] == ' ' ) ioff++;
      if ( lc_units[ioff] == '_' ) ioff++;
      if ( lc_units[ioff] == 'n' || lc_units[ioff] == 's' ) status = true;
    }

  return status;
}


bool is_x_axis(const char *units, const char *stdname)
{
  (void)units;
  return (strcmp(stdname, "projection_x_coordinate") == 0);
}


bool is_y_axis(const char *units, const char *stdname)
{
  (void)units;
  return (strcmp(stdname, "projection_y_coordinate") == 0);
}


void set_gridtype(const char *attstring, int *gridtype)
{
  // clang-format off
  if      ( strcmp(attstring, "gaussian_reduced") == 0 ) *gridtype = GRID_GAUSSIAN_REDUCED;
  else if ( strcmp(attstring, "gaussian") == 0 ) *gridtype = GRID_GAUSSIAN;
  else if ( strncmp(attstring, "spectral", 8) == 0 ) *gridtype = GRID_SPECTRAL;
  else if ( strncmp(attstring, "fourier", 7) == 0 ) *gridtype = GRID_FOURIER;
  else if ( strcmp(attstring, "trajectory") == 0 ) *gridtype = GRID_TRAJECTORY;
  else if ( strcmp(attstring, "generic") == 0 ) *gridtype = GRID_GENERIC;
  else if ( strcmp(attstring, "cell") == 0 ) *gridtype = GRID_UNSTRUCTURED;
  else if ( strcmp(attstring, "unstructured") == 0 ) *gridtype = GRID_UNSTRUCTURED;
  else if ( strcmp(attstring, "curvilinear") == 0 ) ;
  else if ( strcmp(attstring, "characterxy") == 0 ) *gridtype = GRID_CHARXY;
  else if ( strcmp(attstring, "sinusoidal") == 0 ) ;
  else if ( strcmp(attstring, "laea") == 0 ) ;
  else if ( strcmp(attstring, "lcc2") == 0 ) ;
  else if ( strcmp(attstring, "linear") == 0 ) ; // ignore grid type linear
  else
    {
      static bool warn = true;
      if ( warn )
        {
          warn = false;
          Warning("NetCDF attribute grid_type='%s' unsupported!", attstring);
        }
    }
  // clang-format on
}


void cdf_set_zaxistype(const char *attstring, int *zaxistype)
{
  // clang-format off
  if      (strIsEqual(attstring, "toa"))              *zaxistype = ZAXIS_TOA;
  else if (strIsEqual(attstring, "tropopause"))       *zaxistype = ZAXIS_TROPOPAUSE;
  else if (strIsEqual(attstring, "cloudbase"))        *zaxistype = ZAXIS_CLOUD_BASE;
  else if (strIsEqual(attstring, "cloudtop"))         *zaxistype = ZAXIS_CLOUD_TOP;
  else if (strIsEqual(attstring, "isotherm0"))        *zaxistype = ZAXIS_ISOTHERM_ZERO;
  else if (strIsEqual(attstring, "seabottom"))        *zaxistype = ZAXIS_SEA_BOTTOM;
  else if (strIsEqual(attstring, "lakebottom"))       *zaxistype = ZAXIS_LAKE_BOTTOM;
  else if (strIsEqual(attstring, "sedimentbottom"))   *zaxistype = ZAXIS_SEDIMENT_BOTTOM;
  else if (strIsEqual(attstring, "sedimentbottomta")) *zaxistype = ZAXIS_SEDIMENT_BOTTOM_TA;
  else if (strIsEqual(attstring, "sedimentbottomtw")) *zaxistype = ZAXIS_SEDIMENT_BOTTOM_TW;
  else if (strIsEqual(attstring, "mixlayer"))         *zaxistype = ZAXIS_MIX_LAYER;
  else if (strIsEqual(attstring, "atmosphere"))       *zaxistype = ZAXIS_ATMOSPHERE;
  else
    {
      static bool warn = true;
      if (warn)
        {
          warn = false;
          Warning("NetCDF attribute level_type='%s' unsupported!", attstring);
        }
    }
  // clang-format on
}


void cdf_set_calendar(const char *attstring, int *calendar)
{
  // clang-format off
  if      (strStartsWith(attstring, "standard"))  *calendar = CALENDAR_STANDARD;
  else if (strStartsWith(attstring, "gregorian")) *calendar = CALENDAR_GREGORIAN;
  else if (strStartsWith(attstring, "none"))      *calendar = CALENDAR_NONE;
  else if (strStartsWith(attstring, "proleptic")) *calendar = CALENDAR_PROLEPTIC;
  else if (strStartsWith(attstring, "360"))       *calendar = CALENDAR_360DAYS;
  else if (strStartsWith(attstring, "365") ||
           strStartsWith(attstring, "noleap"))    *calendar = CALENDAR_365DAYS;
  else if (strStartsWith(attstring, "366") ||
           strStartsWith(attstring, "all_leap"))  *calendar = CALENDAR_366DAYS;
  else Warning("calendar >%s< unsupported!", attstring);
  // clang-format on
}
