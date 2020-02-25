#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "stream_cdf.h"

#ifdef HAVE_LIBNETCDF
static inline bool
filetypeIsNetCDF(int filetype)
{
  return filetype == CDI_FILETYPE_NC
    ||   filetype == CDI_FILETYPE_NC2
    ||   filetype == CDI_FILETYPE_NC5
    ||   filetype == CDI_FILETYPE_NC4
    ||   filetype == CDI_FILETYPE_NC4C;
}
#endif

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
