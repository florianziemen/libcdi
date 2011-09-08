/** @file ioUtil.c
*/

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


int pioType;

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
MPI_Comm pioInit_c ( MPI_Comm comm, int nIOP, int mode )
{
  MPI_Comm commModel;
  int isCalcPE;

  commGlob = comm;
  pioType  = mode;

  xmpi ( MPI_Comm_size ( commGlob, &sizeGlob ));

  nProcsCalc = sizeGlob - nIOP;

  xmpi ( MPI_Comm_rank ( commGlob, &rankGlob ));
  xmpi ( MPI_Comm_set_name ( commGlob, "commGlob" ));
  
  if ( ddebug >= 1 ) xdebugComm (  &commGlob );
  
  if ( rankGlob >= nProcsCalc )
    isCalcPE = 0;
  else 
    isCalcPE = 1;

  xmpi ( MPI_Comm_split ( comm, isCalcPE, 0, &commModel ));

  commsIOCreate ( comm, nIOP );

  if ( !isCalcPE ) 
    {
      IOServer ( commModel, mode );
      commsIOCleanup ();
      MPI_Finalize ();
      exit ( 0 );
    }
  
  return commModel;
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
  
  if ( ddebug > 2 )
  {
      xprintArray (  "problemSizes = ", problemSizes, nProblems, DATATYPE_INT );     
      xprintArray ( "vector of indices, qsort of problemSizes", dummy, 
		   nProblems, DATATYPE_INT ); 
      xprintArray ( "problemMapping", problemMapping, nProblems, DATATYPE_INT );
      xprintArray ( "meanBucket", meanBucket, nWriter, DATATYPE_FLT );
      xprintArray ( "actual buckets", buckets, nWriter, DATATYPE_INT );     
  }  
  
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

  int buckets[nProcsIO];

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
	      * ( varMappingNode + offsetN ++ ) +  nProcsCalc + summandRank;
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

      for ( i = 0; i < nProcsIO; i++ )
	buckets[i] = 0;
      for ( i = 0; i < nVars; i ++ )
	buckets[*(varMapping + i ) - nProcsCalc] += * ( vSizes + i );
      xprintArray ( "buckets", buckets, nProcsIO, DATATYPE_INT );
    }
  
}
#endif

/************************************************************************/ 

#ifdef USE_MPI
void defVarDeco ( int vlistID, int varID )
{
  int varSize, cRank, lChunk, rem, lOffset;
  deco_t deco[nProcsCalc];
 
  varSize = vlistInqVarSize ( vlistID, varID );

  for ( cRank = 0; cRank < nProcsCalc; cRank++ )
    {
      lChunk = varSize / nProcsCalc;
      lOffset = cRank * lChunk;
      rem = varSize % nProcsCalc;
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

  vlistDefVarDeco ( vlistID, varID, nProcsCalc, &deco[0] );
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

  for ( IOID = 0; IOID < nProcsIO; IOID ++ )
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
          IOID = vlistInqVarIOrank ( vlistID, varID ) - nProcsCalc;
          IOIDchunk = vlistInqVarDecoChunk ( vlistID, varID, rankGlob );
          assert ( IOID != CDI_UNDEFID &&
                   IOIDchunk > 0 );
          winBufferSize[IOID] += (( IOIDchunk + winBufferOverheadChunk ) * 
                                  sizeof ( double ));
        }
    }
  free ( streamIndexArray );
  
  for ( IOID = 0; IOID < nProcsIO; IOID++ ) 
    {
      winBufferSize[IOID] += ( winBufferOverhead * sizeof ( double )); 
      assert ( winBufferSize[IOID] < winBufferSizeMax );
    }
  xprintArray ( "winBufferSize", winBufferSize, nProcsIO, DATATYPE_INT );

  return;
#endif
}

/************************************************************************/

static
  void TEMPgetWinBufferSizes ( void )
{
  int IOID;

  for ( IOID = 0; IOID < nProcsIO; IOID++ ) 
    winBufferSize[IOID] = 1; 
  xprintArray ( "winBufferSize TEMP", winBufferSize, nProcsIO, DATATYPE_INT );

  return;
}

/************************************************************************/


