/** @file ioServer.c
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "pio_server.h"


#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_PARALLEL_NC4
#include <core/ppm_combinatorics.h>
#include <core/ppm_rectilinear.h>
#include <ppm/ppm_uniform_partition.h>
#endif
#include <yaxt.h>

#include "cdi.h"
#include "cdipio.h"
#include "cdi_int.h"
#include "dmemory.h"
#include "namespace.h"
#include "taxis.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_id_set.h"
#include "pio_interface.h"
#include "pio_rpc.h"
#include "pio_util.h"
#ifndef HAVE_NETCDF_PAR_H
#define MPI_INCLUDED
#endif
#include "pio_cdf_int.h"
#include "resource_handle.h"
#include "resource_unpack.h"
#include "stream_cdf.h"
#include "vlist_var.h"


struct clientBuf
{
  size_t size;
  unsigned char *mem;
  int dictSize;
};

static struct
{
  MPI_Win getWin;
  struct clientBuf *clientBuf;
} *rxWin;

static struct idList openStreams;

#ifdef HAVE_PARALLEL_NC4
/* prime factorization of number of pio collectors */
static uint32_t *pioPrimes;
static int numPioPrimes;
#endif

/************************************************************************/

static
void serverWinCleanup ()
{
  for (size_t streamIdx = 0; streamIdx < openStreams.size; ++streamIdx)
    {
      Free(rxWin[streamIdx].clientBuf[0].mem);
      Free(rxWin[streamIdx].clientBuf);
      xmpi(MPI_Win_free(&rxWin[streamIdx].getWin));
    }
  Free(rxWin);

  xdebug("%s", "cleaned up mpi_win");
}


struct serverBufferSetup
{
  int numClientsOfServer;
  struct collSpec collectorData;
  struct clientBufSize *bufSizes;
  size_t numStreams;
};

static enum cdiApplyRet
serverStreamBufSizeIter(int id, void *res, void *data)
{
  (void)res;
  struct serverBufferSetup *setup = data;
  size_t numStreams = setup->numStreams;
  size_t numClientsOfServer = (size_t)setup->numClientsOfServer;
  struct clientBufSize (*bufSizes)[numClientsOfServer]
    = (struct clientBufSize (*)[numClientsOfServer])setup->bufSizes;
  setup->collectorData.sendRPCData = 1;
  for (size_t i = 0; i < numClientsOfServer; ++i)
    {
      bufSizes[numStreams][i]
        = computeClientStreamBufSize(id, &setup->collectorData);
      setup->collectorData.sendRPCData = 0;
    }
  setup->numStreams = ++numStreams;
  size_t insertPos = insertID(&openStreams, id);
  xassert(insertPos == numStreams - 1);
  return CDI_APPLY_GO_ON;
}

static size_t
serverBufSizes(struct serverBufferSetup *setup)
{
  int numClients = cdiPioCommInqSizeClients(),
    numColl = commInqSizeColl(),
    collRank = commInqRankColl(),
    numClientsOfServer
    = cdiPioClientRangeStart(collRank + 1, numClients, numColl)
    - cdiPioClientRangeStart(collRank, numClients, numColl);
  setup->numClientsOfServer = numClientsOfServer;
  setup->collectorData = (struct collSpec){
    .numClients = numClients,
    .numServers = numColl,
  };
  reshLock();
  unsigned streamCount = reshCountType(&streamOps);
  setup->numStreams = 0;
  setup->bufSizes = Malloc((size_t)numClientsOfServer * streamCount
                            * sizeof (setup->bufSizes[0]));
  cdiResHFilterApply(&streamOps, serverStreamBufSizeIter, setup);
  reshUnlock();
  return 0;
}


static int numClients_, *clientRanks_;

static void
setupClientRanks(void)
{
  MPI_Group clientGroup = cdiPioInqRemoteGroup();
  xmpi(MPI_Group_size(clientGroup, &numClients_));
  clientRanks_ = Malloc((size_t)numClients_ * sizeof (clientRanks_[0]));
  int *ranks = Malloc((size_t)numClients_ * sizeof (ranks[0]));
  for (int i = 0; i < numClients_; ++i)
    ranks[i] = i;
  MPI_Comm collClientIntraComm = cdiPioInqCollClientIntraComm();
  MPI_Group groupCollClient;
  xmpi(MPI_Comm_group(collClientIntraComm, &groupCollClient));
  xmpi(MPI_Group_translate_ranks(clientGroup, numClients_, ranks,
                                 groupCollClient, clientRanks_));
  xmpi(MPI_Group_free(&groupCollClient));
  Free(ranks);
}

