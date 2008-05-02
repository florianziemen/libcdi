#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>
#include <errno.h>
#include "cdi.h"

static char UnknownError[] = "Unknown Error";
static char _EUFTYPE[]     = "Unsupported file type";
static char _ELIBNAVAIL[]  = "Unsupported file type (library support not compiled in)";
static char _EUFSTRUCT[]   = "Unsupported file structure";
static char _EUNC4[]       = "Unsupported netCDF4 structure";

char *cdiStringError(int cdiErrno)
{
  switch (cdiErrno) {
  case CDI_ESYSTEM:
    {
      char *cp = (char *) strerror(errno);
      if ( cp == NULL ) break;
      return cp;
    }
  case CDI_EUFTYPE:    return _EUFTYPE;
  case CDI_ELIBNAVAIL: return _ELIBNAVAIL;
  case CDI_EUFSTRUCT:  return _EUFSTRUCT;
  case CDI_EUNC4:      return _EUNC4;
  }

  return UnknownError;
}
