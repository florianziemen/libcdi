#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBGRIB

#ifdef HAVE_LIBFDB5
#include "cdi_fdb.h"
#endif

#include "async_worker.h"
#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "stream_cgribex.h"
#include "stream_grb.h"
#include "stream_gribapi.h"
#include "file.h"
#include "cgribex.h" /* gribZip gribGetZip gribGinfo */

static int
grb_decode(int filetype, int memtype, void *cgribexp, void *gribbuffer, size_t gribsize, void *data, size_t datasize, int unreduced,
           size_t *nmiss, double missval)
{
  int status = 0;

#ifdef HAVE_LIBCGRIBEX
  if (filetype == CDI_FILETYPE_GRB && !CDI_gribapi_grib1)
    {
#ifdef HAVE_LIBGRIB_API
      extern int cdiNAdditionalGRIBKeys;
      if (cdiNAdditionalGRIBKeys > 0) Error("CGRIBEX decode does not support reading of additional GRIB keys!");
#endif
      status = cgribexDecode(memtype, cgribexp, gribbuffer, gribsize, data, datasize, unreduced, nmiss, missval);
    }
  else
#endif
#ifdef HAVE_LIBGRIB_API
    {
      void *datap = (memtype == MEMTYPE_FLOAT) ? Malloc(datasize * sizeof(double)) : data;

      status = gribapiDecode(gribbuffer, gribsize, datap, datasize, unreduced, nmiss, missval);

      if (memtype == MEMTYPE_FLOAT)
        {
          float *dataf = (float *) data;
          double *datad = (double *) datap;
          for (size_t i = 0; i < datasize; ++i) dataf[i] = (float) datad[i];
          Free(datap);
        }
    }
#else
  {
    Error("ecCodes support not compiled in!");
  }
#endif

  return status;
}

// Decompresses the grib data in gribbuffer.
static int
grib1_unzip_record(void *gribbuffer, size_t *gribsize)
{
  int zip = 0;

  const size_t igribsize = *gribsize;
  size_t ogribsize = *gribsize;

  int izip;
  size_t unzipsize;
  if ((izip = gribGetZip(igribsize, (unsigned char *) gribbuffer, &unzipsize)) > 0)
    {
      zip = izip;
      if (izip == 128)  // szip
        {
          if (unzipsize < igribsize)
            {
              fprintf(stderr, "Decompressed size smaller than compressed size (in %zu; out %zu)!\n", igribsize, unzipsize);
              return 0;
            }

          unzipsize += 100;  // need 0 to 1 bytes for rounding of bds

          void *buffer = Malloc(igribsize);
          memcpy(buffer, gribbuffer, igribsize);

          ogribsize
              = (size_t) gribUnzip((unsigned char *) gribbuffer, (long) unzipsize, (unsigned char *) buffer, (long) igribsize);

          Free(buffer);

          if (ogribsize <= 0) Error("Decompression problem!");
        }
      else
        {
          Error("Decompression for %d not implemented!", izip);
        }
    }

  *gribsize = ogribsize;

  return zip;
}

typedef struct DecodeArgs
{
  int recID, tsID, *outZip, filetype, memtype, unreduced;
  void *cgribexp, *gribbuffer, *data;
  size_t recsize, gridsize, nmiss;
  double missval;
} DecodeArgs;

static int
grb_decode_record(void *untypedArgs)
{
  DecodeArgs *args = (DecodeArgs *) untypedArgs;
  *args->outZip = grib1_unzip_record(args->gribbuffer, &args->recsize);
  grb_decode(args->filetype, args->memtype, args->cgribexp, args->gribbuffer, args->recsize, args->data, args->gridsize,
             args->unreduced, &args->nmiss, args->missval);
  return 0;
}

