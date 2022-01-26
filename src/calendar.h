#ifndef CALENDAR_H
#define CALENDAR_H

// clang-format off

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void encode_caldaysec(int calendar, int year, int month, int day, int hour, int minute, int second,
		      int64_t *julday, int *secofday);
void decode_caldaysec(int calendar, int64_t julday, int secofday,
		      int *year, int *month, int *day, int *hour, int *minute, int *second);

int calendar_dpy(int calendar);
int days_per_year(int calendar, int year);
int days_per_month(int calendar, int year, int month);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* CALENDAR_H */
