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
#include "pio_comm.h"
#include "pio_rpc.h"
#include "pio_util.h"
#include "stream_int.h"
#include "resource_handle.h"
#include "vlist_var.h"

extern resOps streamOps;
extern void arrayDestroy ( void );

MPI_Comm * commPio;
MPI_Win win;
MPI_Group groupModel;

int            * getBufferSize = NULL;
unsigned char ** getBuffer     = NULL;
unsigned char ** getBufferHead = NULL;


/************************************************************************/

static
void serverWinCleanup ()
{
  int i;
  int nProcsCalc = commInqNProcsModel ();
  
  xmpi ( MPI_Win_free ( &win ));

  if ( getBuffer )
    {  
      for ( i = 0; i < nProcsCalc; i++ )
        if ( getBuffer[i] ) free ( getBuffer[i] );
      free ( getBuffer );
    }
  if ( getBufferSize ) free ( getBufferSize );
  if ( getBufferHead ) free ( getBufferHead );
  
  xdebug ( "cleaned up mpi_win" );
}
 
 /************************************************************************/

static 
  void collDefBufferSizes ()
{
  int nstreams, * streamIndexArray, streamNo, vlistID, nvars, varID, iorank;
  int modelID, decoChunk, sumGetBufferSizes = 0;
  int rankGlob = commInqRankGlob ();
  int nProcsModel = commInqNProcsModel ();
  int root = commInqRootGlob ();

  xassert ( getBufferSize != NULL );

  nstreams = reshCountType ( &streamOps );
  streamIndexArray = xmalloc ( nstreams * sizeof ( streamIndexArray[0] ));
  reshGetResHArrayOfType ( nstreams, streamIndexArray, &streamOps );
  for ( streamNo = 0; streamNo < nstreams; streamNo++ )
    {
      // space required for data
      vlistID = streamInqVlist ( streamIndexArray[streamNo] );
      nvars = vlistNvars ( vlistID );
      for ( varID = 0; varID < nvars; varID++ )
        {
          iorank = vlistInqVarIOrank ( vlistID, varID );
          xassert ( iorank != CDI_UNDEFID );
          if ( iorank == rankGlob )
            {
              for ( modelID = 0; modelID < nProcsModel; modelID++ )
                {
                  decoChunk =  vlistInqVarDecoChunk ( vlistID, varID, modelID );
                  xassert ( decoChunk > 0 );
                  getBufferSize[modelID] += ( decoChunk * sizeof ( double ) + 
                                        winBufferOverheadChunk * sizeof ( int ));
                }
            }
        }
      // space required for the 3 function calls streamOpen, streamDefVlist, streamClose 
      // once per stream and timestep for all collprocs only on the modelproc root
      getBufferSize[root] += ( 3 * winBufferOverheadFuncCall * sizeof ( int ) +
                               5 * sizeof ( int ) +
                               MAXDATAFILENAME ); 
    }
  free ( streamIndexArray );

  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      getBufferSize[modelID] += ( winBufferOverhead * sizeof ( double ));
      sumGetBufferSizes += getBufferSize[modelID];
    }
  xassert ( sumGetBufferSizes <= MAXWINBUFFERSIZE );
  xprintArray ( "getBufferSize", getBufferSize, nProcsModel, DATATYPE_INT );
}

 /************************************************************************/

static 
 void serverWinCreate ()
{ 
  int ranks[1], modelID;
  MPI_Comm commCalc = commInqCommCalc ();
  MPI_Group groupCalc;
  int nProcsModel = commInqNProcsModel ();

  xmpi ( MPI_Win_create ( MPI_BOTTOM, 0, 1, MPI_INFO_NULL,
                          commCalc, &win ));

  /* target group */
  ranks[0] = nProcsModel;
  xmpi ( MPI_Comm_group ( commCalc, &groupCalc ));
  xmpi ( MPI_Group_excl ( groupCalc, 1, ranks, &groupModel ));

  getBufferSize = xmalloc ( nProcsModel * sizeof ( getBufferSize[0] ));  
  collDefBufferSizes ();
  xprintArray ( "getBufferSizes", getBufferSize, nProcsModel, DATATYPE_INT );

  getBuffer     = xmalloc ( nProcsModel * sizeof ( getBuffer[0] ));
  getBufferHead = xmalloc ( nProcsModel * sizeof ( getBufferHead[0] ));
  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      getBuffer[modelID] = xmalloc ( getBufferSize[modelID] );
      getBufferHead[modelID] = getBuffer[modelID];
    }

  xdebug ( "created mpi_win, allocated getBuffer" );
}

