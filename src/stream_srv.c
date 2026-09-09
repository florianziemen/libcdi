#include "grid.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dmemory.h"

#include "error.h"
#include "file.h"
#include "cdi.h"
#include "cdi_int.h"
#include "varscan.h"
#include "service.h"
#include "stream_scan.h"
#include "stream_srv.h"
#include "get_num_missvals.h"
#include "exse.h"

#ifdef HAVE_LIBSERVICE

static int
srvInqDatatype(int prec)
{
  return (prec == EXSE_PREC_FP64) ? CDI_DATATYPE_FLT64 : CDI_DATATYPE_FLT32;
}

static int
srvDefDatatype(int datatype)
{
  if (datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64)
    Error("CDI/SERVICE library does not support complex numbers!");

  if (datatype != CDI_DATATYPE_FLT32 && datatype != CDI_DATATYPE_FLT64) datatype = CDI_DATATYPE_FLT32;

  return (datatype == CDI_DATATYPE_FLT64) ? EXSE_PREC_FP64 : EXSE_PREC_FP32;
}

static void
srv_read_recordSP(stream_t *streamP, float *data, size_t *numMissVals)
{
  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  int tsID = streamP->curTsID;

  int vrecID = streamP->tsteps[tsID].curRecID;
  int recID = streamP->tsteps[tsID].recIDs[vrecID];
  int varID = streamP->tsteps[tsID].recinfo[recID].varID;
  off_t recpos = streamP->tsteps[tsID].records[recID].position;

  fileSetPos(fileID, recpos, SEEK_SET);

  void *srvp = streamP->record->objectp;
  if (srvRead(fileID, srvp) < 0) Error("Failed to read record from SRV file");

  int header[8];
  srvInqHeader(srvp, header);
  srvInqDataFP32(srvp, data);

  double missval = vlistInqVarMissval(vlistID, varID);
  SizeType size = gridInqSize(vlistInqVarGrid(vlistID, varID));

  *numMissVals = get_num_missvalsSP((size_t) size, data, (float) missval);

  streamP->numvals += size;
}

static void
srv_read_recordDP(stream_t *streamP, double *data, size_t *numMissVals)
{
  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  int tsID = streamP->curTsID;

  int vrecID = streamP->tsteps[tsID].curRecID;
  int recID = streamP->tsteps[tsID].recIDs[vrecID];
  int varID = streamP->tsteps[tsID].recinfo[recID].varID;
  off_t recpos = streamP->tsteps[tsID].records[recID].position;

  fileSetPos(fileID, recpos, SEEK_SET);

  void *srvp = streamP->record->objectp;
  if (srvRead(fileID, srvp) < 0) Error("Failed to read record from SRV file");

  int header[8];
  srvInqHeader(srvp, header);
  srvInqDataFP64(srvp, data);

  double missval = vlistInqVarMissval(vlistID, varID);
  SizeType size = gridInqSize(vlistInqVarGrid(vlistID, varID));

  *numMissVals = get_num_missvalsDP((size_t) size, data, missval);

  streamP->numvals += size;
}

void
srv_read_field(stream_t *streamP, int memtype, void *data, size_t *numMissVals)
{
  if (memtype == MEMTYPE_DOUBLE)
    srv_read_recordDP(streamP, (double *) data, numMissVals);
  else
    srv_read_recordSP(streamP, (float *) data, numMissVals);
}

void
srvCopyField(stream_t *streamP2, stream_t *streamP1)
{
  streamFCopyRecord(streamP2, streamP1, "SRV");
}

void
srvDefField(stream_t *streamP)
{
  Record *record = streamP->record;

  int pdis, pcat, pnum;
  cdiDecodeParam(record->param, &pnum, &pcat, &pdis);

  int header[8];
  header[0] = pnum;
  header[1] = record->ilevel;
  header[2] = record->vdate;
  header[3] = record->vtime;

  int gridID = record->gridID;
  SizeType gridSize = gridInqSize(gridID);
  SizeType xsize = gridInqXsize(gridID), ysize = gridInqYsize(gridID);
  if (xsize == 0 || ysize == 0)
  {
    xsize = gridSize;
    ysize = 1;
  }
  if (gridInqType(gridID) == GRID_UNSTRUCTURED) ysize = 1;
  if (gridSize != xsize * ysize) Error("Internal problem with gridsize!");

  cdi_check_gridsize_int_limit("SERVICE", gridSize);

  header[4] = (int) xsize;
  header[5] = (int) ysize;
  header[6] = 0;
  header[7] = 0;

  srvrec_t *srvp = (srvrec_t *) record->objectp;
  srvp->dprec = srvDefDatatype(record->prec);

  srvDefHeader(srvp, header);
}

