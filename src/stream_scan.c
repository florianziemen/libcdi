#include "stream_scan.h"


void streamScanTimeConstAdjust(stream_t *streamptr, const taxis_t *taxis)
{
  const int vlistID = streamptr->vlistID;
  if ( streamptr->ntsteps == 1 )
    {
      if ( taxis->vdate == 0 && taxis->vtime == 0 )
	{
	  streamptr->ntsteps = 0;
	  for ( int varID = 0; varID < streamptr->nvars; varID++ )
            vlistDefVarTimetype(vlistID, varID, TIME_CONSTANT);
	}
    }
}
