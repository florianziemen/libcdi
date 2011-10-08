#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include "limits.h" 
#include "cdi.h"
#include "pio_util.h"

#ifdef USE_MPI
#include "pio_interface.h"
#include "pio_comm.h"
#include "pio_rpc.h"
#include "pio_server.h"
#include "namespace.h"
#include "resource_handle.h"
#include "stream_int.h"
#include "vlist.h"
extern resOps streamOps;
#endif

int       * winBufferSize = NULL;
double   ** winBuffer     = NULL;
double   ** winBufferHead = NULL;
MPI_Win   * win           = NULL;
int       * winPostSet    = NULL;
int       * winFirstWrite = NULL;
MPI_Group * groupsIO      = NULL;
MPI_Group * groupsIONetto = NULL;

/*****************************************************************************/


int pioInit ( int comm, int nIOP, int mode )
{
#ifdef USE_MPI
#endif
  return 0;
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
MPI_Comm pioInit_c ( MPI_Comm commGlob, int nIOP, int IOModus )
{
  if ( IOModus < PIO_NONE || IOModus > PIO_MAX_IOMODUS )
    xabort ( "PIOTYPE is no valid modus" );
  
#ifdef _SX
  if ( IOModus ==  PIO_POSIX_ASYNCH )
    xabort ( "PIO_POSIX_ASYNCH does not work on SX" );
#endif

  commInit ();
  commDefCommGlob ( commGlob );
  commDefNProcsIO ( nIOP );
  commDefIOModus  ( IOModus );
  commDefCommPio ();

  if ( commInqIsProcIO ()) 
    {
      IOServer ();
      commDestroy ();
      MPI_Finalize ();
      exit ( 0 );
    }
  else
    { 
      commEvalPhysNodes ();
      commDefCommsIO ();        
    }
  
  return commInqCommModel ();
}
#endif

/************************************************************************/

int cmp ( const void * va, const void * vb )
{
  const int ** a, ** b;
 
  a = ( const int ** ) va;
  b = ( const int ** ) vb;
  
  return (( **a < **b ) - ( **a > **b ));
}

/****************************************************/
 
void mapProblems ( int problemSizes[], int * problemMapping, int nProblems, 
		   int nWriter, double * w )
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

#ifdef USE_MPI   
void  varMapGen ( int * vSizes, int * sSizes, int * varMapping, 
		     int nStreams, int * nodeSizes, int nNodes )
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
  int nProcsModel = commInqNProcsModel ();

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
	      * ( varMappingNode + offsetN ++ ) +  nProcsModel + summandRank;
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
	buckets[*(varMapping + i ) - nProcsModel] += * ( vSizes + i );
      xprintArray ( "buckets", buckets, nProcsColl, DATATYPE_INT );
    }
}
#endif

/************************************************************************/ 

#ifdef USE_MPI
void defVarDeco ( int vlistID, int varID )
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
#endif

/************************************************************************/


static
void varsMapNDeco ( int nNodes, int * nodeSizes )
{
  int nStreams, nVars, * resHs, * streamSizes, * varSizes, * varMapping;
  int i, j, k = 0;

#ifdef USE_MPI

  nStreams = streamSize ();

  resHs       = xmalloc ( nStreams * sizeof ( int ));
  streamSizes = xmalloc ( nStreams * sizeof ( int ));
  streamGetIndexArray ( nStreams, resHs );

  for ( i = 0; i < nStreams; i++ )
    streamSizes[i] = streamInqNvars ( * ( resHs + i )); 

  nVars = xsum ( nStreams, streamSizes );
  varSizes   = xmalloc ( nVars * sizeof ( int ));
  varMapping = xmalloc ( nVars * sizeof ( int ));

  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < * ( streamSizes + i ); j++ )
      varSizes[k++] += vlistInqVarSize ( streamInqVlist ( * resHs + i ), j );

  assert ( k == nVars );

  varMapGen ( varSizes, streamSizes, varMapping, 
	      nStreams, nodeSizes, nNodes );

  k = 0;
  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < * ( streamSizes + i ); j++ )
      {
        defVarDeco ( streamInqVlist ( * resHs + i ), j );
        defVarDeco ( streamInqVlistIDorig ( * resHs + i ), j );
	vlistDefVarIOrank ( streamInqVlist ( * resHs + i ), j, 
			  * ( varMapping + k ));
	vlistDefVarIOrank ( streamInqVlistIDorig ( * resHs + i ), j, 
			  * ( varMapping + k++ ));
      }

  if ( varMapping )  free ( varMapping );
  if ( varSizes )    free ( varSizes );
  if ( streamSizes ) free ( streamSizes );
  if ( resHs )       free ( resHs );
  
#endif
}

