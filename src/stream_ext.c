#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dmemory.h"

#include "error.h"
#include "file.h"
#include "cdi.h"
#include "cdi_int.h"
#include "stream_scan.h"
#include "stream_ext.h"
#include "varscan.h"
#include "extra.h"
#include "get_num_missvals.h"
#include "exse.h"

#ifdef HAVE_LIBEXTRA

static int
extInqDatatype(int prec, int number)
{
  if (number == 2)
    return (prec == EXSE_PREC_FP64) ? CDI_DATATYPE_CPX64 : CDI_DATATYPE_CPX32;
  else
    return (prec == EXSE_PREC_FP64) ? CDI_DATATYPE_FLT64 : CDI_DATATYPE_FLT32;
}

static void
extDefDatatype(int datatype, int *prec, int *number)
{
  if (datatype != CDI_DATATYPE_FLT32 && datatype != CDI_DATATYPE_FLT64 && datatype != CDI_DATATYPE_CPX32
      && datatype != CDI_DATATYPE_CPX64)
    datatype = CDI_DATATYPE_FLT32;

  *number = (datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64) ? 2 : 1;
  *prec = (datatype == CDI_DATATYPE_FLT64 || datatype == CDI_DATATYPE_CPX64) ? EXSE_PREC_FP64 : EXSE_PREC_FP32;
}

static void
ext_read_recordSP(stream_t *streamP, float *data, size_t *numMissVals)
{
  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  int tsID = streamP->curTsID;

  int vrecID = streamP->tsteps[tsID].curRecID;
  int recID = streamP->tsteps[tsID].recIDs[vrecID];
  int varID = streamP->tsteps[tsID].recinfo[recID].varID;
  off_t recpos = streamP->tsteps[tsID].records[recID].position;

  fileSetPos(fileID, recpos, SEEK_SET);

  void *extp = streamP->record->objectp;
  if (extRead(fileID, extp) < 0) Error("Failed to read EXTRA record");

  int header[4];
  extInqHeader(extp, header);
  extInqDataFP32(extp, data);

  double missval = vlistInqVarMissval(vlistID, varID);
  size_t size = (size_t) gridInqSize(vlistInqVarGrid(vlistID, varID));

  if (vlistInqVarNumber(vlistID, varID) == CDI_REAL)
    *numMissVals = get_num_missvalsSP(size, data, (float) missval);
  else
    *numMissVals = get_cplx_num_missvalsSP(size, data, (float) missval);

  streamP->numvals += (SizeType) size;
}

static void
ext_read_recordDP(stream_t *streamP, double *data, size_t *numMissVals)
{
  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  int tsID = streamP->curTsID;

  int vrecID = streamP->tsteps[tsID].curRecID;
  int recID = streamP->tsteps[tsID].recIDs[vrecID];
  int varID = streamP->tsteps[tsID].recinfo[recID].varID;
  off_t recpos = streamP->tsteps[tsID].records[recID].position;

  fileSetPos(fileID, recpos, SEEK_SET);

  void *extp = streamP->record->objectp;
  if (extRead(fileID, extp) < 0) Error("Failed to read EXTRA record");

  int header[4];
  extInqHeader(extp, header);
  extInqDataFP64(extp, data);

  double missval = vlistInqVarMissval(vlistID, varID);
  size_t size = (size_t) gridInqSize(vlistInqVarGrid(vlistID, varID));

  if (vlistInqVarNumber(vlistID, varID) == CDI_REAL)
    *numMissVals = get_num_missvalsDP(size, data, missval);
  else
    *numMissVals = get_cplx_num_missvalsDP(size, data, missval);

  streamP->numvals += (SizeType) size;
}

void
ext_read_field(stream_t *streamP, int memtype, void *data, size_t *numMissVals)
{
  if (memtype == MEMTYPE_FLOAT)
    ext_read_recordSP(streamP, (float *) data, numMissVals);
  else
    ext_read_recordDP(streamP, (double *) data, numMissVals);
}

void
extCopyField(stream_t *streamP2, stream_t *streamP1)
{ streamFCopyRecord(streamP2, streamP1, "EXTRA"); }

void
extDefField(stream_t *streamP)
{
  Record *record = streamP->record;

  int pdis, pcat, pnum;
  cdiDecodeParam(record->param, &pnum, &pcat, &pdis);

  int header[4];
  header[0] = record->vdate;
  header[1] = pnum;
  header[2] = record->ilevel;
  int gridID = record->gridID;
  SizeType gridSize = gridInqSize(gridID);
  cdi_check_gridsize_int_limit("EXTRA", gridSize);
  header[3] = (int) gridSize;

  extrec_t *extp = (extrec_t *) record->objectp;
  extDefDatatype(record->prec, &extp->prec, &extp->number);
  extDefHeader(extp, header);
}

