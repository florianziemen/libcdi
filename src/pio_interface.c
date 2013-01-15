#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "cdi.h"
#include "limits.h"
#include "pio_util.h"
#include "vlist_var.h"

#ifdef USE_MPI
#include "namespace.h"
#include "pio_interface.h"
#include "pio_comm.h"
#include "pio_rpc.h"
#include "pio_server.h"
#include "resource_handle.h"
#include "stream_int.h"
#include "vlist.h"
extern resOps streamOps;


static struct rdmaWin
{
  size_t size;
  unsigned char *buffer, *head;
  MPI_Win win;
  int postSet, refuseFuncCall;
  MPI_Group ioGroup;
  int dictSize, dictDataUsed, dictRPCUsed;
} *txWin = NULL;


char * funcMap[nFuncs] = {"streamOpen", "streamDefVlist", "streamClose" };


/****************************************************/

static int cmp ( const void * va, const void * vb )
{
    const int ** a, ** b;

  a = ( const int ** ) va;
  b = ( const int ** ) vb;

  return (( **a < **b ) - ( **a > **b ));
}

/****************************************************/

static void
mapProblems(int problemSizes[], int * problemMapping, int nProblems,
            int nWriter, double * w)
{
  int *ip[nProblems];
  int dummy[nProblems];
  int buckets[nWriter];
  int currCapacity, nextCapacity;
  double meanBucket[nWriter];
  int sum = 0;
  int writerIdx = 0;
  int i, j;


  for ( i = 0; i < nProblems; i++ )
    {
      ip[i] = &problemSizes[i];
      sum += problemSizes[i];
    }

  qsort ( ip, nProblems, sizeof ( int * ), cmp );

  for ( i = 0; i < nProblems; i++ )
    dummy[i] = ip[i] - problemSizes;

  for ( j = 0; j < nWriter; j++ )
    meanBucket[j] = ( double ) sum * ( * ( w + j ));

  for ( i = 0; i < nProblems; i++ )
    {
      currCapacity = INT_MIN;

      for ( j = 0; j < nWriter; j++ )
	{
	  if ( !i ) buckets[j] = 0.0;
	  nextCapacity = meanBucket[j] - ( buckets[j] + ( *ip[i] ));

	  if ( nextCapacity > currCapacity )
	    {
	      currCapacity = nextCapacity;
	      writerIdx = j;
	    }
	}
      problemMapping[ dummy[i] ] = writerIdx;
      buckets[writerIdx] +=  *ip[i];
    }

  xprintArray3 (  "problemSizes = ", problemSizes, nProblems, DATATYPE_INT );
  xprintArray3 ( "vector of indices, qsort of problemSizes", dummy,
                nProblems, DATATYPE_INT );
  xprintArray3 ( "problemMapping", problemMapping, nProblems, DATATYPE_INT );
  xprintArray3 ( "meanBucket", meanBucket, nWriter, DATATYPE_FLT );
  xprintArray3 ( "actual buckets", buckets, nWriter, DATATYPE_INT );
}

/****************************************************/

/**
   @brief is encapsulated in CDI library.

   @param vSizes array with number of levels for all var_t 's in order of streams

   @param sSizes array with number of var_t for all stream_t 's

   @param varMapping return value, array with ranks of I/O PEs assigned to var_t 's
in order of vSizes

   @param nStreams number of stream_t 's

   @param nodeSizes array of number of I/O PEs on each physical nodes, increasing
   order of ranks assumed

   @param nNodes number of physical nodes hosting I/O PEs

   @return
*/

