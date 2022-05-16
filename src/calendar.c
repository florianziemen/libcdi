#include <limits.h>
#include <stdio.h>

#include "calendar.h"
#include "error.h"
#include "timebase.h"

static const int month_360[12] = { 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
static const int month_365[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const int month_366[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int
calendar_dpy(int calendar)
{
  int daysPerYear = 0;

  // clang-format off
  if      (calendar == CALENDAR_360DAYS) daysPerYear = 360;
  else if (calendar == CALENDAR_365DAYS) daysPerYear = 365;
  else if (calendar == CALENDAR_366DAYS) daysPerYear = 366;
  // clang-format on

  return daysPerYear;
}

static const int *
get_dayspermonth_array(int daysPerYear)
{
  // clang-format off
  return  (daysPerYear == 360) ? month_360 :
          (daysPerYear == 365) ? month_365 :
          (daysPerYear == 366) ? month_366 : NULL;
  // clang-format on
}

int
days_per_month(int calendar, int year, int month)
{
  const int daysPerYear = calendar_dpy(calendar);
  const int *daysPerMonthArray = (daysPerYear == 360) ? month_360 : ((daysPerYear == 365) ? month_365 : month_366);

  int daysPerMonth = (month >= 1 && month <= 12) ? daysPerMonthArray[month - 1] : 0;

  if (daysPerYear == 0 && month == 2) daysPerMonth = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 29 : 28;

  return daysPerMonth;
}

int
days_per_year(int calendar, int year)
{
  int daysPerYear = calendar_dpy(calendar);
  if (daysPerYear == 0)
    {
      if (year == 1582 && (calendar == CALENDAR_STANDARD || calendar == CALENDAR_GREGORIAN))
        daysPerYear = 355;
      else if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
        daysPerYear = 366;
      else
        daysPerYear = 365;
    }

  return daysPerYear;
}

static void
decode_day(int daysPerYear, int days, int *year, int *month, int *day)
{
  *year = (days - 1) / daysPerYear;
  days -= (*year * daysPerYear);

  const int *daysPerMonthArray = get_dayspermonth_array(daysPerYear);

  int i = 0;
  if (daysPerMonthArray)
    for (i = 0; i < 12; i++)
      {
        if (days > daysPerMonthArray[i])
          days -= daysPerMonthArray[i];
        else
          break;
      }

  *month = i + 1;
  *day = days;
}

static int64_t
encode_day(int daysPerYear, int year, int month, int day)
{
  int64_t rval = (int64_t) daysPerYear * year + day;

  const int *daysPerMonthArray = get_dayspermonth_array(daysPerYear);

  if (daysPerMonthArray)
    for (int i = 0; i < month - 1; i++) rval += daysPerMonthArray[i];
  if (rval > LONG_MAX || rval < LONG_MIN) Error("Unhandled date: %lld", rval);

  return rval;
}

int64_t
encode_calday(int calendar, int year, int month, int day)
{
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    return encode_day(dpy, year, month, day);
  else
    return encode_julday(calendar, year, month, day);
}

void
decode_calday(int calendar, int64_t julday, int *year, int *month, int *day)
{
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    decode_day(dpy, julday, year, month, day);
  else
    decode_julday(calendar, julday, year, month, day);
}

int64_t
calDay_encode(int calendar, CdiDate date)
{
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    return encode_day(dpy, date.year, date.month, date.day);
  else
    return encode_julday(calendar, date.year, date.month, date.day);
}

CdiDate
calDay_decode(int calendar, int64_t julday)
{
  int year, month, day;
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    decode_day(dpy, julday, &year, &month, &day);
  else
    decode_julday(calendar, julday, &year, &month, &day);

  CdiDate date;
  date.year = year;
  date.month = month;
  date.day = day;

  return date;
}

#ifdef TEST
static int
date_to_calday(int calendar, int64_t date)
{
  const int dpy = calendar_dpy(calendar);

  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);

  int calendarDay;
  if (dpy == 360 || dpy == 365 || dpy == 366)
    calendarDay = encode_day(dpy, year, month, day);
  else
    calendarDay = encode_julday(calendar, year, month, day);

  return calendarDay;
}

static int64_t
calday_to_date(int calendar, int calendarDay)
{
  int year, month, day;
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    decode_day(dpy, calendarDay, &year, &month, &day);
  else
    decode_julday(calendar, calendarDay, &year, &month, &day);

  return cdiEncodeDate(year, month, day);
}

int
main(void)
{
  int calendar = CALENDAR_STANDARD;
  int64_t vdate;
  int vtime;
  int ijulinc;
  int j = 0;
  int year, mon, day, hour, minute, second;
  int calendarDay, secondOfDay;

  // 1 - Check valid range of years

  int nmin = 11000;
  int64_t vdate0 = -80001201;
  int vtime0 = 120500;

  printf("start time: %8ld %4d\n", vdate0, vtime0);

  for (int i = 0; i < nmin; i++)
    {
      cdiDecodeDate(vdate0, &year, &mon, &day);
      cdiDecodeTime(vtime0, &hour, &minute, &second);

      calendarDay = date_to_calday(calendar, vdate0);
      secondOfDay = time_to_sec(vtime0);

      vdate = calday_to_date(calendar, calendarDay);
      vtime = sec_to_time(secondOfDay);

      if (vdate0 != vdate || vtime0 != vtime)
        printf("%4d %8ld %4d %8ld %4d %9d %9d\n", ++j, vdate0, vtime0, vdate, vtime, calendarDay, secondOfDay);

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

  calendarDay = date_to_calday(calendar, vdate0);
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

      julday_add_seconds(ijulinc, &calendarDay, &secondOfDay);

      vdate = calday_to_date(calendar, calendarDay);
      vtime = sec_to_time(secondOfDay);
      if (vdate0 != vdate || vtime0 != vtime)
        printf("%4d %8ld %4d %8ld %4d %9d %9d\n", ++j, vdate0, vtime0, vdate, vtime, calendarDay, secondOfDay);
    }

  printf("stop time: %8ld %4d\n", vdate0, vtime0);

  return 0;
}
#endif

#ifdef TEST2
int
main(void)
{
  int value = 30;
  int factor = 86400;

  int calendar = CALENDAR_360DAYS;

  int year = 1979;
  int month = 1;
  int day = 15;
  int hour = 12;
  int minute = 30;
  int second = 0;
  int ms = 0;

  printf("calendar = %d\n", calendar);
  printf("%d/%02d/%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);

  int64_t calendarDay = encode_calday(calendar, year, month, day);
  double secondOfDay = encode_secofday(hour, minute, second, ms);

  decode_calday(calendar, calendarDay, &year, &month, &day);
  decode_secofday(secondOfDay, &hour, &minute, &second, &ms);
  printf("%d/%02d/%02d %02d:%02d:%02d   %d %d\n", year, month, day, hour, minute, second, calendarDay, secondOfDay);

  for (int i = 0; i < 420; i++)
    {
      decode_calday(calendar, calendarDay, &year, &month, &day);
      decode_secofday(secondOfDay, &hour, &minute, &second, &ms);
      printf("%2d %d/%02d/%02d %02d:%02d:%02d\n", i, year, month, day, hour, minute, second);
      julday_add_seconds(value * factor, &calendarDay, &secondOfDay);
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
