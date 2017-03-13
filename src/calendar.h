#ifndef CALENDAR_H
#define CALENDAR_H

// clang-format off

#include <inttypes.h>

#include <stdlib.h>

#include "cdi.h"

#ifdef __cplusplus
extern "C" {
#endif

void encode_caldaysec(int calendar, int year, int month, int day, int hour, int minute, int second,
		      int64_t *julday, int *secofday);
void decode_caldaysec(int calendar, int64_t julday, int secofday,
		      int *year, int *month, int *day, int *hour, int *minute, int *second);

static inline int
calendar_dpy(int calendar)
{
  int daysPerYear = 0;

  if      (calendar == CALENDAR_360DAYS) daysPerYear = 360;
  else if (calendar == CALENDAR_365DAYS) daysPerYear = 365;
  else if (calendar == CALENDAR_366DAYS) daysPerYear = 366;

  return daysPerYear;
}

int days_per_year(int calendar, int year);
int days_per_month(int calendar, int year, int month);

void cdi_decode_day(int daysPerYear, int days, int *year, int *month, int *day);
int64_t cdi_encode_day(int daysPerYear, int year, int month, int day);

struct yearMonth
{
  int year, month;
};

/* normalizes month to range [1,12] and adjusts year accordingly */
static inline struct yearMonth
normalize_month(int year, int month)
{
  div_t modres = div(month - 1, 12);
  year += modres.quot - ((month < 1) & (modres.rem != 0));
  return (struct yearMonth){ .year = year, .month = (modres.rem + 12) % 12 + 1 };
}

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* CALENDAR_H */