static void
serverWinCreate(void)
{
  struct serverBufferSetup setup;
  serverBufSizes(&setup);
  setupClientRanks();
  MPI_Comm collClientIntraComm = cdiPioInqCollClientIntraComm();
  MPI_Info no_locks_info;
  xmpi(MPI_Info_create(&no_locks_info));
  xmpi(MPI_Info_set(no_locks_info, "no_locks", "true"));

  rxWin = Calloc((size_t)setup.numStreams, sizeof (rxWin[0]));

  struct clientBufSize (*bufSizes)[numClients_]
    = (struct clientBufSize (*)[numClients_])setup.bufSizes;
  for (size_t streamIdx = 0; streamIdx < openStreams.size; ++streamIdx)
    {
      rxWin[streamIdx].clientBuf
        = Malloc((size_t)numClients_ * sizeof (rxWin[streamIdx].clientBuf[0]));

      if (openStreams.entries[streamIdx] != CDI_UNDEFID) { } else
        continue;
      xmpi(MPI_Win_create(MPI_BOTTOM, 0, 1, no_locks_info, collClientIntraComm,
                          &rxWin[streamIdx].getWin));
      size_t streamBufferSize = 0;
      for (size_t i = 0; i < (size_t)numClients_; ++i)
        {
          streamBufferSize +=
            (rxWin[streamIdx].clientBuf[i].size
             = bufSizes[streamIdx][i].bufSize);
          rxWin[streamIdx].clientBuf[i].dictSize
            = bufSizes[streamIdx][i].numDataRecords
            + bufSizes[streamIdx][i].numRPCRecords;
        }
      rxWin[streamIdx].clientBuf[0].mem = Malloc(streamBufferSize);
      for (size_t i = 1; i < (size_t)numClients_; ++i)
        rxWin[streamIdx].clientBuf[i].mem = rxWin[streamIdx].clientBuf[i-1].mem
          + bufSizes[streamIdx][i-1].bufSize;
    }

  xmpi(MPI_Info_free(&no_locks_info));
  Free(setup.bufSizes);
  xdebug("%s", "created mpi_win, allocated getBuffer");
}

/************************************************************************/

static void
readFuncCall(struct winHeaderEntry *header, size_t streamIdx)
{
  int funcID = header->id;
  union funcArgs *funcArgs = &(header->specific.funcArgs);

  xassert(funcID >= MINFUNCID && funcID <= MAXFUNCID);
  switch ( funcID )
    {
    case STREAMCLOSE:
      {
        int streamID
          = namespaceAdaptKey2(funcArgs->streamChange.streamID);
        streamClose(streamID);
        xdebug("READ FUNCTION CALL FROM WIN:  %s, streamID=%d,"
               " closed stream",
               funcMap[(-1 - funcID)], streamID);
      }
      break;
    case STREAMOPEN:
      {
        size_t filenamesz = (size_t)funcArgs->newFile.fnamelen;
        xassert ( filenamesz > 0 && filenamesz < MAXDATAFILENAME );
        const char *filename
          = (const char *)(rxWin[streamIdx].clientBuf[0].mem + header->offset);
        xassert(filename[filenamesz] == '\0');
        int filetype = funcArgs->newFile.filetype;
        int streamID = streamOpenWrite(filename, filetype);
        xassert(streamID != CDI_ELIBNAVAIL);
        xdebug("READ FUNCTION CALL FROM WIN:  %s, filenamesz=%zu,"
               " filename=%s, filetype=%d, OPENED STREAM %d",
               funcMap[(-1 - funcID)], filenamesz, filename,
               filetype, streamID);
      }
      break;
    case STREAMDEFVLIST:
      {
        int streamID
          = namespaceAdaptKey2(funcArgs->streamChange.streamID);
        int vlistID = namespaceAdaptKey2(funcArgs->streamChange.vlistID);
        streamDefVlist(streamID, vlistID);
        xdebug("READ FUNCTION CALL FROM WIN:  %s, streamID=%d,"
               " vlistID=%d, called streamDefVlist ().",
               funcMap[(-1 - funcID)], streamID, vlistID);
      }
      break;
    case STREAMDEFTIMESTEP:
      {
        MPI_Comm pioInterComm = cdiPioInqInterComm();
        int streamID = funcArgs->streamNewTimestep.streamID;
        int originNamespace = namespaceResHDecode(streamID).nsp;
        streamID = namespaceAdaptKey2(streamID);
        int oldTaxisID
          = vlistInqTaxis(streamInqVlist(streamID));
        int position = header->offset;
        int changedTaxisID
          = taxisUnpack((char *)rxWin[streamIdx].clientBuf[0].mem,
                        (int)rxWin[streamIdx].clientBuf[0].size,
                        &position, originNamespace, &pioInterComm, 0);
        taxis_t *oldTaxisPtr = taxisPtr(oldTaxisID);
        taxis_t *changedTaxisPtr = taxisPtr(changedTaxisID);
        ptaxisCopy(oldTaxisPtr, changedTaxisPtr);
        taxisDestroy(changedTaxisID);
        streamDefTimestep(streamID, funcArgs->streamNewTimestep.tsID);
      }
      break;
    default:
      xabort ( "REMOTE FUNCTIONCALL NOT IMPLEMENTED!" );
    }
}

/************************************************************************/

static void
resizeVarGatherBuf(int size, double **buf, int *bufSize)
{
  if (size <= *bufSize) ; else
    *buf = Realloc(*buf, (size_t)(*bufSize = size) * sizeof (buf[0][0]));
}

