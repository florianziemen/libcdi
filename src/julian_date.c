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