/************************************************************************/

static
  void getBufferGetFromEnd ( const char * caller, int line,  
                             int ID, void * argBuffer, size_t size )
{
  char text[1024];

  if ( getBufferHead == NULL ||
       argBuffer     == NULL ||
       size           < 0    ||
       ID             < 0    ||
       ID             >= commInqNProcsModel () ||
       getBufferHead[ID] - getBuffer[ID] + size > getBufferSize[ID] )
    {
      sprintf ( text, "caller: %s, line %d, ID = %d, nProcsModel=%d, size = %d, "
                "getBufferHead[%d] = %d, getBufferSize[%d] = %d",
                caller, line, ID, size, ID, commInqNProcsModel (), 
                getBufferHead[ID] - getBuffer[ID],
                ID, getBufferSize[ID] );
      xabort ( text );
    }  
  memcpy ( argBuffer, getBufferHead[ID], size );
  getBufferHead[ID] += size;
}

/************************************************************************/

static
  void readFuncCall ( void )
{
  int funcID, tokenID;
  int root = commInqRootGlob ();

  getBufferGetFromEnd ( __func__, __LINE__, 
                        root, &funcID, sizeof ( funcID ));
  xassert ( funcID >= MINFUNCID && funcID <= MAXFUNCID );
  
  switch ( funcID )
    {
    case STREAMCLOSE:
      {
        int streamID;

        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, &streamID, sizeof ( streamID ));
        streamClose ( streamID );

        xdebug ( "READ FUNCTION CALL FROM WIN:  %s, streamID=%d,"
                 " closed stream", 
                 funcMap[funcID], streamID );
      }
      break;
    case STREAMOPEN:
      {
        char * filename, * endname = "\0";
        size_t filenamesz;
        int filetype, streamID;

        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, &filenamesz, sizeof ( filenamesz ));
        xassert ( filenamesz > 0 && filenamesz < MAXDATAFILENAME );
        filename = xmalloc ( filenamesz + 1 );
        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, filename, filenamesz );
        memcpy ( filename + filenamesz, endname, 1 );
        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, &filetype, sizeof ( filetype ));
        streamID = streamOpenWrite ( filename, filetype );
        free ( filename );

        xdebug ( "READ FUNCTION CALL FROM WIN:  %s, filenamesz=%d,"
                 " filename=%s, filetype=%d, opened stream %d", 
                 funcMap[funcID], filenamesz, filename, filetype, streamID );
      }
      break; 
    case STREAMDEFVLIST:
      {
        int streamID, vlistID;

        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, &streamID, sizeof ( vlistID ));
        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, &vlistID, sizeof ( vlistID ));
        streamDefVlist ( streamID, vlistID );

        xdebug ( "READ FUNCTION CALL FROM WIN:  %s, streamID=%d,"
                 " vlistID=%d, called streamDefVlist ().", 
                 funcMap[funcID], streamID, vlistID );
      }
      break;
    default:
      xabort ( "REMOTE FUNCTIONCALL NOT IMPLEMENTED!" );
    }
  getBufferGetFromEnd ( __func__, __LINE__, 
                        root, &tokenID, sizeof ( tokenID ));
  xassert ( tokenID == SEPARATOR );
}

/************************************************************************/

