#include "cdi_datetime.h"
#include <stdlib.h>

// ==================================================================
void
cdiDecodeDate(int date, int *year, int *month, int *day)
{
  int iyear = date / 10000;
  *year = iyear;
  int idate = date - iyear * 10000;
  if (idate < 0) idate = -idate;
  int imonth = idate / 100;
  *month = imonth;
  *day = idate - imonth * 100;
}

int
cdiEncodeDate(int year, int month, int day)
{
  int iyear = abs(year);
  int date = iyear * 10000 + month * 100 + day;
  if (year < 0) date = -date;

  return date;
}

void
cdiDecodeTime(int time, int *hour, int *minute, int *second)
{
  int ihour = time / 10000, itime = time - ihour * 10000, iminute = itime / 100;
  *hour = ihour;
  *minute = iminute;
  *second = itime - iminute * 100;
}

int
cdiEncodeTime(int hour, int minute, int second)
{
  return hour * 10000 + minute * 100 + second;
}
// ==================================================================

CdiDate
cdiDateSet(int date)
{
  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);

  CdiDate cdiDate;
  cdiDate.year = year;
  cdiDate.month = (short)month;
  cdiDate.day = (short)day;

  return cdiDate;
}

CdiTime
cdiTimeSet(int time)
{
  int hour, minute, second, ms = 0;
  cdiDecodeTime(time, &hour, &minute, &second);

  CdiTime cdiTime;
  cdiTime.hour = (short)hour;
  cdiTime.minute = (short)minute;
  cdiTime.second = (short)second;
  cdiTime.ms = ms;

  return cdiTime;
}

CdiDateTime
cdiDateTime_set(int date, int time)
{
  CdiDateTime cdiDateTime;
  cdiDateTime.date = cdiDateSet(date);
  cdiDateTime.time = cdiTimeSet(time);

  return cdiDateTime;
}

int
cdiDateGet(CdiDate cdiDate)
{
  return cdiEncodeDate(cdiDate.year, cdiDate.month, cdiDate.day);
}

int
cdiTimeGet(CdiTime cdiTime)
{
  return cdiEncodeTime(cdiTime.hour, cdiTime.minute, cdiTime.second);
}

CdiDate
cdiDate_encode(int year, int month, int day)
{
  CdiDate cdiDate;
  cdiDate.year = year;
  cdiDate.month = month;
  cdiDate.day = day;

  return cdiDate;
}

void
cdiDate_decode(CdiDate cdiDate, int *year, int *month, int *day)
{
  *year = cdiDate.year;
  *month = cdiDate.month;
  *day = cdiDate.day;
}

CdiTime
cdiTime_encode(int hour, int minute, int second, int ms)
{
  CdiTime cdiTime;
  cdiTime.hour = hour;
  cdiTime.minute = minute;
  cdiTime.second = second;
  cdiTime.ms = ms;

  return cdiTime;
}

void
cdiTime_decode(CdiTime cdiTime, int *hour, int *minute, int *second, int *ms)
{
  *hour = cdiTime.hour;
  *minute = cdiTime.minute;
  *second = cdiTime.second;
  *ms = cdiTime.ms;
}

void
cdiDate_init(CdiDate *cdiDate)
{
  cdiDate->year = 0;
  cdiDate->month = 0;
  cdiDate->day = 0;
}

void
cdiTime_init(CdiTime *cdiTime)
{
  cdiTime->hour = 0;
  cdiTime->minute = 0;
  cdiTime->second = 0;
  cdiTime->ms = 0;
}

void
cdiDateTime_init(CdiDateTime *cdiDateTime)
{
  cdiDate_init(&cdiDateTime->date);
  cdiTime_init(&cdiDateTime->time);
}

bool
cdiDateTime_isEQ(const CdiDateTime cdiDateTime1, const CdiDateTime cdiDateTime2)
{
  // clang-format off
  return (cdiDateTime1.date.year == cdiDateTime2.date.year
      && cdiDateTime1.date.month == cdiDateTime2.date.month
      && cdiDateTime1.date.day == cdiDateTime2.date.day
      && cdiDateTime1.time.hour == cdiDateTime2.time.hour
      && cdiDateTime1.time.minute == cdiDateTime2.time.minute
      && cdiDateTime1.time.second == cdiDateTime2.time.second
      && cdiDateTime1.time.ms == cdiDateTime2.time.ms);
  // clang-format on
}

bool
cdiDateTime_isNE(const CdiDateTime cdiDateTime1, const CdiDateTime cdiDateTime2)
{
  return !cdiDateTime_isEQ(cdiDateTime1, cdiDateTime2);
}

bool
cdiDateTime_isNull(const CdiDateTime cdiDateTime)
{
  // clang-format off
  return (cdiDateTime.date.year == 0
      && cdiDateTime.date.month == 0
      && cdiDateTime.date.day == 0
      && cdiDateTime.time.hour == 0
      && cdiDateTime.time.minute == 0
      && cdiDateTime.time.second == 0
      && cdiDateTime.time.ms == 0);
  // clang-format on
}