static
void modelWinCreate ( void )
{
#ifdef USE_MPI

  int IOID, ranks[1];

  winBufferSize   = xmalloc ( nProcsIO * sizeof ( winBufferSize[0] ));
  winBuffer       = xmalloc ( nProcsIO * sizeof ( winBuffer[0] ));
  winBufferHead   = xmalloc ( nProcsIO * sizeof ( winBufferHead[0] ));
  win             = xmalloc ( nProcsIO * sizeof ( win[0] ));
  winPostSet      = xmalloc ( nProcsIO * sizeof ( winPostSet[0] ));
  winFirstWrite   = xmalloc ( nProcsIO * sizeof ( winFirstWrite[0] ));
  groupsIONetto   = xmalloc ( nProcsIO * sizeof ( groupsIONetto[0] ));

  getWinBufferSizes ();
  ranks[0] = nProcsCalc;

  for ( IOID = 0; IOID < nProcsIO; IOID ++ )
    {
      assert ( winBufferSize[IOID] > 0 );
      winBuffer[IOID] = NULL;
      xmpi ( MPI_Alloc_mem (( MPI_Aint ) ( winBufferSize[IOID] * sizeof ( double )), 
                           MPI_INFO_NULL, &winBuffer[IOID] ));
      assert ( winBuffer[IOID] != NULL ); 
      winBufferHead[IOID] = winBuffer[IOID];
      winFirstWrite[IOID] = 1;
      xmpi ( MPI_Win_create ( winBuffer[IOID], 
                              ( MPI_Aint ) winBufferSize[IOID] * sizeof ( double ),
                              sizeof ( double ),
                              MPI_INFO_NULL, commsIO[IOID], &win[IOID] )); 
      xmpi ( MPI_Comm_group ( commsIO[IOID], &groupsIONetto[IOID] ));
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
  double temp, start = ( double ) START, sep = ( double ) SEPARATOR;

  
  vlistID = streamInqVlist ( streamID );
  IOID    = vlistInqVarIOrank ( vlistID, varID ) - nProcsCalc;
  chunk   = vlistInqVarDecoChunk ( vlistID, varID, rankGlob );
  
  assert ( IOID >= 0 &&
           IOID < nProcsIO &&
           chunk >= 0 );
  
  xdebug ("streamID=%d, vlistID=%d, varID=%d, IOID=%d, "
          "chunk=%d, data=%.2f, nmiss=%d",
          streamID, vlistID, varID, IOID, chunk, data[0], nmiss );

  if ( winPostSet[IOID] ) 
    { 
      xmpi ( MPI_Win_wait ( win[IOID] )); 
      winPostSet[IOID] = 0;
    }

  if ( winFirstWrite[IOID] )
    {
      memcpy ( winBufferHead[IOID], &start, size);
      winBufferHead[IOID] ++; 
      winFirstWrite[IOID] = 0;
    }
  
  temp = ( double ) streamID;
  memcpy ( winBufferHead[IOID], &temp, size);
  winBufferHead[IOID] ++;
  temp = ( double ) varID;
  memcpy ( winBufferHead[IOID], &temp, size);
  winBufferHead[IOID] ++;
  size *= chunk;
  memcpy ( winBufferHead[IOID], data, size);
  winBufferHead[IOID] += chunk;
  size = sizeof ( double );
  memcpy ( winBufferHead[IOID], &sep, size);
  winBufferHead[IOID] ++;      
}

/************************************************************************/

void  pioMetadata ( int nNodes, int * nodeSizes )
{
#ifdef USE_MPI
  char   * buffer;
  int bufferSize;

  varsMapNDeco ( nNodes, nodeSizes );

  if ( rankGlob < nProcsIO )
    {
      reshPackBufferCreate ( &buffer, &bufferSize, commsIO[rankGlob] );
      
      xmpi ( MPI_Send ( buffer, bufferSize, MPI_PACKED, nProcsCalc, 
                        METADATA, * ( commsIO + rankGlob )));
      
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
  
  for ( i = 0; i < nProcsIO; i++ )
    {
      xmpi ( MPI_Send ( &ibuffer, 1, MPI_INT, nProcsCalc, 
                      FINALIZE, *( commsIO + i )));
      xdebug ( "sent message FINALIZE" );
    }

  modelWinCleanup ();
  commsIOCleanup ();
#endif
} 
 
 /************************************************************************/
 
void pioWriteTimestep ( int ts, int vdate, int vtime )
{
#ifdef USE_MPI
  int IOID, buffer[timestepSize], iAssert = 0;
  double end = ( double ) END, size = sizeof ( double );

  assert (    ts >= 0 &&
           vdate >= 0 &&
           vtime >= 0 );

  buffer[0] = ts;
  buffer[1] = vdate;
  buffer[2] = vtime;

  for ( IOID = 0; IOID < nProcsIO; IOID++ )
    {
      memcpy ( winBufferHead[IOID], &end, size);
      winBufferHead[IOID] ++; 
      xmpi ( MPI_Win_post ( groupsIONetto[IOID], iAssert, win[IOID] ));
      winPostSet[IOID] = 1;
      winBufferHead[IOID] = winBuffer[IOID];
      winFirstWrite[IOID] = 1;
    }
  
  if ( rankGlob < nProcsIO ) 
    {  
      xmpi ( MPI_Send ( &buffer[0], timestepSize, MPI_INTEGER, nProcsCalc, 
                      WRITETS, * ( commsIO + rankGlob )));
      xdebug ( "sent message WRITETS" );
    }
#endif
}


/************************************************************************/


void pioWriteVar ()
{
#ifdef USE_MPI
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
