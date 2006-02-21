#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

/*
 * A version string.
 */

#if defined (VERSION)
   static const char cdi_libvers[] = VERSION " of "__DATE__" "__TIME__;
#else
#  error "VERSION undefined"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

const char *cdiLibraryVersion(void);

#if defined(__cplusplus)
}
#endif

const char *cdiLibraryVersion(void)
{
  return (cdi_libvers);
}
