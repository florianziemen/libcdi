#include <limits.h>
#include <stdio.h>

#include "cdi.h" /* CALENDAR_ */
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

void
cdi_decode_day(int daysPerYear, int days, int *year, int *month, int *day)
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

int64_t
cdi_encode_day(int daysPerYear, int year, int month, int day)
{
  int64_t rval = (int64_t) daysPerYear * year + day;

  const int *daysPerMonthArray = get_dayspermonth_array(daysPerYear);

  if (daysPerMonthArray)
    for (int i = 0; i < month - 1; i++) rval += daysPerMonthArray[i];
  if (rval > LONG_MAX || rval < LONG_MIN) Error("Unhandled date: %lld", rval);

  return rval;
}

void
encode_caldaysec(int calendar, int year, int month, int day, int hour, int minute, int second, int64_t *julday, int *secofday)
{
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    *julday = cdi_encode_day(dpy, year, month, day);
  else
    *julday = encode_julday(calendar, year, month, day);

  *secofday = hour * 3600 + minute * 60 + second;
}

void
decode_caldaysec(int calendar, int64_t julday, int secofday, int *year, int *month, int *day, int *hour, int *minute, int *second)
{
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    cdi_decode_day(dpy, julday, year, month, day);
  else
    decode_julday(calendar, julday, year, month, day);

  *hour = secofday / 3600;
  *minute = secofday / 60 - *hour * 60;
  *second = secofday - *hour * 3600 - *minute * 60;
}

#ifdef TEST2
int
main(void)
{
  int calendar = CALENDAR_STANDARD;
  int calday, secofday;
  int year, month, day, hour, minute, second;
  int value = 30;
  int factor = 86400;

  calendar = CALENDAR_360DAYS;

  year = 1979;
  month = 1;
  day = 15;
  hour = 12;
  minute = 30;
  second = 0;

  printf("calendar = %d\n", calendar);
  printf("%d/%02d/%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);

  encode_caldaysec(calendar, year, month, day, hour, minute, second, &calday, &secofday);

  decode_caldaysec(calendar, calday, secofday, &year, &month, &day, &hour, &minute, &second);
  printf("%d/%02d/%02d %02d:%02d:%02d   %d %d\n", year, month, day, hour, minute, second, calday, secofday);

  for (int i = 0; i < 420; i++)
    {
      decode_caldaysec(calendar, calday, secofday, &year, &month, &day, &hour, &minute, &second);
      printf("%2d %d/%02d/%02d %02d:%02d:%02d\n", i, year, month, day, hour, minute, second);
      julday_add_seconds(value * factor, &calday, &secofday);
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
