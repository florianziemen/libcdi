#include <string.h>
#include <ctype.h>

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "cdf_util.h"
#include "error.h"

char *
strToLower(char *str)
{
  if (str)
    for (size_t i = 0; str[i]; ++i) str[i] = (char) tolower((int) str[i]);

  return str;
}

bool
strStartsWith(const char *vstr, const char *cstr)
{
  bool is_equal = false;
  if (vstr && cstr)
    {
      const size_t clen = strlen(cstr);
      const size_t vlen = strlen(vstr);
      if (clen <= vlen) is_equal = (memcmp(vstr, cstr, clen) == 0);
    }
  return is_equal;
}

int
get_timeunit(size_t len, const char *ptu)
{
  int timeunit = -1;

  while (isspace(*ptu) && len)
    {
      ptu++;
      len--;
    }

  // clang-format off
  if (len > 2)
    {
      if      (strStartsWith(ptu, "sec"))            timeunit = TUNIT_SECOND;
      else if (strStartsWith(ptu, "minute"))         timeunit = TUNIT_MINUTE;
      else if (strStartsWith(ptu, "hour"))           timeunit = TUNIT_HOUR;
      else if (strStartsWith(ptu, "day"))            timeunit = TUNIT_DAY;
      else if (strStartsWith(ptu, "month"))          timeunit = TUNIT_MONTH;
      else if (strStartsWith(ptu, "calendar_month")) timeunit = TUNIT_MONTH;
      else if (strStartsWith(ptu, "year"))           timeunit = TUNIT_YEAR;
    }
  else if (len == 1 && ptu[0] == 's')  timeunit = TUNIT_SECOND;
  // clang-format on

  return timeunit;
}

bool
is_time_units(const char *timeunits)
{
  while (isspace(*timeunits)) timeunits++;

  // clang-format off
  bool status = strStartsWith(timeunits, "sec")
             || strStartsWith(timeunits, "minute")
             || strStartsWith(timeunits, "hour")
             || strStartsWith(timeunits, "day")
             || strStartsWith(timeunits, "month")
             || strStartsWith(timeunits, "calendar_month")
             || strStartsWith(timeunits, "year");
  // clang-format on

  return status;
}

bool
is_timeaxis_units(const char *timeunits)
{
  bool status = false;

  const size_t len = strlen(timeunits);
  char *tu = (char *) malloc((len + 1) * sizeof(char));

  for (size_t i = 0; i < len; i++) tu[i] = (char) tolower((int) timeunits[i]);

  int timeunit = get_timeunit(len, tu);
  if (timeunit != -1)
    {
      size_t pos = 0;
      while (!isspace(tu[pos]) && tu[pos] != 0) pos++;
      if (tu[pos])
        {
          while (isspace(tu[pos])) pos++;

          status = strStartsWith(tu + pos, "as") || strStartsWith(tu + pos, "since");
        }
    }

  free(tu);

  return status;
}

bool
is_height_units(const char *units)
{
  const int u0 = units[0];

  // clang-format off
  bool status = (u0=='m' && (!units[1] || strStartsWith(units, "meter")))
    || (!units[2] && units[1]=='m' && (u0=='c' || u0=='d' || u0=='k'))
    || (strStartsWith(units, "decimeter"))
    || (strStartsWith(units, "centimeter"))
    || (strStartsWith(units, "millimeter"))
    || (strStartsWith(units, "kilometer"));
  // clang-format on

  return status;
}

bool
is_pressure_units(const char *units)
{
  // clang-format off
  bool status = strStartsWith(units, "millibar")
             || strStartsWith(units, "mb")
             || strStartsWith(units, "hectopas")
             || strStartsWith(units, "hPa")
             || strStartsWith(units, "Pa");
  // clang-format on

  return status;
}

bool
is_DBL_axis(const char *longname)
{
  // clang-format off
  bool status = strIsEqual(longname, "depth below land")
             || strIsEqual(longname, "depth_below_land")
             || strIsEqual(longname, "levels below the surface");
  // clang-format on

  return status;
}

bool
is_depth_axis(const char *stdname, const char *longname)
{
  // clang-format off
  bool status = strIsEqual(stdname, "depth")
             || strIsEqual(longname, "depth_below_sea")
             || strIsEqual(longname, "depth below sea");
  // clang-format ofn

  return status;
}


bool is_height_axis(const char *stdname, const char *longname)
{
  // clang-format off
  bool status = strIsEqual(stdname, "height")
             || strIsEqual(longname, "height")
             || strIsEqual(longname, "height above the surface");
  // clang-format on

  return status;
}

bool
is_altitude_axis(const char *stdname, const char *longname)
{
  // clang-format off
  bool status = strIsEqual(stdname, "altitude")
             || strIsEqual(longname, "altitude");
  // clang-format on

  return status;
}