/************************************************************************/

static 
void modelWinCleanup ( void )
{
#ifdef USE_MPI
  int IOID;

  for ( IOID = 0; IOID < commInqNProcsColl (); IOID ++ )
    {
      if ( winPostSet[IOID] )
        xmpi ( MPI_Win_wait ( win[IOID] )); 
      xmpi ( MPI_Win_free ( &win[IOID] ));
      xmpi ( MPI_Free_mem ( winBuffer[IOID] ));
      xmpi ( MPI_Group_free ( &groupsIONetto[IOID] ));
    }

  if ( groupsIONetto )   free ( groupsIONetto );
  if ( winFirstWrite )   free ( winFirstWrite );
  if ( winPostSet )      free ( winPostSet );
  if ( win )             free ( win );
  if ( winBufferHead )   free ( winBufferHead );
  if ( winBuffer )       free ( winBuffer );
  if ( winBufferSize )   free ( winBufferSize );

  xdebug ( "cleaned up mpi_win's" );
#endif
}

/************************************************************************/

static
  void getWinBufferSizes ( void )
{
#ifdef USE_MPI
  
  int IOID, nstreams, * streamIndexArray, streamNo, vlistID, nvars, varID;
  int IOIDchunk = 0;
  int rankGlob = commInqRankGlob ();
  int nProcsColl = commInqNProcsColl ();
  int nProcsModel = commInqNProcsModel ();
  
  nstreams = reshCountType ( &streamOps );
  streamIndexArray = xmalloc ( nstreams * sizeof ( int ));
  reshGetResHArrayOfType ( nstreams, streamIndexArray, &streamOps );
  for ( streamNo = 0; streamNo < nstreams; streamNo++ )
    {
      vlistID = streamInqVlist ( streamIndexArray[streamNo] );
      nvars = vlistNvars ( vlistID );
      for ( varID = 0; varID < nvars; varID++ )
        {
          IOID = CDI_UNDEFID;
          IOID = vlistInqVarIOrank ( vlistID, varID ) - nProcsModel;
          IOIDchunk = vlistInqVarDecoChunk ( vlistID, varID, rankGlob );
          assert ( IOID != CDI_UNDEFID &&
                   IOIDchunk > 0 );
          winBufferSize[IOID] += (( IOIDchunk + winBufferOverheadChunk ) * 
                                  sizeof ( double ));
        }
    }
  free ( streamIndexArray );
  
  for ( IOID = 0; IOID < nProcsColl; IOID++ ) 
    {
      winBufferSize[IOID] += ( winBufferOverhead * sizeof ( double )); 
      assert ( winBufferSize[IOID] < winBufferSizeMax );
    }
  xprintArray ( "winBufferSize", winBufferSize, nProcsColl, DATATYPE_INT );

  return;
#endif
}

/************************************************************************/


static
void modelWinCreate ( void )
{
#ifdef USE_MPI

  int IOID, ranks[1];
  int nProcsColl = commInqNProcsColl ();
  int nProcsModel = commInqNProcsModel ();

  winBufferSize   = xmalloc ( nProcsColl * sizeof ( winBufferSize[0] ));
  winBuffer       = xmalloc ( nProcsColl * sizeof ( winBuffer[0] ));
  winBufferHead   = xmalloc ( nProcsColl * sizeof ( winBufferHead[0] ));
  win             = xmalloc ( nProcsColl * sizeof ( win[0] ));
  winPostSet      = xmalloc ( nProcsColl * sizeof ( winPostSet[0] ));
  winFirstWrite   = xmalloc ( nProcsColl * sizeof ( winFirstWrite[0] ));
  groupsIONetto   = xmalloc ( nProcsColl * sizeof ( groupsIONetto[0] ));

  getWinBufferSizes ();
  ranks[0] = commInqNProcsModel ();

  for ( IOID = 0; IOID < nProcsColl; IOID ++ )
    {
      assert ( winBufferSize[IOID] > 0 );
      winBuffer[IOID] = NULL;
      xmpi ( MPI_Alloc_mem (( MPI_Aint )( winBufferSize[IOID] * 
                                          sizeof ( double )), 
                            MPI_INFO_NULL, &winBuffer[IOID] ));
      assert ( winBuffer[IOID] != NULL ); 
      winBufferHead[IOID] = winBuffer[IOID];
      winFirstWrite[IOID] = 1;
      xmpi ( MPI_Win_create ( winBuffer[IOID], 
                              ( MPI_Aint ) winBufferSize[IOID] * 
                              sizeof ( double ), sizeof ( double ),
                              MPI_INFO_NULL, commInqCommsIO ( IOID ), 
                              &win[IOID] )); 
      xmpi ( MPI_Comm_group ( commInqCommsIO ( IOID ), &groupsIONetto[IOID] ));
      xmpi ( MPI_Group_incl ( groupsIONetto[IOID], 1, ranks, 
                              &groupsIONetto[IOID] ));
    }
  xdebug ( "created mpi_win's" );

#endif
}

