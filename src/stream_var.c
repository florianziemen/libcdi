#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>

#include "dmemory.h"
#include "error.h"

#include "cdi.h"
#include "stream_int.h"


static void streamvarInitEntry(int streamID, int varID)
{
  streams[streamID].vars[varID].ncvarid      = CDI_UNDEFID;
  streams[streamID].vars[varID].defmiss      = 0;
  streams[streamID].vars[varID].nlevs        = 0;
  streams[streamID].vars[varID].level        = NULL;
  streams[streamID].vars[varID].lindex       = NULL;

  streams[streamID].vars[varID].timeID       = CDI_UNDEFID;
  streams[streamID].vars[varID].gridID       = CDI_UNDEFID;
  streams[streamID].vars[varID].zaxisID      = CDI_UNDEFID;
  streams[streamID].vars[varID].level        = NULL;
  streams[streamID].vars[varID].nlevs        = 0;
}


static int streamvarNewEntry(int streamID)
{
  static char func[] = "streamvarNewEntry";
  int varID = 0;
  int streamvarSize;
  SVARINFO *streamvar;

  streamvarSize = streams[streamID].varsAllocated;
  streamvar     = streams[streamID].vars;
  /*
    Look for a free slot in streamvar.
    (Create the table the first time through).
  */
  if ( ! streamvarSize )
    {
      int i;

      streamvarSize = 2;
      streamvar = (SVARINFO *) malloc(streamvarSize*sizeof(SVARINFO));
      if ( streamvar == NULL )
	{
          Message(func, "streamvarSize = %d", streamvarSize);
	  SysError(func, "Allocation of SVARINFO failed");
	}

      for ( i = 0; i < streamvarSize; i++ )
	streamvar[i].isUsed = FALSE;
    }
  else
    {
      while ( varID < streamvarSize )
	{
	  if ( ! streamvar[varID].isUsed ) break;
	  varID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if ( varID == streamvarSize )
    {
      int i;

      streamvarSize = 2*streamvarSize;
      streamvar = (SVARINFO *) realloc(streamvar, streamvarSize*sizeof(SVARINFO));
      if ( streamvar == NULL )
	{
          Message(func, "streamvarSize = %d", streamvarSize);
	  SysError(func, "Reallocation of SVARINFO failed");
	}
      varID = streamvarSize/2;

      for ( i = varID; i < streamvarSize; i++ )
	streamvar[i].isUsed = FALSE;
    }

  streams[streamID].varsAllocated = streamvarSize;
  streams[streamID].vars          = streamvar;

  streamvarInitEntry(streamID, varID);

  streams[streamID].vars[varID].isUsed = TRUE;

  return (varID);
}


int streamNewVar(int streamID, int gridID, int zaxisID)
{
  static char func[] = "streamNewVar";
  int varID;
  int *level;
  int *lindex;
  int nlevs;
  int levID;

  if ( CDI_Debug )
    Message(func, "gridID = %d  zaxisID = %d", gridID, zaxisID);

  varID = streamvarNewEntry(streamID);

  streams[streamID].nvars++;

  streams[streamID].vars[varID].gridID  = gridID;
  streams[streamID].vars[varID].zaxisID = zaxisID;

  nlevs = zaxisInqSize(zaxisID);

  level  = (int *) malloc(nlevs*sizeof(int));
  lindex = (int *) malloc(nlevs*sizeof(int));

  for ( levID = 0; levID < nlevs; levID++ )
    level[levID] = CDI_UNDEFID;

  for ( levID = 0; levID < nlevs; levID++ )
    lindex[levID] = levID;

  streams[streamID].vars[varID].nlevs  = nlevs;
  streams[streamID].vars[varID].level  = level;
  streams[streamID].vars[varID].lindex = lindex;

  return (varID);
}
