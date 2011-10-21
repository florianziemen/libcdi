#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "cdi.h"
#include "limits.h" 
#include "pio_util.h"
#include "vlist_var.h"

#ifdef USE_MPI
#include "namespace.h"
#include "pio_interface.h"
#include "pio_comm.h"
#include "pio_rpc.h"
#include "pio_server.h"
#include "resource_handle.h"
#include "stream_int.h"
#include "vlist.h"
extern resOps streamOps;

size_t         * winBufferSize  = NULL;
unsigned char ** winBuffer      = NULL;
unsigned char ** winBufferHead  = NULL;
MPI_Win        * win            = NULL;
int            * winPostSet     = NULL;
int            * refuseFuncCall = NULL;
MPI_Group      * groupsIO       = NULL;
MPI_Group      * groupsIONetto  = NULL;

char * funcMap[nFuncs] = {"streamOpen", "streamDefVlist", "streamClose" };


/****************************************************/

int cmp ( const void * va, const void * vb )
{
  const int ** a, ** b;
 
  a = ( const int ** ) va;
  b = ( const int ** ) vb;
  
  return (( **a < **b ) - ( **a > **b ));
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
  int writerIdx = 0;
  int i, j;
  

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
      problemMapping[ dummy[i] ] = writerIdx;
      buckets[writerIdx] +=  *ip[i];
    }

  xprintArray3 (  "problemSizes = ", problemSizes, nProblems, DATATYPE_INT );     
  xprintArray3 ( "vector of indices, qsort of problemSizes", dummy, 
                nProblems, DATATYPE_INT ); 
  xprintArray3 ( "problemMapping", problemMapping, nProblems, DATATYPE_INT );
  xprintArray3 ( "meanBucket", meanBucket, nWriter, DATATYPE_FLT );
  xprintArray3 ( "actual buckets", buckets, nWriter, DATATYPE_INT );      
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

  int weightsStreams[nStreams];
  int streamMapping[nStreams];
  int nPEs = 0, nVars = 0;
  int i, j, k, offset = 0, offsetN = 0;
  double * w;
  
  int * weightsVarsNode;
  int * varMappingNode;
  int nVarsNode, summandRank = 0;
  int nProcsColl = commInqNProcsColl ();

  int buckets[nProcsColl];

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
              commCollID2RankGlob ( * ( varMappingNode + offsetN ++ ) +  summandRank );
	else
	  offset += * ( sSizes + j );

      summandRank += * ( nodeSizes + i );

      free ( w );
      free ( varMappingNode );
      free ( weightsVarsNode );
    }
  
  if ( ddebug )
    {
      xprintArray ( "varMapping", varMapping, nVars, DATATYPE_INT  );
      for ( i = 0; i < nProcsColl; i++ )
	buckets[i] = 0;
      for ( i = 0; i < nVars; i ++ )
	buckets[commRankGlob2CollID ( *(varMapping + i ))] += * ( vSizes + i );
      xprintArray ( "buckets", buckets, nProcsColl, DATATYPE_INT );
    }
}

/************************************************************************/ 

void defVarDeco ( int vlistID, int varID )
{
  int varSize, cRank, lChunk, rem, lOffset;
  int nProcsModel = commInqNProcsModel ();
  deco_t deco[nProcsModel];
 
  varSize = vlistInqVarSize ( vlistID, varID );

  for ( cRank = 0; cRank < nProcsModel; cRank++ )
    {
      lChunk = varSize / nProcsModel;
      lOffset = cRank * lChunk;
      rem = varSize % nProcsModel;
      if ( cRank < rem ) 
        {
          lChunk++;
          lOffset += cRank;
        }
      else
        lOffset += rem;
      
      deco[cRank].rank   = cRank;
      deco[cRank].offset = lOffset;
      deco[cRank].chunk  = lChunk;
    }

  vlistDefVarDeco ( vlistID, varID, nProcsModel, &deco[0] );
}

/************************************************************************/


