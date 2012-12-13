/** @file ioServer.c
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include "pio_server.h"


#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_PARALLEL_NC4
#include <core/ppm_combinatorics.h>
#include <core/ppm_rectilinear.h>
#include <ppm/ppm_uniform_partition.h>
#include <yaxt.h>
#endif

#include "cdi.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_rpc.h"
#include "pio_util.h"
#include "stream_int.h"
#include "resource_handle.h"
#include "vlist_var.h"


extern resOps streamOps;
extern void arrayDestroy ( void );

static struct
{
  size_t size;
  unsigned char *buffer, *head;
  int currentRecord, dictSize;
} *rxWin = NULL;

static MPI_Win getWin = MPI_WIN_NULL;
static MPI_Group groupModel = MPI_GROUP_NULL;

#ifdef HAVE_PARALLEL_NC4
/* prime factorization of number of pio collectors */
static uint32_t *pioPrimes;
static int numPioPrimes;
#endif

/************************************************************************/

static
void serverWinCleanup ()
{
  int i;
  int nProcsCalc = commInqNProcsModel ();
  
  if ( getWin != MPI_WIN_NULL )
    xmpi ( MPI_Win_free ( &getWin ));
  
  if (rxWin)
    {
      for ( i = 0; i < nProcsCalc; i++ )
        free(rxWin[i].buffer);
      free(rxWin);
    }

  xdebug("%s", "cleaned up mpi_win");
}
 
 /************************************************************************/

static 
  void collDefBufferSizes ()
{
  int nstreams, * streamIndexList, streamNo, vlistID, nvars, varID, iorank;
  int modelID, decoChunk, sumGetBufferSizes = 0;
  int rankGlob = commInqRankGlob ();
  int nProcsModel = commInqNProcsModel ();
  int root = commInqRootGlob ();

  xassert(rxWin != NULL);

  nstreams = reshCountType ( &streamOps );
  streamIndexList = xmalloc ( nstreams * sizeof ( streamIndexList[0] ));
  reshGetResHListOfType ( nstreams, streamIndexList, &streamOps );
  for ( streamNo = 0; streamNo < nstreams; streamNo++ )
    {
      // space required for data
      vlistID = streamInqVlist ( streamIndexList[streamNo] );
      nvars = vlistNvars ( vlistID );
      for ( varID = 0; varID < nvars; varID++ )
        {
          iorank = vlistInqVarIOrank ( vlistID, varID );
          xassert ( iorank != CDI_UNDEFID );
          if ( iorank == rankGlob )
            {
              for ( modelID = 0; modelID < nProcsModel; modelID++ )
                {
                  decoChunk =  vlistInqVarDecoChunk ( vlistID, varID, modelID );
                  xassert ( decoChunk > 0 );
                  rxWin[modelID].size += decoChunk * sizeof (double)
                    + sizeof (union winHeaderEntry);
                  ++(rxWin[modelID].dictSize);
                }
            }
        }
      // space required for the 3 function calls streamOpen, streamDefVlist, streamClose 
      // once per stream and timestep for all collprocs only on the modelproc root
      rxWin[root].size += 3 * sizeof (union winHeaderEntry)
        + MAXDATAFILENAME;
      rxWin[root].dictSize += 3;
    }
  free ( streamIndexList );

  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      /* account for size header */
      rxWin[modelID].dictSize += 1;
      rxWin[modelID].size += sizeof (union winHeaderEntry);
      sumGetBufferSizes += rxWin[modelID].size;
    }
  xassert ( sumGetBufferSizes <= MAXWINBUFFERSIZE );
}

 /************************************************************************/

