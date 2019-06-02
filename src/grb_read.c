#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBGRIB

#include "async_worker.h"
#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "stream_cgribex.h"
#include "stream_grb.h"
#include "stream_gribapi.h"
#include "file.h"
#include "cgribex.h"  /* gribZip gribGetZip gribGinfo */


static
int grbDecode(int filetype, int memtype, void *cgribexp, void *gribbuffer, size_t gribsize, void *data, size_t datasize,
	      int unreduced, size_t *nmiss, double missval)
{
  int status = 0;

#ifdef HAVE_LIBCGRIBEX
  if ( filetype == CDI_FILETYPE_GRB && !CDI_gribapi_grib1 )
    {
#ifdef HAVE_LIBGRIB_API
      extern int cdiNAdditionalGRIBKeys;
      if ( cdiNAdditionalGRIBKeys > 0 )
	Error("CGRIBEX decode does not support reading of additional GRIB keys!");
#endif
      status = cgribexDecode(memtype, cgribexp, gribbuffer, gribsize, data, datasize, unreduced, nmiss, missval);
    }
  else
#endif
#ifdef HAVE_LIBGRIB_API
    {
      void *datap = (memtype == MEMTYPE_FLOAT) ? Malloc(datasize*sizeof(double)) : data;

      status = gribapiDecode(gribbuffer, gribsize, datap, datasize, unreduced, nmiss, missval);

      if ( memtype == MEMTYPE_FLOAT )
        {
          float *dataf = (float*) data;
          double *datad = (double*) datap;
          for ( size_t i = 0; i < datasize; ++i ) dataf[i] = (float) datad[i];
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
static
int grbUnzipRecord(void *gribbuffer, size_t *gribsize)
{
  int zip = 0;

  const size_t igribsize = *gribsize;
  size_t ogribsize = *gribsize;

  int izip;
  size_t unzipsize;
  if ( (izip = gribGetZip(igribsize, (unsigned char *)gribbuffer, &unzipsize)) > 0 )
    {
      zip = izip;
      if ( izip == 128 ) /* szip */
	{
	  if ( unzipsize < igribsize )
	    {
	      fprintf(stderr, "Decompressed size smaller than compressed size (in %zu; out %zu)!\n", igribsize, unzipsize);
	      return 0;
	    }

	  unzipsize += 100; /* need 0 to 1 bytes for rounding of bds */

	  void *buffer = Malloc(igribsize);
	  memcpy(buffer, gribbuffer, igribsize);

	  ogribsize = (size_t)gribUnzip(gribbuffer, (long)unzipsize, buffer, (long)igribsize);

          Free(buffer);

	  if ( ogribsize <= 0 ) Error("Decompression problem!");
	}
      else
	{
	  Error("Decompression for %d not implemented!", izip);
	}
    }

  *gribsize = ogribsize;

  return zip;
}


typedef struct DecodeArgs {
  int recID, *outZip, filetype, memtype, unreduced;
  void *cgribexp, *gribbuffer, *data;
  size_t recsize, gridsize, nmiss;
  double missval;
} DecodeArgs;


static
int grb_decode_record(void *untypedArgs)
{
  DecodeArgs *args = untypedArgs;
  *args->outZip = grbUnzipRecord(args->gribbuffer, &args->recsize);
  grbDecode(args->filetype, args->memtype, args->cgribexp, args->gribbuffer, args->recsize, args->data, args->gridsize,
            args->unreduced, &args->nmiss, args->missval);
  return 0;
}

static
DecodeArgs grb_read_raw_data(stream_t *streamptr, int recID, int memtype, void *gribbuffer, void *data, bool resetFilePos)
{
  const int vlistID = streamptr->vlistID;
  const int fileID = streamptr->fileID;
  const int tsID = streamptr->curTsID;	// FIXME: This should be looked up from the given recID
  const off_t recpos = streamptr->tsteps[tsID].records[recID].position;
  const size_t recsize = streamptr->tsteps[tsID].records[recID].size;
  const int varID = streamptr->tsteps[tsID].records[recID].varID;

  const int gridID = vlistInqVarGrid(vlistID, varID);
  const size_t gridsize = gridInqSize(gridID);
  if ( CDI_Debug ) Message("gridID = %d gridsize = %zu", gridID, gridsize);

  if ( recsize == 0 ) Error("Internal problem! Recordsize is zero for record %d at timestep %d", recID+1, tsID+1);

  if (!gribbuffer) gribbuffer = Malloc(streamptr->record->buffersize);
  if (!data) data = Malloc(gridsize*(memtype == MEMTYPE_FLOAT ? sizeof(float) : sizeof(double)));

  if (resetFilePos)
    {
      const off_t currentfilepos = fileGetPos(fileID);
      fileSetPos(fileID, recpos, SEEK_SET);
      if (fileRead(fileID, gribbuffer, recsize) != recsize) Error("Failed to read GRIB record");
      fileSetPos(fileID, currentfilepos, SEEK_SET);
    }
  else
    {
      fileSetPos(fileID, recpos, SEEK_SET);
      if (fileRead(fileID, gribbuffer, recsize) != recsize) Error("Failed to read GRIB record");
      streamptr->numvals += gridsize;
    }

  return (DecodeArgs){
    .recID = recID,
    .outZip = &streamptr->tsteps[tsID].records[recID].zip,
    .filetype = streamptr->filetype,
    .memtype = memtype,
    .cgribexp = streamptr->record->cgribexp,
    .gribbuffer = gribbuffer,
    .recsize = recsize,
    .data = data,
    .gridsize = gridsize,
    .unreduced = streamptr->unreduced,
    .nmiss = 0,
    .missval = vlistInqVarMissval(vlistID, varID),
  };
}

typedef struct JobDescriptor {
  DecodeArgs args;
  AsyncJob *job;
} JobDescriptor;

static
void JobDescriptor_startJob(JobDescriptor *me, stream_t *streamptr, int recID, int memtype, bool resetFilePos)
{
  me->args = grb_read_raw_data(streamptr, recID, memtype, NULL, NULL, resetFilePos);
  me->job = AsyncWorker_requestWork(grb_decode_record, &me->args);
  if (!me->job) xabort("error while trying to send job to worker thread");
}

static
void JobDescriptor_finishJob(JobDescriptor *me, void *data, size_t *nmiss)
{
  if (AsyncWorker_wait(me->job)) xabort("error executing job in worker thread");
  memcpy(data, me->args.data, me->args.gridsize*(me->args.memtype == MEMTYPE_FLOAT ? sizeof(float) : sizeof(double)));
  *nmiss = me->args.nmiss;

  Free(me->args.gribbuffer);
  Free(me->args.data);
  me->args.recID = -1;	// mark as inactive
}

static
void grb_read_next_record(stream_t *streamptr, int recID, int memtype, void *data, size_t *nmiss, bool resetFilePos)
{
  bool jobFound = false;

  const int workerCount = streamptr->numWorker;
  if (workerCount > 0)
    {
      JobDescriptor *jobs = (JobDescriptor *) streamptr->jobs;

      // if this is the first call, init and start worker threads
      tsteps_t *timestep = &streamptr->tsteps[streamptr->curTsID];

      if (!jobs)
        {
          jobs = malloc(workerCount*sizeof*jobs);
          streamptr->jobs = jobs;
          for (int i = 0; i < workerCount; i++) jobs[i].args.recID = -1;
          if (AsyncWorker_init(workerCount)) xabort("error while trying to start worker threads");
        }

      if (recID == 0) streamptr->nextRecID = 0;
      if (recID == 0) streamptr->cachedTsID = streamptr->curTsID; // no active workers -> we may start processing records of a new timestep

      if (streamptr->cachedTsID == streamptr->curTsID)
        {
          // Start as many new jobs as possible.
          for (int i = 0; streamptr->nextRecID < timestep->nrecs && i < workerCount; i++)
            {
              JobDescriptor *jd = &jobs[i];
              if (jd->args.recID < 0)
                {
                  JobDescriptor_startJob(jd, streamptr, timestep->recIDs[streamptr->nextRecID++], memtype, resetFilePos);
                }
            }

          // search for a job descriptor with the given recID, and use its results if it exists
          for (int i = 0; !jobFound && i < workerCount; i++)
            {
              JobDescriptor *jd = &jobs[i];
              if (jd->args.recID == recID)
                {
                  jobFound = true;
                  JobDescriptor_finishJob(jd, data, nmiss);
                  if (streamptr->nextRecID < timestep->nrecs)
                    {
                      JobDescriptor_startJob(jd, streamptr, timestep->recIDs[streamptr->nextRecID++], memtype, resetFilePos);
                    }
                }
            }
        }
    }

  // perform the work synchronously if we didn't start a job for it yet
  if (!jobFound)
    {
      DecodeArgs args = grb_read_raw_data(streamptr, recID, memtype, streamptr->record->buffer, data, resetFilePos);
      grb_decode_record(&args);
      *nmiss = args.nmiss;
    }
}


void grb_read_record(stream_t *streamptr, int memtype, void *data, size_t *nmiss)
{
  const int tsID = streamptr->curTsID;
  const int vrecID = streamptr->tsteps[tsID].curRecID;
  const int recID = streamptr->tsteps[tsID].recIDs[vrecID];

  grb_read_next_record(streamptr, recID, memtype, data, nmiss, false);
}


void grb_read_var(stream_t *streamptr, int varID, int memtype, void *data, size_t *nmiss)
{
  const int filetype = streamptr->filetype;

  void *gribbuffer = streamptr->record->buffer;
  void *cgribexp = streamptr->record->cgribexp;

  const int vlistID = streamptr->vlistID;
  const int fileID = streamptr->fileID;
  const int tsID = streamptr->curTsID;

  const int gridID = vlistInqVarGrid(vlistID, varID);
  const size_t gridsize = gridInqSize(gridID);

  const off_t currentfilepos = fileGetPos(fileID);

  const int isub = subtypeInqActiveIndex(streamptr->vars[varID].subtypeID);
  const int nlevs = streamptr->vars[varID].recordTable[0].nlevs;

  if ( CDI_Debug ) Message("nlevs = %d gridID = %d gridsize = %zu", nlevs, gridID, gridsize);

  *nmiss = 0;
  for (int levelID = 0; levelID < nlevs; levelID++ )
    {
      const int recID = streamptr->vars[varID].recordTable[isub].recordID[levelID];
      const off_t recpos = streamptr->tsteps[tsID].records[recID].position;
      size_t recsize = streamptr->tsteps[tsID].records[recID].size;

      fileSetPos(fileID, recpos, SEEK_SET);
      fileRead(fileID, gribbuffer, recsize);

      streamptr->tsteps[tsID].records[recID].zip = grbUnzipRecord(gribbuffer, &recsize);

      void *datap = NULL;
      if ( memtype == MEMTYPE_FLOAT )
        datap = (float*)data + levelID*gridsize;
      else
        datap = (double*)data + levelID*gridsize;

      const double missval = vlistInqVarMissval(vlistID, varID);
      size_t imiss;
      grbDecode(filetype, memtype, cgribexp, gribbuffer, recsize, datap, gridsize, streamptr->unreduced, &imiss, missval);
      *nmiss += imiss;
    }

  fileSetPos(fileID, currentfilepos, SEEK_SET);
}


void grb_read_var_slice(stream_t *streamptr, int varID, int levelID, int memtype, void *data, size_t *nmiss)
{
  const int isub = subtypeInqActiveIndex(streamptr->vars[varID].subtypeID);
  const int recID = streamptr->vars[varID].recordTable[isub].recordID[levelID];

  grb_read_next_record(streamptr, recID, memtype, data, nmiss, true);
}

#endif
