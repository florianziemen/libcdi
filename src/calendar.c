#include <limits.h>
#include <stdio.h>

#include "cdi.h"  		/* CALENDAR_ */
#include "calendar.h"
#include "error.h"
#include "timebase.h"


static const int month_360[12] = {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30};
static const int month_365[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const int month_366[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


int calendar_dpy(int calendar)
{
  int daysperyear = 0;

  if      ( calendar == CALENDAR_360DAYS ) daysperyear = 360;
  else if ( calendar == CALENDAR_365DAYS ) daysperyear = 365;
  else if ( calendar == CALENDAR_366DAYS ) daysperyear = 366;

  return daysperyear;
}


int days_per_month(int calendar, int year, int month)
{
  int daysperyear = calendar_dpy(calendar);

  const int *dpm;
  if      ( daysperyear == 360 ) dpm = month_360;
  else if ( daysperyear == 365 ) dpm = month_365;
  else                           dpm = month_366;

  int dayspermonth = 0;
  if ( month >= 1 && month <= 12 )
    dayspermonth = dpm[month-1];
  /*
  else
    fprintf(stderr, "days_per_month: month %d out of range\n", month);
  */
  if ( daysperyear == 0 && month == 2 )
    {
      if ( (year%4 == 0 && year%100 != 0) || year%400 == 0 )
	dayspermonth = 29;
      else
	dayspermonth = 28;
    }

  return dayspermonth;
}


int days_per_year(int calendar, int year)
{
  int daysperyear = calendar_dpy(calendar);

  if ( daysperyear == 0 )
    {
      if ( year == 1582 && calendar == CALENDAR_STANDARD )
        daysperyear = 355;
      else if ( (year%4 == 0 && year%100 != 0) || year%400 == 0 )
        daysperyear = 366;
      else
        daysperyear = 365;
    }

  return daysperyear;
}


void cdi_decode_day(int dpy, int days, int *year, int *month, int *day)
{
  int i = 0;

  *year = (days-1) / dpy;
  days -= (*year*dpy);

  const int *dpm = NULL;
  if      ( dpy == 360 ) dpm = month_360;
  else if ( dpy == 365 ) dpm = month_365;
  else if ( dpy == 366 ) dpm = month_366;

  if ( dpm )
    for ( i = 0; i < 12; i++ )
      {
	if ( days > dpm[i] ) days -= dpm[i];
	else break;
      }

  *month = i + 1;
  *day   = days;
}


int cdi_encode_day(int dpy, int year, int month, int day)
{
  long rval = (long)dpy * year + day;

  const int *dpm = NULL;
  if      ( dpy == 360 ) dpm = month_360;
  else if ( dpy == 365 ) dpm = month_365;
  else if ( dpy == 366 ) dpm = month_366;

  if ( dpm ) for ( int i = 0; i < month-1; i++ ) rval += dpm[i];
  if ( rval > INT_MAX || rval < INT_MIN )
    Error("Unhandled date: %ld", rval);

  return (int)rval;
}


void encode_caldaysec(int calendar, int year, int month, int day, int hour, int minute, int second,
		      int *julday, int *secofday)
{
  int dpy = calendar_dpy(calendar);

  if ( dpy == 360 || dpy == 365 || dpy == 366 )
    *julday = cdi_encode_day(dpy, year, month, day);
  else
    *julday = encode_julday(calendar, year, month, day);

  *secofday = hour*3600 + minute*60 + second;
}


void decode_caldaysec(int calendar, int julday, int secofday,
		      int *year, int *month, int *day, int *hour, int *minute, int *second)
{
  int dpy = calendar_dpy(calendar);

  if ( dpy == 360 || dpy == 365 || dpy == 366 )
    cdi_decode_day(dpy, julday, year, month, day);
  else
    decode_julday(calendar, julday, year, month, day);

  *hour   = secofday/3600;
  *minute = secofday/60 - *hour*60;
  *second = secofday - *hour*3600 - *minute*60;
}


#ifdef TEST2
int main(void)
{
  int calendar = CALENDAR_STANDARD;
  int i;
  int calday, secofday;
  int year, month, day, hour, minute, second;
  int value = 30;
  int factor = 86400;

  calendar = CALENDAR_360DAYS;

  year=1979; month=1; day=15; hour=12; minute=30; second = 0;

  printf("calendar = %d\n", calendar);
  printf("%d/%02d/%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);

  encode_caldaysec(calendar, year, month, day, hour, minute, second, &calday, &secofday);

  decode_caldaysec(calendar, calday, secofday, &year, &month, &day, &hour, &minute, &second);
  printf("%d/%02d/%02d %02d:%02d:%02d   %d %d\n", year, month, day, hour, minute, second, calday, secofday);

  for ( i = 0; i < 420; i++ )
    {

      decode_caldaysec(calendar, calday, secofday, &year, &month, &day, &hour, &minute, &second);
      printf("%2d %d/%02d/%02d %02d:%02d:%02d\n", i, year, month, day, hour, minute, second);
      julday_add_seconds(value*factor, &calday, &secofday);
    }

  return (0);
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
