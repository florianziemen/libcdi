#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <mpi.h>
#include <yaxt.h>

#include "cdi.h"
#include "cdipio.h"
#include "cdi_int.h"
#include "dmemory.h"
#include "namespace.h"
#include "pio.h"
#include "pio_client.h"
#include "pio_id_set.h"
#include "pio_impl.h"
#include "pio_serialize.h"
#include "pio_interface.h"
#include "pio_comm.h"
#include "pio_rpc.h"
#include "pio_server.h"
#include "pio_util.h"
#include "resource_handle.h"
#include "vlist.h"
#include "vlist_var.h"


static struct rdmaWin
{
  size_t size;
  unsigned char *buffer, *head;
  MPI_Win win;
  int dictSize, dictDataUsed, dictRPCUsed;
  bool postSet, refuseFuncCall;
  /** data or meta-data updates on this stream are to be communicated */
  bool pendingUpdate;
} *txWin = NULL;

static struct idList openStreams;

const char * const funcMap[numRPCFuncs] = {
  "streamOpen",
  "streamDefVlist",
  "streamClose",
  "streamDefTimestep",
};

float cdiPIOpartInflate_;

/****************************************************/

void memcpyPackFunc(void *dataDesc, void *buf, int size, int *pos,
                    void *context)
{
  struct memCpyDataDesc *p = dataDesc;
  (void)context;
  xassert(size >= *pos && (size_t)(size - *pos) >= p->obj_size);
  memcpy((unsigned char *)buf + *pos, p->obj, p->obj_size);
  *pos += (int)p->obj_size;
}

/****************************************************/

struct clientBufferSetup
{
  struct collSpec collectData;
  struct clientBufSize *bufSizes;
  size_t bufSizesCount;
};

static enum cdiApplyRet
clientStreamBufSizeIter(int id, void *res, void *data)
{
  (void)res;
  struct clientBufferSetup *setup = data;
  size_t bufSizesCount = setup->bufSizesCount;
  setup->bufSizes[bufSizesCount]
    = computeClientStreamBufSize(id, &setup->collectData);
  setup->bufSizesCount = ++bufSizesCount;
  size_t insertPos = insertID(&openStreams, id);
  xassert(insertPos == bufSizesCount - 1);
  return CDI_APPLY_GO_ON;
}


static void
clientBufSizes(struct clientBufferSetup *setup)
{
  int numClients = cdiPioCommInqSizeClients(),
    clientRank = commInqRankModel(),
    numColl = commInqSizeColl(),
    collRank = cdiPioCollRank(clientRank, numClients, numColl);
  setup->collectData = (struct collSpec){
    .numClients = numClients,
    .numServers = numColl,
    .sendRPCData
    = (clientRank == cdiPioClientRangeStart(collRank, numClients, numColl)),
  };
  reshLock();
  unsigned streamCount = reshCountType(&streamOps);
  setup->bufSizesCount = 0;
  setup->bufSizes = Malloc(streamCount * sizeof (setup->bufSizes[0]));
  cdiResHFilterApply(&streamOps, clientStreamBufSizeIter, setup);
  reshUnlock();
}


/************************************************************************/

static void
modelWinDelete(void)
{
  xdebug("%s", "START");
  if (txWin != NULL)
    {
      for (size_t streamIdx = 0; streamIdx < openStreams.size; ++streamIdx)
        {
          if (txWin[streamIdx].postSet)
            xmpi(MPI_Win_wait(txWin[streamIdx].win));
          xmpi(MPI_Win_free(&txWin[streamIdx].win));
          xmpi(MPI_Free_mem(txWin[streamIdx].buffer));
        }
      Free(txWin);
    }

  xdebug("%s", "RETURN. CLEANED UP MPI_WIN'S");
}

/************************************************************************/

static void
modelWinFlushBuffer(size_t streamIdx)
{
  size_t numStreams = openStreams.size;

  xassert(streamIdx < numStreams
          && openStreams.entries[streamIdx] != CDI_UNDEFID
          && txWin != NULL
          && txWin[streamIdx].buffer != NULL);
  memset(txWin[streamIdx].buffer, 0, txWin[streamIdx].size);
  txWin[streamIdx].head = txWin[streamIdx].buffer
    + (size_t)txWin[streamIdx].dictSize * sizeof (struct winHeaderEntry);
  txWin[streamIdx].refuseFuncCall = 0;
  txWin[streamIdx].dictDataUsed = 1;
  txWin[streamIdx].dictRPCUsed = 0;
  txWin[streamIdx].pendingUpdate = false;
  txWin[streamIdx].postSet = false;
  *(struct winHeaderEntry *)txWin[streamIdx].buffer
    = (struct winHeaderEntry){
    .id = HEADERSIZEMARKER,
    .specific.headerSize = { .numDataEntries = 0, .numRPCEntries = 0 },
  };
}