static void
varMapGen(int *vSizes, int *sSizes, int *varMapping,
          int nStreams, int *nodeSizes, int nNodes)
{

  int weightsStreams[nStreams];
  int streamMapping[nStreams];
  int nPEs = 0, nVars = 0;
  int i, j, k, offset = 0, offsetN = 0;
  double * w;

  int * weightsVarsNode;
  int * varMappingNode;
  int nVarsNode, summandRank = 0;
  int nProcsColl = commInqNProcsColl ();

  int buckets[nProcsColl];

  for ( i = 0; i < nStreams; i++ )
    {
      nVars += * ( sSizes + i );
      weightsStreams[i] = 0;
      for ( j = 0; j < * ( sSizes + i ); j++ )
	weightsStreams[i] += * ( vSizes + offset++ );
    }

  w = ( double * ) xmalloc ( nNodes * sizeof ( double ));
  for ( j = 0; j < nNodes; j++ )
    nPEs += * ( nodeSizes + j );

  for ( j = 0; j < nNodes; j++ )
    w[j] = ( double ) * ( nodeSizes + j ) / ( double ) nPEs;

  mapProblems ( weightsStreams, streamMapping, nStreams, nNodes, w );
  free ( w );

  for ( i = 0; i < nNodes; i++ )
    {
      nVarsNode = 0;
      for ( j = 0; j < nStreams; j++ )
	if ( * ( streamMapping + j ) == i )
	  nVarsNode += * ( sSizes + j );

      weightsVarsNode = ( int * ) xmalloc ( nVarsNode * sizeof ( int ));
      varMappingNode = ( int * ) xmalloc ( nVarsNode * sizeof ( int ));
      w = ( double * ) xmalloc ( * ( nodeSizes + i ) * sizeof ( double ));
      offset = 0;
      offsetN = 0;

      for ( j = 0; j < nStreams; j++ )
	if ( * ( streamMapping + j ) == i )
	  for ( k = 0; k < * ( sSizes + j ); k ++ )
	    * ( weightsVarsNode + offsetN++ ) = * ( vSizes + offset++ );
	else
	  offset += * ( sSizes + j );

      for ( j = 0; j < * ( nodeSizes + i ); j ++ )
	w[j] = 1.0 / ( double ) * ( nodeSizes + i );

      mapProblems ( weightsVarsNode, varMappingNode, nVarsNode,
		    * ( nodeSizes + i ),  w );

      offset = 0;
      offsetN = 0;

      for ( j = 0; j < nStreams; j++ )
	if ( * ( streamMapping + j ) == i )
	  for ( k = 0; k < * ( sSizes + j ); k ++ )
	    * ( varMapping + offset ++ ) =
              commCollID2RankGlob ( * ( varMappingNode + offsetN ++ ) +
                                    summandRank );
	else
	  offset += * ( sSizes + j );

      summandRank += * ( nodeSizes + i );

      free ( w );
      free ( varMappingNode );
      free ( weightsVarsNode );
    }

  if ( ddebug )
    {
      xprintArray ( "varMapping", varMapping, nVars, DATATYPE_INT  );
      for ( i = 0; i < nProcsColl; i++ )
	buckets[i] = 0;
      for ( i = 0; i < nVars; i ++ )
	buckets[commRankGlob2CollID ( *(varMapping + i ))] += * ( vSizes + i );
      xprintArray ( "buckets", buckets, nProcsColl, DATATYPE_INT );
    }
}

/************************************************************************/

static void
defVarDeco(int vlistID, int varID)
{
  int varSize, cRank, lChunk, rem, lOffset;
  int nProcsModel = commInqNProcsModel ();
  deco_t deco[nProcsModel];

  varSize = vlistInqVarSize ( vlistID, varID );

  for ( cRank = 0; cRank < nProcsModel; cRank++ )
    {
      lChunk = varSize / nProcsModel;
      lOffset = cRank * lChunk;
      rem = varSize % nProcsModel;
      if ( cRank < rem )
        {
          lChunk++;
          lOffset += cRank;
        }
      else
        lOffset += rem;

      deco[cRank].rank   = cRank;
      deco[cRank].offset = lOffset;
      deco[cRank].chunk  = lChunk;
    }
  vlistDefVarDeco ( vlistID, varID, nProcsModel, &deco[0] );
}

/************************************************************************/


