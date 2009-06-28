#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#if  defined  (HAVE_LIBGRIB_API)
#  include "grib_api.h"
#endif

#include <stdio.h>


#define XSTRING(x)	#x
#define STRING(x)	XSTRING(x)

#if  defined  (HAVE_LIBGRIB_API)
static const char gribapi_libvers[] = "xxx";
#else
static const char gribapi_libvers[] = "";
#endif

const char *gribapiLibraryVersion(void)
{
  return (gribapi_libvers);
}
