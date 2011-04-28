/** @file ioServer.c
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include "pio_server.h"


#include <stdlib.h>
#include <stdio.h>
#include "limits.h"
#include "cdi.h"
#include "pio.h"
#include "pio_rpc.h"
#include "pio_util.h"
 

MPI_Comm * commPio;



/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ( MPI_Comm comm, int ptype )
{
  char fnName[] = "IOServer()";
  int source, tag, iBuffer;
  MPI_Status status;
  static int nfinished;
  void * vp;
  double * dp;
  size_t len, len2;
  MPI_Comm commColl;
  int nnodes, color;

  
  if ( ddebug >= 1 )
    myDebugComm ( __FILE__, fnName, __LINE__, commCalc );
     
  nfinished = 0;
  commColl = bInit ( ptype, comm, &color, &nnodes );

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
    
  return;
}

#endif