static void
srv_write_recordSP(stream_t *streamP, const float *data)
{
  void *srvp = streamP->record->objectp;
  srvDefDataFP32(srvp, data);
  srvWrite(streamP->fileID, srvp);
}

static void
srv_write_recordDP(stream_t *streamP, const double *data)
{
  void *srvp = streamP->record->objectp;
  srvDefDataFP64(srvp, data);
  srvWrite(streamP->fileID, srvp);
}

void
srv_write_field(stream_t *streamP, int memtype, const void *data)
{
  if (memtype == MEMTYPE_DOUBLE)
    srv_write_recordDP(streamP, (const double *) data);
  else
    srv_write_recordSP(streamP, (const float *) data);
}

static void
srv_add_record(stream_t *streamP, int param, int level, size_t xsize, size_t ysize, size_t recsize, off_t position, int prec)
{
  int vlistID = streamP->vlistID;
  int tsID = streamP->curTsID;
  int recID = recordNewEntry(streamP, tsID);
  recinfo_t *recinfo = &(streamP->tsteps[tsID].recinfo[recID]);
  record_t *record = &(streamP->tsteps[tsID].records[recID]);

  record->size = recsize;
  record->position = position;
  record->param = param;
  record->ilevel = level;

  grid_t *grid = (grid_t *) Malloc(sizeof(*grid));
  grid_init(grid);
  cdiGridTypeInit(grid, GRID_GENERIC, xsize * ysize);
  grid->x.size = xsize;
  grid->y.size = ysize;
  struct addIfNewRes gridAdded = cdiVlistAddGridIfNew(vlistID, grid, 0);
  int gridID = gridAdded.Id;
  if (!gridAdded.isNew)
  {
    grid_free(grid);
    Free(grid);
  }

  int leveltype = ZAXIS_GENERIC;
  int datatype = srvInqDatatype(prec);

  int varID, levelID = 0;
  varAddRecord(recID, param, gridID, leveltype, 0, level, 0, 0, 0, datatype, &varID, &levelID, TSTEP_INSTANT, 0, -1, NULL, NULL,
               NULL, NULL);

  xassert(varID <= SHRT_MAX && levelID <= SHRT_MAX);
  recinfo->varID = (short) varID;
  recinfo->levelID = levelID;

  streamP->tsteps[tsID].nallrecs++;
  streamP->nrecs++;

  if (CDI_Debug) Message("varID = %d gridID = %d levelID = %d", varID, gridID, levelID);
}

