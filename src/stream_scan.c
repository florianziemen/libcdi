#include "stream_scan.h"


void streamScanTimeConstAdjust(stream_t *streamptr, const taxis_t *taxis)
{
  const int vlistID = streamptr->vlistID;
  if (streamptr->ntsteps == 1)
    {
      if (taxis->vdate == 0 && taxis->vtime == 0)
	{
	  streamptr->ntsteps = 0;
	  for (int varID = 0; varID < streamptr->nvars; ++varID)
            vlistDefVarTimetype(vlistID, varID, TIME_CONSTANT);
	}
    }
}


void streamScanTsFixNtsteps(stream_t *streamptr, off_t recpos)
{
  if (streamptr->ntsteps == -1)
    {
      const int tsID = tstepsNewEntry(streamptr);
      if (tsID != streamptr->rtsteps)
	Error("Internal error. tsID = %d", tsID);

      streamptr->tsteps[tsID-1].next = true;
      streamptr->tsteps[tsID].position = recpos;
    }
}
