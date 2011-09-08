/** @file ioServer.c
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include "pio_server.h"


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "limits.h"
#include "cdi.h"
#include "pio.h"
#include "pio_rpc.h"
#include "pio_util.h"

extern void reshArrayPrint ( char * );
extern void arrayDestroy ( void );

MPI_Comm * commPio;


/************************************************************************/

void serverWinCreate ()
{
  xdebug ();
}


/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ( MPI_Comm comm, int ptype )
{
  int source, tag, * iBuffer;
  MPI_Status status;
  static int nfinished;
  MPI_Comm commColl;
  int nnodes, color;
  char * buffer;
  int size;
  
  xdebugComm ( &commCalc );
     
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
          iBuffer = xmalloc ( sizeof ( int ));     
	  xmpi ( MPI_Recv ( iBuffer, 1, MPI_INTEGER, source, 
                            tag, commCalc, &status ));
          free ( iBuffer );
	  nfinished++;

	  if ( nfinished == nProcsCalc )  
              {
                  xdebug();
                  arrayDestroy();
                  bFinalize();
                  return;
              }
	  
	  break;

	case METADATA:
	  xmpi ( MPI_Get_count ( &status, MPI_CHAR, &size ));
	  buffer = xmalloc ( size * sizeof ( char ));
	  xmpi ( MPI_Recv ( buffer, size, MPI_PACKED, source,
                            tag, commCalc, &status ));
	  rpcUnpackResources ( buffer, size, commCalc );
	  free ( buffer );

	  if ( rankGlob == nProcsCalc ) reshArrayPrint ( "reshArrayIOServer" );

	  if  ( ddebug >= 2 ) xdebugMsg (  tag, source, nfinished );
 
	  serverWinCreate ();

	  break;

	case WRITETS:
	  if  ( ddebug >= 2 ) xdebugMsg (  tag, source, nfinished );
          xmpi ( MPI_Get_count ( &status, MPI_INTEGER, &size ));     
          assert ( size == tsSize );
          iBuffer = xmalloc ( size * sizeof ( int ));
          xmpi ( MPI_Recv ( iBuffer, size, MPI_INTEGER, source,
                            tag, commCalc, &status ));
          xdebug (" recved iBuffer %d %d %d", iBuffer[0], iBuffer[1], iBuffer[2] );       
          free ( iBuffer );

	  break;

	default:
	  xabort ( "" );
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
