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
#include "pio_comm.h"
#include "pio.h"
#include "pio_rpc.h"
#include "pio_util.h"
#include "stream_int.h"
#include "resource_handle.h"

extern resOps streamOps;


extern void reshArrayPrint ( char * );
extern void arrayDestroy ( void );

MPI_Comm * commPio;
MPI_Win win;
MPI_Group groupModel;

int     * getBufferSize = NULL;
double ** getBuffer     = NULL;


/************************************************************************/

static
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

static 
  void getGetBufferSize ()
{
  int nstreams, * streamIndexArray, streamNo, vlistID, nvars, varID, iorank;
  int i, decoChunk;
  int rankGlob = commInqRankGlob ();

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
          assert ( iorank != CDI_UNDEFID );
          if ( iorank == rankGlob )
            {
              for ( i = 0; i < nProcsCalc; i++ )
                {
                  decoChunk =  vlistInqVarDecoChunk ( vlistID, varID, i );
                  assert ( decoChunk > 0 );
                  getBufferSize[i] += (( decoChunk + winBufferOverheadChunk ) *
                                       sizeof ( double ));
                }
            }
        }
    }
  for ( i = 0; i < nProcsCalc; i++ )
    {
      getBufferSize[i] += ( winBufferOverhead * sizeof ( double ));
      assert ( getBufferSize[i] < winBufferSizeMax );
    }
  xprintArray ( "getBufferSize", getBufferSize, nProcsCalc, DATATYPE_INT );
  free ( streamIndexArray );
}

 /************************************************************************/

static 
 void serverWinCreate ()
{ 
  int ranks[1], i;
  MPI_Group groupCalc;
  xdebug();

  xmpi ( MPI_Win_create ( MPI_BOTTOM, 0, sizeof ( double ), MPI_INFO_NULL,
                         commCalc, &win ));
  xdebug3 ( "created mpi_win" );

  /* target group */
  ranks[0] = nProcsCalc;
  xmpi ( MPI_Comm_group ( commCalc, &groupCalc ));
  xmpi ( MPI_Group_excl ( groupCalc, 1, ranks, &groupModel ));

  /* first shot with fixed getBuffers */
  getBufferSize = xmalloc ( nProcsCalc * sizeof ( getBufferSize[0] ));  
  getGetBufferSize ();

  xprintArray ( "getBufferSize", getBufferSize, nProcsCalc, DATATYPE_INT );

  getBuffer = xmalloc ( nProcsCalc * sizeof ( getBuffer[0] ));
  for ( i = 0; i < nProcsCalc; i++ )
    getBuffer[i] = xmalloc ( getBufferSize[i] * sizeof ( double ));

  xdebug ( "created mpi_win, allocated getBuffer" );
}

/************************************************************************/

static
void writeVars ( int tsID, int vdate, int vtime )
{
  int i;
  double ** getBufferHead = NULL, * data = NULL, * dataHead = NULL;
  int streamID = CDI_UNDEFID, varID, vlistID, taxisID;
  int size, offset, chunk, chunkSize;
  
  int nmiss = 0;
  char text[1024];

  getBufferHead = xmalloc ( nProcsCalc * sizeof ( getBufferHead[0] ));

  for ( i = 0; i < nProcsCalc; i++ )
    getBufferHead[i] = getBuffer[i];

  while (( int ) * getBufferHead[0] != END )
    {
      assert (( int ) * getBufferHead[0] != CDI_UNDEFID );
      if (( int ) * getBufferHead[0] != streamID )
        {
          streamID = ( int ) * getBufferHead[0];
          vlistID = streamInqVlist ( streamID );
          taxisID = vlistInqTaxis ( vlistID );
          taxisDefVdate ( taxisID, vdate + tsID );
          taxisDefVtime ( taxisID, vtime );
          streamDefTimestep ( streamID, tsID );
        }
      getBufferHead[0]++;
      varID = ( int ) * getBufferHead[0];
      getBufferHead[0]++;
      size = vlistInqVarSize ( vlistID, varID );
      data = xmalloc ( size * sizeof ( double ));
      dataHead = data;

      for ( i = 0; i < nProcsCalc; i++ )
        {
          if ( i )
            {
              assert ( * ( getBufferHead[i] ) == ( double ) streamID );
              getBufferHead[i]++;
              assert ( * ( getBufferHead[i] ) == ( double ) varID );
              getBufferHead[i]++;
            }
          chunk  = vlistInqVarDecoChunk ( vlistID, varID, i );
          memcpy ( dataHead, getBufferHead[i], chunk * sizeof ( double ));
          dataHead += chunk;
          getBufferHead[i] += chunk;
          assert (( int ) * ( getBufferHead[i] ) == SEPARATOR );
          getBufferHead[i]++;
        }

      streamWriteVar ( streamID, varID, data, nmiss );

      if ( ddebug > 2 )
        {
          sprintf ( text, "streamID=%d, var[%d], size=%d", streamID, varID, size );
          xprintArray ( text, data, size, DATATYPE_FLT );
        }
          
      free ( data );
    }

  free ( getBufferHead );
} 

