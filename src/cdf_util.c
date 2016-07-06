#include <string.h>
#include <ctype.h>
#include "dmemory.h"
#include "cdi.h"
#include "cdf_util.h"


void strtolower(char *str)
{
  if ( str )
    for ( size_t i = 0; str[i]; ++i )
      str[i] = (char)tolower((int)str[i]);
}


int get_timeunit(size_t len, const char *ptu)
{
  int timeunit = -1;

  if ( len > 2 )
    {
      if      ( memcmp(ptu, "sec",    3) == 0 )          timeunit = TUNIT_SECOND;
      else if ( memcmp(ptu, "minute", 6) == 0 )          timeunit = TUNIT_MINUTE;
      else if ( memcmp(ptu, "hour",   4) == 0 )          timeunit = TUNIT_HOUR;
      else if ( memcmp(ptu, "day",    3) == 0 )          timeunit = TUNIT_DAY;
      else if ( memcmp(ptu, "month",  5) == 0 )          timeunit = TUNIT_MONTH;
      else if ( memcmp(ptu, "calendar_month", 14) == 0 ) timeunit = TUNIT_MONTH;
      else if ( memcmp(ptu, "year",   4) == 0 )          timeunit = TUNIT_YEAR;
    }
  else if ( len == 1 )
    {
      if ( ptu[0] == 's' ) timeunit = TUNIT_SECOND;
    }

  return timeunit;
}


bool is_time_units(const char *timeunits)
{
  bool status = strncmp(timeunits, "sec",    3) == 0
             || strncmp(timeunits, "minute", 6) == 0
             || strncmp(timeunits, "hour",   4) == 0
             || strncmp(timeunits, "day",    3) == 0
             || strncmp(timeunits, "month",  5) == 0;

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

          int timetype = memcmp(ptu, "as", 2) == 0 ? TAXIS_ABSOLUTE :
            memcmp(ptu, "since", 5) == 0 ? TAXIS_RELATIVE : -1;

          status = timetype != -1;
        }
    }

  Free(tu);

  return status;
}


bool is_height_units(const char *units)
{
  bool status = false;
  int u0 = units[0];

  if ( (u0=='m' && (!units[1] || strncmp(units, "meter", 5) == 0)) ||
       (!units[2] && units[1]=='m' && (u0=='c' || u0=='d' || u0=='k')) )
    {
      status = true;
    }

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
    status = true;
  else
    if ( strcmp(longname, "depth_below_sea") == 0 ||
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
    status = true;
  else
    if ( strcmp(longname, "height") == 0 ||
         strcmp(longname, "height above the surface") == 0 )
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
  strtolower(lc_units);

  if ( ((memcmp(lc_units, "degree", 6) == 0 || memcmp(lc_units, "radian", 6) == 0) &&
        (memcmp(stdname, "grid_longitude", 14) == 0 || memcmp(stdname, "longitude", 9) == 0)) )
    {
      status = true;
    }

  if ( status == false &&
       memcmp(stdname, "grid_latitude", 13) && memcmp(stdname, "latitude", 8) &&
       memcmp(lc_units, "degree", 6) == 0 )
    {
      int ioff = 6;
      if ( lc_units[ioff] == 's' ) ioff++;
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
  strtolower(lc_units);

  if ( ((memcmp(lc_units, "degree", 6) == 0 || memcmp(lc_units, "radian", 6) == 0) &&
        (memcmp(stdname, "grid_latitude", 13) == 0 || memcmp(stdname, "latitude", 8) == 0)) )
    {
      status = true;
    }

  if ( status == false &&
       memcmp(stdname, "grid_longitude", 14) && memcmp(stdname, "longitude", 9) &&
       memcmp(lc_units, "degree", 6) == 0 )
    {
      int ioff = 6;
      if ( lc_units[ioff] == 's' ) ioff++;
      if ( lc_units[ioff] == '_' ) ioff++;
      if ( lc_units[ioff] == 'n' || lc_units[ioff] == 's' ) status = true;
    }

  return status;
}