static 
 void serverWinCreate ()
{ 
  int ranks[1], modelID;
  MPI_Comm commCalc = commInqCommCalc ();
  MPI_Group groupCalc;
  int nProcsModel = commInqNProcsModel ();

  xmpi ( MPI_Win_create ( MPI_BOTTOM, 0, 1, MPI_INFO_NULL,
                          commCalc, &getWin ));

  /* target group */
  ranks[0] = nProcsModel;
  xmpi ( MPI_Comm_group ( commCalc, &groupCalc ));
  xmpi ( MPI_Group_excl ( groupCalc, 1, ranks, &groupModel ));

  rxWin = xcalloc(nProcsModel, sizeof (rxWin[0]));
  collDefBufferSizes ();

  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      rxWin[modelID].buffer = xmalloc(rxWin[modelID].size);
      rxWin[modelID].head = rxWin[modelID].buffer;
    }

  xdebug("%s", "created mpi_win, allocated getBuffer");
}

/************************************************************************/

static void
readFuncCall(struct funcCallDesc *header)
{
  int root = commInqRootGlob ();
  int funcID = header->funcID;

  xassert(funcID >= MINFUNCID && funcID <= MAXFUNCID);
  switch ( funcID )
    {
    case STREAMCLOSE:
      {
        int streamID = header->funcArgs.streamChange.streamID;
        streamClose(streamID);
        xdebug("READ FUNCTION CALL FROM WIN:  %s, streamID=%d,"
               " closed stream",
               funcMap[(-1 - funcID)], streamID);
      }
      break;
    case STREAMOPEN:
      {
        size_t filenamesz = header->funcArgs.newFile.fnamelen;
        xassert ( filenamesz > 0 && filenamesz < MAXDATAFILENAME );
        const char *filename
          = (const char *)(rxWin[root].buffer
                           + header->funcArgs.newFile.offset);
        xassert(filename[filenamesz] == '\0');
        int filetype = header->funcArgs.newFile.filetype;
        xassert ( filetype >= MINFILETYPE && filetype <= MAXFILETYPE );
        int streamID = streamOpenWrite(filename, filetype);
        xdebug("READ FUNCTION CALL FROM WIN:  %s, filenamesz=%zu,"
               " filename=%s, filetype=%d, OPENED STREAM %d",
               funcMap[(-1 - funcID)], filenamesz, filename,
               filetype, streamID);
      }
      break;
    case STREAMDEFVLIST:
      {
        int streamID = header->funcArgs.streamChange.streamID;
        int vlistID = header->funcArgs.streamChange.vlistID;
        streamDefVlist(streamID, vlistID);
        xdebug("READ FUNCTION CALL FROM WIN:  %s, streamID=%d,"
               " vlistID=%d, called streamDefVlist ().",
               funcMap[(-1 - funcID)], streamID, vlistID);
      }
      break;
    default:
      xabort ( "REMOTE FUNCTIONCALL NOT IMPLEMENTED!" );
    }
}

/************************************************************************/

static void
resizeVarGatherBuf(int vlistID, int varID, double **buf, int *bufSize)
{
  int size = vlistInqVarSize(vlistID, varID);
  if (size <= *bufSize) ; else
    *buf = xrealloc(*buf, (*bufSize = size) * sizeof (buf[0][0]));
}