static void
varsMapNDeco(int nNodes, int *nodeSizes)
{
  int nStreams, nVars, * resHs, * streamSizes, * varSizes, * varMapping,
    * collectsData;
  int i, j, k = 0;
  int nProcsColl = commInqNProcsColl ();
  char text[1024];

  xdebug ( "START, nProcsColl=%d", nProcsColl );

  nStreams = streamSize ();

  resHs       = xmalloc ( nStreams * sizeof ( resHs[0] ));
  streamSizes = xmalloc ( nStreams * sizeof ( streamSizes[0] ));
  collectsData = xmalloc ( nProcsColl * sizeof ( collectsData[0] ));
  streamGetIndexList ( nStreams, resHs );

  for ( i = 0; i < nStreams; i++ )
    streamSizes[i] = streamInqNvars ( * ( resHs + i ));

  nVars = xsum ( nStreams, streamSizes );
  varSizes   = xmalloc ( nVars * sizeof ( varSizes[0] ));
  varMapping = xmalloc ( nVars * sizeof ( varMapping[0] ));

  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < * ( streamSizes + i ); j++ )
      varSizes[k++] += vlistInqVarSize ( streamInqVlist ( * ( resHs + i )), j );

  xassert ( k == nVars );

  varMapGen ( varSizes, streamSizes, varMapping,
	      nStreams, nodeSizes, nNodes );

  k = 0;
  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < * ( streamSizes + i ); j++ )
      {
        defVarDeco ( streamInqVlist ( *( resHs + i )), j );
        defVarDeco ( streamInqVlistIDorig ( * ( resHs + i )), j );
        vlistDefVarIOrank ( streamInqVlist ( * ( resHs + i )), j,
                            * ( varMapping + k ));
        vlistDefVarIOrank ( streamInqVlistIDorig ( * ( resHs + i )), j,
                            * ( varMapping + k ));
        collectsData[commRankGlob2CollID ( varMapping[k++] )] = 1;
      }

  for ( j = 0; j < nProcsColl; j++ )
    if ( collectsData[j] == 0 )
      {
        sprintf ( text,
                  "\nAT LEAST ONE COLLECTOR PROCESS IDLES, "
                  "CURRENTLY NOT COVERED: "
                  "PE%d collects no data",
                  commCollID2RankGlob ( j ));
        xabort ( text );
      }

  if ( varMapping )   free ( varMapping );
  if ( varSizes )     free ( varSizes );
  if ( collectsData ) free ( collectsData );
  if ( streamSizes )  free ( streamSizes );
  if ( resHs )        free ( resHs );

  xdebug("%s", "RETURN");
}

/************************************************************************/

static
void modelWinCleanup ( void )
{
  int collID;

  xdebug("%s", "START");
  if (txWin != NULL)
    {
      for ( collID = 0; collID < commInqNProcsColl (); collID ++ )
        {
          if (txWin[collID].postSet)
            xmpi(MPI_Win_wait(txWin[collID].win));
          xmpi(MPI_Win_free(&txWin[collID].win));
          xmpi ( MPI_Free_mem ( txWin[collID].buffer ));
          xmpi(MPI_Group_free(&txWin[collID].ioGroup));
        }
      free(txWin);
    }

  xdebug("%s", "RETURN. CLEANED UP MPI_WIN'S");
}

/************************************************************************/

struct collDesc
{
  int numDataRecords, numRPCRecords;
};

static void
modelWinDefBufferSizes(void)
{
  int collID, nstreams, * streamIndexList, streamNo, nvars, varID;
  int collIDchunk = 0, sumWinBufferSize = 0;
  int nProcsColl  = commInqNProcsColl ();
  int rankGlob    = commInqRankGlob ();
  int rankModel   = commInqRankModel ();
  int root = commInqRootGlob ();
  struct collDesc *collIndex;

  xdebug("%s", "START");
  xassert(txWin != NULL);

  nstreams = reshCountType ( &streamOps );
  streamIndexList = xmalloc ( nstreams * sizeof ( streamIndexList[0] ));
  collIndex = xcalloc(nProcsColl, sizeof (collIndex[0]));
  reshGetResHListOfType ( nstreams, streamIndexList, &streamOps );
  for ( streamNo = 0; streamNo < nstreams; streamNo++ )
    {
      // memory required for data
      int streamID = streamIndexList[streamNo];
      int vlistID = streamInqVlist(streamID);
      nvars = vlistNvars ( vlistID );
      for ( varID = 0; varID < nvars; varID++ )
        {
          collID = CDI_UNDEFID;
          collID = commRankGlob2CollID ( vlistInqVarIOrank ( vlistID, varID ));
          collIDchunk = vlistInqVarDecoChunk ( vlistID, varID, rankModel );
          xassert ( collID != CDI_UNDEFID && collIDchunk > 0 );
          ++(collIndex[collID].numDataRecords);
          txWin[collID].size += collIDchunk * sizeof (double) +
            sizeof (union winHeaderEntry);
        }

      // memory required for the 3 function calls streamOpen, streamDefVlist, streamClose
      // once per stream and timestep for all collprocs only on the modelproc root
      if ( rankGlob == root )
        for ( collID = 0; collID < nProcsColl; collID++ )
          {
            collIndex[collID].numRPCRecords += 3;
            txWin[collID].size +=
              3 * sizeof (union winHeaderEntry)
              + MAXDATAFILENAME;
          }
    }
  for (collID = 0; collID < nProcsColl; ++collID)
    {
      int numRecords = 1 + collIndex[collID].numDataRecords
        + collIndex[collID].numRPCRecords;
      txWin[collID].dictSize = numRecords;
      txWin[collID].dictDataUsed = 1;
      txWin[collID].dictRPCUsed = 0;
      txWin[collID].size += sizeof (union winHeaderEntry);
      sumWinBufferSize += txWin[collID].size;
    }
  free(collIndex);
  free ( streamIndexList );

  xdebug("sumWinBufferSize=%zu, MAXWINBUFFERSIZE=%zu", (size_t)sumWinBufferSize,
         (size_t)MAXWINBUFFERSIZE);
  xassert ( sumWinBufferSize <= MAXWINBUFFERSIZE );
  xdebug("%s", "RETURN");
}


