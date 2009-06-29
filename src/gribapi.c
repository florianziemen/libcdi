#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#if  defined  (HAVE_LIBGRIB_API)
#  include "grib_api.h"
#endif

#include <stdio.h>


#define XSTRING(x)	#x
#define STRING(x)	XSTRING(x)

static char gribapi_libvers[64] = "";

const char *gribapiLibraryVersion(void)
{
#if  defined  (HAVE_LIBGRIB_API)
  long version = grib_get_api_version();
  int major_version, minor_version, revision_version;

  major_version    = version/10000;
  minor_version    = (version-major_version*10000)/100;
  revision_version = (version-major_version*10000-minor_version*100);

  sprintf(gribapi_libvers, "%d.%d.%d",
	  major_version, minor_version, revision_version);
#endif

  return (gribapi_libvers);
}
