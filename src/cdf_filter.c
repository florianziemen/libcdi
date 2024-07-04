#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cdf_filter.h"

#if defined HAVE_DECL_NC_INQ_VAR_FILTER_IDS && HAVE_DECL_NC_INQ_VAR_FILTER_IDS
#include <netcdf.h>
#include <netcdf_filter.h>

#include <string.h>
#include "error.h"

static void
check_length(size_t maxLength, size_t len)
{
  if (len >= maxLength) Error("Internal error: size of filterSpec to small!");
}
#endif

bool
cdf_get_var_filter(int ncid, int varid, char *filterSpec, size_t maxLength)
{
  bool hasFilter = false;
#if defined HAVE_DECL_NC_INQ_VAR_FILTER_IDS && HAVE_DECL_NC_INQ_VAR_FILTER_IDS
  size_t numFilters = 0;
  nc_inq_var_filter_ids(ncid, varid, &numFilters, NULL);
  if (numFilters > 0 && numFilters < 16)
    {
      unsigned int filterids[16];
      nc_inq_var_filter_ids(ncid, varid, &numFilters, filterids);
      for (size_t k = 0; k < numFilters; ++k)
        {
          if (k > 0)
            {
              size_t len = strlen(filterSpec);
              check_length(maxLength, len);
              strncat(filterSpec, "|", maxLength - len - 1);
            }
          unsigned filterId = filterids[k];
          {
            size_t len = strlen(filterSpec);
            check_length(maxLength, len);
            snprintf(filterSpec + len, maxLength - len - 1, "%u", filterId);
          }
          size_t numParams;
          nc_inq_var_filter_info(ncid, varid, filterId, &numParams, NULL);
          if (numParams <= 16)
            {
              unsigned int params[16];
              nc_inq_var_filter_info(ncid, varid, filterId, &numParams, params);
              for (size_t i = 0; i < numParams; ++i)
                {
                  size_t len = strlen(filterSpec);
                  check_length(maxLength, len);
                  snprintf(filterSpec + len, maxLength - len - 1, ",%u", params[i]);
                }
            }
        }
      if (filterSpec[0]) hasFilter = true;
    }
#else
  (void) ncid;
  (void) varid;
  (void) filterSpec;
  (void) maxLength;
#endif
  return hasFilter;
}