/************************************************************************/


static
  void modelWinFlushBuffer ( int collID )
{
  int nProcsColl = commInqNProcsColl ();

  xassert ( collID                >= 0         &&
            collID                < nProcsColl &&
            txWin[collID].buffer     != NULL      &&
            txWin != NULL      &&
            txWin[collID].size >= 0         &&
            txWin[collID].size <= MAXWINBUFFERSIZE);
  memset(txWin[collID].buffer, 0, txWin[collID].size);
  txWin[collID].head = txWin[collID].buffer
    + txWin[collID].dictSize * sizeof (union winHeaderEntry);
  txWin[collID].refuseFuncCall = 0;
  txWin[collID].dictDataUsed = 1;
  txWin[collID].dictRPCUsed = 0;
}


/************************************************************************/


static
void modelWinCreate ( void )
{
  int collID, ranks[1];
  int nProcsColl = commInqNProcsColl ();

  xdebug("%s", "START");
  txWin = xmalloc(nProcsColl * sizeof (txWin[0]));

  modelWinDefBufferSizes ();
  ranks[0] = commInqNProcsModel ();

  for ( collID = 0; collID < nProcsColl; collID ++ )
    {
      xassert(txWin[collID].size > 0);
      txWin[collID].buffer = NULL;
      xmpi(MPI_Alloc_mem((MPI_Aint)txWin[collID].size, MPI_INFO_NULL,
                         &txWin[collID].buffer));
      xassert ( txWin[collID].buffer != NULL );
      txWin[collID].head = txWin[collID].buffer
        + txWin[collID].dictSize * sizeof (union winHeaderEntry);
      xmpi(MPI_Win_create(txWin[collID].buffer, (MPI_Aint)txWin[collID].size, 1,
                          MPI_INFO_NULL, commInqCommsIO(collID),
                          &txWin[collID].win));
      xmpi(MPI_Comm_group(commInqCommsIO(collID), &txWin[collID].ioGroup));
      xmpi(MPI_Group_incl(txWin[collID].ioGroup, 1, ranks,
                          &txWin[collID].ioGroup ));
    }
  xdebug("%s", "RETURN, CREATED MPI_WIN'S");
}

/************************************************************************/

static void
modelWinEnqueue(int collID,
                union winHeaderEntry header, const void *data, size_t size)
{
  union winHeaderEntry *winDict
    = (union winHeaderEntry *)txWin[collID].buffer;
  int targetEntry;
  if (header.dataRecord.streamID > 0)
    {
      targetEntry = (txWin[collID].dictDataUsed)++;
      header.dataRecord.offset
        = (int)(txWin[collID].head - txWin[collID].buffer);
      memcpy(txWin[collID].head, data, size);
      txWin[collID].head += size;
    }
  else
    {
      targetEntry = txWin[collID].dictSize - ++(txWin[collID].dictRPCUsed);
      if (header.funcCall.funcID == STREAMOPEN)
        {
          header.funcCall.funcArgs.newFile.offset
            = (int)(txWin[collID].head - txWin[collID].buffer);
          memcpy(txWin[collID].head, data, size);
          txWin[collID].head += size;
        }
    }
  winDict[targetEntry] = header;
}

