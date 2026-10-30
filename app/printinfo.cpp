#include "printinfo.hpp"

constexpr const char *DATE_FORMAT = "%5.4d-%2.2d-%2.2d";
constexpr const char *TIME_FORMAT = "%2.2d:%2.2d:%2.2d";

std::string
date_to_string(CdiDate date)
{
  char cstr[32];
  std::snprintf(cstr, sizeof(cstr), DATE_FORMAT, date.year, date.month, date.day);
  return { cstr };
}

std::string
time_to_string(CdiTime time)
{
  char cstr[32];
  std::snprintf(cstr, sizeof(cstr), TIME_FORMAT, time.hour, time.minute, time.second);
  return { cstr };
}

std::string
datetime_to_string(CdiDateTime dt)
{
  return date_to_string(dt.date) + "T" + time_to_string(dt.time);
}

const char *
time_units_cstr(TimeUnits timeUnit)
{
  if (timeUnit == TimeUnits::SECONDS) return "second";
  if (timeUnit == TimeUnits::MINUTES) return "minute";
  if (timeUnit == TimeUnits::HOURS) return "hour";
  if (timeUnit == TimeUnits::DAYS) return "day";
  if (timeUnit == TimeUnits::MONTHS) return "month";
  if (timeUnit == TimeUnits::YEARS) return "year";

  return nullptr;
}

const char *
tunit_to_cstr(int tunits)
{
  // clang-format off
  if (tunits == TUNIT_YEAR)       return "years";
  if (tunits == TUNIT_MONTH)      return "months";
  if (tunits == TUNIT_DAY)        return "days";
  if (tunits == TUNIT_12HOURS)    return "12hours";
  if (tunits == TUNIT_6HOURS)     return "6hours";
  if (tunits == TUNIT_3HOURS)     return "3hours";
  if (tunits == TUNIT_HOUR)       return "hours";
  if (tunits == TUNIT_30MINUTES)  return "30minutes";
  if (tunits == TUNIT_QUARTER)    return "15minutes";
  if (tunits == TUNIT_MINUTE)     return "minutes";
  if (tunits == TUNIT_SECOND)     return "seconds";
  return "unknown";
  // clang-format on
}

const char *
calendar_to_cstr(int calendar)
{
  // clang-format off
  if (calendar == CALENDAR_STANDARD)  return "standard";
  if (calendar == CALENDAR_GREGORIAN) return "gregorian";
  if (calendar == CALENDAR_PROLEPTIC) return "proleptic_gregorian";
  if (calendar == CALENDAR_360DAYS)   return "360_day";
  if (calendar == CALENDAR_365DAYS)   return "365_day";
  if (calendar == CALENDAR_366DAYS)   return "366_day";
  return "unknown";
  // clang-format on
}

TimeIncrement
get_time_increment(double jdelta, CdiDate vDate0, CdiDate vDate1)
{
  auto seconds = static_cast<int64_t>(std::llround(jdelta));

  int sign = 1;
  if (seconds < 0)
  {
    std::swap(vDate0, vDate1);
    seconds = -seconds;
    sign = -1;
  }

  auto deltay = vDate1.year - vDate0.year;
  auto deltam = deltay * 12 + (vDate1.month - vDate0.month);
  if (deltay == 0) deltay = 1;
  if (deltam == 0) deltam = 1;

  constexpr int64_t secPerMinute = 60;
  constexpr int64_t secPerHour = 60 * secPerMinute;
  constexpr int64_t secPerDay = 24 * secPerHour;
  constexpr int64_t secPerMonth = 30 * secPerDay;
  constexpr int64_t secPerYear = 12 * secPerMonth;

  TimeIncrement timeIncr;
  if (seconds >= secPerYear) { timeIncr = { deltay, TimeUnits::YEARS }; }
  else if (seconds >= secPerMonth) { timeIncr = { deltam, TimeUnits::MONTHS }; }
  else if (seconds >= secPerDay)
  {
    timeIncr = { seconds / secPerDay, TimeUnits::DAYS };
    if (timeIncr.period >= 27 && deltam == 1) timeIncr = { 1, TimeUnits::MONTHS };
  }
  else if (seconds >= secPerHour && seconds % secPerHour == 0) { timeIncr = { seconds / secPerHour, TimeUnits::HOURS }; }
  else if (seconds >= secPerMinute && seconds % secPerMinute == 0) { timeIncr = { seconds / secPerMinute, TimeUnits::MINUTES }; }
  else
  {
    timeIncr = { seconds, TimeUnits::SECONDS };
  }

  timeIncr.period *= sign;

  return timeIncr;
}