static void
srvScanTimestep1(stream_t *streamP)
{
  CdiDateTime datetime0;
  cdiDateTime_init(&datetime0);
  off_t recpos;
  srvrec_t *srvp = (srvrec_t *) streamP->record->objectp;

  streamP->curTsID = 0;

  int tsID = tstepsNewEntry(streamP);
  if (tsID != 0) Error("Internal problem! tstepsNewEntry returns %d", tsID);
  taxis_t *taxis = &streamP->tsteps[tsID].taxis;

  int fileID = streamP->fileID;

  int nrecs = 0;
  while (true)
  {
    recpos = fileGetPos(fileID);
    if (srvRead(fileID, srvp) != 0)
    {
      streamP->ntsteps = 1;
      break;
    }

    size_t recsize = (size_t) (fileGetPos(fileID) - recpos);

    int header[8];
    srvInqHeader(srvp, header);

    int prec = srvp->dprec;
    int rcode = header[0];
    int rlevel = header[1];
    int vdate = header[2];
    int vtime = header[3];
    int rxsize = header[4];
    int rysize = header[5];
    int param = cdiEncodeParam(rcode, 255, 255);
    CdiDateTime datetime = cdiDateTime_set(vdate, vtime);

    if (nrecs == 0)
    {
      datetime0 = datetime;
      taxis->vDateTime = datetime;
    }
    else
    {
      record_t *records = streamP->tsteps[tsID].records;
      for (int recID = 0; recID < nrecs; recID++)
        if (param == records[recID].param && rlevel == records[recID].ilevel) goto tstepScanLoopFinished;

      if (cdiDateTime_isNE(datetime, datetime0)) Warning("Inconsistent verification time for code %d level %d", rcode, rlevel);
    }

    nrecs++;

    if (CDI_Debug) Message("%4d%8d%4d%8d%8d%6d", nrecs, (int) recpos, rcode, rlevel, vdate, vtime);

    srv_add_record(streamP, param, rlevel, (size_t) rxsize, (size_t) rysize, recsize, recpos, prec);
  }

tstepScanLoopFinished:
  streamP->rtsteps = 1;

  cdi_generate_vars(streamP);

  int taxisID = taxisCreate(TAXIS_ABSOLUTE);
  taxis->type = TAXIS_ABSOLUTE;
  taxis->rDateTime = taxis->vDateTime;

  int vlistID = streamP->vlistID;
  vlistDefTaxis(vlistID, taxisID);

  vlist_check_contents(vlistID);

  streamScanResizeRecords1(streamP);

  streamScanTsFixNtsteps(streamP, recpos);
  streamScanTimeConstAdjust(streamP, taxis);
}

static int
srvScanTimestep2(stream_t *streamP)
{
  int header[8];
  off_t recpos = 0;
  void *srvp = streamP->record->objectp;

  streamP->curTsID = 1;

  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;

  int tsID = streamP->rtsteps;
  if (tsID != 1) Error("Internal problem! unexpected timestep %d", tsID + 1);

  taxis_t *taxis = &streamP->tsteps[tsID].taxis;

  fileSetPos(fileID, streamP->tsteps[tsID].position, SEEK_SET);

  cdi_create_records(streamP, tsID, true);
  recinfo_t *recinfo = streamP->tsteps[tsID].recinfo;
  record_t *records = streamP->tsteps[tsID].records;

  int nrecords = streamScanInitRecords2(streamP);

  for (int rindex = 0; rindex <= nrecords; rindex++)
  {
    recpos = fileGetPos(fileID);
    if (srvRead(fileID, srvp) != 0)
    {
      streamP->ntsteps = 2;
      break;
    }

    size_t recsize = (size_t) (fileGetPos(fileID) - recpos);

    srvInqHeader(srvp, header);

    int rcode = header[0];
    int rlevel = header[1];
    int vdate = header[2];
    int vtime = header[3];
    int param = cdiEncodeParam(rcode, 255, 255);

    if (rindex == 0)
    {
      taxis->type = TAXIS_ABSOLUTE;
      taxis->vDateTime = cdiDateTime_set(vdate, vtime);
    }

    bool nextstep = false;
    int recID;
    for (recID = 0; recID < nrecords; recID++)
    {
      if (param == records[recID].param && rlevel == records[recID].ilevel)
      {
        if (recinfo[recID].used) { nextstep = true; }
        else
        {
          recinfo[recID].used = true;
          streamP->tsteps[tsID].recIDs[rindex] = recID;
        }
        break;
      }
    }
    if (recID == nrecords)
    {
      Warning("Code %d level %d not found at timestep %d", rcode, rlevel, tsID + 1);
      return CDI_EUFSTRUCT;
    }

    if (nextstep) break;

    if (CDI_Debug) Message("%4d%8d%4d%8d%8d%6d", rindex + 1, (int) recpos, rcode, rlevel, vdate, vtime);

    if (param != records[recID].param || rlevel != records[recID].ilevel)
    {
      Message("tsID = %d recID = %d param = %3d new %3d  level = %3d new %3d", tsID, recID, records[recID].param, param,
              records[recID].ilevel, rlevel);
      return CDI_EUFSTRUCT;
    }

    records[recID].position = recpos;
    records[recID].size = recsize;
  }

  int nrecs = 0;
  for (int recID = 0; recID < nrecords; recID++)
  {
    if (recinfo[recID].used)
      nrecs++;
    else
      vlistDefVarTimetype(vlistID, recinfo[recID].varID, TIME_CONSTANT);
  }
  streamP->tsteps[tsID].nrecs = nrecs;

  streamP->rtsteps = 2;

  streamScanTsFixNtsteps(streamP, recpos);

  return 0;
}

