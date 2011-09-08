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
MPI_Win win;
MPI_Group groupModel;

int     * getBufferSize = NULL;
double ** getBuffer     = NULL;


/************************************************************************/

void serverWinCleanup ()
{
  int i;
  
  xmpi ( MPI_Win_free ( &win ));

  if ( getBuffer )
    {  
      for ( i = 0; i < nProcsCalc; i++ )
        if ( getBuffer[i] ) free ( getBuffer[i] );
      free ( getBuffer );
    }
  if ( getBufferSize ) free ( getBufferSize );
  
  xdebug ( "cleaned up mpi_win" );
}
 
 /************************************************************************/
/*
static 
  void getGetBufferSize ()
{
  int nstreams, * streamIndexArray, streamNo, vlistID, nvars, varID, iorank;

  nstreams = reshCountType ( &streamOps );
  streamIndexArray = xmalloc ( nstreams * sizeof ( int ));
  reshGetResHArrayOfType ( nstreams, streamIndexArray, &streamOps );
  for ( streamNo = 0; streamNo < nstreams; streamNo++ )
    {
      vlistID = streamInqVlist ( streamIndexArray[streamNo] );
      nvars = vlistNvars ( vlistID );
      for ( varID = 0; varID < nvars; varID++ )
        {
          iorank = vlistInqVarIOrank ( vlistID, varID );
          if ( iorank == rankGlob )
            {
              for ( i = 0; i < nProcsCalc; i++ )
                getBufferSize[i] += 
                  ( vlistInqVarDecoChunk ( vlistID, varID, i ) *
                    sizeof ( double ) + wbChunkOverhead * sizeof ( int ));
            }
        }
    }
  for ( i = 0; i < nProcsCalc; i++ )
    getBufferSize[i] += ( wbGenOverhead * sizeof ( int ));
  free ( streamIndexArray );
  return;
}
*/
 /************************************************************************/

static 
  void TEMPgetGetBufferSize ()
{
  int i;

  for ( i = 0; i < nProcsCalc; i++ )
    getBufferSize[i] = 1;

  return;
}
 
 /************************************************************************/
 
 void serverWinCreate ()
{ 
  int ranks[1], i;
  MPI_Group groupCalc;

  xmpi ( MPI_Win_create ( MPI_BOTTOM, 0, sizeof ( double ), MPI_INFO_NULL,
                         commCalc, &win ));
  xdebug ( "created mpi_win" );

  /* target group */
  ranks[0] = nProcsCalc;
  xmpi ( MPI_Comm_group ( commCalc, &groupCalc ));
  xmpi ( MPI_Group_excl ( groupCalc, 1, ranks, &groupModel ));


  /* first shot with fixed getBuffers */
  getBufferSize = xmalloc ( nProcsCalc * sizeof ( getBufferSize[0] ));  
  TEMPgetGetBufferSize ();

  xprintArray ( "getBufferSize", getBufferSize, nProcsCalc, DATATYPE_INT );

  getBuffer = xmalloc ( nProcsCalc * sizeof ( getBuffer[0] ));
  for ( i = 0; i < nProcsCalc; i++ )
    getBuffer[i] = xmalloc ( getBufferSize[i] * sizeof ( double ));

  xdebug ( "done" );
}

/************************************************************************/

void getData ( int ts, int vdate, int vtime )
{
  int iAssert   = 0, i;

  xdebug();

  // todo put in correct lbs and ubs
  assert (    ts >= 0 &&
           vdate >= 0 &&
           vtime >= 0 );

  xmpi ( MPI_Win_start ( groupModel, iAssert, win ));  
  for ( i = 0; i < nProcsCalc; i++ )
    xmpi ( MPI_Get ( &getBuffer[i][0], getBufferSize[i], MPI_DOUBLE, 
                     i, 0, getBufferSize[i], MPI_DOUBLE, win ));

  xmpi ( MPI_Win_complete ( win ));

  for ( i = 0; i < nProcsCalc; i++ )
    xdebug ( "getBufferSize[%d]=%d, got data: %.2f from PE%d", 
             i, getBufferSize[i], getBuffer[i][0], i );

  xdebug();
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
                  serverWinCleanup ();
                  xdebug();
                  arrayDestroy();
                  xdebug();
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
          getData ( iBuffer[0], iBuffer[1], iBuffer[2] );      
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