static void
gatherArray(int root, int nProcsModel, int headerIdx,
            int vlistID,
            double *gatherBuf, int *nmiss)
{
  double *p = gatherBuf;
  union winHeaderEntry *winDict
    = (union winHeaderEntry *)rxWin[root].buffer;
  int streamID = winDict[headerIdx].dataRecord.streamID;
  int varID = winDict[headerIdx].dataRecord.varID;
  *nmiss = 0;
  for (int modelID = 0; modelID < nProcsModel; modelID++)
    {
      struct dataRecord *dataHeader
        = (struct dataRecord *)rxWin[modelID].buffer
        + headerIdx;
      xassert(dataHeader->streamID == streamID
              && dataHeader->varID == varID);
      int chunk = vlistInqVarDecoChunk(vlistID, varID,
                                       modelID);
      memcpy(p, rxWin[modelID].buffer
             + dataHeader->offset, chunk * sizeof (p[0]));
      p += chunk;
      *nmiss = dataHeader->nmiss;
    }
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
queryVarDims(struct PPM_extent varShape[3], int vlistID, int varID)
{
  int gridID = vlistInqVarGrid(vlistID, varID);
  int zaxisID = vlistInqVarZaxis(vlistID, varID);
  int gridType = gridInqType(gridID);
  switch (gridType)
    {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
      varShape[0].first = 0;
      varShape[0].size = gridInqXsize(gridID);
      varShape[1].first = 0;
      varShape[1].size = gridInqYsize(gridID);
      break;
    case GRID_GENERIC:
    case GRID_LCC:
    case GRID_SPECTRAL:
    case GRID_GME:
    case GRID_CURVILINEAR:
    case GRID_UNSTRUCTURED:
    case GRID_REFERENCE:
      xabort("unimplemented grid type: %d", gridType);
      break;
    }
  varShape[2].first = 0;
  varShape[2].size = zaxisInqSize(zaxisID);
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
#endif

static
void readGetBuffers ( int tsID, int vdate, int vtime )
{
  int nProcsModel = commInqNProcsModel ();
  int root        = commInqRootGlob ();
  int myCollRank = commInqRankColl();
  MPI_Comm collComm = commInqCommColl();
  xdebug("%s", "START");

  union winHeaderEntry *winDict
    = (union winHeaderEntry *)rxWin[root].buffer;
  xassert(winDict[0].headerSize.sizeID == HEADERSIZEMARKER);
  {
    int dictSize = rxWin[root].dictSize,
      firstNonRPCEntry = dictSize - winDict[0].headerSize.numRPCEntries - 1,
      headerIdx,
      numFuncCalls = 0;
    for (headerIdx = dictSize - 1;
         headerIdx > firstNonRPCEntry;
         --headerIdx)
      {
        struct funcCallDesc *header
          = &(winDict[headerIdx].funcCall);
        xassert(header->funcID >= MINFUNCID
                && header->funcID <= MAXFUNCID);
        ++numFuncCalls;
        readFuncCall(header);
      }
    xassert(numFuncCalls == winDict[0].headerSize.numRPCEntries);
  }
  /* build list of streams, data was transferred for */
  {
    int numDataEntries = winDict[0].headerSize.numDataEntries;
    int streamIdx;
    struct {
      int streamID, filetype;
      int firstHeaderIdx, lastHeaderIdx;
      int numVars, *varMap;
    } *streamMap;
    int numStreamIDs = 0, sizeStreamMap = 16;
    streamMap = xmalloc(sizeStreamMap * sizeof (streamMap[0]));
    int streamIDOld = CDI_UNDEFID;
    int headerIdx, oldStreamIdx = CDI_UNDEFID;
    int filetype = CDI_UNDEFID;
    for (headerIdx = 1; headerIdx < numDataEntries; ++headerIdx)
      {
        int streamID = winDict[headerIdx].dataRecord.streamID;
        xassert(streamID > 0);
        if (streamID != streamIDOld)
          {
            for (int i = numStreamIDs - 1; i >= 0; --i)
              if ((streamIDOld = streamMap[i].streamID) == streamID)
                {
                  filetype = streamMap[i].filetype;
                  oldStreamIdx = i;
                  goto streamIDInventorized;
                }
            if (numStreamIDs < sizeStreamMap) ; else
              streamMap = xrealloc(streamMap,
                                   (sizeStreamMap *= 2)
                                   * sizeof (streamMap[0]));
            streamMap[numStreamIDs].streamID = streamID;
            streamMap[numStreamIDs].firstHeaderIdx = headerIdx;
            streamMap[numStreamIDs].numVars = -1;
            oldStreamIdx = numStreamIDs;
            streamIDOld = streamID;
            filetype = streamInqFiletype(streamID);
            streamMap[numStreamIDs].filetype = filetype;
            if (filetype == FILETYPE_NC || filetype == FILETYPE_NC2
                || filetype == FILETYPE_NC4)
              {
                int vlistID = streamInqVlist(streamID);
                int nvars = vlistNvars(vlistID);
                streamMap[numStreamIDs].numVars = nvars;
                streamMap[numStreamIDs].varMap
                  = xmalloc(sizeof (streamMap[numStreamIDs].varMap[0])
                            * nvars);
                for (int i = 0; i < nvars; ++i)
                  streamMap[numStreamIDs].varMap[i] = -1;
              }
            ++numStreamIDs;
          }
        streamIDInventorized:
        streamMap[oldStreamIdx].lastHeaderIdx = headerIdx;
        if (filetype == FILETYPE_NC || filetype == FILETYPE_NC2
                || filetype == FILETYPE_NC4)
          {
            int varID = winDict[headerIdx].dataRecord.varID;
            streamMap[oldStreamIdx].varMap[varID] = headerIdx;
          }
      }
    double *data = NULL, *writeBuf = NULL;
    int currentDataBufSize = 0;
    for (streamIdx = 0; streamIdx < numStreamIDs; ++streamIdx)
      {
        int streamID = streamMap[streamIdx].streamID;
        int vlistID = streamInqVlist(streamID);
        int fileType = streamMap[streamIdx].filetype;
        int taxisID = vlistInqTaxis(vlistID);
        taxisDefVdate(taxisID, vdate);
        taxisDefVtime(taxisID, vtime);
        streamDefTimestep(streamID, tsID);

        switch (fileType)
          {
          case FILETYPE_GRB:
          case FILETYPE_GRB2:
            {
              int headerIdx, lastHeaderIdx = streamMap[streamIdx].lastHeaderIdx;
              for (headerIdx = streamMap[streamIdx].firstHeaderIdx;
                   headerIdx <= lastHeaderIdx;
                   ++headerIdx)
                if (streamID == winDict[headerIdx].dataRecord.streamID)
                  {
                    int varID = winDict[headerIdx].dataRecord.varID;
                    int size = vlistInqVarSize(vlistID, varID);
                    int nmiss;
                    resizeVarGatherBuf(vlistID, varID, &data,
                                       &currentDataBufSize);
                    gatherArray(root, nProcsModel, headerIdx,
                                vlistID, data, &nmiss);
                    streamWriteVar(streamID, varID, data, nmiss);
                    if ( ddebug > 2 )
                      {
                        char text[1024];
                        sprintf(text, "streamID=%d, var[%d], size=%d",
                                streamID, varID, size);
                        xprintArray(text, data, size, DATATYPE_FLT);
                      }
                  }
            }
            break;
#ifdef HAVE_NETCDF4
          case FILETYPE_NC:
          case FILETYPE_NC2:
          case FILETYPE_NC4:
#ifdef HAVE_PARALLEL_NC4
            /* HAVE_PARALLE_NC4 implies having ScalES-PPM and yaxt */
            {
              int nvars = streamMap[streamIdx].numVars;
              int *varMap = streamMap[streamIdx].varMap;
              int *varIsWritten = xmalloc(sizeof (varIsWritten[0]) * nvars);
              for (int varID = 0; varID < nvars; ++varID)
                varIsWritten[varID] = ((varMap[varID] != -1)
                                       ?myCollRank+1 : 0);
              xmpi(MPI_Allreduce(MPI_IN_PLACE, varIsWritten, nvars,
                                 MPI_INT, MPI_BOR, collComm));
              for (int varID = 0; varID < nvars; ++varID)
                if (varIsWritten[varID])
                  {
                    struct PPM_extent varShape[3];
                    queryVarDims(varShape, vlistID, varID);
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
                    Xt_idxlist preRedistChunk, preWriteChunk;
                    /* prepare yaxt descriptor for current data
                       distribution after collect */
                    int nmiss;
                    if (varMap[varID] == -1)
                      {
                        preRedistChunk = xt_idxempty_new();
                        xdebug("%s", "I got none\n");
                      }
                    else
                      {
                        Xt_int preRedistStart[3] = { 0, 0, 0 };
                        preRedistChunk
                          = xt_idxsection_new(0, 3, varSize, varSize,
                                              preRedistStart);
                        resizeVarGatherBuf(vlistID, varID, &data,
                                           &currentDataBufSize);
                        int headerIdx = varMap[varID];
                        gatherArray(root, nProcsModel, headerIdx,
                                    vlistID, data, &nmiss);
                        xdebug("%s", "I got all\n");
                      }
                    MPI_Bcast(&nmiss, 1, MPI_INT, varIsWritten[varID] - 1,
                              collComm);
                    /* prepare yaxt descriptor for write chunk */
                    {
                      Xt_int preWriteChunkStart[3], preWriteChunkSize[3];
                      for (int i = 0; i < 3; ++i)
                        {
                          preWriteChunkStart[2 - i] = varChunk[i].first;
                          preWriteChunkSize[2 - i] = varChunk[i].size;
                        }
                      preWriteChunk = xt_idxsection_new(0, 3, varSize,
                                                        preWriteChunkSize,
                                                        preWriteChunkStart);
                    }
                    /* prepare redistribution */
                    {
                      Xt_xmap xmap = xt_xmap_all2all_new(preRedistChunk,
                                                         preWriteChunk,
                                                         collComm);
                      Xt_redist redist = xt_redist_p2p_new(xmap, MPI_DOUBLE);
                      xt_idxlist_delete(preRedistChunk);
                      xt_idxlist_delete(preWriteChunk);
                      xt_xmap_delete(xmap);
                      writeBuf = xrealloc(writeBuf,
                                          sizeof (double)
                                          * PPM_extents_size(3, varChunk));
                      xt_redist_s_exchange(redist, (void **)&data, 1,
                                           (void **)&writeBuf, 1);
                      xt_redist_delete(redist);
                    }
                    /* write chunk */
                    streamWriteVarChunk(streamID, varID,
                                        (const int (*)[2])myChunk, writeBuf,
                                        nmiss);
                  }
            }
#else
#error "serial netcdf libraries not yet supported in MPI mode"
            /* determine process which has stream open (writer) and
             * which has data for which variable (var owner) */
            /* for each variable with a var owner send full variable
             * contents to writer */
            /* writer calls streamWrite */
#endif
            break;
#endif
          default:
            xabort("unhandled filetype in parallel I/O.");
          }
      }
    free(streamMap);
    free(data);
  }
  xdebug("%s", "RETURN");
} 

/************************************************************************/


static
void clearModelWinBuffer(int modelID)
{
  int nProcsModel = commInqNProcsModel ();

  xassert ( modelID                >= 0           &&
            modelID                 < nProcsModel &&
            rxWin != NULL && rxWin[modelID].buffer != NULL &&
            rxWin[modelID].size > 0 &&
            rxWin[modelID].size <= MAXWINBUFFERSIZE );
  memset(rxWin[modelID].buffer, 0, rxWin[modelID].size);
  rxWin[modelID].head = rxWin[modelID].buffer;
}


/************************************************************************/


static
void getTimeStepData ( int tsID, int vdate, int vtime )
{
  int modelID;
  char text[1024];
  int nProcsModel = commInqNProcsModel ();
  void *getWinBaseAddr;
  int attrFound;
          
  xdebug("%s", "START");

  // todo put in correct lbs and ubs
  xassert ( tsID >= 0 && vdate >= 0 && vtime >= 0 );
  xmpi(MPI_Win_start(groupModel, 0, getWin));
  xmpi(MPI_Win_get_attr(getWin, MPI_WIN_BASE, &getWinBaseAddr, &attrFound));
  xassert(attrFound);
  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      clearModelWinBuffer(modelID);
      xdebug("modelID=%d, nProcsModel=%d, rxWin[%d].size=%zu,"
             " getWin=%p, sizeof(int)=%u",
             modelID, nProcsModel, modelID, rxWin[modelID].size,
             getWinBaseAddr, (unsigned)sizeof(int));
      xmpi(MPI_Get(rxWin[modelID].buffer, rxWin[modelID].size,
                   MPI_UNSIGNED_CHAR, modelID, 0,
                   rxWin[modelID].size, MPI_UNSIGNED_CHAR, getWin));
    }
  xmpi ( MPI_Win_complete ( getWin ));

  if ( ddebug > 2 )
    for ( modelID = 0; modelID < nProcsModel; modelID++ )
      {
        sprintf(text, "rxWin[%d].size=%zu from PE%d rxWin[%d].buffer",
                modelID, rxWin[modelID].size, modelID, modelID);
        xprintArray(text, rxWin[modelID].buffer,
                    rxWin[modelID].size / sizeof (double),
                    DATATYPE_FLT);
      }
  readGetBuffers ( tsID, vdate, vtime );
          
  xdebug("%s", "RETURN");
}

/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ()
{
  int source, tag, size, nProcsModel=commInqNProcsModel();
  static int nfinished = 0;
  char * buffer;
  MPI_Comm commCalc;
  MPI_Status status;

  xdebug("%s", "START");

  backendInit ();
  if ( commInqRankNode () == commInqSpecialRankNode ()) 
    backendFinalize ();
  commCalc = commInqCommCalc ();
  numPioPrimes = PPM_prime_factorization_32((uint32_t)commInqSizeColl(),
                                            &pioPrimes);
  xt_initialize(commInqCommColl());

  for ( ;; )
    {
      xmpi ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commCalc, &status ));
      
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      
      switch ( tag )
        {
        case FINALIZE:
          {
            int i;
            xdebugMsg(tag, source, nfinished);
            xmpi(MPI_Recv(&i, 1, MPI_INTEGER, source,
                          tag, commCalc, &status));
          }
          xdebug("%s", "RECEIVED MESSAGE WITH TAG \"FINALIZE\"");
          nfinished++;
          xdebug("nfinished=%d, nProcsModel=%d", nfinished, nProcsModel);
          if ( nfinished == nProcsModel )
            {
              {
                int nStreams = streamSize ();

                if ( nStreams > 0 )
                  {
                    int streamNo;
                    int * resHs;

                    resHs       = xmalloc ( nStreams * sizeof ( resHs[0] ));
                    streamGetIndexList ( nStreams, resHs );
                    for ( streamNo = 0; streamNo < nStreams; streamNo++ )
                      streamClose ( resHs[streamNo] );
                    free ( resHs );
                  }
              }
              backendCleanup();
              serverWinCleanup();
              /* listDestroy(); */
              xdebug("%s", "RETURN");
              return;
            }
	  
          break;
          
	case RESOURCES:
	  xdebugMsg (  tag, source, nfinished );
	  xmpi ( MPI_Get_count ( &status, MPI_CHAR, &size ));
	  buffer = xmalloc(size);
	  xmpi ( MPI_Recv ( buffer, size, MPI_PACKED, source,
                            tag, commCalc, &status ));
          xdebug("%s", "RECEIVED MESSAGE WITH TAG \"RESOURCES\"");
	  rpcUnpackResources ( buffer, size, commCalc );
          xdebug("%s", "");
	  free ( buffer );
          if ( ddebug > 0 && commInqRankGlob () == nProcsModel ) 
            reshListPrint ( "reshListIOServer" );
	  serverWinCreate ();
	  break;

	case WRITETS:
          {
            int iBuffer[timestepSize];
            xdebugMsg(tag, source, nfinished);
            xmpi(MPI_Get_count(&status, MPI_INTEGER, &size));
            xassert(size == timestepSize);
            xmpi(MPI_Recv(iBuffer, size, MPI_INTEGER, source,
                          tag, commCalc, &status));
            xdebug("RECEIVED MESSAGE WITH TAG \"WRITETS\": "
                   "tsID=%d, vdate=%d, vtime=%d, source=%d",
                   iBuffer[0], iBuffer[1], iBuffer[2], source );
            getTimeStepData(iBuffer[0], iBuffer[1], iBuffer[2]);
          }
	  break;

	default:
	  xabort ( "TAG NOT DEFINED!" );
	}
    }
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