static
void readGetBuffers ( int tsID, int vdate, int vtime )
{
  int modelID;
  double * data = NULL, * dataHead = NULL;
  int streamID = CDI_UNDEFID, streamIDNew = CDI_UNDEFID;
  int tempID, varID, vlistID, taxisID;
  int size, offset, chunk, chunkSize;
  int tokenID, tokenID2;
  
  int nmiss = 0;
  char text[1024];
  int nProcsModel = commInqNProcsModel ();
  int root        = commInqRootGlob ();
  
  getBufferGetFromEnd ( __func__, __LINE__, 
                        root, &tokenID, sizeof ( tokenID ));

  while ( tokenID != END )
    {
      switch ( tokenID )
        {
        case DATATOKEN:
          getBufferGetFromEnd ( __func__, __LINE__, 
                                root, &streamIDNew, sizeof ( streamID ));
          if ( streamIDNew != streamID )
            {
              streamID = streamIDNew;
              vlistID = streamInqVlist ( streamID );
              taxisID = vlistInqTaxis ( vlistID );
              taxisDefVdate ( taxisID, vdate + tsID );
              taxisDefVtime ( taxisID, vtime );
              streamDefTimestep ( streamID, tsID );
            }
          getBufferGetFromEnd ( __func__, __LINE__, 
                                root, &varID, sizeof ( varID ));
          size = vlistInqVarSize ( vlistID, varID );
          data = xmalloc ( size * sizeof ( double ));
          dataHead = data;
          
          for ( modelID = 0; modelID < nProcsModel; modelID++ )
            {
              if ( modelID != root )
                {
                  getBufferGetFromEnd ( __func__, __LINE__, 
                                        modelID, &tempID, sizeof ( tempID ));
                  xassert ( tempID == DATATOKEN );
                  getBufferGetFromEnd ( __func__, __LINE__, 
                                        modelID, &tempID, sizeof ( tempID ));
                  xassert ( tempID == streamID );
                  getBufferGetFromEnd ( __func__, __LINE__, 
                                        modelID, &tempID, sizeof ( tempID ));
                  xassert ( tempID == varID );
                }
              chunk  = vlistInqVarDecoChunk ( vlistID, varID, modelID );
              getBufferGetFromEnd ( __func__, __LINE__, 
                                    modelID, dataHead, chunk * sizeof ( double ));
              dataHead += chunk;
              getBufferGetFromEnd ( __func__, __LINE__, 
                                    modelID, &nmiss   , sizeof ( nmiss ));
              getBufferGetFromEnd ( __func__, __LINE__, 
                                    modelID, &tokenID2, sizeof ( tokenID2 ));
              xassert ( tokenID2 == SEPARATOR );
            }         
          streamWriteVar ( streamID, varID, data, nmiss );

          if ( ddebug > 2 )
            {
              sprintf ( text, "streamID=%d, var[%d], size=%d", streamID, varID, size );
              xprintArray ( text, data, size, DATATYPE_FLT );
            }
          
          free ( data );
          break;
        case FUNCCALL:
          readFuncCall ();
          break;
        default:
          xabort ( "BUFFER NOT READABLE!" );
          
        }
      getBufferGetFromEnd ( __func__, __LINE__, 
                            root, &tokenID, sizeof ( tokenID ));
    }
} 

/************************************************************************/


static 
  void getFlushBuffer ( int modelID )
{
  int nProcsModel = commInqNProcsModel ();

  xassert ( modelID                >= 0           &&
            modelID                 < nProcsModel &&
            getBuffer              != NULL        &&
            getBuffer[modelID]     != NULL        &&
            getBufferHead          != NULL        &&
            getBufferSize          != NULL        &&
            getBufferSize[modelID]  > 0           &&
            getBufferSize[modelID] <= MAXWINBUFFERSIZE );
  memset ( getBuffer[modelID], 0, getBufferSize[modelID] );
  getBufferHead[modelID] = getBuffer[modelID];
}


/************************************************************************/


