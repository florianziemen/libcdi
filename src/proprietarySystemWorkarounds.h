#ifndef INCLUDE_GUARD_CDI_PROPRIETARY_SYSTEM_WORKAROUNDS_H
#define INCLUDE_GUARD_CDI_PROPRIETARY_SYSTEM_WORKAROUNDS_H

char* myStrDup(const char* string);      //This exactly implements the standardized behavior of strdup().

char* myAsprintf(char* format, ...) __attribute__((format(printf, 1, 2)));       //This implementation differs from standard asprintf() function in the way the resulting string pointer is returned.

#endif