/************************************************************************/

static void
cdiPioClientWinCreate(struct clientBufferSetup *setup)
{
  MPI_Info no_locks_info;
  xmpi(MPI_Info_create(&no_locks_info));
  xmpi(MPI_Info_set(no_locks_info, "no_locks", "true"));
  size_t numStreams = setup->bufSizesCount;
  txWin = Malloc(numStreams * sizeof (txWin[0]));
  struct clientBufSize *restrict bufSpec = setup->bufSizes;
  MPI_Comm collClientIntraComm = cdiPioInqCollClientIntraComm();
  for (size_t streamIdx = 0; streamIdx < numStreams; ++streamIdx)
    {
      int dictSize = bufSpec[streamIdx].numDataRecords + bufSpec[streamIdx].numRPCRecords;
      size_t streamBufSize = bufSpec[streamIdx].bufSize;
      txWin[streamIdx].dictSize = dictSize;
      txWin[streamIdx].size = streamBufSize;
      xmpi(MPI_Alloc_mem((MPI_Aint)streamBufSize, MPI_INFO_NULL,
                         &txWin[streamIdx].buffer));
      txWin[streamIdx].head = txWin[streamIdx].buffer
        + (size_t)dictSize * sizeof (struct winHeaderEntry);
      xmpi(MPI_Win_create(txWin[streamIdx].buffer, (MPI_Aint)streamBufSize, 1,
                          no_locks_info, collClientIntraComm,
                          &txWin[streamIdx].win));
      modelWinFlushBuffer(streamIdx);
    }
  xmpi(MPI_Info_free(&no_locks_info));
}
/************************************************************************/

static void
modelWinEnqueue(size_t streamIdx,
                struct winHeaderEntry header, const void *data,
                valPackFunc packFunc)
{
  struct winHeaderEntry *winDict
    = (struct winHeaderEntry *)(void *)txWin[streamIdx].buffer;
  int targetEntry;
  if (header.id > 0 || header.id == PARTDESCMARKER)
    targetEntry = (txWin[streamIdx].dictDataUsed)++;
  else
    targetEntry = txWin[streamIdx].dictSize - ++(txWin[streamIdx].dictRPCUsed);
  if (header.id > 0)
    {
      int offset = header.offset
        = (int)roundUpToMultiple((size_t)(txWin[streamIdx].head
                                          - txWin[streamIdx].buffer),
                                 sizeof (double));
      MPI_Comm comm = cdiPioInqInterComm();
      packFunc((void *)data, txWin[streamIdx].buffer, (int)txWin[streamIdx].size,
               &offset, &comm);
      txWin[streamIdx].head = txWin[streamIdx].buffer + offset;
    }
  else if (header.id == PARTDESCMARKER)
    {
      Xt_uid uid = header.specific.partDesc.uid;
      int offset = -1;
      /* search if same uid entry has already been enqueued */
      for (int entry = 2; entry < targetEntry; entry += 2)
        {
          xassert(winDict[entry].id == PARTDESCMARKER);
          if (winDict[entry].specific.partDesc.uid == uid)
            {
              offset = winDict[entry].offset;
              break;
            }
        }
      if (offset == -1)
        {
          /* not yet used partition descriptor, serialize at
           * current position */
          int position = header.offset
            = (int)(txWin[streamIdx].head - txWin[streamIdx].buffer);
          MPI_Comm comm = cdiPioInqInterComm();
          packFunc((void *)data, txWin[streamIdx].buffer, (int)txWin[streamIdx].size,
                   &position, &comm);
          txWin[streamIdx].head = txWin[streamIdx].buffer + position;
        }
      else
        /* duplicate entries are copied only once per timestep */
        header.offset = offset;
    }
  else
    {
      int position = header.offset
        = (int)(txWin[streamIdx].head - txWin[streamIdx].buffer);
      MPI_Comm comm = cdiPioInqInterComm();
      packFunc((void *)data, txWin[streamIdx].buffer,
               (int)txWin[streamIdx].size, &position, &comm);
      txWin[streamIdx].head = txWin[streamIdx].buffer + position;
    }
  txWin[streamIdx].pendingUpdate = true;
  winDict[targetEntry] = header;
}

