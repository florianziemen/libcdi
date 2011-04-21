/** @file ioUtil.c
*/


#include "pio_interface.h"

#include <stdlib.h>
#include <stdio.h> 
#include "limits.h"
#include "pio_util.h"
#include "pio_rpc.h"
#include "pio_server.h"
#include "cdi.h"


/*****************************************************************************/


int pioInit ( int comm, int nIOP, int mode )
{
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

MPI_Comm pioInit_c ( MPI_Comm comm, int nIOP, int mode )
{
  char fnName[] = "pioInit_c()";
  MPI_Comm commModel;
  int isCalcPE;

  commGlob = comm;

  MPI_Comm_size ( commGlob, &sizeGlob );

  nProcsCalc = sizeGlob - nIOP;

  MPI_Comm_rank ( commGlob, &rankGlob );
  MPI_Comm_set_name ( commGlob, "commGlob" );
  
  if ( ddebug >= 1 ) myDebugComm (  __FILE__, fnName,__LINE__ , &commGlob );
  
  if ( rankGlob >= nProcsCalc )
    isCalcPE = 0;
  else
    isCalcPE = 1;

  CHECK_MPI ( MPI_Comm_split ( comm, isCalcPE, 0, &commModel ), rankGlob );

  commsIOcreate ( comm, nIOP );

  if ( !isCalcPE ) 
    {
      IOServer ( commModel, mode );
      commsIOCleanup ();
      MPI_Finalize ();
      exit ( 0 );
    }
  
  return commModel;
}

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
  /*
  len = strlen ( ps );
  memcpy ( as, ps, len );
  as[len] = '\0';
  */

  fprintf ( stdout, "pe%d in %s, %s, line %d: %s = ",
	    rankGlob, funname, filename, line, ps );
  //fprintf ( stdout, "%s = ", as );

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
  char fnName[] = "mapProblems()";
  int *ip[nProblems];
  int dummy[nProblems];
  int buckets[nWriter];
  int currCapacity, nextCapacity;
  double meanBucket[nWriter];
  int sum = 0;
  int writerIdx;
  int i, j;
  char text[1024], * cp;
  
  if ( ddebug >= 1 )
    myDebug ( __FILE__, fnName, __LINE__ );
  

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
      printArray (  "problemSizes = ", problemSizes, nProblems, 
		    DATATYPE_INT, fnName, __FILE__, __LINE__ );
      
      printArray ( "vector of indices, qsort of problemSizes", dummy, 
		   nProblems, DATATYPE_INT, fnName, __FILE__, __LINE__ ); 
      printArray ( "problemMapping", problemMapping, nProblems, DATATYPE_INT,
		   fnName, __FILE__, __LINE__ );
      printArray ( "meanBucket", meanBucket, nWriter, DATATYPE_FLT,
		   fnName, __FILE__, __LINE__ );
      printArray ( "actual buckets", buckets, nWriter, DATATYPE_INT,
		   fnName, __FILE__, __LINE__ );
      
    }  

  return;
}

/****************************************************/ 

void  IOMetadata ()
{
  int metadataBuffer = 1299;

  if ( rankGlob < nProcsIO )
    MPI_Send ( &metadataBuffer, 1, MPI_INT, nProcsCalc, 
	       METADATA, * ( commsIO + rankGlob ));

  fprintf ( stdout, "pe%d send message metadata\n", rankGlob );

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
   
void  varMapGen ( int * vSizes, int * sSizes, int * varMapping, 
		     int nStreams, int * nodeSizes, int nNodes )
{
  char fnName[] = "varMapGen()";
  int weightsStreams[nStreams];
  int streamMapping[nStreams];
  int nPEs = 0, nVars = 0;
  int i, j, k, offset = 0, offsetN = 0;
  double * w;
  
  int * weightsVarsNode;
  int * varMappingNode;
  int nVarsNode, summandRank = 0;

  int buckets[nProcsIO];
  
  if ( ddebug >= 1 )
    myDebug ( __FILE__, fnName, __LINE__ );
  

  for ( i = 0; i < nStreams; i++ )
    {
      nVars += * ( sSizes + i );
      weightsStreams[i] = 0;
      for ( j = 0; j < * ( sSizes + i ); j++ )
	weightsStreams[i] += * ( vSizes + offset++ );
    }

  w = ( double * ) myMalloc ( nNodes * sizeof ( double ),
					 __FILE__, __LINE__ );

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
      
      weightsVarsNode = ( int * ) myMalloc ( nVarsNode * sizeof ( int ),
					     __FILE__, __LINE__ );
      
      varMappingNode = ( int * ) myMalloc ( nVarsNode * sizeof ( int ),
					     __FILE__, __LINE__ );

      w = ( double * ) myMalloc ( * ( nodeSizes + i ) * sizeof ( double ),
					 __FILE__, __LINE__ );
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

      mapProblems ( weightsVarsNode, varMappingNode, nVarsNode, * ( nodeSizes + i ), 
		    w );

      offset = 0;
      offsetN = 0;

      for ( j = 0; j < nStreams; j++ )
	if ( * ( streamMapping + j ) == i )
	  for ( k = 0; k < * ( sSizes + j ); k ++ )
	    * ( varMapping + offset ++ ) = * ( varMappingNode + offsetN ++ ) + nProcsCalc + summandRank;
	else
	  offset += * ( sSizes + j );

      summandRank += * ( nodeSizes + i );

      free ( w );
      free ( varMappingNode );
      free ( weightsVarsNode );
    }

  if ( ddebug > 1 )
    {
      printArray ( "varMapping", varMapping, nVars, DATATYPE_INT,
		   fnName, __FILE__, __LINE__  );

      for ( i = 0; i < nProcsIO; i++ )
	buckets[i] = 0;
      for ( i = 0; i < nVars; i ++ )
	buckets[*(varMapping + i ) - nProcsCalc] += * ( vSizes + i );
      printArray ( "buckets", buckets, nProcsIO, DATATYPE_INT, 
		   fnName, __FILE__, __LINE__  );
    }

  IOMetadata ();

  return;
}

/************************************************************************/

/**
  @brief is invoked by the calculator PEs, to inform
  the I/O PEs that no more data will be written.

  @param

  @return
*/

void pioFinalize ()
{
  char fnName[] = "pioFinalize()";
  int i, ibuffer = 1111;
  
  if ( ddebug >= 1 )
    myDebug ( __FILE__, fnName, __LINE__ );
  
  for ( i = 0; i < nProcsIO; i++ )
    MPI_Send ( &ibuffer, 1, MPI_INT, nProcsCalc, FINALIZE, *( commsIO + i ));

  commsIOCleanup ();
 
  return;
}

/************************************************************************/
