#include "julian_date.h"

JulianDate
julianDateEncode(int calendar, CdiDateTime dt)
{
  JulianDate julianDate;

  julianDate.julianDay = encode_calday(calendar, dt.date.year, dt.date.month, dt.date.day);
  julianDate.secondOfDay = encode_secofday(dt.time.hour, dt.time.minute, dt.time.second, dt.time.ms);

  return julianDate;
}
