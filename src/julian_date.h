#ifndef JULIAN_DATE_H
#define JULIAN_DATE_H

// clang-format off

#include "cdi.h"
#include "calendar.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int64_t julianDay;
  double secondOfDay;
} JulianDate;

JulianDate julianDate_encode(int calendar, CdiDateTime dt);
double julianDate_sub(JulianDate julianDate1, JulianDate julianDate2, int64_t *days, double *secs);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* JULIAN_DATE_H */
