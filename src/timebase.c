#include <stdio.h>		/* for NULL */
#include <math.h>		/* for floor() */

#include "cdi.h"  		/* CALENDAR_ */


static int month_360[12] = {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30};
static int month_365[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int month_366[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


void decode_date(int date, int *year, int *month, int *day)
{
  int idate;

  *year  =  date / 10000;
  idate  = date - *year*10000;
  if ( idate < 0 ) idate = -idate;
  *month = idate / 100;
  *day   = idate - *month*100;
}


int encode_date(int year, int month, int day)
{
  int date;
  int iyear;

  iyear = year;
  if ( iyear < 0 ) iyear = -iyear;
  date = iyear*10000 + month*100 + day;
  if ( year < 0 ) date = -date;

  return (date);
}


void decode_time(int time, int *hour, int *minute)
{
  *hour   =  time / 100;
  *minute =  time - *hour*100;
}


int encode_time(int hour, int minute)
{
  int time;

  time = hour*100 + minute;

  return (time);
}


/* convert Julian date into year, months, day */
static void julday_to_gregdate(
    int  	       julday,		/* Julian day number to convert */
    int			*year,		/* Gregorian year (out) */
    int			*mon,		/* Gregorian month (1-12) (out) */
    int			*day		/* Gregorian day (1-31) (out) */
    )
{
  int a = julday;
  double b, c;
  double d, e, f;

  if ( a < 2299161 )
    {
      c = a + 1524;
    } 
  else
    {
      b = floor((a - 1867216.25)/36524.25);
      c = a + b - floor(b/4) + 1525;
    }

  d = floor((c - 122.1)/365.25);
  e = floor(365.25*d);
  f = floor((c - e)/30.6001);

  *day  = (int)(c - e - floor(30.6001*f));
  *mon  = (int)(f - 1 - 12*floor(f/14));
  *year = (int)(d - 4715 - floor((7 + *mon)/10));
}

/* convert year, month, day into Julian calendar day */
static int gregdate_to_julday(int year, int month, int day)
{
  int y;
  int m;
  int ib;
  int julday;

  if ( month <= 2 )
    {
      y = year  - 1;
      m = month + 12;
    }
  else
    {
      y = year;
      m = month;
    }

  if ( year > 1582 || (year == 1582 && (month > 10 || (month == 10 && day >= 15))) )
    {
      /*
      ** 15th October 1582 AD or later
      */
      ib = (int)(y/400) - (int)(y/100);
    }
  else
    {
      /*
      ** 4th October 1582 AD or earlier
      */
      ib = -2;
    }

  julday = (int) (floor(365.25*y) + (int)(30.6001*(m+1)) + ib + 1720996.5 + day + 0.5);

  return (julday);
}


static void decode_days(int dpy, int days, int *year, int *month, int *day)
{
  int i = 0;
  int *dpm = NULL;

  *year = (days-1) / dpy;
  days -= (*year*dpy);

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


static int encode_days(int dpy, int year, int month, int day)
{
  int i;
  int *dpm = NULL;
  double rval;

  rval = dpy * year + day;

  if      ( dpy == 360 ) dpm = month_360;
  else if ( dpy == 365 ) dpm = month_365;
  else if ( dpy == 366 ) dpm = month_366;
  
  if ( dpm ) for ( i = 0; i < month-1; i++ ) rval += dpm[i];

  return (rval);
}


int calendar_dpy(int calendar)
{
  int dpy = 0;

  if      ( calendar == CALENDAR_360DAYS ) dpy = 360;
  else if ( calendar == CALENDAR_365DAYS ) dpy = 365;
  else if ( calendar == CALENDAR_366DAYS ) dpy = 366;

  return (dpy);
}


int days_per_month(int calendar, int year, int month)
{
  int dayspermonth = 0;
  int *dpm = NULL;
  int dpy;

  dpy = calendar_dpy(calendar);

  if      ( dpy == 360 ) dpm = month_360;
  else if ( dpy == 365 ) dpm = month_365;
  else                   dpm = month_366;

  if ( month >= 1 && month <= 12 )
    dayspermonth = dpm[month-1];
  else
    fprintf(stderr, "days_per_month: month %d out of range\n", month);

  if ( dpy == 0 && month == 2 )
    {
      if ( (year%4 == 0 && year%100 != 0) || year%400 == 0 )
	dayspermonth = 29;
      else
	dayspermonth = 28;
    }

  return (dayspermonth);
}


int days_per_year(int calendar, int year)
{
  int daysperyear;
  int dpy;

  dpy = calendar_dpy(calendar);

  if ( dpy == 0 )
    {
      if ( year == 1582 )
	dpy = 355;
      else if ( (year%4 == 0 && year%100 != 0) || year%400 == 0 )
	dpy = 366;
      else
	dpy = 365;
    }

  daysperyear = dpy;
  
  return (daysperyear);
}


int encode_juldays(int dpy, int date)
{
  int juldays;
  int year, month, day;

  decode_date(date, &year, &month, &day);

  if ( dpy == 360 || dpy == 365 || dpy == 366 )
    juldays = encode_days(dpy, year, month, day);
  else
    juldays = gregdate_to_julday(year, month, day);

  return (juldays);
}


int decode_juldays(int dpy, int juldays)
{
  int date;
  int year, month, day;

  if ( dpy == 360 || dpy == 365 || dpy == 366 )
    decode_days(dpy, juldays, &year, &month, &day);
  else
    julday_to_gregdate(juldays, &year, &month, &day);

  date = encode_date(year, month, day);

  return (date);
}


int encode_secofday(int time)
{
  int secofday;
  int hour, minute;

  decode_time(time, &hour, &minute);

  secofday = (hour*60 + minute)*60;

  return (secofday);
}


int decode_secofday(int secofday)
{
  int time;
  int hour, minute;

  hour   = secofday/3600;
  minute = secofday/60 - hour*60;

  time = hour*100 + minute;

  return (time);
}


void julian_add_seconds(int dpy, int seconds, int *juldays, int *secofday)
{
  int days;

  *secofday += seconds;

  days = *secofday/86400;

  *secofday -= days*86400;

  *juldays += days;
}

#define TEST
#ifdef TEST
int main(void)
{
  int nmin;
  int vdate0, vtime0;
  int vdate, vtime;
  int dpy = 0;
  int ijulinc;
  int i, j = 0;
  int year, mon, day, hour, min;
  int juldays, secofday;

  /* 1 - Check valid range of years */

  nmin = 11000;
  vdate0 = -80001201;
  vtime0 = 1205;

  printf("start time: %8d %4d\n", vdate0, vtime0);

  for ( i = 0; i < nmin; i++ )
    {
      decode_date(vdate0, &year, &mon, &day);
      decode_time(vtime0, &hour, &min);

      juldays  = encode_juldays(dpy, vdate0);
      secofday = encode_secofday(vtime0);

      vdate = decode_juldays(dpy, juldays);
      vtime = decode_secofday(secofday);

      if ( vdate0 != vdate || vtime0 != vtime )
	printf("%4d %8d %4d %8d %4d %9d %9d\n",
	       ++j, vdate0, vtime0, vdate, vtime, juldays, secofday);

      year++;
      vdate0 = encode_date(year, mon, day);
      vtime0 = encode_time(hour, min);
    }

  printf("stop time: %8d %4d\n", vdate0, vtime0);

  /* 2 - Check time increment of one minute */

  nmin = 120000;
  ijulinc = 60;
  vdate0 = 20001201;
  vtime0 = 0;

  printf("start time: %8d %4d\n", vdate0, vtime0);

  juldays = encode_juldays(dpy, vdate0);
  secofday = encode_secofday(vtime0);
  for ( i = 0; i < nmin; i++ )
    {
      decode_date(vdate0, &year, &mon, &day);
      decode_time(vtime0, &hour, &min);

      if ( ++min >= 60 )
	{
	  min = 0;
	  if ( ++hour >= 24 )
	    {
	      hour = 0;
	      if ( ++day >= 32 )
		{
		  day = 1;
		  if ( ++mon >= 13 )
		    {
		      mon = 1;
		      year++;
		    }
		}
	    }
	}

      vdate0 = encode_date(year, mon, day);
      vtime0 = encode_time(hour, min);

      julian_add_seconds(dpy, ijulinc, &juldays, &secofday);

      vdate = decode_juldays(dpy, juldays);
      vtime = decode_secofday(secofday);
      if ( vdate0 != vdate || vtime0 != vtime )
	printf("%4d %8d %4d %8d %4d %9d %9d\n",
	       ++j, vdate0, vtime0, vdate, vtime, juldays, secofday);
    }

  printf("stop time: %8d %4d\n", vdate0, vtime0);

  return (0);
}
#endif


void encode_calendar(int dpy, int year, int month, int day, int hour, int minute, int *juldays, int *secofday)
{
  if ( dpy == 360 || dpy == 365 || dpy == 366 )
    *juldays = encode_days(dpy, year, month, day);
  else
    *juldays = gregdate_to_julday(year, month, day);

  *secofday = (hour*60 + minute)*60;
}


void decode_calendar(int dpy, int juldays, int secofday, int *year, int *month, int *day, int *hour, int *minute)
{
  if ( dpy == 360 || dpy == 365 || dpy == 366 )
    decode_days(dpy, juldays, year, month, day);
  else
    julday_to_gregdate(juldays, year, month, day);

  *hour   = secofday/3600;
  *minute = secofday/60 - *hour*60;
}


#ifdef TEST2
int main(void)
{
  int i, dpy = -1;
  int julday, secofday;
  int year, month, day, hour, minute;
  int value = 30;
  int factor = 86400;

  year=1979; month=1; day=15; hour=12; minute=30;

  printf("dpy = %d\n", dpy);
  printf("%d/%02d/%02d %02d:%02d\n", year, month, day, hour, minute);

  encode_calendar(dpy, year, month, day, hour, minute, &julday, &secofday);

  decode_calendar(dpy, julday, secofday, &year, &month, &day, &hour, &minute);
  printf("%d/%02d/%02d %02d:%02d   %d %d\n", year, month, day, hour, minute, julday, secofday);

  for ( i = 0; i < 420; i++ )
    {

      decode_calendar(dpy, julday, secofday, &year, &month, &day, &hour, &minute);
      printf("%2d %d/%02d/%02d %02d:%02d\n", i, year, month, day, hour, minute);
      julian_add_seconds(dpy, value*factor, &julday, &secofday);
    }

  return (0);
}
#endif
