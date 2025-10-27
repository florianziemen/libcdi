#include "cdi.h"
#include "cdf_config.h"

#include <netcdf_meta.h>

int
cdiGetConfig(int confType)
{
#if defined HAVE_NC4FILTER && HAVE_NC4FILTER
  static const int cdi_nc_has_filter = 1;
#else
  static const int cdi_nc_has_filter = 0;
#endif
#if defined NC_HAS_DAP2 && NC_HAS_DAP2
  static const int cdi_nc_has_dap = 1;
#else
  static const int cdi_nc_has_dap = 0;
#endif

  if (confType == CDI_NC_HAS_FILTER) return cdi_nc_has_filter;
  if (confType == CDI_NC_HAS_DAP) return cdi_nc_has_dap;

  return 0;
}