static void
cdiPio_xt_idxlist_pack_wrap(void *data, void *buf, int size, int *pos,
                            void *context)
{
  MPI_Comm comm = *(MPI_Comm *)context;
  size_t pack_size = xt_idxlist_get_pack_size((Xt_idxlist)data, comm);
  xassert(size >= *pos && pack_size <= (size_t)(size - *pos));
  xt_idxlist_pack((Xt_idxlist)data, (unsigned char *)buf,
                  size, pos, comm);
}

static inline void
collWait(size_t streamIdx)
{
  if (txWin[streamIdx].postSet)
    {
      xmpi(MPI_Win_wait(txWin[streamIdx].win));
      modelWinFlushBuffer(streamIdx);
    }
}

static inline void
collProbe(size_t streamIdx)
{
  if (!txWin[streamIdx].postSet)
    return;
  int flag;
  xmpi(MPI_Win_test(txWin[streamIdx].win, &flag));
  if (flag)
    modelWinFlushBuffer(streamIdx);
}

void
cdiPioRDMAProgress(void)
{
  size_t nStreams = openStreams.size;
  for (size_t streamIdx = 0; streamIdx < nStreams; ++streamIdx)
    if (openStreams.entries[streamIdx] != CDI_UNDEFID)
      collProbe(streamIdx);
}


static void
pioBufferPartData_(int streamID, int varID,
                   const void *packData, valPackFunc packDataFunc,
                   int nmiss, Xt_idxlist partDesc)
{
  size_t streamIdx = indexOfID(&openStreams, streamID);
  xassert(streamIdx != SIZE_MAX);
  xassert(varID >= 0 && varID < streamInqNvars(streamID));
  collWait(streamIdx);

  struct winHeaderEntry dataHeader
    = { .id = streamID, .specific.dataRecord = { varID, nmiss }, .offset = -1 };
  modelWinEnqueue(streamIdx, dataHeader, packData, packDataFunc);
  {
    struct winHeaderEntry partHeader
      = { .id = PARTDESCMARKER,
          .specific.partDesc = { .uid = xt_idxlist_get_uid(partDesc) },
          .offset = 0 };
    modelWinEnqueue(streamIdx, partHeader, partDesc, cdiPio_xt_idxlist_pack_wrap);
  }
  txWin[streamIdx].refuseFuncCall = 1;
}

void
pioBufferPartData(int streamID, int varID, const double *data,
                  int nmiss, Xt_idxlist partDesc)
{
  int chunk = xt_idxlist_get_num_indices(partDesc);
  xassert(chunk <= INT_MAX);
  pioBufferPartData_(streamID, varID,
                     &(struct memCpyDataDesc){data, (size_t)chunk * sizeof (data[0])},
                     memcpyPackFunc,
                     nmiss, partDesc);
}

struct scatterGatherDesc
{
  void *data;
  const int *blocklengths, *displacements;
  size_t elemSize;
  unsigned numBlocks;
  unsigned numElems;
};

static void
scatterGatherPackFunc(void *dataDesc, void *buf, int size, int *pos,
                      void *context)
{
  (void)context;
  const struct scatterGatherDesc *p = dataDesc;
  unsigned numBlocks = p->numBlocks;
  const int *bls = p->blocklengths, *disps = p->displacements;
  int pos_ = *pos;
  unsigned char *dstBuf = (unsigned char *)buf + pos_,
    *bufEnd = (unsigned char *)buf + size;
  size_t elemSize = p->elemSize;
  xassert(elemSize <= SSIZE_MAX);
  const unsigned char *data = p->data;
  unsigned copyCount = 0, numElems = p->numElems;
  for (unsigned j = 0; j < numBlocks && copyCount < numElems; ++j)
    {
      int bl = bls[j];
      if (bl > 0)
        {
          if ((unsigned)bl + copyCount > numElems)
            {
              bl = (int)(numElems - copyCount);
              Warning("%s: %s", "streamWriteScatteredVarPart",
                      "blocks longer than number of elements in index list!");
            }
          size_t bsize = (size_t)bl * elemSize;
          xassert(dstBuf + bsize <= bufEnd);
          memcpy(dstBuf, data + (ssize_t)elemSize * (ssize_t)disps[j], bsize);
          dstBuf += bsize;
        }
    }
  *pos = (int)(dstBuf - (unsigned char *)buf);
}


