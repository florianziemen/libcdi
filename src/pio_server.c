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

static struct
{
  size_t size;
  unsigned char *buffer, *head;
} *rxWin = NULL;

static MPI_Win getWin = MPI_WIN_NULL;
static MPI_Group groupModel = MPI_GROUP_NULL;


/************************************************************************/

static
void serverWinCleanup ()
{
  int i;
  int nProcsCalc = commInqNProcsModel ();
  
  if ( getWin != MPI_WIN_NULL )
    xmpi ( MPI_Win_free ( &getWin ));
  
  if (rxWin)
    {
      for ( i = 0; i < nProcsCalc; i++ )
        free(rxWin[i].buffer);
      free(rxWin);
    }

  xdebug("%s", "cleaned up mpi_win");
}
 
 /************************************************************************/

static 
  void collDefBufferSizes ()
{
  int nstreams, * streamIndexList, streamNo, vlistID, nvars, varID, iorank;
  int modelID, decoChunk, sumGetBufferSizes = 0;
  int rankGlob = commInqRankGlob ();
  int nProcsModel = commInqNProcsModel ();
  int root = commInqRootGlob ();

  xassert(rxWin != NULL);

  nstreams = reshCountType ( &streamOps );
  streamIndexList = xmalloc ( nstreams * sizeof ( streamIndexList[0] ));
  reshGetResHListOfType ( nstreams, streamIndexList, &streamOps );
  for ( streamNo = 0; streamNo < nstreams; streamNo++ )
    {
      // space required for data
      vlistID = streamInqVlist ( streamIndexList[streamNo] );
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
                  rxWin[modelID].size += decoChunk * sizeof (double)
                    + winBufferOverheadChunk * sizeof (int);
                }
            }
    }
      // space required for the 3 function calls streamOpen, streamDefVlist, streamClose 
      // once per stream and timestep for all collprocs only on the modelproc root
      rxWin[root].size += 3 * winBufferOverheadFuncCall * sizeof (int)
        + 5 * sizeof (int) + MAXDATAFILENAME;
    }
  free ( streamIndexList );

  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      rxWin[modelID].size += winBufferOverhead * sizeof (int);
      sumGetBufferSizes += rxWin[modelID].size;
    }
  xassert ( sumGetBufferSizes <= MAXWINBUFFERSIZE );
  /* xprintArray ( "getBufferSize", getBufferSize, nProcsModel, DATATYPE_INT ); */
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
                          commCalc, &getWin ));

  /* target group */
  ranks[0] = nProcsModel;
  xmpi ( MPI_Comm_group ( commCalc, &groupCalc ));
  xmpi ( MPI_Group_excl ( groupCalc, 1, ranks, &groupModel ));

  rxWin = xmalloc(nProcsModel * sizeof (rxWin[0]));
  collDefBufferSizes ();
  /* xprintArray ( "getBufferSizes", getBufferSize, nProcsModel, DATATYPE_INT ); */

  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      rxWin[modelID].buffer = xmalloc(rxWin[modelID].size);
      rxWin[modelID].head = rxWin[modelID].buffer;
    }

  xdebug("%s", "created mpi_win, allocated getBuffer");
}

/************************************************************************/

