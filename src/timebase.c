#include <math.h> /* for floor() */

#include "timebase.h"

// convert Julian date into year, months, day
void
decode_julday(int calendar, int64_t julianDay, /* Julian day number to convert */
              int *year,                       /* Gregorian year (out)         */
              int *mon,                        /* Gregorian month (1-12) (out) */
              int *day)                        /* Gregorian day (1-31) (out)   */
{
  const int64_t a = julianDay;

  const double b = floor((a - 1867216.25) / 36524.25);
  double c = a + b - floor(b / 4) + 1525;

  if (calendar == CALENDAR_STANDARD || calendar == CALENDAR_GREGORIAN)
    if (a < 2299161) c = a + 1524;

  const double d = floor((c - 122.1) / 365.25);
  const double e = floor(365.25 * d);
  const double f = floor((c - e) / 30.6001);

  *day = (int) (c - e - floor(30.6001 * f));
  *mon = (int) (f - 1 - 12 * floor(f / 14));
  *year = (int) (d - 4715 - floor((7 + *mon) / 10));
}

// convert year, month, day into Julian calendar day
int64_t
encode_julday(int calendar, int year, int month, int day)
{
  const int iy = (month <= 2) ? year - 1 : year;
  const int im = (month <= 2) ? month + 12 : month;
  int ib = (iy < 0) ? ((iy + 1) / 400 - (iy + 1) / 100) : (iy / 400 - iy / 100);

  if (calendar == CALENDAR_STANDARD || calendar == CALENDAR_GREGORIAN)
    {
      if (year > 1582 || (year == 1582 && (month > 10 || (month == 10 && day >= 15))))
        {
          // 15th October 1582 AD or later
        }
      else
        {
          // 4th October 1582 AD or earlier
          ib = -2;
        }
    }

  int64_t julianDay = (int64_t) (floor(365.25 * iy) + (int64_t) (30.6001 * (im + 1)) + ib + 1720996.5 + day + 0.5);

  return julianDay;
}

int64_t
date_to_julday(int calendar, int64_t date)
{
  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);

  return encode_julday(calendar, year, month, day);
}

int64_t
julday_to_date(int calendar, int64_t julianDay)
{
  int year, month, day;
  decode_julday(calendar, julianDay, &year, &month, &day);

  return cdiEncodeDate(year, month, day);
}

int
time_to_sec(int time)
{
  int hour, minute, second;
  cdiDecodeTime(time, &hour, &minute, &second);

  const int seconds = hour * 3600 + minute * 60 + second;

  return seconds;
}