static
void varsMapNDeco ( int nNodes, int * nodeSizes )
{
  int nStreams, nVars, * resHs, * streamSizes, * varSizes, * varMapping;
  int i, j, k = 0;

  nStreams = streamSize ();

  resHs       = xmalloc ( nStreams * sizeof ( resHs[0] ));
  streamSizes = xmalloc ( nStreams * sizeof ( streamSizes[0] ));
  streamGetIndexArray ( nStreams, resHs );

  for ( i = 0; i < nStreams; i++ )
    streamSizes[i] = streamInqNvars ( * ( resHs + i )); 

  nVars = xsum ( nStreams, streamSizes );
  varSizes   = xmalloc ( nVars * sizeof ( varSizes[0] ));
  varMapping = xmalloc ( nVars * sizeof ( varMapping[0] ));

  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < * ( streamSizes + i ); j++ )
      varSizes[k++] += vlistInqVarSize ( streamInqVlist ( * resHs + i ), j );

  xassert ( k == nVars );

  varMapGen ( varSizes, streamSizes, varMapping, 
	      nStreams, nodeSizes, nNodes );

  k = 0;
  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < * ( streamSizes + i ); j++ )
      {
        defVarDeco ( streamInqVlist ( * resHs + i ), j );
        defVarDeco ( streamInqVlistIDorig ( * resHs + i ), j );
	vlistDefVarIOrank ( streamInqVlist ( * resHs + i ), j, 
			  * ( varMapping + k ));
	vlistDefVarIOrank ( streamInqVlistIDorig ( * resHs + i ), j, 
			  * ( varMapping + k++ ));
      }

  if ( varMapping )  free ( varMapping );
  if ( varSizes )    free ( varSizes );
  if ( streamSizes ) free ( streamSizes );
  if ( resHs )       free ( resHs );
}

/************************************************************************/

static 
void modelWinCleanup ( void )
{
  int collID;

  for ( collID = 0; collID < commInqNProcsColl (); collID ++ )
    {
      if ( winPostSet[collID] )
        xmpi ( MPI_Win_wait ( win[collID] )); 
      xmpi ( MPI_Win_free ( &win[collID] ));
      xmpi ( MPI_Free_mem ( winBuffer[collID] ));
      xmpi ( MPI_Group_free ( &groupsIONetto[collID] ));
    }

  if ( groupsIONetto )   free ( groupsIONetto );
  if ( refuseFuncCall )  free ( refuseFuncCall );
  if ( winPostSet )      free ( winPostSet );
  if ( win )             free ( win );
  if ( winBufferHead )   free ( winBufferHead );
  if ( winBuffer )       free ( winBuffer );
  if ( winBufferSize )   free ( winBufferSize );

  xdebug ( "cleaned up mpi_win's" );
}

/************************************************************************/

static
  void modelWinDefBufferSizes ( void )
{
  int collID, nstreams, * streamIndexArray, streamNo, vlistID, nvars, varID;
  int collIDchunk = 0, sumWinBufferSize = 0;
  int nProcsColl  = commInqNProcsColl ();
  int rankGlob    = commInqRankGlob ();
  int rankModel   = commInqRankModel ();
  int root = commInqRootGlob ();

  xassert ( winBufferSize != NULL );
  
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
          collID = CDI_UNDEFID;
          collID = commRankGlob2CollID ( vlistInqVarIOrank ( vlistID, varID ));
          collIDchunk = vlistInqVarDecoChunk ( vlistID, varID, rankModel );
          xassert ( collID != CDI_UNDEFID && collIDchunk > 0 );
          winBufferSize[collID] += ( collIDchunk * sizeof ( double ) + 
                                     winBufferOverheadChunk  * sizeof ( int ));
        }

      // space required for the 3 function calls streamOpen, streamDefVlist, streamClose 
      // once per stream and timestep for all collprocs only on the modelproc root
      if ( rankGlob == root ) 
        for ( collID = 0; collID < nProcsColl; collID++ ) 
          winBufferSize[collID] += ( 3 * winBufferOverheadFuncCall * sizeof ( int ) +
                                     5 * sizeof ( int ) +
                                     MAXDATAFILENAME );                                     
    }
  free ( streamIndexArray );
  
  for ( collID = 0; collID < nProcsColl; collID++ ) 
    {
      winBufferSize[collID] += ( winBufferOverhead * sizeof ( int )); 
      sumWinBufferSize += winBufferSize[collID];
    }
  xassert ( sumWinBufferSize <= MAXWINBUFFERSIZE );
  xprintArray ( "winBufferSize", winBufferSize, nProcsColl, DATATYPE_INT );
}


/************************************************************************/


static 
  void modelWinFlushBuffer ( int collID )
{
  int nProcsColl = commInqNProcsColl ();

  xassert ( collID                >= 0         &&
            collID                < nProcsColl &&
            winBuffer             != NULL      &&
            winBuffer[collID]     != NULL      &&
            winBufferHead         != NULL      &&
            winBufferSize         != NULL      &&
            winBufferSize[collID] >= 0         &&
            winBufferSize[collID] <= MAXWINBUFFERSIZE &&
            refuseFuncCall        != NULL );
  memset ( winBuffer[collID], 0, winBufferSize[collID] );
  winBufferHead[collID] = winBuffer[collID];
  refuseFuncCall[collID] = 0;
}