static
void getData ( int tsID, int vdate, int vtime )
{
  int iAssert   = 0, modelID;
  char text[1024];
  int nProcsModel = commInqNProcsModel ();

  // todo put in correct lbs and ubs
  xassert ( tsID >= 0 && vdate >= 0 && vtime >= 0 );

  xmpi ( MPI_Win_start ( groupModel, iAssert, win )); 
  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      getFlushBuffer ( modelID );
      xmpi ( MPI_Get ( &getBuffer[modelID][0], getBufferSize[modelID], 
                       MPI_UNSIGNED_CHAR, modelID, 0, 
                       getBufferSize[modelID], MPI_UNSIGNED_CHAR, win ));
    }
  xmpi ( MPI_Win_complete ( win ));

  if ( ddebug > 2 )
    for ( modelID = 0; modelID < nProcsModel; modelID++ )
      {
        sprintf ( text, "getBufferSize[%d]=%d from PE%d getBuffer[%d]", 
                  modelID, getBufferSize[modelID], modelID, modelID );
        xprintArray ( text, &getBuffer[modelID][0], 
                      getBufferSize[modelID] / sizeof ( double ), 
                      DATATYPE_FLT );
      }
  readGetBuffers ( tsID, vdate, vtime );

}

/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ()
{
  int source, tag, * iBuffer, size, nProcsModel = commInqNProcsModel ();
  static int nfinished = 0;
  char * buffer;
  MPI_Comm commCalc;
  MPI_Status status;
     
  backendInit ();
  if ( commInqRankNode () == commInqSpecialRankNode ()) 
    backendFinalize ();
  commCalc = commInqCommCalc ();

  for ( ;; )
    {
      xmpi ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commCalc, &status ));
      
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      
      switch ( tag )
	{
	case FINALIZE: 
	  xdebugMsg (  tag, source, nfinished );
          iBuffer = xmalloc ( sizeof ( int ));     
	  xmpi ( MPI_Recv ( iBuffer, 1, MPI_INTEGER, source, 
                            tag, commCalc, &status ));
          xdebug ( "RECEIVED MESSAGE WITH TAG \"FINALIZE\"" );
          free ( iBuffer );
	  nfinished++;

	  if ( nfinished == nProcsModel )  
            {              
              {
                int nStreams = streamSize (), streamNo;
                int * resHs;
                
                resHs       = xmalloc ( nStreams * sizeof ( resHs[0] ));
                streamGetIndexArray ( nStreams, resHs );
                for ( streamNo = 0; streamNo < nStreams; streamNo++ )
                    streamClose ( resHs[streamNo] );
                free ( resHs );
                backendCleanup ();
              } 
              serverWinCleanup ();
              arrayDestroy();
              return;
            }
	  
	  break;

	case RESOURCES:
	  xdebugMsg (  tag, source, nfinished );
	  xmpi ( MPI_Get_count ( &status, MPI_CHAR, &size ));
	  buffer = xmalloc ( size * sizeof ( char ));
	  xmpi ( MPI_Recv ( buffer, size, MPI_PACKED, source,
                            tag, commCalc, &status ));
          xdebug ( "RECEIVED MESSAGE WITH TAG \"RESOURCES\"" );
	  rpcUnpackResources ( buffer, size, commCalc );
	  free ( buffer );
          if ( ddebug > 0 && commInqRankGlob () == nProcsModel ) 
            reshArrayPrint ( "reshArrayIOServer" );
	  serverWinCreate ();
	  break;

	case WRITETS:
	  xdebugMsg (  tag, source, nfinished );
          xmpi ( MPI_Get_count ( &status, MPI_INTEGER, &size ));     
          xassert ( size == timestepSize );
          iBuffer = xmalloc ( size * sizeof ( int ));
          xmpi ( MPI_Recv ( iBuffer, size, MPI_INTEGER, source,
                            tag, commCalc, &status ));
          xdebug ( "RECEIVED MESSAGE WITH TAG \"WRITETS\": "
                   "tsID=%d, vdate=%d, vtime=%d, source=%d", 
                   iBuffer[0], iBuffer[1], iBuffer[2], source );
          getData ( iBuffer[0], iBuffer[1], iBuffer[2] );     
          free ( iBuffer );

	  break;

	default:
	  xabort ( "TAG NOT DEFINED!" );
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