bool
is_lon_axis(const char *units, const char *stdname)
{
  bool status = false;
  char lc_units[16];

  memcpy(lc_units, units, 15);
  lc_units[15] = 0;
  strToLower(lc_units);

  if ((strStartsWith(lc_units, "degree") || strStartsWith(lc_units, "radian"))
      && (strStartsWith(stdname, "grid_longitude") || strStartsWith(stdname, "longitude")))
    {
      status = true;
    }
  else if (strStartsWith(lc_units, "degree") && !strStartsWith(stdname, "grid_latitude") && !strStartsWith(stdname, "latitude"))
    {
      int ioff = 6;
      if (lc_units[ioff] == 's') ioff++;
      if (lc_units[ioff] == ' ') ioff++;
      if (lc_units[ioff] == '_') ioff++;
      if (lc_units[ioff] == 'e') status = true;
    }

  return status;
}

bool
is_lat_axis(const char *units, const char *stdname)
{
  bool status = false;
  char lc_units[16];

  memcpy(lc_units, units, 15);
  lc_units[15] = 0;
  strToLower(lc_units);

  if ((strStartsWith(lc_units, "degree") || strStartsWith(lc_units, "radian"))
      && (strStartsWith(stdname, "grid_latitude") || strStartsWith(stdname, "latitude")))
    {
      status = true;
    }
  else if (strStartsWith(lc_units, "degree") && !strStartsWith(stdname, "grid_longitude") && !strStartsWith(stdname, "longitude"))
    {
      int ioff = 6;
      if (lc_units[ioff] == 's') ioff++;
      if (lc_units[ioff] == ' ') ioff++;
      if (lc_units[ioff] == '_') ioff++;
      if (lc_units[ioff] == 'n' || lc_units[ioff] == 's') status = true;
    }

  return status;
}

bool
is_x_axis(const char *units, const char *stdname)
{
  (void) units;
  return (strIsEqual(stdname, "projection_x_coordinate"));
}

bool
is_y_axis(const char *units, const char *stdname)
{
  (void) units;
  return (strIsEqual(stdname, "projection_y_coordinate"));
}

void
cdf_set_gridtype(const char *attstring, int *gridtype)
{
  // clang-format off
  if      (strIsEqual(attstring, "gaussian_reduced")) *gridtype = GRID_GAUSSIAN_REDUCED;
  else if (strIsEqual(attstring, "gaussian"))         *gridtype = GRID_GAUSSIAN;
  else if (strStartsWith(attstring, "spectral"))      *gridtype = GRID_SPECTRAL;
  else if (strStartsWith(attstring, "fourier"))       *gridtype = GRID_FOURIER;
  else if (strIsEqual(attstring, "trajectory"))       *gridtype = GRID_TRAJECTORY;
  else if (strIsEqual(attstring, "generic"))          *gridtype = GRID_GENERIC;
  else if (strIsEqual(attstring, "cell"))             *gridtype = GRID_UNSTRUCTURED;
  else if (strIsEqual(attstring, "unstructured"))     *gridtype = GRID_UNSTRUCTURED;
  else if (strIsEqual(attstring, "curvilinear")) ;
  else if (strIsEqual(attstring, "characterxy"))      *gridtype = GRID_CHARXY;
  else if (strIsEqual(attstring, "sinusoidal")) ;
  else if (strIsEqual(attstring, "laea")) ;
  else if (strIsEqual(attstring, "lcc2")) ;
  else if (strIsEqual(attstring, "linear")) ; // ignore grid type linear
  else
    {
      static bool warn = true;
      if (warn)
        {
          warn = false;
          Warning("NetCDF attribute grid_type='%s' unsupported!", attstring);
        }
    }
  // clang-format on
}

void
cdf_set_zaxistype(const char *attstring, int *zaxistype)
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

int
attribute_to_calendar(const char *attstring)
{
  int calendar = CALENDAR_STANDARD;
  // clang-format off
  if      (strStartsWith(attstring, "standard"))  calendar = CALENDAR_STANDARD;
  else if (strStartsWith(attstring, "gregorian")) calendar = CALENDAR_GREGORIAN;
  else if (strStartsWith(attstring, "none"))      calendar = CALENDAR_NONE;
  else if (strStartsWith(attstring, "proleptic")) calendar = CALENDAR_PROLEPTIC;
  else if (strStartsWith(attstring, "360"))       calendar = CALENDAR_360DAYS;
  else if (strStartsWith(attstring, "365") ||
           strStartsWith(attstring, "noleap"))    calendar = CALENDAR_365DAYS;
  else if (strStartsWith(attstring, "366") ||
           strStartsWith(attstring, "all_leap"))  calendar = CALENDAR_366DAYS;
  else Warning("calendar >%s< unsupported!", attstring);
  // clang-format on
  return calendar;
}