static void
ext_write_recordSP(stream_t *streamP, const float *data)
{
  void *extp = streamP->record->objectp;
  extDefDataFP32(extp, data);
  extWrite(streamP->fileID, extp);
}

static void
ext_write_recordDP(stream_t *streamP, const double *data)
{
  void *extp = streamP->record->objectp;
  extDefDataFP64(extp, data);
  extWrite(streamP->fileID, extp);
}

void
ext_write_field(stream_t *streamP, int memtype, const void *data)
{
  if (memtype == MEMTYPE_FLOAT)
    ext_write_recordSP(streamP, (const float *) data);
  else
    ext_write_recordDP(streamP, (const double *) data);
}

static void
extAddRecord(stream_t *streamP, int param, int level, size_t xysize, size_t recsize, off_t position, int prec, int number)
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
  cdiGridTypeInit(grid, GRID_GENERIC, xysize);
  grid->x.size = xysize;
  grid->y.size = 0;
  struct addIfNewRes gridAdded = cdiVlistAddGridIfNew(vlistID, grid, 0);
  int gridID = gridAdded.Id;
  if (!gridAdded.isNew)
  {
    grid_free(grid);
    Free(grid);
  }

  int leveltype = ZAXIS_GENERIC;
  int datatype = extInqDatatype(prec, number);

  int varID, levelID = 0;
  varAddRecord(recID, param, gridID, leveltype, 0, level, 0, 0, 0, datatype, &varID, &levelID, TSTEP_INSTANT, 0, -1, NULL, NULL,
               NULL, NULL);

  recinfo->varID = (short) varID;
  recinfo->levelID = levelID;

  streamP->tsteps[tsID].nallrecs++;
  streamP->nrecs++;

  if (CDI_Debug) Message("varID = %d gridID = %d levelID = %d", varID, gridID, levelID);
}