/************************************************************************/


static
void modelWinCreate ( void )
{
  int collID, ranks[1];
  int nProcsColl = commInqNProcsColl ();

  winBufferSize   = xmalloc ( nProcsColl * sizeof ( winBufferSize[0] ));
  winBuffer       = xmalloc ( nProcsColl * sizeof ( winBuffer[0] ));
  winBufferHead   = xmalloc ( nProcsColl * sizeof ( winBufferHead[0] ));
  win             = xmalloc ( nProcsColl * sizeof ( win[0] ));
  winPostSet      = xmalloc ( nProcsColl * sizeof ( winPostSet[0] ));
  refuseFuncCall  = xmalloc ( nProcsColl * sizeof ( refuseFuncCall[0] ));
  groupsIONetto   = xmalloc ( nProcsColl * sizeof ( groupsIONetto[0] ));

  modelWinDefBufferSizes ();
  ranks[0] = commInqNProcsModel ();

  for ( collID = 0; collID < nProcsColl; collID ++ )
    {
      xassert ( winBufferSize[collID] > 0 );
      winBuffer[collID] = NULL;
      xmpi ( MPI_Alloc_mem (( MPI_Aint ) winBufferSize[collID], 
                            MPI_INFO_NULL, &winBuffer[collID] ));
      xassert ( winBuffer[collID] != NULL ); 
      winBufferHead[collID] = winBuffer[collID];
      xmpi ( MPI_Win_create ( winBuffer[collID],  
                              ( MPI_Aint ) winBufferSize[collID], 1,
                              MPI_INFO_NULL, commInqCommsIO ( collID ), 
                              &win[collID] )); 
      xmpi ( MPI_Comm_group ( commInqCommsIO ( collID ), &groupsIONetto[collID] ));
      xmpi ( MPI_Group_incl ( groupsIONetto[collID], 1, ranks, 
                              &groupsIONetto[collID] ));
    }
  xdebug ( "created mpi_win's" );
}

/************************************************************************/

static
  void modelWinBufferPutAtEnd ( const char * caller, 
                                int collID, const void * argBuffer, size_t size )
{
  char text[1024];
  /*
    xdebug ( "collID=%d, size=%d, newBufferHead=%d, oldBufferSize=%d",
    collID, size, winBufferHead[collID] - winBuffer[collID] + size, 
    winBufferSize[collID] );
  */
  if ( winBufferHead == NULL ||
       argBuffer     == NULL ||
       size           < 0    ||
       collID         < 0    ||
       collID        >= commInqNProcsColl () ||
       winBufferHead[collID] - winBuffer[collID] + size > winBufferSize[collID] )
    {
      sprintf ( text, "caller: %s", caller );
      xabort ( text );
    }
  
  memcpy ( winBufferHead[collID], argBuffer, size );
  winBufferHead[collID] += size;
}

/************************************************************************/

void pioBufferData ( const int streamID, const int varID, const double *data, int nmiss )
{
  int chunk, vlistID, collID = CDI_UNDEFID;
  int tokenSep = SEPARATOR, tokenData = DATATOKEN;
  size_t size;
  void * vp;
  int rankModel = commInqRankModel ();

  vlistID  = streamInqVlist ( streamID );
  collID   = commRankGlob2CollID ( vlistInqVarIOrank ( vlistID, varID ));
  chunk    = vlistInqVarDecoChunk ( vlistID, varID, rankModel );
  xassert ( collID         >= 0                    &&
            collID         <  commInqNProcsColl () &&
            chunk          >= 0                    &&
            winBufferSize  != NULL                 &&
            winBuffer      != NULL                 &&
            winBufferHead  != NULL                 &&
            winPostSet     != NULL                 &&
            refuseFuncCall != NULL                &&
            win            != NULL );
  
  xdebug3 ("streamID=%d, vlistID=%d, varID=%d, collID=%d, "
          "chunk=%d, data=%.2f, nmiss=%d",
          streamID, vlistID, varID, collID, chunk, data[0], nmiss );  

  if ( winPostSet[collID] ) 
    { 
      xmpi ( MPI_Win_wait ( win[collID] )); 
      winPostSet[collID] = 0;
      modelWinFlushBuffer ( collID );
    }

  size = chunk * sizeof ( double ) + winBufferOverheadChunk * sizeof ( int );
  xassert ( winBufferHead[collID] - winBuffer[collID] + size < 
            winBufferSize[collID] ); 

  modelWinBufferPutAtEnd ( __func__, collID, &tokenData, sizeof ( tokenData ));
  modelWinBufferPutAtEnd ( __func__, collID, &streamID , sizeof ( streamID ));
  modelWinBufferPutAtEnd ( __func__, collID, &varID    , sizeof ( varID ));
  modelWinBufferPutAtEnd ( __func__, collID, data      , chunk * sizeof ( double ));
  modelWinBufferPutAtEnd ( __func__, collID, &nmiss    , sizeof ( nmiss ));
  modelWinBufferPutAtEnd ( __func__, collID, &tokenSep , sizeof ( tokenSep )); 

  refuseFuncCall[collID] = 1;
}