/************************************************************************/

void pioBuffer ( int streamID, int varID, const double *data, int nmiss )
{
  int size = sizeof ( double ), chunk, vlistID, IOID = CDI_UNDEFID;
  double temp, sep = ( double ) SEPARATOR;
  int rankGlob = commInqRankGlob ();
  
  vlistID = streamInqVlist ( streamID );
  IOID    = vlistInqVarIOrank ( vlistID, varID ) - commInqNProcsModel ();
  chunk   = vlistInqVarDecoChunk ( vlistID, varID, rankGlob );
  
  assert ( IOID >= 0 &&
           IOID < commInqNProcsIO () &&
           chunk >= 0 );
  
  xdebug3 ("streamID=%d, vlistID=%d, varID=%d, IOID=%d, "
          "chunk=%d, data=%.2f, nmiss=%d",
          streamID, vlistID, varID, IOID, chunk, data[0], nmiss );

  if ( winPostSet[IOID] ) 
    { 
      xmpi ( MPI_Win_wait ( win[IOID] )); 
      winPostSet[IOID] = 0;
    }
  
  temp = ( double ) streamID;
  memcpy ( winBufferHead[IOID], &temp, size);
  winBufferHead[IOID]++;
  temp = ( double ) varID;
  memcpy ( winBufferHead[IOID], &temp, size);
  winBufferHead[IOID]++;
  size *= chunk;
  memcpy ( winBufferHead[IOID], data, size);
  winBufferHead[IOID] += chunk;
  size = sizeof ( double );
  memcpy ( winBufferHead[IOID], &sep, size);
  winBufferHead[IOID]++;      
}

/************************************************************************/

void  pioEndDef ( void )
{
#ifdef USE_MPI
  char   * buffer;
  int bufferSize;
  int rankGlob = commInqRankGlob ();

  varsMapNDeco ( commInqNNodes (), commInqNodeSizes ());

  if ( rankGlob < commInqNProcsColl ())
    {
      reshPackBufferCreate ( &buffer, &bufferSize, commInqCommsIO ( rankGlob ));
      
      xmpi ( MPI_Send ( buffer, bufferSize, MPI_PACKED, commInqNProcsModel (), 
                        RESOURCES, commInqCommsIO ( rankGlob )));
      
      xdebug ( "sent message metadata" );
      
      reshPackBufferDestroy ( &buffer );
    }

  modelWinCreate ();
#endif
}


/****************************************************/


/**
  @brief is invoked by the calculator PEs, to inform
  the I/O PEs that no more data will be written.

  @param

  @return
*/

void pioFinalize ()
{
#ifdef USE_MPI
  int i, ibuffer = 1111;
  
  for ( i = 0; i < commInqNProcsColl (); i++ )
    {
      xmpi ( MPI_Send ( &ibuffer, 1, MPI_INT, commInqNProcsModel (), 
                        FINALIZE, commInqCommsIO ( i )));
      xdebug ( "sent message FINALIZE" );
    }

  modelWinCleanup ();
  commDestroy ();
#endif
} 
 
 /************************************************************************/
 
void pioWriteTimestep ( int ts, int vdate, int vtime )
{
#ifdef USE_MPI
  int IOID, buffer[timestepSize], iAssert = 0;
  double end = ( double ) END, size = sizeof ( double );
  int rankGlob = commInqRankGlob ();
  int nProcsColl = commInqNProcsColl ();
  int nProcsModel = commInqNProcsModel ();

  assert (    ts >= 0 &&
           vdate >= 0 &&
           vtime >= 0 );

  buffer[0] = ts;
  buffer[1] = vdate;
  buffer[2] = vtime;

  for ( IOID = 0; IOID < nProcsColl; IOID++ )
    {
      memcpy ( winBufferHead[IOID], &end, size);
      winBufferHead[IOID] ++; 
      xmpi ( MPI_Win_post ( groupsIONetto[IOID], iAssert, win[IOID] ));
      winPostSet[IOID] = 1;
      winBufferHead[IOID] = winBuffer[IOID];
      winFirstWrite[IOID] = 1;
    }
  
  if ( rankGlob < nProcsColl ) 
    {
      xmpi ( MPI_Send ( &buffer[0], timestepSize, MPI_INTEGER, nProcsModel, 
                        WRITETS, commInqCommsIO ( rankGlob )));
      xdebug ( "sent message WRITETS" );
    }
#endif
}


/************************************************************************/
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
