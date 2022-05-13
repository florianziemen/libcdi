#ifndef CALENDAR_H
#define CALENDAR_H

// clang-format off

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t encode_calday(int calendar, int year, int month, int day);
void decode_calday(int calendar, int64_t julday, int *year, int *month, int *day);

int calendar_dpy(int calendar);
int days_per_year(int calendar, int year);
int days_per_month(int calendar, int year, int month);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* CALENDAR_H */