int
srvInqContents(stream_t *streamP)
{
  streamP->curTsID = 0;

  srvScanTimestep1(streamP);

  int status = (streamP->ntsteps == -1) ? srvScanTimestep2(streamP) : 0;

  fileSetPos(streamP->fileID, 0, SEEK_SET);

  return status;
}

static long
srvScanTimestep(stream_t *streamP)
{
  int header[8];
  off_t recpos = 0;
  int nrecs = 0;
  void *srvp = streamP->record->objectp;

  int tsID = streamP->rtsteps;
  taxis_t *taxis = &streamP->tsteps[tsID].taxis;

  if (streamP->tsteps[tsID].recordSize == 0)
  {
    cdi_create_records(streamP, tsID, true);
    record_t *records = streamP->tsteps[tsID].records;

    nrecs = streamScanInitRecords(streamP, tsID);

    int fileID = streamP->fileID;

    fileSetPos(fileID, streamP->tsteps[tsID].position, SEEK_SET);

    for (int rindex = 0; rindex <= nrecs; rindex++)
    {
      recpos = fileGetPos(fileID);
      if (srvRead(fileID, srvp) != 0)
      {
        streamP->ntsteps = streamP->rtsteps + 1;
        break;
      }

      size_t recsize = (size_t) (fileGetPos(fileID) - recpos);

      srvInqHeader(srvp, header);

      int rcode = header[0];
      int rlevel = header[1];
      int vdate = header[2];
      int vtime = header[3];
      int param = cdiEncodeParam(rcode, 255, 255);

      // if ( rindex == nrecs ) break; gcc-4.5 internal compiler error
      if (rindex == nrecs) continue;
      int recID = streamP->tsteps[tsID].recIDs[rindex];

      if (rindex == 0)
      {
        taxis->type = TAXIS_ABSOLUTE;
        taxis->vDateTime = cdiDateTime_set(vdate, vtime);
      }

      if (param != records[recID].param || rlevel != records[recID].ilevel)
      {
        Message("tsID = %d recID = %d param = %3d new %3d  level = %3d new %3d", tsID, recID, records[recID].param, param,
                records[recID].ilevel, rlevel);
        Error("Invalid, unsupported or inconsistent record structure!");
      }

      records[recID].position = recpos;
      records[recID].size = recsize;

      if (CDI_Debug) Message("%4d%8d%4d%8d%8d%6d", rindex, (int) recpos, rcode, rlevel, vdate, vtime);
    }

    streamP->rtsteps++;

    if (streamP->ntsteps != streamP->rtsteps)
    {
      tsID = tstepsNewEntry(streamP);
      if (tsID != streamP->rtsteps) Error("Internal error. tsID = %d", tsID);

      streamP->tsteps[tsID - 1].next = true;
      streamP->tsteps[tsID].position = recpos;
    }

    fileSetPos(fileID, streamP->tsteps[tsID].position, SEEK_SET);
    streamP->tsteps[tsID].position = recpos;
  }

  if (nrecs > 0 && nrecs < streamP->tsteps[tsID].nrecs)
  {
    Warning("Incomplete timestep. Stop scanning at timestep %d.", tsID);
    streamP->ntsteps = tsID;
  }

  return streamP->ntsteps;
}

int
srvInqTimestep(stream_t *streamP, int tsID)
{
  if (tsID == 0 && streamP->rtsteps == 0) Error("Call to cdiInqContents missing!");

  if (CDI_Debug) Message("tsID = %d rtsteps = %d", tsID, streamP->rtsteps);

  long ntsteps = CDI_UNDEFID;
  while ((tsID + 1) > streamP->rtsteps && ntsteps == CDI_UNDEFID) ntsteps = srvScanTimestep(streamP);

  int nrecs = 0;
  if (!(tsID >= streamP->ntsteps && streamP->ntsteps != CDI_UNDEFID))
  {
    streamP->curTsID = tsID;
    nrecs = streamP->tsteps[tsID].nrecs;
  }

  return nrecs;
}