static DecodeArgs
grb_read_raw_data(stream_t *streamptr, int tsID, int recID, int memtype, void *gribbuffer, void *data, bool resetFilePos)
{
  const int vlistID = streamptr->vlistID;
  const int varID = streamptr->tsteps[tsID].records[recID].varID;
  size_t recsize = streamptr->tsteps[tsID].records[recID].size;

  const int gridID = vlistInqVarGrid(vlistID, varID);
  const size_t gridsize = gridInqSize(gridID);
  if (CDI_Debug) Message("gridID = %d gridsize = %zu", gridID, gridsize);

  void *cgribexp = (gribbuffer && streamptr->record->objectp) ? streamptr->record->objectp : NULL;
  if (!gribbuffer) gribbuffer = Malloc(streamptr->record->buffersize);
  if (!data) data = Malloc(gridsize * ((memtype == MEMTYPE_FLOAT) ? sizeof(float) : sizeof(double)));

  if (streamptr->protocol == CDI_PROTOCOL_FDB)
    {
#ifdef HAVE_LIBFDB5
      void *fdbItem = streamptr->tsteps[tsID].records[recID].fdbItem;
      if (!fdbItem) Error("fdbItem not available!");

      recsize = fdb_read_record(streamptr->protocolData, fdbItem, &(streamptr->record->buffersize), &gribbuffer);
#endif
    }
  else
    {
      if (recsize == 0) Error("Internal problem! Recordsize is zero for record %d at timestep %d", recID + 1, tsID + 1);

      const int fileID = streamptr->fileID;
      const off_t recpos = streamptr->tsteps[tsID].records[recID].position;
      const off_t currentfilepos = (resetFilePos ? fileGetPos(fileID) : 0);

      fileSetPos(fileID, recpos, SEEK_SET);
      if (fileRead(fileID, gribbuffer, recsize) != recsize) Error("Failed to read GRIB record!");

      if (resetFilePos) fileSetPos(fileID, currentfilepos, SEEK_SET);
      if (!resetFilePos) streamptr->numvals += gridsize;
    }

  return (DecodeArgs){
    .recID = recID,
    .tsID = tsID,
    .outZip = &streamptr->tsteps[tsID].records[recID].zip,
    .filetype = streamptr->filetype,
    .memtype = memtype,
    .cgribexp = cgribexp,
    .gribbuffer = gribbuffer,
    .recsize = recsize,
    .data = data,
    .gridsize = gridsize,
    .unreduced = streamptr->unreduced,
    .nmiss = 0,
    .missval = vlistInqVarMissval(vlistID, varID),
  };
}

static size_t
grb_read_and_decode_record(stream_t *streamptr, int recID, int memtype, void *data, bool resetFilePos)
{
  DecodeArgs args = grb_read_raw_data(streamptr, streamptr->curTsID, recID, memtype, streamptr->record->buffer, data, resetFilePos);
  grb_decode_record(&args);
  return args.nmiss;
}

typedef struct JobDescriptor
{
  DecodeArgs args;
  AsyncJob *job;
} JobDescriptor;

static void
JobDescriptor_startJob(AsyncManager *jobManager, JobDescriptor *me, stream_t *streamptr, int tsID, int recID, int memtype)
{
  me->args = grb_read_raw_data(streamptr, tsID, recID, memtype, NULL, NULL, false);
  me->job = AsyncWorker_requestWork(jobManager, grb_decode_record, &me->args);
  if (!me->job) xabort("error while trying to send job to worker thread");
}

static void
JobDescriptor_finishJob(AsyncManager *jobManager, JobDescriptor *me, void *data, size_t *nmiss)
{
  if (AsyncWorker_wait(jobManager, me->job)) xabort("error executing job in worker thread");
  memcpy(data, me->args.data, me->args.gridsize * ((me->args.memtype == MEMTYPE_FLOAT) ? sizeof(float) : sizeof(double)));
  *nmiss = me->args.nmiss;

  Free(me->args.gribbuffer);
  Free(me->args.data);
  me->args.recID = -1;  // mark as inactive
  me->args.tsID = -1;   // mark as inactive
}
/*
static long
get_global_recId(stream_t *streamptr, int tsID, int recID)
{
  const tsteps_t *tsteps = streamptr->tsteps;
  long globalRecId = recID;
  if (tsID > 0) globalRecId += tsteps[0].nrecs;
  if (tsID > 1) globalRecId += tsteps[1].nrecs * (tsID - 1);
  return globalRecId;
}
*/
static long
get_max_global_recs(stream_t *streamptr)
{
  long maxGlobalRecs = -1;
  const long numSteps = streamptr->ntsteps;
  if (numSteps > 0)
    {
      const tsteps_t *tsteps = streamptr->tsteps;
      maxGlobalRecs = tsteps[0].nrecs;
      if (numSteps > 1) maxGlobalRecs += tsteps[1].nrecs * (numSteps - 1);
    }
  return maxGlobalRecs;
}

static void
get_local_step_and_recId(stream_t *streamptr, long globalRecId, int *tsID, int *recID)
{
  int localTsId = 0;
  const long numSteps = streamptr->ntsteps;
  const tsteps_t *tsteps = streamptr->tsteps;
  if (numSteps > 0 && globalRecId >= tsteps[0].nrecs)
    {
      localTsId++;
      globalRecId -= tsteps[0].nrecs;
    }
  while (globalRecId >= tsteps[1].nrecs)
    {
      localTsId++;
      globalRecId -= tsteps[1].nrecs;
    }

  *tsID = localTsId;
  *recID = globalRecId;
}