void
cdiPioBufferPartDataGather(int streamID, int varID, const double *data,
                           int numBlocks, const int blocklengths[],
                           const int displacements[],
                           int nmiss, Xt_idxlist partDesc)
{
  xassert(numBlocks >= 0);
  pioBufferPartData_(streamID, varID,
                     &(struct scatterGatherDesc)
                     { .data = (void *)data, .blocklengths = blocklengths,
                       .displacements = displacements,
                       .elemSize = sizeof (data[0]),
                       .numBlocks = (unsigned)numBlocks,
                       .numElems
                         = (unsigned)xt_idxlist_get_num_indices(partDesc) },
                     scatterGatherPackFunc,
                     nmiss, partDesc);
}


/************************************************************************/

void pioBufferFuncCall(int streamID,
                       struct winHeaderEntry header,
                       const void *data, valPackFunc dataPackFunc)
{
  int funcID = header.id;
  int clientRank = commInqRankModel(),
    numClients = cdiPioCommInqSizeClients(),
    numColl = commInqSizeColl(),
    collRank = cdiPioCollRank(clientRank, numClients, numColl);

  xassert(funcID >= MINFUNCID && funcID <= MAXFUNCID);
  xdebug("%s, func: %s", "START", funcMap[(-1 - funcID)]);

  /* FIXME: move this check to cdiPioClientSetup */
  if (clientRank != cdiPioClientRangeStart(collRank, numClients, numColl))
    return;

  xassert(txWin != NULL);

  size_t streamIdx = indexOfID(&openStreams, streamID);
  if (streamIdx == SIZE_MAX)
    streamIdx = insertID(&openStreams, streamID);
  /* FIXME: handle stream close specially */
  collWait(streamIdx);
  xassert(txWin[streamIdx].dictRPCUsed + txWin[streamIdx].dictDataUsed
          < txWin[streamIdx].dictSize
          && txWin[streamIdx].refuseFuncCall == 0);
  modelWinEnqueue(streamIdx, header, data, dataPackFunc);
  xdebug("%s", "RETURN");
}


void
cdiPioNoPostCommSetup(void)
{
}

/*****************************************************************************/

static int pioNamespace_ = -1;
static int xtInitByCDI = 0;

/* pioInit definition must currently compile even in non-MPI configurations */
/**
   @brief initializes the MPI_Communicators needed for the
  communication between the calculator PEs and the I/O PEs and within the
  group of I/O PEs.

  commGlob: all PEs

  commPIO: I/O PEs, PEs with highest ranks in commGlob

  commModel: calculating PEs, no I/O PEs

  Collective call

  @param commGlob MPI_Communicator of all calling PEs
  @param nProcsIO number of I/O PEs
  @param partInflate allow for array partitions on comute
  PE that are at most sized \f$ partInflate * \lceil arraySize /
  numComputePEs \rceil \f$
  @param postCommSetupActions function which is called by all I/O servers
  after communicator split
  @return int indicating wether the calling PE is a calcutator (1) or not (0)
*/
MPI_Comm
pioInit(MPI_Comm commGlob, int nProcsIO, int IOMode,
        int *pioNamespace, float partInflate,
        void (*postCommSetupActions)(void))
{
  return cdiPioInit(commGlob, IOMode,
                    cdiPioCSRLastN(commGlob, IOMode, nProcsIO),
                    pioNamespace, partInflate, postCommSetupActions);
}

static void cdiPioFileWritingInit(void);