void
srvReadVarSliceDP(stream_t *streamP, int varID, int levID, double *data, size_t *numMissVals)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d  levID = %d", streamP->self, varID, levID);

  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  int tsid = streamP->curTsID;

  off_t currentfilepos = fileGetPos(fileID);

  int recID = streamP->vars[varID].recordTable[0].recordID[levID];
  off_t recpos = streamP->tsteps[tsid].records[recID].position;

  fileSetPos(fileID, recpos, SEEK_SET);

  void *srvp = streamP->record->objectp;
  if (srvRead(fileID, srvp) < 0) abort();

  int header[8];
  srvInqHeader(srvp, header);
  srvInqDataFP64(srvp, data);

  fileSetPos(fileID, currentfilepos, SEEK_SET);

  double missval = vlistInqVarMissval(vlistID, varID);
  SizeType size = gridInqSize(vlistInqVarGrid(vlistID, varID));

  *numMissVals = get_num_missvalsDP((size_t) size, data, missval);
}

void
srvReadVarDP(stream_t *streamP, int varID, double *data, size_t *numMissVals)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamP->self, varID);

  int vlistID = streamP->vlistID;
  size_t gridsize = (size_t) gridInqSize(vlistInqVarGrid(vlistID, varID));
  size_t nlevs = (size_t) streamP->vars[varID].recordTable[0].nlevs;

  for (size_t levID = 0; levID < nlevs; levID++)
    srvReadVarSliceDP(streamP, varID, (int) levID, &data[levID * gridsize], numMissVals);
}

void
srvWriteVarSliceDP(stream_t *streamP, int varID, int levID, const double *data)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d  levID = %d", streamP->self, varID, levID);

  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  int tsID = streamP->curTsID;
  CdiDateTime vDateTime = streamP->tsteps[tsID].taxis.vDateTime;
  int gridID = vlistInqVarGrid(vlistID, varID);

  int pdis, pcat, pnum;
  cdiDecodeParam(vlistInqVarParam(vlistID, varID), &pnum, &pcat, &pdis);

  int header[8];
  header[0] = pnum;
  header[1] = (int) lround(zaxisInqLevel(vlistInqVarZaxis(vlistID, varID), levID));
  header[2] = (int) cdiDate_get(vDateTime.date);
  header[3] = cdiTime_get(vDateTime.time);

  SizeType gridSize = gridInqSize(gridID);
  SizeType xsize = gridInqXsize(gridID), ysize = gridInqYsize(gridID);
  if (xsize == 0 || ysize == 0)
  {
    xsize = gridSize;
    ysize = 1;
  }
  if (gridInqType(gridID) == GRID_UNSTRUCTURED) ysize = 1;
  if (gridSize != xsize * ysize) Error("Internal problem with gridsize!");

  cdi_check_gridsize_int_limit("SERVICE", gridSize);

  header[4] = (int) xsize;
  header[5] = (int) ysize;
  header[6] = 0;
  header[7] = 0;

  int datatype = vlistInqVarDatatype(vlistID, varID);

  srvrec_t *srvp = (srvrec_t *) streamP->record->objectp;
  srvp->dprec = srvDefDatatype(datatype);

  srvDefHeader(srvp, header);
  srvDefDataFP64(srvp, data);
  srvWrite(fileID, srvp);
}

void
srvWriteVarDP(stream_t *streamP, int varID, const double *data)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamP->self, varID);

  int vlistID = streamP->vlistID;
  SizeType gridsize = gridInqSize(vlistInqVarGrid(vlistID, varID));
  int nlevs = zaxisInqSize(vlistInqVarZaxis(vlistID, varID));

  for (int levID = 0; levID < nlevs; levID++) srvWriteVarSliceDP(streamP, varID, levID, data + (size_t) levID * (size_t) gridsize);
}

#endif /* HAVE_LIBSERVICE */

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