static void
extScanTimestep1(stream_t *streamP)
{
  CdiDateTime datetime0;
  cdiDateTime_init(&datetime0);
  int header[4];
  off_t recpos = 0;
  extrec_t *extp = (extrec_t *) streamP->record->objectp;

  streamP->curTsID = 0;

  int tsID = tstepsNewEntry(streamP);
  if (tsID != 0) Error("Internal problem! tstepsNewEntry returns %d", tsID);
  taxis_t *taxis = &streamP->tsteps[tsID].taxis;

  int fileID = streamP->fileID;

  int nrecs = 0;
  while (true)
  {
    recpos = fileGetPos(fileID);
    if (extRead(fileID, extp) != 0)
    {
      streamP->ntsteps = 1;
      break;
    }

    size_t recsize = (size_t) (fileGetPos(fileID) - recpos);

    extInqHeader(extp, header);

    int vdate = header[0];
    int vtime = 0;
    int rcode = header[1];
    int rlevel = header[2];
    int rxysize = header[3];
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

    extAddRecord(streamP, param, rlevel, (size_t) rxysize, recsize, recpos, extp->prec, extp->number);
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
extScanTimestep2(stream_t *streamP)
{
  int header[4];
  off_t recpos = 0;
  void *extp = streamP->record->objectp;

  streamP->curTsID = 1;

  int fileID = streamP->fileID;
  int vlistID = streamP->vlistID;

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
    if (extRead(fileID, extp) != 0)
    {
      streamP->ntsteps = 2;
      break;
    }

    size_t recsize = (size_t) (fileGetPos(fileID) - recpos);

    extInqHeader(extp, header);

    int vdate = header[0];
    int vtime = 0;
    int rcode = header[1];
    int rlevel = header[2];
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
extInqContents(stream_t *streamP)
{
  streamP->curTsID = 0;

  extScanTimestep1(streamP);

  int status = (streamP->ntsteps == -1) ? extScanTimestep2(streamP) : 0;

  fileSetPos(streamP->fileID, 0, SEEK_SET);

  return status;
}

static long
extScanTimestep(stream_t *streamP)
{
  int header[4];
  off_t recpos = 0;
  int nrecs = 0;
  void *extp = streamP->record->objectp;

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
      if (extRead(fileID, extp) != 0)
      {
        streamP->ntsteps = streamP->rtsteps + 1;
        break;
      }

      size_t recsize = (size_t) (fileGetPos(fileID) - recpos);

      extInqHeader(extp, header);

      int vdate = header[0];
      int vtime = 0;
      int rcode = header[1];
      int rlevel = header[2];
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
extInqTimestep(stream_t *streamP, int tsID)
{
  if (tsID == 0 && streamP->rtsteps == 0) Error("Call to cdiInqContents missing!");

  if (CDI_Debug) Message("tsID = %d rtsteps = %d", tsID, streamP->rtsteps);

  long ntsteps = CDI_UNDEFID;
  while ((tsID + 1) > streamP->rtsteps && ntsteps == CDI_UNDEFID) ntsteps = extScanTimestep(streamP);

  int nrecs = 0;
  if (!(tsID >= streamP->ntsteps && streamP->ntsteps != CDI_UNDEFID))
  {
    streamP->curTsID = tsID;
    nrecs = streamP->tsteps[tsID].nrecs;
  }

  return nrecs;
}

void
extReadVarSliceDP(stream_t *streamP, int varID, int levID, double *data, size_t *numMissVals)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d  levID = %d", streamP->self, varID, levID);

  void *extp = streamP->record->objectp;

  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;

  double missval = vlistInqVarMissval(vlistID, varID);
  size_t gridsize = (size_t) gridInqSize(vlistInqVarGrid(vlistID, varID));
  int tsid = streamP->curTsID;

  off_t currentfilepos = fileGetPos(fileID);

  int recID = streamP->vars[varID].recordTable[0].recordID[levID];
  off_t recpos = streamP->tsteps[tsid].records[recID].position;
  fileSetPos(fileID, recpos, SEEK_SET);
  extRead(fileID, extp);
  int header[4];
  extInqHeader(extp, header);
  extInqDataFP64(extp, data);

  fileSetPos(fileID, currentfilepos, SEEK_SET);

  if (vlistInqVarNumber(vlistID, varID) == CDI_REAL)
    *numMissVals = get_num_missvalsDP(gridsize, data, missval);
  else
    *numMissVals = get_cplx_num_missvalsDP(gridsize, data, missval);
}

void
extReadVarDP(stream_t *streamP, int varID, double *data, size_t *numMissVals)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamP->self, varID);

  int vlistID = streamP->vlistID;
  size_t gridsize = (size_t) gridInqSize(vlistInqVarGrid(vlistID, varID));
  size_t nlevs = (size_t) streamP->vars[varID].recordTable[0].nlevs;

  for (size_t levID = 0; levID < nlevs; levID++)
    extReadVarSliceDP(streamP, varID, (int) levID, &data[levID * gridsize], numMissVals);
}

void
extWriteVarSliceDP(stream_t *streamP, int varID, int levID, const double *data)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d  levID = %d", streamP->self, varID, levID);

  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  int tsID = streamP->curTsID;
  CdiDateTime vDateTime = streamP->tsteps[tsID].taxis.vDateTime;

  int pdis, pcat, pnum;
  cdiDecodeParam(vlistInqVarParam(vlistID, varID), &pnum, &pcat, &pdis);

  int header[4];
  header[0] = (int) cdiDate_get(vDateTime.date);
  header[1] = pnum;
  header[2] = (int) lround(zaxisInqLevel(vlistInqVarZaxis(vlistID, varID), levID));
  int gridID = vlistInqVarGrid(vlistID, varID);
  SizeType gridSize = gridInqSize(gridID);
  cdi_check_gridsize_int_limit("EXTRA", gridSize);
  header[3] = (int) gridSize;

  extrec_t *extp = (extrec_t *) streamP->record->objectp;
  extDefDatatype(vlistInqVarDatatype(vlistID, varID), &extp->prec, &extp->number);
  extDefHeader(extp, header);

  extDefDataFP64(extp, data);
  extWrite(fileID, extp);
}

void
extWriteVarDP(stream_t *streamP, int varID, const double *data)
{
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamP->self, varID);

  int vlistID = streamP->vlistID;
  size_t gridsize = (size_t) gridInqSize(vlistInqVarGrid(vlistID, varID));
  size_t nlevs = (size_t) zaxisInqSize(vlistInqVarZaxis(vlistID, varID));

  for (size_t levID = 0; levID < nlevs; levID++) extWriteVarSliceDP(streamP, varID, (int) levID, &data[levID * gridsize]);
}

#endif /* HAVE_LIBEXTRA */

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