static void
read_next_record(AsyncManager *jobManager, JobDescriptor *jd, stream_t *streamptr, int memtype)
{
  int tsId = -1, recId = -1;
  get_local_step_and_recId(streamptr, streamptr->nextGlobalRecId, &tsId, &recId);
  const int xRecId = streamptr->tsteps[tsId].recIDs[recId];
  JobDescriptor_startJob(jobManager, jd, streamptr, tsId, xRecId, memtype);
  streamptr->nextGlobalRecId++;
}

static void
grb_read_next_record(stream_t *streamptr, int recID, int memtype, void *data, size_t *nmiss)
{
  bool jobFound = false;

  int workerCount = streamptr->numWorker;
  if (workerCount > 0)
    {
      const int tsID = streamptr->curTsID;
      const long maxGlobalRecs = get_max_global_recs(streamptr);
      // const long globalRecId = get_global_recId(streamptr, streamptr->curTsID, recID);
      if (maxGlobalRecs == -1) xabort("Internal error: number of timesteps missing!");

      if (workerCount > maxGlobalRecs) workerCount = maxGlobalRecs;

      AsyncManager *jobManager = (AsyncManager *) streamptr->jobManager;
      JobDescriptor *jobs = (JobDescriptor *) streamptr->jobs;

      // if this is the first call, init and start worker threads

      if (!jobs)
        {
          jobs = (JobDescriptor *) malloc(workerCount * sizeof(*jobs));
          streamptr->jobs = jobs;
          for (int i = 0; i < workerCount; i++) jobs[i].args.recID = -1;
          for (int i = 0; i < workerCount; i++) jobs[i].args.tsID = -1;
          if (AsyncWorker_init(&jobManager, workerCount)) xabort("error while trying to start worker threads");
          streamptr->jobManager = jobManager;

          // Start as many new jobs as possible.
          for (int i = 0; streamptr->nextGlobalRecId < maxGlobalRecs && i < workerCount; i++)
            {
              JobDescriptor *jd = &jobs[i];
              if (jd->args.recID < 0 && jd->args.tsID < 0) read_next_record(jobManager, jd, streamptr, memtype);
            }
        }

      // search for a job descriptor with the given tsID and recID, and use its results if it exists
      for (int i = 0; !jobFound && i < workerCount; i++)
        {
          JobDescriptor *jd = &jobs[i];
          if (jd->args.recID == recID && jd->args.tsID == tsID)
            {
              jobFound = true;
              JobDescriptor_finishJob(jobManager, jd, data, nmiss);
              if (streamptr->nextGlobalRecId < maxGlobalRecs) read_next_record(jobManager, jd, streamptr, memtype);
            }
        }
    }

  // perform the work synchronously if we didn't start a job for it yet
  if (!jobFound) *nmiss = grb_read_and_decode_record(streamptr, recID, memtype, data, false);
}

void
grb_read_record(stream_t *streamptr, int memtype, void *data, size_t *nmiss)
{
  const int tsID = streamptr->curTsID;
  const int vrecID = streamptr->tsteps[tsID].curRecID;
  const int recID = streamptr->tsteps[tsID].recIDs[vrecID];

  grb_read_next_record(streamptr, recID, memtype, data, nmiss);
}

void
grb_read_var_slice(stream_t *streamptr, int varID, int levelID, int memtype, void *data, size_t *nmiss)
{
  const int isub = subtypeInqActiveIndex(streamptr->vars[varID].subtypeID);
  const int recID = streamptr->vars[varID].recordTable[isub].recordID[levelID];

  *nmiss = grb_read_and_decode_record(streamptr, recID, memtype, data, true);
}

void
grb_read_var(stream_t *streamptr, int varID, int memtype, void *data, size_t *nmiss)
{
  const int vlistID = streamptr->vlistID;
  const int fileID = streamptr->fileID;

  const int gridID = vlistInqVarGrid(vlistID, varID);
  const size_t gridsize = gridInqSize(gridID);

  const off_t currentfilepos = fileGetPos(fileID);

  const int isub = subtypeInqActiveIndex(streamptr->vars[varID].subtypeID);
  const int nlevs = streamptr->vars[varID].recordTable[0].nlevs;

  if (CDI_Debug) Message("nlevs = %d gridID = %d gridsize = %zu", nlevs, gridID, gridsize);

  *nmiss = 0;
  for (int levelID = 0; levelID < nlevs; levelID++)
    {
      const int recID = streamptr->vars[varID].recordTable[isub].recordID[levelID];
      const size_t offset = levelID * gridsize;
      void *datap = (memtype == MEMTYPE_FLOAT) ? (void *) ((float *) data + offset) : (void *) ((double *) data + offset);

      *nmiss += grb_read_and_decode_record(streamptr, recID, memtype, datap, false);
    }

  fileSetPos(fileID, currentfilepos, SEEK_SET);
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
