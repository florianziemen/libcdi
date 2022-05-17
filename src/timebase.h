#ifndef TIMEBASE_H
#define TIMEBASE_H

// clang-format off

#include <julian_date.h>

#ifdef __cplusplus
extern "C" {
#endif

// date format:  YYYYMMDD
// time format:  hhmmss

int64_t date_to_julday(int calendar, int64_t date);  // Used in paraview vtkCDIReader.cxx
int64_t julday_to_date(int calendar, int64_t julianDay);

int time_to_sec(int time);                           // Used in paraview vtkCDIReader.cxx
int sec_to_time(int seconds);

void decode_julday(int calendar, int64_t julianDay, int *year, int *mon, int *day);
int64_t encode_julday(int calendar, int year, int month, int day);

double encode_secofday(int hour, int minute, int second, int ms);
void decode_secofday(double secOfDay, int *hour, int *minute, int *second, int *ms);

void julday_add_seconds(int64_t seconds, int64_t *julianDay, double *secondOfDay);
double julday_sub(int64_t julianDay1, double secondOfDay1, int64_t julianDay2, double secondOfDay2, int64_t *days, double *secs);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif /* TIMEBASE_H */
