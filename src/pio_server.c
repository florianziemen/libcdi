/** @file ioServer.c
*/
#include "pio_server.h"


#include <stdlib.h>
#include <stdio.h>
#include "limits.h"
#include "cdi.h"
#include "pio_util.h"


extern MPI_Comm * commGlob;
extern int sizeGlob;
extern int rankGlob;
extern int nProcsIO;
extern int nProcsCalc;
extern MPI_Comm * commsIO;
extern MPI_Comm * commCalc; 
extern int isCalcPE;  



/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ()
{
  char fnName[] = "IOServer()";
  int source, tag, iBuffer;
  MPI_Status status;
  static int nfinished;
  void * vp;
  double * dp;
  char * outtext;
  size_t len, len2;
  
  if ( ddebug >= 1 )
    myDebugComm ( __FILE__, fnName, __LINE__, commCalc );
     
  nfinished = 0;
  outtext = ( char * ) myMalloc ( MAXSTRING * sizeof ( char ), __FILE__, __LINE__ );

  for ( ;; )
    {  
      fprintf ( stdout, "pe%d before mpi_probe()\n", rankGlob );
      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, * commCalc, &status );
      
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      
      switch ( tag )
	{
	case FINALIZE:      
	  MPI_Recv ( &iBuffer, 1, MPI_INTEGER, source, 
		 tag, * commCalc, &status );
	  nfinished++;

	  if ( ddebug >= 2 )
	    myDebugMsg (  __FILE__, fnName, __LINE__, tag, source, nfinished );

	  if ( nfinished == nProcsCalc ) 
	    return;
	  
	  break;

	case METADATA:
	  MPI_Recv ( &iBuffer, 1, MPI_INTEGER, source,
		     tag, * commCalc, &status );

	  if  ( ddebug >= 2 )
	    myDebugMsg (  __FILE__, fnName, __LINE__, tag, source, nfinished );

	  break;

	default:
	  MPI_Recv ( &iBuffer, 1, MPI_INTEGER, source,
		     tag, * commCalc, &status );

	  if ( ddebug >= 2 )
	    myDebugMsg (  __FILE__, fnName, __LINE__, tag, source, nfinished );
	}
    }

  free ( outtext );
    
  return;
}

