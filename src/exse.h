#ifndef EXSE_H
#define EXSE_H

#include <limits.h>


enum {
  EXSE_SINGLE_PRECISION = 4,
  EXSE_DOUBLE_PRECISION = 8,
};


static inline
void exse_check_gridsize(const char *format, size_t gridsize)
{
  if ( gridsize > INT_MAX ) Error("%s format grid size (%zu) limit exceeded (%zu)!", format, gridsize, INT_MAX);
}

#endif