static Xt_redist
buildVarRedist(int headerIdx, size_t streamIdx,
               /* index list representing the data elements gathered on
                * this rank */
               Xt_idxlist dstList)
{
  const struct clientBuf *restrict clientBuf = rxWin[streamIdx].clientBuf;
  const struct winHeaderEntry *winDict
    = (struct winHeaderEntry *)clientBuf[0].mem;
  int streamID = openStreams.entries[streamIdx];
  int varID = winDict[headerIdx].specific.dataRecord.varID;
  struct Xt_offset_ext *partExts
    = Malloc((size_t)numClients_ * sizeof (partExts[0]));
  Xt_idxlist *part = Malloc((size_t)numClients_ * sizeof (part[0]));
  MPI_Comm pioInterComm = cdiPioInqInterComm(),
    collComm = commInqCommColl();
  for (size_t clientIdx = 0; clientIdx < (size_t)numClients_; ++clientIdx)
    {
      unsigned char *clientMem = clientBuf[clientIdx].mem;
      struct dataRecord *dataHeader
        = &((struct winHeaderEntry *)clientMem)[headerIdx].specific.dataRecord;
      int position = ((struct winHeaderEntry *)clientMem)[headerIdx + 1].offset;
      xassert(namespaceAdaptKey2(((struct winHeaderEntry *)
                                  clientMem)[headerIdx].id)
              == streamID
              && dataHeader->varID == varID
              && (((struct winHeaderEntry *)clientMem)[headerIdx + 1].id
                  == PARTDESCMARKER)
              && position > 0
              && ((size_t)position
                  >= sizeof (struct winHeaderEntry)
                  * (size_t)clientBuf[clientIdx].dictSize)
              && ((size_t)position < clientBuf[clientIdx].size));
      part[clientIdx]
        = xt_idxlist_unpack(clientMem, (int)clientBuf[clientIdx].size,
                            &position, pioInterComm);
      unsigned partSize
        = (unsigned)xt_idxlist_get_num_indices(part[clientIdx]);
      size_t charOfs = (size_t)((clientMem
                                 + ((struct winHeaderEntry *)
                                    clientMem)[headerIdx].offset)
                                - clientBuf[0].mem);
      xassert(charOfs % sizeof (double) == 0
              && charOfs / sizeof (double) + partSize <= INT_MAX);
      int elemOfs = (int)(charOfs / sizeof (double));
      partExts[clientIdx].start = elemOfs;
      partExts[clientIdx].size = (int)partSize;
      partExts[clientIdx].stride = 1;
    }
  Xt_idxlist srcList = xt_idxlist_collection_new(part, numClients_);
  for (size_t clientIdx = 0; clientIdx < (size_t)numClients_; ++clientIdx)
    xt_idxlist_delete(part[clientIdx]);
  Free(part);
  Xt_xmap gatherXmap = xt_xmap_all2all_new(srcList, dstList, collComm);
  xt_idxlist_delete(srcList);
  struct Xt_offset_ext gatherExt
    = { .start = 0,
        .size = xt_idxlist_get_num_indices(dstList),
        .stride = 1 };
  Xt_redist varRedist
    = xt_redist_p2p_ext_new(gatherXmap, numClients_, partExts, 1, &gatherExt,
                            MPI_DOUBLE);
  xt_xmap_delete(gatherXmap);
  Free(partExts);
  return varRedist;
}


static void
gatherArray(int headerIdx, size_t streamIdx,
            double *gatherBuf,
            /* index list representing the data elements gathered on
             * this rank */
            Xt_idxlist dstList)
{
  Xt_redist gatherRedist = buildVarRedist(headerIdx, streamIdx,
                                          dstList);
  xt_redist_s_exchange1(gatherRedist,
                        rxWin[streamIdx].clientBuf[0].mem, gatherBuf);
  xt_redist_delete(gatherRedist);
}

struct xyzDims
{
  int sizes[3];
};

static inline int
xyzGridSize(struct xyzDims dims)
{
  return dims.sizes[0] * dims.sizes[1] * dims.sizes[2];
}

#ifdef HAVE_PARALLEL_NC4
static void
queryVarBounds(struct PPM_extent varShape[3], int vlistID, int varID)
{
  varShape[0].first = 0;
  varShape[1].first = 0;
  varShape[2].first = 0;
  int sizes[3];
  cdiPioQueryVarDims(sizes, vlistID, varID);
  for (unsigned i = 0; i < 3; ++i)
    varShape[i].size = sizes[i];
}

/* compute distribution of collectors such that number of collectors
 * <= number of variable grid cells in each dimension */
static struct xyzDims
varDimsCollGridMatch(const struct PPM_extent varDims[3])
{
  xassert(PPM_extents_size(3, varDims) >= commInqSizeColl());
  struct xyzDims collGrid = { { 1, 1, 1 } };
  /* because of storage order, dividing dimension 3 first is preferred */
  for (int i = 0; i < numPioPrimes; ++i)
    {
      for (int dim = 2; dim >=0; --dim)
        if (collGrid.sizes[dim] * pioPrimes[i] <= varDims[dim].size)
          {
            collGrid.sizes[dim] *= pioPrimes[i];
            goto nextPrime;
          }
      /* no position found, retrack */
      xabort("Not yet implemented back-tracking needed.");
      nextPrime:
      ;
    }
  return collGrid;
}

