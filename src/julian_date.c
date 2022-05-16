#include "julian_date.h"
#include <math.h>

double
secofday_encode(CdiTime time)
{
  const int hour = time.hour;
  const int minute = time.minute;
  const int second = time.second;
  return hour * 3600 + minute * 60 + second + time.ms / 1000.0;
}

CdiTime
secofday_decode(double secondOfDay)
{
  CdiTime time;

  const int fullSeconds = (int) secondOfDay;

  time.ms = (int) lround((secondOfDay - fullSeconds) * 1000);

  const int hour = fullSeconds / 3600;
  const int minute = fullSeconds / 60 - hour * 60;
  const int second = fullSeconds - hour * 3600 - minute * 60;

  time.hour = hour;
  time.minute = minute;
  time.second = second;

  return time;
}

JulianDate
julianDate_encode(int calendar, CdiDateTime dt)
{
  JulianDate julianDate;

  julianDate.julianDay = encode_calday(calendar, dt.date.year, dt.date.month, dt.date.day);
  julianDate.secondOfDay = encode_secofday(dt.time.hour, dt.time.minute, dt.time.second, dt.time.ms);

  return julianDate;
}
/*
CdiDateTime
julianDate_decode(int calendar, JulianDate julianDate)
{
  CdiDateTime dt;

  decode_calday(calendar, julianDate.julianDay, &dt.date.year, &dt.date.month, &dt.date.day);
  decode_secofday(julianDate.secondOfDay, &dt.time.hour, &dt.time.minute, &dt.time.second, &dt.time.ms);

  return dt;
}
*/
static void
adjust_seconds(JulianDate *julianDate)
{
  const double SecondsPerDay = 86400.0;

  while (julianDate->secondOfDay >= SecondsPerDay)
    {
      julianDate->secondOfDay -= SecondsPerDay;
      julianDate->julianDay++;
    }

  while (julianDate->secondOfDay < 0.0)
    {
      julianDate->secondOfDay += SecondsPerDay;
      julianDate->julianDay--;
    }
}

// add julianDate1 and julianDate2
JulianDate
julianDate_add(JulianDate julianDate1, JulianDate julianDate2)
{
  JulianDate julianDateSub;
  julianDateSub.julianDay = julianDate2.julianDay + julianDate1.julianDay;
  julianDateSub.secondOfDay = julianDate2.secondOfDay + julianDate1.secondOfDay;

  adjust_seconds(&julianDateSub);

  return julianDateSub;
}

// subtract julianDate1 from julianDate2
JulianDate
julianDate_sub(JulianDate julianDate1, JulianDate julianDate2)
{
  JulianDate julianDateSub;
  julianDateSub.julianDay = julianDate2.julianDay - julianDate1.julianDay;
  julianDateSub.secondOfDay = julianDate2.secondOfDay - julianDate1.secondOfDay;

  adjust_seconds(&julianDateSub);

  return julianDateSub;
}