/************************************************************************/

int func2Int ( const char * func )
{
  int iret = CDI_UNDEFID, funcID;

  for ( funcID = 0; funcID < nFuncs; funcID ++ )
    if ( strlen ( funcMap[funcID] ) == strlen ( func ) && 
         memcmp ( func, funcMap[funcID], strlen ( func )) == 0 )
      iret = funcID;

  return iret;
}

/************************************************************************/

void pioBufferFuncCall ( const char * func, int argc, ... )
{
  va_list ap;
  int rankGlob = commInqRankGlob ();
  int root = commInqRootGlob ();
  int collID, nProcsColl = commInqNProcsColl ();
  int tokenSep = SEPARATOR, tokenFuncCall = FUNCCALL;
  size_t size = 0;
  int funcID = func2Int ( func );
  
  if ( rankGlob != root ) return;

  xassert ( func          != NULL                 &&
            argc          >= 1                    &&
            argc          <= 2                    &&
            winBufferSize != NULL                 &&
            winBuffer     != NULL                 &&
            winBufferHead != NULL                 &&
            winPostSet    != NULL                 &&
            win           != NULL );
         
  va_start ( ap, argc );
  
  switch ( funcID ) 
    {
    case STREAMCLOSE:
      {
        int streamID;

        xassert ( argc == 1 );
        streamID  = va_arg ( ap, int );
        
        for ( collID = 0; collID < nProcsColl; collID++ ) 
          {
            size = ( winBufferOverheadFuncCall + 1 ) * sizeof ( int );
            xassert ( winBufferHead[collID] - winBuffer[collID] + size < 
                      winBufferSize[collID] );  

            if ( winPostSet[collID] ) 
              { 
                xmpi ( MPI_Win_wait ( win[collID] )); 
                winPostSet[collID] = 0;
                modelWinFlushBuffer ( collID );
              }

            xassert ( refuseFuncCall[collID] == 0 );

            modelWinBufferPutAtEnd ( __func__, collID, &tokenFuncCall, 
                                     sizeof ( tokenFuncCall));
            modelWinBufferPutAtEnd ( __func__, collID, &funcID, 
                                     sizeof ( funcID ));
            modelWinBufferPutAtEnd ( __func__, collID, &streamID, 
                                     sizeof ( streamID ));
            modelWinBufferPutAtEnd ( __func__, collID, &tokenSep, 
                                     sizeof ( tokenSep ));
          }
      xdebug ( "WROTE FUNCTION CALL IN BUFFER OF WINS:  %s, streamID=%d", 
               funcMap[funcID], streamID );
      }       
      break;
    case STREAMOPEN:
      {
        char * filename;
        int    filetype;
        size_t filenamesz;
        
        xassert ( argc == 2 );
        filename  = va_arg ( ap, char * );
        filenamesz = strlen ( filename );
        xassert ( filenamesz > 0 &&
                  filenamesz < MAXDATAFILENAME );
        filetype  = va_arg ( ap, int );
        
        for ( collID = 0; collID < nProcsColl; collID++ ) 
          {
            size = ( winBufferOverheadFuncCall + 2 ) * sizeof ( int ) +  
              MAXDATAFILENAME;
            xassert ( winBufferHead[collID] - winBuffer[collID] + size <
                      winBufferSize[collID] );  

            if ( winPostSet[collID] ) 
              { 
                xmpi ( MPI_Win_wait ( win[collID] )); 
                winPostSet[collID] = 0;
                modelWinFlushBuffer ( collID );
              }
            modelWinBufferPutAtEnd ( __func__, collID, &tokenFuncCall, 
                                     sizeof ( tokenFuncCall));
            modelWinBufferPutAtEnd ( __func__, collID, &funcID, 
                                     sizeof ( funcID ));
            modelWinBufferPutAtEnd ( __func__, collID, &filenamesz, 
                                     sizeof ( filenamesz ));
            modelWinBufferPutAtEnd ( __func__, collID, filename, 
                                     filenamesz );
            modelWinBufferPutAtEnd ( __func__, collID, &filetype, 
                                     sizeof ( filetype ));
            modelWinBufferPutAtEnd ( __func__, collID, &tokenSep, 
                                     sizeof ( tokenSep ));
          }
        
        xdebug ( "WROTE FUNCTION CALL IN BUFFER OF WINS:  %s, filenamesz=%d,"
                 " filename=%s, filetype=%d", 
                 funcMap[funcID], filenamesz, filename, filetype );
      }
      break;
    case STREAMDEFVLIST:
      {
        int streamID, vlistID;

        xassert ( argc == 2 );
        streamID  = va_arg ( ap, int );
        vlistID   = va_arg ( ap, int );
        
        for ( collID = 0; collID < nProcsColl; collID++ ) 
          {
            size = ( winBufferOverheadFuncCall + 2 ) * sizeof ( int );
            xassert ( winBufferHead[collID] - winBuffer[collID] + size < 
                      winBufferSize[collID] );  

            if ( winPostSet[collID] ) 
              { 
                xmpi ( MPI_Win_wait ( win[collID] )); 
                winPostSet[collID] = 0;
                modelWinFlushBuffer ( collID );
              }
            modelWinBufferPutAtEnd ( __func__, collID, &tokenFuncCall, 
                                     sizeof ( tokenFuncCall));
            modelWinBufferPutAtEnd ( __func__, collID, &funcID, 
                                     sizeof ( funcID ));
            modelWinBufferPutAtEnd ( __func__, collID, &streamID, 
                                     sizeof ( streamID ));
            modelWinBufferPutAtEnd ( __func__, collID, &vlistID, 
                                     sizeof ( streamID ));
            modelWinBufferPutAtEnd ( __func__, collID, &tokenSep, 
                                     sizeof ( tokenSep ));
          }
        
        xdebug ( "WROTE FUNCTION CALL IN BUFFER OF WINS:  %s, streamID=%d,"
                 " vlistID=%d", 
                 funcMap[funcID], streamID, vlistID );
      }
      break;
    default:
      xabort ( "FUNCTION NOT MAPPED!" );
    }
  
  va_end ( ap );  
}

