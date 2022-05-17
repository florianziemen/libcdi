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

  julianDate.julianDay = calDay_encode(calendar, dt.date);
  julianDate.secondOfDay = secofday_encode(dt.time);

  return julianDate;
}

CdiDateTime
julianDate_decode(int calendar, JulianDate julianDate)
{
  CdiDateTime dt;

  dt.date = calDay_decode(calendar, julianDate.julianDay);
  dt.time = secofday_decode(julianDate.secondOfDay);

  return dt;
}

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

// add seconds to julianDate
JulianDate
julianDate_add_seconds(JulianDate julianDate, int64_t seconds)
{
  julianDate.secondOfDay += seconds;

  adjust_seconds(&julianDate);

  return julianDate;
}

// add julianDate1 and julianDate2
JulianDate
julianDate_add(JulianDate julianDate1, JulianDate julianDate2)
{
  JulianDate julianDate;
  julianDate.julianDay = julianDate2.julianDay + julianDate1.julianDay;
  julianDate.secondOfDay = julianDate2.secondOfDay + julianDate1.secondOfDay;

  adjust_seconds(&julianDate);

  return julianDate;
}

// subtract julianDate1 from julianDate2
JulianDate
julianDate_sub(JulianDate julianDate1, JulianDate julianDate2)
{
  JulianDate julianDate;
  julianDate.julianDay = julianDate2.julianDay - julianDate1.julianDay;
  julianDate.secondOfDay = julianDate2.secondOfDay - julianDate1.secondOfDay;

  adjust_seconds(&julianDate);

  return julianDate;
}
