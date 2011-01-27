#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#if  defined  (HAVE_LIBGRIB_API)
#  include <grib_api.h>
#endif

#include <stdio.h>

#include "cdi.h"
#include "stream_int.h"
#include "gribapi.h"
#include "dmemory.h"

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


void gribapiDefHandles(int streamID)
{
  stream_t *streamptr;
   
  streamptr = stream_to_pointer(streamID);

#if  defined  (HAVE_LIBCGRIBEX)
  if ( streamptr->filetype == FILETYPE_GRB )
    {
    }
  else
#endif
    {
      int i, editionNumber = 2;
      if ( streamptr->filetype == FILETYPE_GRB ) editionNumber = 1;

      streamptr->gribHandle = (void **) malloc(streamptr->nvars*sizeof(void *));

      for ( i = 0; i < streamptr->nvars; ++i )
	streamptr->gribHandle[i] = gribHandleNew(editionNumber);
    }
}