MPI_Comm
cdiPioInit(MPI_Comm commGlob, int IOMode, int clientServerRole,
           int *pioNamespace, float partInflate,
           void (*postCommSetupActions)(void))
{
  int sizeGlob;

  namespaceSwitchSet(NSSWITCH_ABORT, NSSW_FUNC(cdiAbortC_MPI));
  namespaceSwitchSet(NSSWITCH_WARNING, NSSW_FUNC(cdiPioWarning));

  if ( IOMode < PIO_MINIOMODE || IOMode > PIO_MAXIOMODE )
    xabort ( "IOMODE IS NOT VALID." );

#ifdef _SX
  if ( IOMode ==  PIO_ASYNCH )
    xabort ( "PIO_ASYNCH DOES NOT WORK ON SX." );
#endif

  if ((xtInitByCDI = (!xt_initialized() || xt_finalized())))
    xt_initialize(commGlob);
  int nProcsIO = cdiPioCommInit(commGlob, IOMode, clientServerRole);
  sizeGlob = commInqSizeGlob ();


  if (((IOMode != PIO_NONE && (nProcsIO <= 0 || nProcsIO > sizeGlob - 1)))
      || (IOMode == PIO_NONE && nProcsIO != 1))
    xabort("DISTRIBUTION OF TASKS ON PROCS IS NOT VALID.\n"
           "nProcsIO=%d, sizeGlob=%d\n", nProcsIO, sizeGlob);

  xassert(partInflate >= 1.0);
  cdiPIOpartInflate_ = partInflate;

  // JUST FOR TEST CASES WITH ONLY ONE MPI TASK
  if ( commInqSizeGlob () == 1 )
    {
      pioNamespace_ = *pioNamespace = namespaceNew();
      return commInqCommGlob ();
    }

  if ( commInqIsProcIO ())
    {
      cdiPioSerializeSetMPI();
      postCommSetupActions();
      cdiPioFileWritingInit();
      if (commInqRankColl() >= 0)
        {
          cdiPioCollectorMessageLoop();
          cdiPioFileWritingFinalize();
        }
      cdiPioCommFinalize();
      if (xtInitByCDI)
        xt_finalize();
      return MPI_COMM_NULL;
    }
  else
    cdiPioClientSetup(&pioNamespace_, pioNamespace);

  xdebug ( "nProcsGlob=%d, RETURN", sizeGlob );
  return commInqCommModel ();
}

static void
cdiPioFileWritingInit(void)
{
  int IOMode = commInqIOMode ();

  xassert ( IOMode != PIO_NONE  || commInqSizeGlob() == 1 );

  switch ( IOMode )
    {
    case PIO_NONE:
      break;
    case PIO_MPI:
      initMPINONB();
      break;
#ifndef _SX
    case PIO_ASYNCH:
#endif
    case PIO_WRITER:
      pioSendInitialize();
      break;
    case PIO_FPGUARD:
      initPOSIXFPGUARDSENDRECV();
      break;
    case PIO_MPI_FW_ORDERED:
      cdiPioFileWriteOrderedInit();
      break;
    }
}

static void cdiPioFileWritingFinalizeDefault(void)
{
  xabort("error: failed to setup file writing finalization function!");
}

void (*cdiPioFileWritingFinalize)(void) = cdiPioFileWritingFinalizeDefault;

/*****************************************************************************/

void  pioEndDef ( void )
{
  char   * buffer;
  int bufferSize;
  int clientRank = commInqRankModel(),
    numClients = cdiPioCommInqSizeClients(),
    numColl = commInqSizeColl(),
    collRank = cdiPioCollRank(clientRank, numClients, numColl);

  xdebug("%s", "START");

  struct clientBufferSetup setup;
  clientBufSizes(&setup);

  if (clientRank == cdiPioClientRangeStart(collRank, numClients, numColl))
    {
      MPI_Comm comm = cdiPioInqInterComm();
      reshPackBufferCreate(&buffer, &bufferSize, &comm);

      xmpi(MPI_Send(buffer, bufferSize, MPI_PACKED, collRank,
                    RESOURCES, comm));

      xdebug("%s", "SENT MESSAGE WITH TAG \"RESOURCES\"");

      reshPackBufferDestroy ( &buffer );
    }

  cdiPioClientWinCreate(&setup);
  Free(setup.bufSizes);
  namespaceDefResStatus ( STAGE_TIMELOOP );
  xdebug("%s", "RETURN");
}

/************************************************************************/

void  pioEndTimestepping ( void )
{
  xdebug("%s", "START");
  namespaceDefResStatus ( STAGE_CLEANUP );
  xdebug("%s", "RETURN");
}


/****************************************************/


/**
 * @brief is invoked by the calculator PEs, to inform
 * the I/O PEs that no more data will be written.
 */
void pioFinalize ( void )
{
  xdebug("%s", "START");

  /* pioNamespace_ is unchanged on I/O servers */
  if (pioNamespace_ == -1)
    return;
  namespaceDelete(pioNamespace_);

  int clientRank = commInqRankModel(),
    numClients = cdiPioCommInqSizeClients(),
    numColl = commInqSizeColl(),
    collRank = cdiPioCollRank(clientRank, numClients, numColl);

  if (clientRank == cdiPioClientRangeStart(collRank, numClients, numColl))
    xmpi(MPI_Send(NULL, 0, MPI_INT, collRank,
                  FINALIZE, cdiPioInqInterComm()));
  xdebug("%s", "SENT MESSAGE WITH TAG \"FINALIZE\"");
  modelWinDelete();
  cdiPioCommFinalize();
  idSetDestroy(&openStreams);
  if (xtInitByCDI)
    xt_finalize();
  xdebug("%s", "RETURN");
}

 /************************************************************************/

