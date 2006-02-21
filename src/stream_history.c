#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "stream_cdf.h"


void streamDefHistory(int streamID, int length, const char *history)
{
  static char func[] = "streamDefHistory";

  if ( streams[streamID].filetype == FILETYPE_NC || streams[streamID].filetype == FILETYPE_NC2 )
    {
      char *histstring;
      size_t len;
      if ( history )
	{
	  len = strlen(history);
	  if ( len )
	    {
	      histstring = strdupx(history);
	      cdfDefHistory(streamID, length, histstring);
	      free(histstring);
	    }
	}
    }
}


int streamInqHistorySize(int streamID)
{
  int size = 0;

  if ( streams[streamID].filetype == FILETYPE_NC || streams[streamID].filetype == FILETYPE_NC2 )
    {
      size = cdfInqHistorySize(streamID);
    }

  return (size);
}


void streamInqHistoryString(int streamID, char *history)
{

  if ( streams[streamID].filetype == FILETYPE_NC || streams[streamID].filetype == FILETYPE_NC2 )
    {
      cdfInqHistoryString(streamID, history);
    }
}