static
  void getBufferGetFromEnd ( const char * caller, int line,  
                             int ID, void * argBuffer, size_t size )
{
  if (rxWin == NULL ||
      argBuffer     == NULL ||
      size           < 0    ||
      ID             < 0    ||
      ID             >= commInqNProcsModel () ||
      rxWin[ID].head - rxWin[ID].buffer + size > rxWin[ID].size)
    xabort("caller: %s, line %d, ID = %d, nProcsModel=%d,"
           " size = %lu, rxWin[%d].head = %ld, rxWin[%d].size = %lu",
           caller, line, ID, (unsigned long)size, ID,
           commInqNProcsModel(), rxWin[ID].head - rxWin[ID].buffer,
           ID, (unsigned long)rxWin[ID].size);
  memcpy ( argBuffer, rxWin[ID].head, size );
  rxWin[ID].head += size;
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
        char *filename;
        size_t filenamesz;
        int filetype, streamID;

        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, &filenamesz, sizeof ( filenamesz ));
        xassert ( filenamesz > 0 && filenamesz < MAXDATAFILENAME );
        filename = xmalloc(filenamesz + 1);
        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, filename, filenamesz );
        filename[filenamesz] = '\0';
        getBufferGetFromEnd ( __func__, __LINE__, 
                              root, &filetype, sizeof ( filetype ));
        xassert ( filetype >= MINFILETYPE && filetype <= MAXFILETYPE );
        streamID = streamOpenWrite ( filename, filetype );
        xdebug("READ FUNCTION CALL FROM WIN:  %s, filenamesz=%zu,"
               " filename=%s, filetype=%d, OPENED STREAM %d",
               funcMap[funcID], filenamesz, filename,
               filetype, streamID);
        free(filename);
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
  int varID, vlistID = CDI_UNDEFID, taxisID;
  int size, chunk;
  int tokenID, tokenID2;
  
  int nmiss = 0;
  char text[1024];
  int nProcsModel = commInqNProcsModel ();
  int root        = commInqRootGlob ();
  
  xdebug("%s", "START");
  
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
              taxisDefVdate ( taxisID, vdate );
              taxisDefVtime ( taxisID, vtime );
              streamDefTimestep ( streamID, tsID );
            }
          getBufferGetFromEnd(__func__, __LINE__, root, &varID, sizeof (varID));
          size = vlistInqVarSize ( vlistID, varID );
          data = xmalloc ( size * sizeof ( double ));
          dataHead = data;
          
          for ( modelID = 0; modelID < nProcsModel; modelID++ )
            {
              if ( modelID != root )
                {
                  int tempID;
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
  xdebug("%s", "RETURN");
} 

/************************************************************************/


static 
  void getFlushBuffer ( int modelID )
{
  int nProcsModel = commInqNProcsModel ();

  xassert ( modelID                >= 0           &&
            modelID                 < nProcsModel &&
            rxWin != NULL && rxWin[modelID].buffer != NULL &&
            rxWin[modelID].size > 0 &&
            rxWin[modelID].size <= MAXWINBUFFERSIZE );
  memset(rxWin[modelID].buffer, 0, rxWin[modelID].size);
  rxWin[modelID].head = rxWin[modelID].buffer;
}


/************************************************************************/


static
void getData ( int tsID, int vdate, int vtime )
{
  int modelID;
  char text[1024];
  int nProcsModel = commInqNProcsModel ();
  void *getWinBaseAddr;
  int attrFound;
          
  xdebug("%s", "START");

  // todo put in correct lbs and ubs
  xassert ( tsID >= 0 && vdate >= 0 && vtime >= 0 );
  xmpi(MPI_Win_start(groupModel, 0, getWin));
  xmpi(MPI_Win_get_attr(getWin, MPI_WIN_BASE, &getWinBaseAddr, &attrFound));
  xassert(attrFound);
  for ( modelID = 0; modelID < nProcsModel; modelID++ )
    {
      getFlushBuffer ( modelID );
      xdebug("modelID=%d, nProcsModel=%d, rxWin[%d].size=%zu,"
             " getWin=%p, sizeof(int)=%u",
             modelID, nProcsModel, modelID, rxWin[modelID].size,
             getWinBaseAddr, (unsigned)sizeof(int));
      xmpi(MPI_Get(rxWin[modelID].buffer, rxWin[modelID].size,
                   MPI_UNSIGNED_CHAR, modelID, 0,
                   rxWin[modelID].size, MPI_UNSIGNED_CHAR, getWin));
    }
  xmpi ( MPI_Win_complete ( getWin ));

  if ( ddebug > 2 )
    for ( modelID = 0; modelID < nProcsModel; modelID++ )
      {
        sprintf(text, "rxWin[%d].size=%zu from PE%d rxWin[%d].buffer",
                modelID, rxWin[modelID].size, modelID, modelID);
        xprintArray(text, rxWin[modelID].buffer,
                    rxWin[modelID].size / sizeof (double),
                    DATATYPE_FLT);
      }
  readGetBuffers ( tsID, vdate, vtime );
          
  xdebug("%s", "RETURN");
}

/************************************************************************/

/**
  @brief is encapsulated in CDI library and run on I/O PEs.

  @param

  @return
*/

void IOServer ()
{
  int source, tag, *iBuffer, size, nProcsModel=commInqNProcsModel();
  static int nfinished = 0;
  char * buffer;
  MPI_Comm commCalc;
  MPI_Status status;

  xdebug("%s", "START");

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
          {
            int i;
            xdebugMsg(tag, source, nfinished);
            xmpi(MPI_Recv(&i, 1, MPI_INTEGER, source,
                          tag, commCalc, &status));
          }
          xdebug("%s", "RECEIVED MESSAGE WITH TAG \"FINALIZE\"");
          nfinished++;
          xdebug("nfinished=%d, nProcsModel=%d", nfinished, nProcsModel);
          if ( nfinished == nProcsModel )
            {
              {
                int nStreams = streamSize ();

                if ( nStreams > 0 )
                  {
                    int streamNo;
                    int * resHs;

                    resHs       = xmalloc ( nStreams * sizeof ( resHs[0] ));
                    streamGetIndexList ( nStreams, resHs );
                    for ( streamNo = 0; streamNo < nStreams; streamNo++ )
                      streamClose ( resHs[streamNo] );
                    free ( resHs );
                  }
              }
              backendCleanup();
              serverWinCleanup();
              /* listDestroy(); */
              xdebug("%s", "RETURN");
              return;
            }
	  
          break;
          
	case RESOURCES:
	  xdebugMsg (  tag, source, nfinished );
	  xmpi ( MPI_Get_count ( &status, MPI_CHAR, &size ));
	  buffer = xmalloc(size);
	  xmpi ( MPI_Recv ( buffer, size, MPI_PACKED, source,
                            tag, commCalc, &status ));
          xdebug("%s", "RECEIVED MESSAGE WITH TAG \"RESOURCES\"");
	  rpcUnpackResources ( buffer, size, commCalc );
          xdebug("%s", "");
	  free ( buffer );
          if ( ddebug > 0 && commInqRankGlob () == nProcsModel ) 
            reshListPrint ( "reshListIOServer" );
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
