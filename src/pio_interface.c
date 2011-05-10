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

#endif

#define xprintArray(ps,array,n,datatype) printArray ( ps, array, n, datatype, \
						      __func__, __FILE__, __LINE__ )

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

  MPI_Comm_size ( commGlob, &sizeGlob );

  nProcsCalc = sizeGlob - nIOP;

  MPI_Comm_rank ( commGlob, &rankGlob );
  MPI_Comm_set_name ( commGlob, "commGlob" );
  
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
      namespaceCleanup ();
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

void printArray ( char * ps, void * array, int n, int datatype, 
		  const char * funname, const char * filename, int line )
{ 
  int i;
  size_t len;
  char as[1024];
  int * iArray;
  double * dArray;

#ifdef USE_MPI
  fprintf ( stdout, "pe%d in %s, %s, line %d: %s = ",
	    rankGlob, funname, filename, line, ps );
#else 
  fprintf ( stdout, "%s, %s, line %d: %s = ",
	    funname, filename, line, ps );
#endif

  switch ( datatype )
    {
    case DATATYPE_INT:
      iArray = ( int * ) array;
      for ( i = 0; i < n-1; i++ )
	fprintf ( stdout, "%d ", * ( iArray + i ));
      fprintf ( stdout, "%d\n", * ( iArray + n - 1 ));
      break;
    case DATATYPE_FLT:
      dArray = ( double * ) array;
      for ( i = 0; i < n-1; i++ )
	fprintf ( stdout, "%.2f ", * ( dArray + i ));
      fprintf ( stdout, "%.2f\n", * ( dArray + n-1 ));
      break;
    default:
      fprintf ( stdout, " ... no datatype defined\n" );
    } 
      
  return;
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
  int writerIdx;
  int i, j;
  char text[1024], * cp;
  
  if ( ddebug >= 1 ) xdebug ();
  

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
      * ( problemMapping + dummy[i] ) = writerIdx;
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

  return;
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
  
  if ( ddebug >= 1 ) xdebug ();
  

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

  if ( ddebug > 1 )
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


void  pioMetadata ()
{
#ifdef USE_MPI
  int metadataBuffer = 1299;

  size_t buffSize;
  char * buff;

  buffSize = reshGetSendSize ();

  buff = xcalloc ( 1, buffSize );

  reshGetSendData ( buffSize, buff );

  if ( rankGlob < nProcsIO )
    MPI_Send ( &metadataBuffer, 1, MPI_INT, nProcsCalc, 
	       METADATA, * ( commsIO + rankGlob ));

  if ( ddebug ) xdebugText ( "send message metadata" );

  if ( buff ) free ( buff );
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
  
  if ( ddebug >= 1 ) xdebug ();
  
  for ( i = 0; i < nProcsIO; i++ )
    MPI_Send ( &ibuffer, 1, MPI_INT, nProcsCalc, FINALIZE, *( commsIO + i ));

  commsIOCleanup ();

  namespaceCleanup ();
#endif
}

/************************************************************************/