void pioWriteTimestep(void)
{
  int clientRank = commInqRankModel(),
    numClients = cdiPioCommInqSizeClients(),
    numColl = commInqSizeColl(),
    collRank = cdiPioCollRank(clientRank, numClients, numColl);

  xdebug("%s", "START");

  xassert(txWin != NULL);

  size_t nStreams = openStreams.size;
  int *hasUpdates = Malloc(nStreams * sizeof (hasUpdates[0]));
  for (size_t streamIdx = 0; streamIdx < nStreams; ++streamIdx)
    {
      collWait(streamIdx);
      hasUpdates[streamIdx] = txWin[streamIdx].pendingUpdate;
    }

  if (clientRank == cdiPioClientRangeStart(collRank, numClients, numColl))
    {
      xmpi(MPI_Send(hasUpdates, (int)nStreams, MPI_INT,
                    collRank, WRITETS, cdiPioInqInterComm()));
      xdebug("%s", "SENT MESSAGE WITH TAG \"WRITETS\"");
    }

  MPI_Group remoteGroup = cdiPioInqRemoteGroup();
  for (size_t streamIdx = 0; streamIdx < nStreams; ++streamIdx)
    {
      if (openStreams.entries[streamIdx] != CDI_UNDEFID
          && hasUpdates[streamIdx]) { }
      else continue;
      struct winHeaderEntry header
        = { .id = HEADERSIZEMARKER,
            .specific.headerSize
            = { .numDataEntries = txWin[streamIdx].dictDataUsed,
                .numRPCEntries = txWin[streamIdx].dictRPCUsed } };
      struct winHeaderEntry *winDict
        = (struct winHeaderEntry *)(void *)txWin[streamIdx].buffer;
      winDict[0] = header;
      xmpi(MPI_Win_post(remoteGroup, MPI_MODE_NOPUT, txWin[streamIdx].win));
      txWin[streamIdx].postSet = true;
    }

  Free(hasUpdates);
  xdebug("%s", "RETURN. messages sent, windows posted");
}

void
streamWriteVarPart(int streamID, int varID, const void *data,
                   int nmiss, Xt_idxlist partDesc)
{
  if ( CDI_Debug ) Message("streamID = %d varID = %d", streamID, varID);

  int chunk = xt_idxlist_get_num_indices(partDesc);
  xassert(chunk == 0 || data);

  void (*myStreamWriteVarPart)(int streamID, int varID, const void *data,
                               int nmiss, Xt_idxlist partDesc)
    = (void (*)(int, int, const void *, int, Xt_idxlist))
    namespaceSwitchGet(NSSWITCH_STREAM_WRITE_VAR_PART_).func;

  if (!myStreamWriteVarPart)
    xabort("local part writing is unsupported!");

  myStreamWriteVarPart(streamID, varID, data, nmiss, partDesc);
}

void
streamWriteScatteredVarPart(int streamID, int varID, const void *data,
                            int numBlocks, const int blocklengths[],
                            const int displacements[],
                            int nmiss, Xt_idxlist partDesc)
{
  if ( CDI_Debug ) Message("streamID = %d varID = %d", streamID, varID);

  int chunk = xt_idxlist_get_num_indices(partDesc);
  xassert(chunk == 0 || data);

  void (*myStreamWriteScatteredVarPart)(int streamID, int varID,
                                        const void *data,
                                        int numBlocks, const int blocklengths[],
                                        const int displacements[],
                                        int nmiss, Xt_idxlist partDesc)
    = (void (*)(int, int, const void *, int, const int[], const int[], int,
                Xt_idxlist))
    namespaceSwitchGet(NSSWITCH_STREAM_WRITE_SCATTERED_VAR_PART_).func;

  if (!myStreamWriteScatteredVarPart)
    xabort("local part writing is unsupported!");

  myStreamWriteScatteredVarPart(streamID, varID, data,
                                numBlocks, blocklengths, displacements,
                                nmiss, partDesc);
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