void pioBufferData ( const int streamID, const int varID, const double *data, int nmiss )
{
  int chunk, vlistID, collID = CDI_UNDEFID;
  int rankModel = commInqRankModel ();

  vlistID  = streamInqVlist ( streamID );
  collID   = commRankGlob2CollID ( vlistInqVarIOrank ( vlistID, varID ));
  chunk    = vlistInqVarDecoChunk ( vlistID, varID, rankModel );
  xassert ( collID         >= 0                    &&
            collID         <  commInqNProcsColl () &&
            chunk          >= 0                    &&
            txWin != NULL);

  if (txWin[collID].postSet)
    {
      xmpi(MPI_Win_wait(txWin[collID].win));
      txWin[collID].postSet = 0;
      modelWinFlushBuffer ( collID );
    }

  union winHeaderEntry header
    = { .dataRecord = { streamID, varID, -1, nmiss } };
  modelWinEnqueue(collID, header, data, chunk * sizeof (data[0]));

  txWin[collID].refuseFuncCall = 1;
}

/************************************************************************/

void pioBufferFuncCall(int funcID, int argc, ... )
{
  va_list ap;
  int rankGlob = commInqRankGlob ();
  int root = commInqRootGlob ();
  int collID, nProcsColl = commInqNProcsColl ();

  xassert(funcID >= MINFUNCID && funcID <= MAXFUNCID);
  xdebug("%s, func: %s", "START", funcMap[(-1 - funcID)]);

  if ( rankGlob != root ) return;

  xassert (argc          >= 1                    &&
           argc          <= 2                    &&
           txWin != NULL);

  va_start ( ap, argc );

  union winHeaderEntry header;
  char * filename = NULL;
  size_t filenamesz = 0;
  int streamID = 0, vlistID = 0, filetype = 0;

  switch ( funcID )
    {
    case STREAMCLOSE:
      xassert ( argc == 1 );
    case STREAMDEFVLIST:
      {
        streamID = va_arg(ap, int);
        vlistID = CDI_UNDEFID;
        if (funcID == STREAMDEFVLIST)
          {
            xassert ( argc == 2 );
            vlistID = va_arg(ap, int);
          }
        struct funcCallDesc f = { .funcID = funcID,
                                  .funcArgs.streamChange = { streamID,
                                                             vlistID } };
        header.funcCall = f;
      }
      break;
    case STREAMOPEN:
      {
        xassert ( argc == 2 );
        filename = va_arg(ap, char *);
        filenamesz = strlen(filename);
        xassert ( filenamesz > 0 &&
                  filenamesz < MAXDATAFILENAME );
        filetype = va_arg(ap, int);
        struct funcCallDesc f = { .funcID = STREAMOPEN,
                                  .funcArgs.newFile
                                  = { .fnamelen = filenamesz,
                                      .filetype = filetype } };
        header.funcCall = f;
        ++filenamesz;
      }
      break;
    default:
      xabort("FUNCTION NOT MAPPED!");
    }

  for (collID = 0; collID < nProcsColl; ++collID)
    {
      xassert((txWin[collID].dictSize - txWin[collID].dictRPCUsed)
              > txWin[collID].dictDataUsed);
      if (txWin[collID].postSet)
        {
          xmpi(MPI_Win_wait(txWin[collID].win));
          txWin[collID].postSet = 0;
          modelWinFlushBuffer ( collID );
        }
      xassert(txWin[collID].refuseFuncCall == 0);
      modelWinEnqueue(collID, header, filename, filenamesz);
    }

  switch ( funcID )
    {
    case STREAMCLOSE:
      xdebug("WROTE FUNCTION CALL IN BUFFER OF WINS:  %s, streamID=%d",
             funcMap[(-1 - funcID)], streamID);
      break;
    case STREAMDEFVLIST:
      xdebug("WROTE FUNCTION CALL IN BUFFER OF WINS:  %s, streamID=%d,"
             " vlistID=%d", funcMap[(-1 - funcID)], streamID, vlistID);
      break;
    case STREAMOPEN:
      xdebug("WROTE FUNCTION CALL IN BUFFER OF WINS:  %s, filenamesz=%zu,"
             " filename=%s, filetype=%d",
             funcMap[(-1 - funcID)], filenamesz, filename, filetype);
      break;
    }

  va_end(ap);
  xdebug("%s", "RETURN");
}

