/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include "dmemory.h"
#include "stream_scan.h"

void
streamScanResizeRecords1(stream_t *streamptr)
{
  tsteps_t *step1 = &streamptr->tsteps[0];
  int nrecords = step1->nallrecs;
  if (nrecords < step1->recordSize)
  {
    step1->recordSize = nrecords;
    step1->recinfo = (recinfo_t *) Realloc(step1->recinfo, (size_t) nrecords * sizeof(recinfo_t));
    step1->records = (record_t *) Realloc(step1->records, (size_t) nrecords * sizeof(record_t));
  }

  step1->recIDs = (int *) Malloc((size_t) nrecords * sizeof(int));
  step1->nrecs = nrecords;
  for (int recID = 0; recID < nrecords; ++recID) step1->recIDs[recID] = recID;
}

int
streamScanInitRecords2(stream_t *streamptr)
{
  tsteps_t *step1 = &streamptr->tsteps[0];
  tsteps_t *step2 = &streamptr->tsteps[1];
  int nrecords = step2->nallrecs;
  step2->recIDs = (int *) Malloc((size_t) nrecords * sizeof(int));
  step2->nrecs = 0;

  for (int recID = 0; recID < nrecords; ++recID)
  {
    step2->recIDs[recID] = -1;
    step2->records[recID].position = step1->records[recID].position;
    step2->records[recID].size = step1->records[recID].size;
  }

  return nrecords;
}

int
streamScanInitRecords(stream_t *streamptr, int tsID)
{
  tsteps_t *step2 = &streamptr->tsteps[1];
  int nrecs = step2->nrecs;

  tsteps_t *step = &streamptr->tsteps[tsID];
  step->nrecs = nrecs;
  step->recIDs = (int *) Malloc((size_t) nrecs * sizeof(int));

  for (int recID = 0; recID < nrecs; ++recID) { step->recIDs[recID] = step2->recIDs[recID]; }

  return nrecs;
}

void
streamScanTimeConstAdjust(stream_t *streamptr, const taxis_t *taxis)
{
  int vlistID = streamptr->vlistID;
  if (streamptr->ntsteps == 1 && cdiDateTime_isNull(taxis->vDateTime))
  {
    streamptr->ntsteps = 0;
    for (int varID = 0; varID < streamptr->nvars; ++varID) vlistDefVarTimetype(vlistID, varID, TIME_CONSTANT);
  }
}

void
streamScanTsFixNtsteps(stream_t *streamptr, off_t recpos)
{
  if (streamptr->ntsteps == -1)
  {
    int tsID = tstepsNewEntry(streamptr);
    if (tsID != streamptr->rtsteps) Error("Internal error. tsID = %d", tsID);

    streamptr->tsteps[tsID - 1].next = true;
    streamptr->tsteps[tsID].position = recpos;
  }
}
