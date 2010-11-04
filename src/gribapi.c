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
  stream_t *streamptr = NULL;
  int vlistID, ngrids, nzaxis, i;
   
  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  ngrids = vlistNgrids(vlistID);
  nzaxis = vlistNzaxis(vlistID);

  streamptr->gribHandleNums = ngrids*nzaxis;
  streamptr->gribHandle = (void **) malloc(streamptr->gribHandleNums*sizeof(void *));

  for ( i = 0; i < streamptr->gribHandleNums; ++i )
    streamptr->gribHandle[i] = gribHandleNew();
}