static void
myVarPart(struct PPM_extent varShape[3], struct xyzDims collGrid,
          struct PPM_extent myPart[3])
{
  int32_t myCollGridCoord[3];
  {
    struct PPM_extent collGridShape[3];
    for (int i = 0; i < 3; ++i)
      {
        collGridShape[i].first = 0;
        collGridShape[i].size = collGrid.sizes[i];
      }
    PPM_lidx2rlcoord_e(3, collGridShape, commInqRankColl(), myCollGridCoord);
    xdebug("my coord: (%d, %d, %d)", myCollGridCoord[0], myCollGridCoord[1],
           myCollGridCoord[2]);
  }
  PPM_uniform_partition_nd(3, varShape, collGrid.sizes,
                           myCollGridCoord, myPart);
}
#elif defined (HAVE_LIBNETCDF)
/* needed for writing when some files are only written to by a single process */
/* cdiOpenFileMap(fileID) gives the writer process */
static int cdiPioSerialOpenFileMap(int streamID)
{
  return stream_to_pointer(streamID)->ownerRank;
}
/* for load-balancing purposes, count number of files per process */
/* cdiOpenFileCounts[rank] gives number of open files rank has to himself */
static int *cdiSerialOpenFileCount = NULL;

static int
cdiPioNextOpenRank()
{
  xassert(cdiSerialOpenFileCount != NULL);
  int commCollSize = commInqSizeColl();
  int minRank = 0, minOpenCount = cdiSerialOpenFileCount[0];
  for (int i = 1; i < commCollSize; ++i)
    if (cdiSerialOpenFileCount[i] < minOpenCount)
      {
        minOpenCount = cdiSerialOpenFileCount[i];
        minRank = i;
      }
  return minRank;
}

static void
cdiPioOpenFileOnRank(int rank)
{
  xassert(cdiSerialOpenFileCount != NULL
          && (unsigned)rank < (unsigned)commInqSizeColl());
  ++(cdiSerialOpenFileCount[rank]);
}

static void
cdiPioCloseFileOnRank(int rank)
{
  xassert(cdiSerialOpenFileCount != NULL
          && rank >= 0 && rank < commInqSizeColl());
  xassert(cdiSerialOpenFileCount[rank] > 0);
  --(cdiSerialOpenFileCount[rank]);
}

static void
cdiPioServerCdfDefVars(stream_t *streamptr)
{
  int rank, rankOpen;
  if (commInqIOMode() == PIO_NONE
      || ((rank = commInqRankColl())
          == (rankOpen = cdiPioSerialOpenFileMap(streamptr->self))))
    cdfDefVars(streamptr);
}

#endif

struct recordWrite
{
  int varID, level;
};

struct streamMapping {
  int numVars;
  /* data entry varMap[i] contains data for variable i or -1 if no
   * data entry for i has been transferred */
  int *varMap;
  /* numLvls[i] number of levels written for variable i or 0 if
   * variable is not written to this timestep */
  int *numLvlsW;
  /* nMiss[i] = missing values were provided for variable i */
  int *hasMissing;
  int numWrittenRecords;
  struct recordWrite writtenRecords[];
};

/* build inventory of written variables for stream */
static struct streamMapping *
streamMappingNew(size_t streamIdx, struct winHeaderEntry *winDict)
{
  int streamID = openStreams.entries[streamIdx];
  int numDataEntries = winDict[0].specific.headerSize.numDataEntries;
  int vlistID = streamInqVlist(streamID);
  int numVars = vlistNvars(vlistID);
  /* varMap[i] == index of header if variable i is written to,
   * numLvlsW[i] == number of levels of variable i or 0 if not written
   */
  int *restrict varMap = Calloc((size_t)numVars * 4, sizeof (varMap[0])),
    *restrict hasMissing = varMap + numVars,
    *restrict numLvlsW = varMap + 2 * numVars,
    *restrict hasMissing_ = varMap + 3 * numVars;
  for (int headerIdx = 1; headerIdx < numDataEntries; headerIdx += 2)
    {
      xassert(namespaceAdaptKey2(winDict[headerIdx].id) == streamID);
      int varID = winDict[headerIdx].specific.dataRecord.varID;
      /* ensure a variable has not been enqueued twice */
      /* FIXME: this could better be ensured on client */
      xassert(varMap[varID] == 0);
      varMap[varID] = headerIdx;
      hasMissing[varID] += winDict[headerIdx].specific.dataRecord.nmiss;
    }
  /* set numLvlsW[i] to 1 if varMap[i] != 0 on any collector,
   * also sets hasMissing_[i] to global reduction of hasMissing[i] */
  xmpi(MPI_Allreduce(varMap, numLvlsW, 2 * numVars, MPI_INT,
                     MPI_LOR, commInqCommColl()));
  /* now find numbers of levels for each variable written anywhere */
  size_t numWrittenRecords = 0;
  for (int varID = 0; varID < numVars; ++varID)
    if (numLvlsW[varID])
      numWrittenRecords
        += (size_t)(numLvlsW[varID]
                    = zaxisInqSize(vlistInqVarZaxis(vlistID, varID)));
  struct streamMapping *result
    = Malloc(sizeof (*result)
              + numWrittenRecords * sizeof (result->writtenRecords[0])
              + (size_t)numVars * 3 * sizeof (result->varMap[0]));
  result->varMap
    = (void *)((unsigned char *)result + sizeof (*result)
               + numWrittenRecords * sizeof (result->writtenRecords[0]));
  result->numLvlsW = result->varMap + numVars;
  result->hasMissing = result->varMap + 2 * numVars;
  {
    size_t j = (size_t)-1;
    for (int varID = 0; varID < numVars; ++varID)
      {
        size_t numLvl = (size_t)(result->numLvlsW[varID] = numLvlsW[varID]);
        result->varMap[varID] = varMap[varID];
        result->hasMissing[varID] = hasMissing_[varID];
        for (size_t lvl = 0; lvl < numLvl; ++lvl)
          result->writtenRecords[++j]
            = (struct recordWrite){ .varID = varID, .level = (int)lvl };
      }
  }
  result->numVars = numVars;
  result->numWrittenRecords = (int)numWrittenRecords;
  Free(varMap);
  return result;
}

