#ifndef _CALENDAR_H
#define _CALENDAR_H

#include <stdlib.h>

#include "cdi.h"

#ifdef __cplusplus
extern "C" {
#endif

void encode_caldaysec(int calendar, int year, int month, int day, int hour, int minute, int second,
		      int *julday, int *secofday);
void decode_caldaysec(int calendar, int julday, int secofday,
		      int *year, int *month, int *day, int *hour, int *minute, int *second);

static inline int
calendar_dpy(int calendar)
{
  int daysperyear = 0;

  if      ( calendar == CALENDAR_360DAYS ) daysperyear = 360;
  else if ( calendar == CALENDAR_365DAYS ) daysperyear = 365;
  else if ( calendar == CALENDAR_366DAYS ) daysperyear = 366;

  return daysperyear;
}

int days_per_year(int calendar, int year);
int days_per_month(int calendar, int year, int month);

void cdi_decode_day(int dpy, int days, int *year, int *month, int *day);
int cdi_encode_day(int dpy, int year, int month, int day);


struct yearMonth
{
  int year, month;
};

/* normalizes month to range [1,12] and adjusts year accordingly */
static inline struct yearMonth
normalize_month(int year, int month)
{
  div_t modres = div(month-1, 12);
  year += modres.quot - ((month < 1) & (modres.rem != 0));
  return (struct yearMonth){ .year = year, .month = (modres.rem + 12)%12 + 1 };
}


#if defined (__cplusplus)
}
#endif

#endif  /* _CALENDAR_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
