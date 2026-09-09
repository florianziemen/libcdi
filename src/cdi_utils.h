#ifndef CDI_UTILS_H
#define CDI_UTILS_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void gaussian_latitudes(size_t nlats, double *latitudes, double *weights);
  bool latitudes_are_gaussian(size_t nlats, const double *latitudes);

  // Used in CDO remap_scrip_io.cc
  void cdf_def_var_filter(int ncid, int ncvarID, const char *filterSpec);

#ifdef __cplusplus
}
#endif

#endif
