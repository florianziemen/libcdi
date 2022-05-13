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

JulianDate julianDateEncode(int calendar, CdiDateTime dt);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* JULIAN_DATE_H */