#endif

/*****************************************************************************/

int pioInqVarDecoChunk ( int vlistID, int varID )
{
#ifdef USE_MPI
   int rankModel = commInqRankModel ();
   xassert ( rankModel != CDI_UNDEFID );
   return vlistInqVarDecoChunk ( vlistID, varID, rankModel );
#endif
   return vlistInqVarDecoChunk ( vlistID, varID, CDI_UNDEFID );
}

/*****************************************************************************/

int pioInqVarDecoOff ( int vlistID, int varID )
{
#ifdef USE_MPI
   int rankModel = commInqRankModel ();
   xassert ( rankModel != CDI_UNDEFID );
   return vlistInqVarDecoOff ( vlistID, varID, rankModel );
#else
   return vlistInqVarDecoOff ( vlistID, varID, CDI_UNDEFID );
#endif
}

/*****************************************************************************/
/**
   @brief initializes the MPI_Communicators needed for the
  communication between the calculator PEs and the I/O PEs and within the
  group of I/O PEs.

  commGlob: all PEs

  commPIO: I/O PEs, PEs with highest ranks in commGlob

  commModel: calculating PEs, no I/O PEs

  commsIO[i]:

  Collective call

  @param comm MPI_Communicator of all calling PEs
  @param nIOP number of I/O PEs
  @return int indicating wether the calling PE is a calcutator (1) or not (0)
*/

#ifdef USE_MPI
MPI_Comm pioInit_c ( MPI_Comm commGlob, int nProcsIO, int IOMode,
                     int nNamespaces, int * hasLocalFile )
{
  int sizeGlob;

  if ( IOMode < PIO_MINIOMODE || IOMode > PIO_MAXIOMODE )
    xabort ( "IOMODE IS NOT VALID." );

#ifdef _SX
  if ( IOMode ==  PIO_ASYNCH )
    xabort ( "PIO_ASYNCH DOES NOT WORK ON SX." );
#endif

  commInit ();
  commDefCommGlob ( commGlob );
  sizeGlob = commInqSizeGlob ();

  if ( nProcsIO <= 0 || nProcsIO > sizeGlob - 1 )
    xabort("DISTRIBUTION OF TASKS ON PROCS IS NOT VALID.\n"
           "nProcsIO=%d, sizeGlob=%d\n", nProcsIO, sizeGlob);

  commDefNProcsIO ( nProcsIO );
  commDefIOMode   ( IOMode, PIO_MAXIOMODE, PIO_MINIOMODEWITHSPECIALPROCS );
  commDefCommPio  ();

  // JUST FOR TEST CASES WITH ONLY ONE MPI TASK
  if ( commInqSizeGlob () == 1 )
    {
      namespaceInit ( nNamespaces, hasLocalFile );
      return commInqCommGlob ();
    }

  if ( commInqIsProcIO ())
    {
      IOServer ();
      commDestroy ();
      MPI_Finalize ();
      exit ( EXIT_SUCCESS );
    }
  else
    {
      commEvalPhysNodes ();
      commDefCommsIO ();
      namespaceInit ( nNamespaces, hasLocalFile );
    }

  xdebug ( "nProcsGlob=%d, RETURN", sizeGlob );
  return commInqCommModel ();
}
#endif

/*****************************************************************************/

int pioInit ( int commGlobArg, int nProcsIO, int IOMode, int nNamespaces,
              int * hasLocalFile )
{
#ifdef USE_MPI
  xdebug("START: %s, nProcsIO=%d, IOMode=%d, nNamespaces=%d",
         "cdi parallel",
         nProcsIO, IOMode, nNamespaces );
#else
  xdebug("START: %s, nProcsIO=%d, IOMode=%d, nNamespaces=%d",
         "cdi serial",
         nProcsIO, IOMode, nNamespaces );
#endif

#ifdef USE_MPI
  MPI_Comm commGlob = MPI_Comm_f2c((MPI_Fint)commGlobArg);
  xassert ( commGlob != MPI_COMM_NULL );

  return MPI_Comm_c2f ( pioInit_c ( commGlob, nProcsIO, IOMode, nNamespaces,
                                    hasLocalFile ));
#endif
  xdebug("%s", "RETURN" );
  return 0;
}

