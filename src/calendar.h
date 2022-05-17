#ifndef CALENDAR_H
#define CALENDAR_H

// clang-format off

#include "cdi.h"

#ifdef __cplusplus
extern "C" {
#endif

int64_t calDay_encode(int calendar, CdiDate date);
CdiDate calDay_decode(int calendar, int64_t julday);

int calendar_dpy(int calendar);
int days_per_year(int calendar, int year);
int days_per_month(int calendar, int year, int month);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* CALENDAR_H */