static void
streamMappingDelete(struct streamMapping **mapping)
{
  Free(*mapping);
  *mapping = NULL;
}

static Xt_idxlist
buildVarSlicesIdxList(int vlistID, int varID, int startLvl, int numLvl)
{
  /* doubles as slice shape later */
  int varShape[3] = { 0, 0, 0 };
  cdiPioQueryVarDims(varShape, vlistID, varID);
  /* int varSize = varShape[0] * varShape[1] * varShape[2]; */
  Xt_int varShapeXt[3];
  Xt_int origin[3] = { 0, 0, 0 };
  for (unsigned i = 0; i < 3; ++i)
    varShapeXt[2 - i] = varShape[i];
  if (startLvl >= 0) origin[0] = (Xt_int)startLvl;
  varShape[0] = varShapeXt[0];
  varShape[2] = varShapeXt[2];
  if (numLvl >= 0) varShape[0] = numLvl;
  return xt_idxsection_new(0, 3, varShapeXt, varShape,
                           origin);
}

static int
countVarChunkMissingVals(int vlistID, int varID,
                         struct streamMapping *mapping,
                         int chunkLen, const double *restrict data)
{
  int nmiss = 0;
  if (mapping->hasMissing[varID])
    {
      double missval = vlistInqVarMissval(vlistID, varID);
      for (size_t i = 0; i < (size_t)chunkLen; ++i)
        nmiss += (data[i] == missval);
    }
  return nmiss;
}

static void
writeGribStream(size_t streamIdx,
                struct streamMapping *mapping,
                double **data_, int *currentDataBufSize)
{
  const struct clientBuf *restrict clientBuf = rxWin[streamIdx].clientBuf;
  int streamID = openStreams.entries[streamIdx];
  int vlistID = streamInqVlist(streamID);
  int numRecords = mapping->numWrittenRecords;
  MPI_Comm collComm = commInqCommColl();
  int collSize = commInqSizeColl();
  int collRank = commInqRankColl();
  int numPasses = (numRecords + collSize - 1)/collSize;
  int varIDsOfPass[collSize];
  Xt_redist varRedists[collSize];
  MPI_Aint displ[collSize];
  memset(displ, 0, sizeof (displ));
  struct recordWrite *restrict writtenRecords = mapping->writtenRecords;
  size_t (*cdiPioFileWrite)(int fileID, const void *restrict buffer,
                            size_t len, int tsID) = 0;
  /* will this task make a synchronization-only write call? */
  if (collRank >= numRecords - (numPasses - 1) * collSize)
    cdiPioFileWrite = (size_t (*)(int, const void *restrict, size_t, int))
      namespaceSwitchGet(NSSWITCH_FILE_WRITE).func;
  for (int pass = 0, base = 0; pass < numPasses; ++pass, base += collSize)
    {
      unsigned numVarsInPass = 1;
      varIDsOfPass[0] = writtenRecords[base].varID;
      int passSize = (pass < numPasses - 1) ? collSize : numRecords - (numPasses - 1) * collSize;
      /* establish variables involved in this pass */
      for (int i = 1; i < passSize; ++i)
        if (varIDsOfPass[numVarsInPass - 1]
            != writtenRecords[base + i].varID)
          varIDsOfPass[numVarsInPass++] = writtenRecords[base + i].varID;
      int mySliceSize = 0;
      /* build redists for all variables involved in current write pass */
      for (unsigned varIdx = 0; varIdx < numVarsInPass; ++varIdx)
        {
          int varID = varIDsOfPass[varIdx];
          Xt_idxlist dstList;
          /* is this process writing part of this variable? */
          if (collRank < passSize
              && writtenRecords[base + collRank].varID == varID)
            {
              int myLevel = writtenRecords[base + collRank].level;
              dstList
                = buildVarSlicesIdxList(vlistID, varID, myLevel, 1);
              mySliceSize = xt_idxlist_get_num_indices(dstList);
            }
          else
            dstList = xt_idxempty_new();
          varRedists[varIdx] = buildVarRedist(mapping->varMap[varID],
                                              streamIdx, dstList);
          xt_idxlist_delete(dstList);
        }
      /* merge all redists for current pass */
      Xt_redist compositePassRedist;
      if (numVarsInPass > 1)
        {
          compositePassRedist
            = xt_redist_collection_static_new(varRedists, (int)numVarsInPass,
                                              displ, displ, collComm);
          /* free individual redists */
          for (unsigned varIdx = 0; varIdx < numVarsInPass; ++varIdx)
            xt_redist_delete(varRedists[varIdx]);
        }
      else
        compositePassRedist = varRedists[0];
      /* resize gather buffer if needed */
      resizeVarGatherBuf(mySliceSize, data_, currentDataBufSize);
      /* execute composite redist */
      xt_redist_s_exchange1(compositePassRedist, clientBuf[0].mem, *data_);
      /* delete composite redist */
      xt_redist_delete(compositePassRedist);
      /* write level (if having one)  */
      if (collRank < passSize)
        {
          int varID = writtenRecords[base + collRank].varID;
          int level = writtenRecords[base + collRank].level;
          int nmiss
            = countVarChunkMissingVals(vlistID, varID, mapping,
                                       mySliceSize, *data_);
          streamWriteVarSlice(streamID, varID, level, *data_, nmiss);
        }
      else
        /* write zero bytes to trigger synchronization code in fileWrite */
        cdiPioFileWrite(streamInqFileID(streamID), NULL, 0,
                        streamInqCurTimestepID(streamID));
    }
}