int
sec_to_time(int secofday)
{
  const int hour = secofday / 3600;
  const int minute = secofday / 60 - hour * 60;
  const int second = secofday - hour * 3600 - minute * 60;

  return cdiEncodeTime(hour, minute, second);
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

void
julday_add_seconds(int64_t seconds, int64_t *julianDay, double *secondOfDay)
{
  *secondOfDay += seconds;

  adjust_seconds(julianDay, secondOfDay);
}

// add days and secs to julianDay/secondOfDay
void
julday_add(int64_t days, double seconds, int64_t *julianDay, double *secondOfDay)
{
  *julianDay += days;
  *secondOfDay += seconds;

  adjust_seconds(julianDay, secondOfDay);
}

// subtract julianDay1/secondOfDay1 from julianDay2/secondOfDay2 and returns the result in seconds
double
julday_sub(int64_t julianDay1, double secondOfDay1, int64_t julianDay2, double secondOfDay2, int64_t *days, double *secs)
{
  *days = julianDay2 - julianDay1;
  *secs = secondOfDay2 - secondOfDay1;

  double sec_of_day = *secs;

  adjust_seconds(days, &sec_of_day);

  *secs = sec_of_day;

  double seconds = (*days) * 86400.0 + sec_of_day;

  return seconds;
}

double
encode_secofday(int hour, int minute, int second, int ms)
{
  return hour * 3600 + minute * 60 + second + ms / 1000.0;
}

void
decode_secofday(double secondOfDay, int *hour, int *minute, int *second, int *ms)
{
  const int fullSeconds = (int) secondOfDay;

  *ms = (int) lround((secondOfDay - fullSeconds) * 1000);

  *hour = fullSeconds / 3600;
  *minute = fullSeconds / 60 - *hour * 60;
  *second = fullSeconds - *hour * 3600 - *minute * 60;
}

#ifdef TEST
int
main(void)
{
  int nmin;
  int64_t vdate0, vdate;
  int vtime0, vtime;
  int ijulinc;
  int j = 0;
  int year, mon, day, hour, minute, second;
  int64_t julianDay;
  int secondOfDay;
  int calendar = CALENDAR_STANDARD;

  // 1 - Check valid range of years

  nmin = 11000;
  vdate0 = -80001201;
  vtime0 = 120500;

  printf("start time: %8ld %4d\n", vdate0, vtime0);

  for (int i = 0; i < nmin; i++)
    {
      cdiDecodeDate(vdate0, &year, &mon, &day);
      cdiDecodeTime(vtime0, &hour, &minute, &second);

      julianDay = date_to_julday(calendar, vdate0);
      secondOfDay = time_to_sec(vtime0);

      vdate = julday_to_date(calendar, julianDay);
      vtime = sec_to_time(secondOfDay);

      if (vdate0 != vdate || vtime0 != vtime)
        printf("%4d %8ld %4d %8ld %4d %9d %9d\n", ++j, vdate0, vtime0, vdate, vtime, julianDay, secondOfDay);

      year++;
      vdate0 = cdiEncodeDate(year, mon, day);
      vtime0 = cdiEncodeTime(hour, minute, second);
    }

  printf("stop time: %8ld %4d\n", vdate0, vtime0);

  // 2 - Check time increment of one minute

  nmin = 120000;
  ijulinc = 60;
  vdate0 = 20001201;
  vtime0 = 0;

  printf("start time: %8ld %4d\n", vdate0, vtime0);

  julianDay = date_to_julday(calendar, vdate0);
  secondOfDay = time_to_sec(vtime0);
  for (int i = 0; i < nmin; i++)
    {
      cdiDecodeDate(vdate0, &year, &mon, &day);
      cdiDecodeTime(vtime0, &hour, &minute, &second);

      if (++minute >= 60)
        {
          minute = 0;
          if (++hour >= 24)
            {
              hour = 0;
              if (++day >= 32)
                {
                  day = 1;
                  if (++mon >= 13)
                    {
                      mon = 1;
                      year++;
                    }
                }
            }
        }

      vdate0 = cdiEncodeDate(year, mon, day);
      vtime0 = cdiEncodeTime(hour, minute, second);

      julday_add_seconds(ijulinc, &julianDay, &secondOfDay);

      vdate = julday_to_date(calendar, julianDay);
      vtime = sec_to_time(secondOfDay);
      if (vdate0 != vdate || vtime0 != vtime)
        printf("%4d %8ld %4d %8ld %4d %9d %9d\n", ++j, vdate0, vtime0, vdate, vtime, julianDay, secondOfDay);
    }

  printf("stop time: %8ld %4d\n", vdate0, vtime0);

  return 0;
}
#endif

#ifdef TEST2
int
main(void)
{
  int year, month, day, hour, minute, second, ms = 0;
  int value = 30;
  int factor = 86400;
  int calendar = CALENDAR_STANDARD;

  CdiDateTime dt;
  year = 1979;
  month = 1;
  day = 15;
  hour = 12;
  minute = 30;
  second = 17;

  printf("%d/%02d/%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);

  int64_t julianDay = encode_julday(calendar, year, month, day);
  double secondOfDay = encode_secofday(hour, minute, second, ms);

  decode_julday(calendar, julianDay, &year, &month, &day);
  decode_secofday(secondOfDay, &hour, &minute, &second, &ms);
  printf("%d/%02d/%02d %02d:%02d:%02d   %d %d\n", year, month, day, hour, minute, second, julianDay, secondOfDay);

  for (int i = 0; i < 420; i++)
    {
      decode_julday(calendar, julianDay, &year, &month, &day);
      decode_secofday(secondOfDay, &hour, &minute, &second, &ms);
      printf("%2d %d/%02d/%02d %02d:%02d:%02d\n", i, year, month, day, hour, minute, second);
      julday_add_seconds(value * factor, &julianDay, &secondOfDay);
    }

  return 0;
}
#endif
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
