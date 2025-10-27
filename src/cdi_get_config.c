#include "cdi.h"
#include "cdf_config.h"

int
cdiGetConfig(int confType)
{
#if defined HAVE_NC4FILTER && HAVE_NC4FILTER
  static const int cdi_nc_has_filter = 1;
#else
  static const int cdi_nc_has_filter = 0;
#endif

  if (confType == CDI_NC_HAS_FILTER) return cdi_nc_has_filter;

  return 0;
}
