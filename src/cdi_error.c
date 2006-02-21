#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>
#include <errno.h>
#include "cdi.h"

static char UnknownError[] = "Unknown Error";

const char *cdiStringError(int cdiErrno)
{
  switch (cdiErrno) {
  case CDI_ESYSTEM:
    {
      const char *cp = (const char *) strerror(errno);
      if ( cp == NULL ) break;
      return cp;
    }
  case CDI_EUFTYPE:
    return "Unsupported file type";
  case CDI_ELIBNAVAIL:
    return "Unsupported file type (library not available)";
  case CDI_EUFSTRUCT:
    return "Unsupported file structure";
  }

  return UnknownError;
}
