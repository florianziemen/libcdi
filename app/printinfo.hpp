#ifndef PRINTINFO_HPP
#define PRINTINFO_HPP

#include <cdi.h>

#include <string>

enum struct TimeUnits
{
  SECONDS = 0,
  MINUTES,
  HOURS,
  DAYS,
  MONTHS,
  YEARS
};

class TimeIncrement
{
public:
  int64_t period = 0;
  TimeUnits units = TimeUnits::SECONDS;

  TimeIncrement() = default;
  TimeIncrement(int64_t _period, TimeUnits _units) : period{ _period }, units{ _units } {}

  bool
  operator==(const TimeIncrement &timeIncr) const
  {
    return (period == timeIncr.period && units == timeIncr.units);
  }

  bool
  operator!=(const TimeIncrement &timeIncr) const
  {
    return (period != timeIncr.period || units != timeIncr.units);
  }
};

TimeIncrement get_time_increment(double jdelta, CdiDate vDate0, CdiDate vDate1);

std::string datetime_to_string(CdiDateTime dt);

const char *time_units_cstr(TimeUnits timeUnit);
const char *tunit_to_cstr(int tunits);
const char *calendar_to_cstr(int calendar);

#endif