#endif

/*****************************************************************************/

int pioInqVarDecoChunk ( int vlistID, int varID )
{
#ifdef USE_MPI
   int rankModel = commInqRankModel ();
   xassert ( rankModel != CDI_UNDEFID );
   return vlistInqVarDecoChunk ( vlistID, varID, rankModel );
#endif
   return vlistInqVarDecoChunk ( vlistID, varID, CDI_UNDEFID );
}

/*****************************************************************************/

int pioInqVarDecoOff ( int vlistID, int varID )
{
#ifdef USE_MPI
   int rankModel = commInqRankModel ();
   xassert ( rankModel != CDI_UNDEFID );
   return vlistInqVarDecoOff ( vlistID, varID, rankModel );
#endif
   return vlistInqVarDecoOff ( vlistID, varID, CDI_UNDEFID );
}

/*****************************************************************************/

int pioInit ( int commGlobArg, int nProcsIO, int IOMode )
{
#ifdef USE_MPI
  MPI_Comm commGlob;
  int rank;
#endif

  xdebug ( "commGlobArg=%d, nProcsIO = %d, IOMode = %d ", 
           commGlobArg, nProcsIO, IOMode );

#ifdef USE_MPI
  commGlob = MPI_COMM_NULL;
  commGlob = MPI_Comm_f2c (( MPI_Fint ) commGlobArg );
  xassert ( commGlob != MPI_COMM_NULL );

  xmpi ( MPI_Comm_rank ( commGlob, &rank ));
  xdebug ( "####################### rank=%d", rank );
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
MPI_Comm pioInit_c ( MPI_Comm commGlob, int nProcsIO, int IOMode, 
                     int nNamespaces, int * hasLocalFile )
{
  int sizeGlob;

  if ( IOMode < PIO_MINIOMODUS || IOMode > PIO_MAXIOMODUS )
    xabort ( "IOMODE IS NOT VALID." );
  
#ifdef _SX
  if ( IOMode ==  PIO_POSIX_ASYNCH )
    xabort ( "PIO_POSIX_ASYNCH DOES NOT WORK ON SX." );
#endif

  commInit ();
  commDefCommGlob ( commGlob );
  sizeGlob = commInqSizeGlob ();
                                      
  if ( nProcsIO <= 0 || nProcsIO > sizeGlob )
    xabort ( "DISTRIBUTION OF TASKS ON PROCS IS NOT VALID." );

  commDefNProcsIO ( nProcsIO );
  commDefIOMode   ( IOMode, PIO_MAXIOMODUS, PIO_MINIOMODUSWITHSPECIALPROCS );
  commDefCommPio  ();
 
  // JUST FOR TEST CASES WITH ONLY ONE MPI TASK
  if ( commInqRankGlob () == 0 && commInqSizeGlob () == 1 ) 
    {
      xdebug ();
      namespaceInit ( nNamespaces, hasLocalFile );
      return commInqCommGlob ();
    }

  xassert ( commInqNProcsModel () > 0 );

  if ( commInqIsProcIO ()) 
    { 
      IOServer ();
      commDestroy ();
      MPI_Finalize ();
      exit ( 0 );
    }
  else
    { 
      commEvalPhysNodes ();
      commDefCommsIO ();  
      xdebug ( "nNamespaces=%d, hasLocalFile= %d, %d",
               nNamespaces, hasLocalFile[0], hasLocalFile[1] );
      namespaceInit ( nNamespaces, hasLocalFile );     
    }
  
  return commInqCommModel ();
}
#endif

/************************************************************************/

void  pioEndDef ( void )
{
#ifdef USE_MPI
  char   * buffer;
  int bufferSize;
  int rankGlob = commInqRankGlob ();

  varsMapNDeco ( commInqNNodes (), commInqNodeSizes ());

  if ( rankGlob < commInqNProcsColl ())
    {
      reshPackBufferCreate ( &buffer, &bufferSize, commInqCommsIO ( rankGlob ));
      
      xmpi ( MPI_Send ( buffer, bufferSize, MPI_PACKED, commInqNProcsModel (), 
                        RESOURCES, commInqCommsIO ( rankGlob )));
      
      xdebug ( "SENT MESSAGE WITH TAG \"RESOURCES\"" );

      namespaceDefResASent ();
      
      reshPackBufferDestroy ( &buffer );
    }

  modelWinCreate ();
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
  int collID, ibuffer = 1111;

  namespaceCleanup (); 
  for ( collID = 0; collID < commInqNProcsColl (); collID++ )
    {
      xmpi ( MPI_Send ( &ibuffer, 1, MPI_INT, commInqNProcsModel (), 
                        FINALIZE, commInqCommsIO ( collID )));
      xdebug ( "SENT MESSAGE WITH TAG \"FINALIZE\"" );
    }
  modelWinCleanup ();
  commDestroy (); 
#endif
} 
 
 /************************************************************************/
 
void pioWriteTimestep ( int ts, int vdate, int vtime )
{
#ifdef USE_MPI
  int collID, buffer[timestepSize], iAssert = 0;
  int tokenEnd = END;
  int rankGlob = commInqRankGlob ();
  int nProcsColl = commInqNProcsColl ();
  int nProcsModel = commInqNProcsModel ();

  xassert ( ts            >= 0     &&
            vdate         >= 0     &&
            vtime         >= 0     &&
            winPostSet    != NULL  &&
            win           != NULL );

  buffer[0] = ts;
  buffer[1] = vdate;
  buffer[2] = vtime;
  
  if ( rankGlob < nProcsColl ) 
    {
      xmpi ( MPI_Send ( &buffer[0], timestepSize, MPI_INTEGER, nProcsModel, 
                        WRITETS, commInqCommsIO ( rankGlob )));
      xdebug ( "SENT MESSAGE WITH TAG \"WRITETS\"" );
    }

  for ( collID = 0; collID < nProcsColl; collID++ )
    {
      xassert ( winPostSet[collID] != 1 );
      modelWinBufferPutAtEnd ( __func__, collID, &tokenEnd, sizeof ( tokenEnd ));
      xmpi ( MPI_Win_post ( groupsIONetto[collID], iAssert, win[collID] ));
      winPostSet[collID] = 1;
    } 
#endif
}


/************************************************************************/
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
