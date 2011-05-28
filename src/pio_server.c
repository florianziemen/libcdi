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

extern void reshArrayPrint ( char * );

MPI_Comm * commPio;



/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ( MPI_Comm comm, int ptype )
{
  int source, tag, iBuffer;
  MPI_Status status;
  static int nfinished;
  MPI_Comm commColl;
  int nnodes, color;
  char * buffer;
  int size;

  
  if ( ddebug >= 1 ) xdebugComm ( &commCalc );
     
  nfinished = 0;
  commColl = bInit ( ptype, comm, &color, &nnodes );

  for ( ;; )
    {  
      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commCalc, &status );
      
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      
      switch ( tag )
	{
	case FINALIZE:      
	  MPI_Recv ( &iBuffer, 1, MPI_INTEGER, source, 
		 tag, commCalc, &status );
	  nfinished++;

	  if ( ddebug >= 2 ) xdebugMsg (  tag, source, nfinished );

	  if ( nfinished == nProcsCalc )  return;
	  
	  break;

	case METADATA:
	  MPI_Get_count ( &status, MPI_CHAR, &size );
	  buffer = xmalloc ( size * sizeof ( char ));
	  MPI_Recv ( buffer, size, MPI_PACKED, source,
		     tag, commCalc, &status );
	  rpcUnpackResources ( buffer, size, commCalc );
	  free ( buffer );

	  if ( rankGlob == nProcsCalc ) reshArrayPrint ( "reshArrayIOServer" );

	  if  ( ddebug >= 2 ) xdebugMsg (  tag, source, nfinished );

	  break;

	default:
	  MPI_Recv ( &iBuffer, 1, MPI_INTEGER, source,
		     tag, commCalc, &status );

	  if ( ddebug >= 2 ) xdebugMsg ( tag, source, nfinished );
	}
    }
    
  return;
}

#endif