static void readGetBuffers(size_t streamIdx)
{
  int streamID = openStreams.entries[streamIdx];
  xdebug("%s", "START");

  struct winHeaderEntry *winDict
    = (struct winHeaderEntry *)rxWin[streamIdx].clientBuf[0].mem;
  xassert(winDict[0].id == HEADERSIZEMARKER);
  {
    int dictSize = rxWin[streamIdx].clientBuf[0].dictSize,
      firstNonRPCEntry = dictSize - winDict[0].specific.headerSize.numRPCEntries - 1,
      headerIdx,
      numFuncCalls = 0;
    for (headerIdx = dictSize - 1;
         headerIdx > firstNonRPCEntry;
         --headerIdx)
      {
        xassert(winDict[headerIdx].id >= MINFUNCID
                && winDict[headerIdx].id <= MAXFUNCID);
        ++numFuncCalls;
        readFuncCall(winDict + headerIdx, streamIdx);
      }
    xassert(numFuncCalls == winDict[0].specific.headerSize.numRPCEntries);
  }
  /* build list of streams, data was transferred for */
  {
    struct streamMapping *map = streamMappingNew(streamIdx, winDict);
    double *data = NULL;
    int currentDataBufSize = 0;
    int vlistID = streamInqVlist(streamID);
    int filetype = streamInqFiletype(streamID);

    switch (filetype)
      {
      case FILETYPE_GRB:
      case FILETYPE_GRB2:
        writeGribStream(streamIdx, map,
                        &data, &currentDataBufSize);
        break;
#ifdef HAVE_NETCDF4
      case FILETYPE_NC:
      case FILETYPE_NC2:
      case FILETYPE_NC4:
#ifdef HAVE_PARALLEL_NC4
        /* HAVE_PARALLE_NC4 implies having ScalES-PPM and yaxt */
        {
          int nvars = map->numVars;
          int *restrict varMap = map->varMap;
          for (int varID = 0; varID < nvars; ++varID)
            if (map->numLvlsW[varID])
              {
                struct PPM_extent varShape[3];
                queryVarBounds(varShape, vlistID, varID);
                struct xyzDims collGrid = varDimsCollGridMatch(varShape);
                xdebug("writing varID %d with dimensions: "
                       "x=%d, y=%d, z=%d,\n"
                       "found distribution with dimensions:"
                       " x=%d, y=%d, z=%d.", varID,
                       varShape[0].size, varShape[1].size, varShape[2].size,
                       collGrid.sizes[0], collGrid.sizes[1],
                       collGrid.sizes[2]);
                struct PPM_extent varChunk[3];
                myVarPart(varShape, collGrid, varChunk);
                int myChunk[3][2];
                for (int i = 0; i < 3; ++i)
                  {
                    myChunk[i][0] = PPM_extent_start(varChunk[i]);
                    myChunk[i][1] = PPM_extent_end(varChunk[i]);
                  }
                xdebug("Writing chunk { { %d, %d }, { %d, %d },"
                       " { %d, %d } }", myChunk[0][0], myChunk[0][1],
                       myChunk[1][0], myChunk[1][1], myChunk[2][0],
                       myChunk[2][1]);
                Xt_int varSize[3];
                for (int i = 0; i < 3; ++i)
                  varSize[2 - i] = varShape[i].size;
                Xt_idxlist preWriteChunk;
                /* prepare yaxt descriptor for write chunk */
                {
                  Xt_int preWriteChunkStart[3];
                  int preWriteChunkSize[3];
                  for (int i = 0; i < 3; ++i)
                    {
                      preWriteChunkStart[2 - i] = (Xt_int)varChunk[i].first;
                      preWriteChunkSize[2 - i] = (int)varChunk[i].size;
                    }
                  preWriteChunk = xt_idxsection_new(0, 3, varSize,
                                                    preWriteChunkSize,
                                                    preWriteChunkStart);
                }
                resizeVarGatherBuf(xt_idxlist_get_num_indices(preWriteChunk),
                                   &data, &currentDataBufSize);
                /* transpose data into write deco */
                {
                  int headerIdx = varMap[varID];
                  gatherArray(headerIdx, streamIdx, data, preWriteChunk);
                  xt_idxlist_delete(preWriteChunk);
                }
                /* count missing values if appropriate */
                int nmiss
                  = countVarChunkMissingVals(vlistID, varID, map,
                                             PPM_extents_size(3, varChunk),
                                             data);
                /* write chunk */
                streamWriteVarChunk(streamID, varID,
                                    (const int (*)[2])myChunk, data,
                                    nmiss);
              }
        }
#else
        /* determine process which has stream open (writer) and
         * which has data for which variable (var owner)
         * three cases need to be distinguished */
        {
          int nvars = map->numVars;
          int *restrict varMap = map->varMap,
            *restrict numLvlsW = map->numLvlsW;
          int writerRank = cdiPioSerialOpenFileMap(streamID);
          int collRank = commInqRankColl();
          for (int varID = 0; varID < nvars; ++varID)
            if (numLvlsW[varID])
              {
                int varSize;
                Xt_idxlist dstList;
                if (writerRank == collRank)
                  {
                    dstList = buildVarSlicesIdxList(vlistID, varID, -1, -1);
                    varSize = xt_idxlist_get_num_indices(dstList);
                    resizeVarGatherBuf(varSize, &data, &currentDataBufSize);
                  }
                else
                  {
                    varSize = 0;
                    dstList = xt_idxempty_new();
                  }
                int headerIdx = varMap[varID];
                gatherArray(headerIdx, streamIdx, data, dstList);
                if (writerRank == collRank)
                  {
                    int nmiss = countVarChunkMissingVals(vlistID, varID,
                                                         map, varSize, data);
                    streamWriteVar(streamID, varID, data, nmiss);
                  }
                xt_idxlist_delete(dstList);
              }
        }
#endif
        break;
#endif
      default:
        xabort("unhandled filetype in parallel I/O.");
      }
    streamMappingDelete(&map);
    Free(map);
    Free(data);
  }
#ifdef HAVE_NETCDF4
#ifdef HAVE_PARALLEL_NC4
#endif
#endif
  xdebug("%s", "RETURN");
}