/************************************************************************/

void  pioEndDef ( void )
{
#ifdef USE_MPI
  char   * buffer;
  int bufferSize;
  int rankGlob = commInqRankGlob ();

  xdebug("%s", "START");

  varsMapNDeco ( commInqNNodes (), commInqNodeSizes ());

  reshListPrint ( "reshListModel" );
  
  if ( rankGlob < commInqNProcsColl ())
    {
      reshPackBufferCreate ( &buffer, &bufferSize, commInqCommsIO ( rankGlob ));

      xmpi ( MPI_Send ( buffer, bufferSize, MPI_PACKED, commInqNProcsModel (),
                        RESOURCES, commInqCommsIO ( rankGlob )));

      xdebug("%s", "SENT MESSAGE WITH TAG \"RESOURCES\"");

      reshPackBufferDestroy ( &buffer );
    }

  modelWinCreate ();
  namespaceDefResStatus ( STAGE_TIMELOOP );
  xdebug("%s", "RETURN");
#endif
}

/************************************************************************/

void  pioEndTimestepping ( void )
{
#ifdef USE_MPI
  xdebug("%s", "START");
  namespaceDefResStatus ( STAGE_CLEANUP );
  xdebug("%s", "RETURN");
#endif
}


/****************************************************/


/**
  @brief is invoked by the calculator PEs, to inform
  the I/O PEs that no more data will be written.

  @param

  @return
*/

void pioFinalize ( void )
{
#ifdef USE_MPI
  int collID, ibuffer = 1111;
  xdebug("%s", "START");
  namespaceCleanup ();
  for ( collID = 0; collID < commInqNProcsColl (); collID++ )
    {
      xmpi ( MPI_Send ( &ibuffer, 1, MPI_INT, commInqNProcsModel (),
                        FINALIZE, commInqCommsIO ( collID )));
      xdebug("%s", "SENT MESSAGE WITH TAG \"FINALIZE\"");
    }
  modelWinCleanup ();
  commDestroy ();
  xdebug("%s", "RETURN");
#endif
}

 /************************************************************************/

void pioWriteTimestep ( int tsID, int vdate, int vtime )
{
#ifdef USE_MPI
  int collID, buffer[timestepSize], iAssert = 0;
  /* int tokenEnd = END; */
  int rankGlob = commInqRankGlob ();
  int nProcsColl = commInqNProcsColl ();
  int nProcsModel = commInqNProcsModel ();

  xdebug("%s", "START");

  xassert ( tsID       >= 0     &&
            vdate      >= 0     &&
            vtime      >= 0     &&
            txWin != NULL);

  buffer[0] = tsID;
  buffer[1] = vdate;
  buffer[2] = vtime;

  if ( rankGlob < nProcsColl )
    {
      xmpi ( MPI_Send ( &buffer[0], timestepSize, MPI_INTEGER, nProcsModel,
                        WRITETS, commInqCommsIO ( rankGlob )));
      xdebug("%s", "SENT MESSAGE WITH TAG \"WRITETS\"");
    }

  for ( collID = 0; collID < nProcsColl; collID++ )
    {
      if (txWin[collID].postSet)
        {
          xmpi(MPI_Win_wait(txWin[collID].win));
          txWin[collID].postSet = 0;
          modelWinFlushBuffer ( collID );
        }
      /*
       * modelWinBufferPutAtEnd ( __func__, collID, &tokenEnd,
       *                          sizeof ( tokenEnd ));
       */
      union winHeaderEntry header
        = { .headerSize = { .sizeID = HEADERSIZEMARKER,
                            .numDataEntries = txWin[collID].dictDataUsed,
                            .numRPCEntries = txWin[collID].dictRPCUsed } };
      union winHeaderEntry *winDict
        = (union winHeaderEntry *)txWin[collID].buffer;
      winDict[0] = header;

      xmpi(MPI_Win_post(txWin[collID].ioGroup, iAssert, txWin[collID].win));
      txWin[collID].postSet = 1;
    }

  xdebug ( "RETURN. messages sent, windows posted: tsID=%d, vdate=%d, vtime=%d", 
           tsID, vdate, vtime );

#endif
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
