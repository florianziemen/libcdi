#include "julian_date.h"

JulianDate
julianDate_encode(int calendar, CdiDateTime dt)
{
  JulianDate julianDate;

  julianDate.julianDay = encode_calday(calendar, dt.date.year, dt.date.month, dt.date.day);
  julianDate.secondOfDay = encode_secofday(dt.time.hour, dt.time.minute, dt.time.second, dt.time.ms);

  return julianDate;
}

static void
adjust_seconds(int64_t *julianDay, double *secondOfDay)
{
  const double SecondsPerDay = 86400.0;

  while (*secondOfDay >= SecondsPerDay)
    {
      *secondOfDay -= SecondsPerDay;
      (*julianDay)++;
    }

  while (*secondOfDay < 0)
    {
      *secondOfDay += SecondsPerDay;
      (*julianDay)--;
    }
}

// subtract jjulianDate1 from julianDate2 and returns the result in seconds
double
julianDate_sub(JulianDate julianDate1, JulianDate julianDate2, int64_t *days, double *secs)
{
  *days = julianDate2.julianDay - julianDate1.julianDay;
  *secs = julianDate2.secondOfDay - julianDate1.secondOfDay;

  double sec_of_day = *secs;

  adjust_seconds(days, &sec_of_day);

  *secs = sec_of_day;

  double seconds = (*days) * 86400.0 + sec_of_day;

  return seconds;
}