/************************************************************************/


static
void clearModelWinBuffer(size_t streamIdx)
{
  xassert(streamIdx < openStreams.size &&
          rxWin != NULL && rxWin[streamIdx].clientBuf[0].mem != NULL);
  size_t bufSizeTotal = (size_t)(rxWin[streamIdx].clientBuf[numClients_ - 1].mem
                                 - rxWin[streamIdx].clientBuf[0].mem)
    + rxWin[streamIdx].clientBuf[numClients_ - 1].size;
  memset(rxWin[streamIdx].clientBuf[0].mem, 0, bufSizeTotal);
}


/************************************************************************/


static
void getTimeStepData(int *streamActivity)
{
  MPI_Group clientGroup = cdiPioInqRemoteGroup();

  xdebug("%s", "START");

  for (size_t streamIdx = 0; streamIdx < openStreams.size; ++streamIdx)
    if (streamActivity[streamIdx])
      {
        clearModelWinBuffer(streamIdx);
        // todo put in correct lbs and ubs
        xmpi(MPI_Win_start(clientGroup, 0, rxWin[streamIdx].getWin));
        /* FIXME: this needs to use MPI_PACKED for portability */
        for (size_t i = 0; i < (size_t)numClients_; ++i)
          xmpi(MPI_Get(rxWin[streamIdx].clientBuf[i].mem,
                       (int)rxWin[streamIdx].clientBuf[i].size, MPI_UNSIGNED_CHAR,
                       clientRanks_[i], 0,
                       (int)rxWin[streamIdx].clientBuf[i].size, MPI_UNSIGNED_CHAR,
                       rxWin[streamIdx].getWin));
        xmpi(MPI_Win_complete(rxWin[streamIdx].getWin));
      }

  for (size_t streamIdx = 0; streamIdx < openStreams.size; ++streamIdx)
    if (streamActivity[streamIdx])
      readGetBuffers(streamIdx);

  xdebug("%s", "RETURN");
}

/************************************************************************/

#if defined (HAVE_LIBNETCDF) && ! defined (HAVE_PARALLEL_NC4)
static int
cdiPioStreamCDFOpenWrap(const char *filename, char filemode,
                        int filetype, stream_t *streamptr,
                        int recordBufIsToBeCreated)
{
  switch (filetype)
    {
    case FILETYPE_NC4:
    case FILETYPE_NC4C:
      {
        /* Only needs initialization to shut up gcc/clang */
        int rank = -1, fileID = -1;
        int ioMode = commInqIOMode();
        if (ioMode == PIO_NONE
            || commInqRankColl() == (rank = cdiPioNextOpenRank()))
          fileID = cdiStreamOpenDefaultDelegate(filename, filemode, filetype,
                                                streamptr,
                                                recordBufIsToBeCreated);
        else
          streamptr->filetype = filetype;
        if (ioMode != PIO_NONE)
          xmpi(MPI_Bcast(&fileID, 1, MPI_INT, rank, commInqCommColl()));
        streamptr->ownerRank = rank;
        cdiPioOpenFileOnRank(rank);
        return fileID;
      }
    default:
      return cdiStreamOpenDefaultDelegate(filename, filemode, filetype,
                                          streamptr, recordBufIsToBeCreated);
    }
}

