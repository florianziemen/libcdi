#ifndef CALENDAR_H
#define CALENDAR_H

#include "cdi.h"
#include <stdint.h>    // int64_t

// clang-format off

#ifdef __cplusplus
extern "C" {
#endif

void decode_calday(int daysPerYear, int days, int *year, int *month, int *day);
int64_t encode_calday(int daysPerYear, int year, int month, int day);

int calendar_dpy(int calendar);
int days_per_year(int calendar, int year);
int days_per_month(int calendar, int year, int month);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* CALENDAR_H */
