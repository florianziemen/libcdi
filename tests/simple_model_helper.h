#ifndef SIMPLE_MODEL_HELPER_H
#define SIMPLE_MODEL_HELPER_H

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <time.h>

void
var_scale(int datatype, double *mscale, double *mrscale);

static inline double
sign_flat(double v)
{
  if (v == 0.0)
    return 0.0;
  return v;
}

time_t
cditime2time_t(int date, int timeofday);
void
time_t2cditime(time_t t, int *date, int *timeofday);

#ifdef USE_MPI
int
uniform_partition_start(int set_interval[2], int nparts, int part_idx);
#endif

#endif