/************************************************************************/

static
void getData ( int tsID, int vdate, int vtime )
{
  int iAssert   = 0, i;
  char text[1024];

  // todo put in correct lbs and ubs
  assert (    tsID >= 0 &&
           vdate >= 0 &&
           vtime >= 0 );

  xmpi ( MPI_Win_start ( groupModel, iAssert, win ));  
  for ( i = 0; i < nProcsCalc; i++ )
    xmpi ( MPI_Get ( &getBuffer[i][0], getBufferSize[i], MPI_DOUBLE, 
                     i, 0, getBufferSize[i], MPI_DOUBLE, win ));

  xmpi ( MPI_Win_complete ( win ));

  if ( ddebug > 2 )
    for ( i = 0; i < nProcsCalc; i++ )
      {
        sprintf ( text, "getBufferSize[%d]=%d from PE%d getBuffer[%d]", 
                  i, getBufferSize[i], i, i );
        xprintArray ( text, &getBuffer[i][0], getBufferSize[i] / sizeof ( double ), 
                      DATATYPE_FLT );
      }

  writeVars ( tsID, vdate, vtime );

}

/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ()
{
  int source, tag, * iBuffer;
  MPI_Status status;
  static int nfinished;
  MPI_Comm commColl;
  int nnodes, color;
  char * buffer;
  int size;
  MPI_Comm comm = commInqCommPio ();
  int ptype = commInqIOModus ();
  
  xdebugComm ( &commCalc );
     
  nfinished = 0;
  commColl = backendInit ( ptype, comm, &color, &nnodes );
  
  for ( ;; )
    {
      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commCalc, &status );
      
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      
      switch ( tag )
	{
	case FINALIZE: 
	  xdebugMsg (  tag, source, nfinished );
          iBuffer = xmalloc ( sizeof ( int ));     
	  xmpi ( MPI_Recv ( iBuffer, 1, MPI_INTEGER, source, 
                            tag, commCalc, &status ));
          free ( iBuffer );
	  nfinished++;

	  if ( nfinished == nProcsCalc )  
              {
                  serverWinCleanup ();
                  arrayDestroy();
                  backendFinalize();
                  return;
              }
	  
	  break;

	case RESOURCES:
	  xdebugMsg (  tag, source, nfinished );
	  xmpi ( MPI_Get_count ( &status, MPI_CHAR, &size ));
	  buffer = xmalloc ( size * sizeof ( char ));
	  xmpi ( MPI_Recv ( buffer, size, MPI_PACKED, source,
                            tag, commCalc, &status ));
	  rpcUnpackResources ( buffer, size, commCalc );
	  free ( buffer );

	  if ( commInqRankGlob () == nProcsCalc ) 
            reshArrayPrint ( "reshArrayIOServer" );
          xdebug();
	  serverWinCreate ();
          xdebug();
	  break;

	case WRITETS:
	  xdebugMsg (  tag, source, nfinished );
          xmpi ( MPI_Get_count ( &status, MPI_INTEGER, &size ));     
          assert ( size == timestepSize );
          iBuffer = xmalloc ( size * sizeof ( int ));
          xmpi ( MPI_Recv ( iBuffer, size, MPI_INTEGER, source,
                            tag, commCalc, &status ));
          xdebug ("command WRITETS, recved:  tsID=%d, vdate=%d, vtime=%d", 
                  iBuffer[0], iBuffer[1], iBuffer[2] );
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