static void
cdiPioStreamCDFCloseWrap(stream_t *streamptr, int recordBufIsToBeDeleted)
{
  int fileID   = streamptr->fileID;
  int filetype = streamptr->filetype;
  if ( fileID == CDI_UNDEFID )
    Warning("File %s not open!", streamptr->filename);
  else
    switch (filetype)
      {
      case FILETYPE_NC:
      case FILETYPE_NC2:
      case FILETYPE_NC4:
      case FILETYPE_NC4C:
        {
          int rank, rankOpen = cdiPioSerialOpenFileMap(streamptr->self);
          if (commInqIOMode() == PIO_NONE
              || ((rank = commInqRankColl()) == rankOpen))
            cdiStreamCloseDefaultDelegate(streamptr, recordBufIsToBeDeleted);
          cdiPioCloseFileOnRank(rankOpen);
          break;
        }
      default:
        cdiStreamCloseDefaultDelegate(streamptr, recordBufIsToBeDeleted);
      }
}

static void
cdiPioCdfDefTimestep(stream_t *streamptr, int tsID)
{
  int rank, rankOpen, streamID = streamptr->self;
  if (commInqIOMode() == PIO_NONE
      || ((rank = commInqRankColl())
          == (rankOpen = cdiPioSerialOpenFileMap(streamID))))
    cdfDefTimestep(streamptr, tsID);
}

#endif

/**
 * @brief is encapsulated in CDI library and run on I/O PEs.
 */

void cdiPioServer(void (*postCommSetupActions)(void))
{
  MPI_Status status;

  xdebug("%s", "START");

  cdiPioFileWritingInit(postCommSetupActions);
  if (commInqRankPio() >= commInqSizeColl())
    return;
  MPI_Comm pioInterComm = cdiPioInqInterComm();
#ifdef HAVE_PARALLEL_NC4
  cdiPioEnableNetCDFParAccess();
  numPioPrimes = PPM_prime_factorization_32((uint32_t)commInqSizeColl(),
                                            &pioPrimes);
#elif defined (HAVE_LIBNETCDF)
  cdiSerialOpenFileCount = Calloc(sizeof (cdiSerialOpenFileCount[0]),
                                   (size_t)commInqSizeColl());
  namespaceSwitchSet(NSSWITCH_STREAM_OPEN_BACKEND,
                     NSSW_FUNC(cdiPioStreamCDFOpenWrap));
  namespaceSwitchSet(NSSWITCH_STREAM_CLOSE_BACKEND,
                     NSSW_FUNC(cdiPioStreamCDFCloseWrap));
  namespaceSwitchSet(NSSWITCH_CDF_DEF_TIMESTEP,
                     NSSW_FUNC(cdiPioCdfDefTimestep));
  namespaceSwitchSet(NSSWITCH_CDF_STREAM_SETUP,
                     NSSW_FUNC(cdiPioServerCdfDefVars));
#endif
  namespaceSwitchSet(NSSWITCH_FILE_WRITE,
                     NSSW_FUNC(cdiPioFileWrite));

  int *streamActivity = NULL;
  for ( ;; )
    {
      xmpi ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, pioInterComm, &status ));

      int source = status.MPI_SOURCE;
      int tag = status.MPI_TAG;

      switch ( tag )
        {
        case FINALIZE:
          xmpi(MPI_Recv(NULL, 0, MPI_INT, source, tag, pioInterComm, &status));
          xdebug("%s", "RECEIVED MESSAGE WITH TAG \"FINALIZE\"");
          {
            unsigned nStreams = reshCountType(&streamOps);
            if ( nStreams > 0 )
              {
                int *resHs = Malloc(nStreams * sizeof (resHs[0]));
                cdiStreamGetIndexList(nStreams, resHs);
                for (unsigned streamNo = 0; streamNo < nStreams; ++streamNo)
                  streamClose(resHs[streamNo]);
                Free(resHs);
              }
          }
          cdiPioFileWritingFinalize();
          serverWinCleanup();
          idSetDestroy(&openStreams);
          Free(streamActivity);
          Free(clientRanks_);
#ifdef HAVE_PARALLEL_NC4
          Free(pioPrimes);
#elif defined (HAVE_LIBNETCDF)
          Free(cdiSerialOpenFileCount);
#endif
          /* listDestroy(); */
          xdebug("%s", "RETURN");
          return;
	case RESOURCES:
          {
            int size;
            xmpi(MPI_Get_count(&status, MPI_PACKED, &size));
            char *buffer = Malloc((size_t)size);
            xmpi(MPI_Recv(buffer, size, MPI_PACKED, source,
                          tag, pioInterComm, &status));
            xdebug("%s", "RECEIVED MESSAGE WITH TAG \"RESOURCES\"");
            reshUnpackResources(buffer, size, &pioInterComm);
            xdebug("%s", "");
            Free(buffer);
          }
          serverWinCreate ();
          streamActivity = Realloc(streamActivity, openStreams.size
                                   * sizeof (streamActivity[0]));
	  break;
	case WRITETS:
          {
            xmpi(MPI_Recv(streamActivity, (int)openStreams.size,
                          MPI_INT, source, tag, pioInterComm, &status));
            xdebug("RECEIVED MESSAGE WITH TAG \"WRITETS\": source=%d",
                   source);
            getTimeStepData(streamActivity);
          }
	  break;

	default:
	  xabort ( "TAG NOT DEFINED!" );
	}
    }
}

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
