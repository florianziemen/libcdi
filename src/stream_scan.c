#include "dmemory.h"
#include "stream_scan.h"


int streamScanInitRecords2(stream_t *streamptr)
{
  const int nrecords = streamptr->tsteps[1].nallrecs;
  streamptr->tsteps[1].recIDs = (int *) Malloc((size_t)nrecords*sizeof(int));
  streamptr->tsteps[1].nrecs = 0;

  for (int recID = 0; recID < nrecords; ++recID)
    streamptr->tsteps[1].recIDs[recID] = -1;

  for (int recID = 0; recID < nrecords; ++recID)
    {
      streamptr->tsteps[1].records[recID].position = streamptr->tsteps[0].records[recID].position;
      streamptr->tsteps[1].records[recID].size     = streamptr->tsteps[0].records[recID].size;
    }

  return nrecords;
}


void streamScanResizeRecords(stream_t *streamptr)
{
  const int nrecords = streamptr->tsteps[0].nallrecs;
  if (nrecords < streamptr->tsteps[0].recordSize)
    {
      streamptr->tsteps[0].recordSize = nrecords;
      streamptr->tsteps[0].records =
        (record_t *) Realloc(streamptr->tsteps[0].records, (size_t)nrecords*sizeof(record_t));
    }

  streamptr->tsteps[0].recIDs = (int *) Malloc((size_t)nrecords*sizeof(int));
  streamptr->tsteps[0].nrecs = nrecords;
  for (int recID = 0; recID < nrecords; ++recID)
    streamptr->tsteps[0].recIDs[recID] = recID;
}


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
