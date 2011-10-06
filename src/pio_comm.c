#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "pio_comm.h"
#include "cdi.h"
#include "pio_util.h"

#ifdef USE_MPI

pioInfo_t * pioinfo2 = NULL;


static
void pioinfoInit ( pioInfo_t * p )
{
  p->IOModus             = CDI_UNDEFID;
  p->nProcsIO            = CDI_UNDEFID;
  p->nProcsModel         = CDI_UNDEFID;
  p->isProcIO            = CDI_UNDEFID;

  p->commGlob            = MPI_COMM_NULL;
  p->sizeGlob            = CDI_UNDEFID;
  p->rankGlob            = CDI_UNDEFID;
  p->root                = CDI_UNDEFID;

  p->commPio             = MPI_COMM_NULL;
  p->sizePio             = CDI_UNDEFID;
  p->rankPio             = CDI_UNDEFID;

  p->commNode            = MPI_COMM_NULL;
  p->sizeNode            = CDI_UNDEFID;
  p->rankNode            = CDI_UNDEFID;
  p->hostname[0]         = 0;
  p->specialRankNode     = CDI_UNDEFID;
     
  p->commColl            = MPI_COMM_NULL;
  p->sizeColl            = CDI_UNDEFID;
  p->rankColl            = CDI_UNDEFID;
     
  p->commCalc            = MPI_COMM_NULL;
  p->sizeCalc            = CDI_UNDEFID;
  p->rankCalc            = CDI_UNDEFID;
     
  p->commsIO             = NULL;
  p->nodeInfo.hostID     = CDI_UNDEFID;
  p->nodeInfo.isProcColl = CDI_UNDEFID;
  p->nodeInfo.nNodes     = CDI_UNDEFID;
  p->nProcsColl          = 0;
  p->nodeSizes           = NULL;
  p->nodeMap             = NULL;
}


void commInit ( void )
{
  pioinfo2 = xmalloc ( sizeof ( pioInfo_t ));
  pioinfoInit ( pioinfo2 );
}


void commDestroy ( void )
{
  int i;

  assert ( pioinfo2 );

  if ( pioinfo2->nodeMap   != NULL ) free ( pioinfo2->nodeMap );
  if ( pioinfo2->nodeSizes != NULL ) free ( pioinfo2->nodeSizes );

  if ( pioinfo2->commsIO ) 
    {
      for ( i = 0; i < pioinfo2->nProcsIO; i++ )
	if ( pioinfo2->commsIO[i] != MPI_COMM_NULL )
	  xmpi ( MPI_Comm_free ( &pioinfo2->commsIO[i] ));
      free ( &pioinfo2->commsIO ); 
    }  
  if ( pioinfo2->commCalc != MPI_COMM_NULL )
    xmpi ( MPI_Comm_free ( &pioinfo2->commCalc ));
  if ( pioinfo2->commColl != MPI_COMM_NULL )
    xmpi ( MPI_Comm_free ( &pioinfo2->commColl ));
  if ( pioinfo2->commNode != MPI_COMM_NULL )
    xmpi ( MPI_Comm_free ( &pioinfo2->commNode ));
  if ( pioinfo2->commPio != MPI_COMM_NULL )
    xmpi ( MPI_Comm_free ( &pioinfo2->commPio ));    

  free ( pioinfo2 );
}


void commDefCommGlob ( MPI_Comm c )
{
  assert ( pioinfo2 != NULL &&
	   c != MPI_COMM_NULL );

  pioinfo2->commGlob = c;
  xmpi ( MPI_Comm_size ( c, &pioinfo2->sizeGlob ));
  xmpi ( MPI_Comm_rank ( c, &pioinfo2->rankGlob ));
  pioinfo2->root = 0;
}
 

MPI_Comm commInqCommGlob ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->commGlob != MPI_COMM_NULL );
  return pioinfo2->commGlob;
}


int commInqSizeGlob ( void )
{
  assert ( pioinfo2 != NULL  &&
           pioinfo2->sizeGlob != CDI_UNDEFID );
  return pioinfo2->sizeGlob;
}


int commInqRankGlob ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->rankGlob != CDI_UNDEFID );
  return pioinfo2->rankGlob;
}


void commDefNProcsIO ( int n )
{
  assert ( pioinfo2 != NULL &&
	   n >= 0 &&
	   n < MAXNPROCSIO &&
           pioinfo2->commGlob != MPI_COMM_NULL );
  pioinfo2->nProcsIO = n;
  pioinfo2->nProcsModel = pioinfo2->sizeGlob - pioinfo2->nProcsIO;
}


int commInqNProcsIO ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->nProcsIO != CDI_UNDEFID );
  return pioinfo2->nProcsIO;
}


int      commInqIsProcIO     ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->isProcIO != CDI_UNDEFID );
  return pioinfo2->isProcIO;
}


int commInqNProcsModel ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->nProcsModel != CDI_UNDEFID );
  return pioinfo2->nProcsModel;
}


void     commDefIOModus  ( int IOModus )
{
  assert ( pioinfo2 != NULL &&
	   IOModus >= 0 &&
	   IOModus <= MAXIOMODUS );
  pioinfo2->IOModus = IOModus;
}


int      commInqIOModus  ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->IOModus != CDI_UNDEFID );
  return pioinfo2->IOModus;
}


void     commDefCommPio  ( void )
{
  int nProcsModel;

  assert ( pioinfo2 != NULL &&
           pioinfo2->commGlob != MPI_COMM_NULL &&
           pioinfo2->IOModus  != CDI_UNDEFID &&
           pioinfo2->nProcsIO != CDI_UNDEFID );

  nProcsModel = pioinfo2->sizeGlob - pioinfo2->nProcsIO;

  pioinfo2->isProcIO = pioinfo2->rankGlob >= nProcsModel ? 1 : 0;

  xmpi ( MPI_Comm_split ( pioinfo2->commGlob, pioinfo2->isProcIO, 0, 
                          &pioinfo2->commPio ));
  xmpi ( MPI_Comm_size ( pioinfo2->commPio, &pioinfo2->sizePio ));
  xmpi ( MPI_Comm_rank ( pioinfo2->commPio, &pioinfo2->rankPio )); 
}


MPI_Comm commInqCommPio  ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->commPio != MPI_COMM_NULL );
  return pioinfo2->commPio;
}


MPI_Comm commInqCommModel( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->commPio != MPI_COMM_NULL );
  return pioinfo2->commPio;
}


void     commDefIsProcColl  ( int isProcColl )
{
  assert ( pioinfo2 != NULL );
  pioinfo2->nodeInfo.isProcColl = isProcColl;
}


void     commDefCommNode    ( MPI_Comm commNode )
{
  assert ( pioinfo2 != NULL );
  pioinfo2->commNode = commNode;  
}


void     commDefHostName    ( char * hostname, size_t len )
{
  assert ( pioinfo2 != NULL );
  strncpy ( pioinfo2->hostname, hostname, len );
}


void     commDefHostID      ( int hostID )
{
  assert ( pioinfo2 != NULL );
  pioinfo2->nodeInfo.hostID = hostID;
}


void commSendNodeInfo ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->root                != CDI_UNDEFID &&
           pioinfo2->nodeInfo.hostID     != CDI_UNDEFID &&
           pioinfo2->nodeInfo.isProcColl != CDI_UNDEFID &&
           pioinfo2->nodeInfo.nNodes     != CDI_UNDEFID &&
           pioinfo2->commGlob            != MPI_COMM_NULL );
  
  xmpi ( MPI_Send ( &pioinfo2->nodeInfo, sizeNodeInfo, MPI_INTEGER, 
                    pioinfo2->root, NODEINFO, pioinfo2->commGlob ));
}


void     commRecvNodeMap    ( void )
{
  MPI_Status status;

  assert ( pioinfo2 != NULL &&
           pioinfo2->nProcsColl != CDI_UNDEFID &&
           pioinfo2->commGlob   != MPI_COMM_NULL &&
           pioinfo2->nProcsModel != CDI_UNDEFID &&
           pioinfo2->nodeMap == NULL );

  pioinfo2->nodeMap = xmalloc ( pioinfo2->nProcsColl * 
                                sizeof ( pioinfo2->nodeMap[0] )); 

  xmpi ( MPI_Recv ( pioinfo2->nodeMap, pioinfo2->nProcsColl, MPI_INTEGER, 
                    pioinfo2->rankGlob - pioinfo2->nProcsModel, NODEMAP, 
                    pioinfo2->commGlob, &status ));
}


void     commDefNNodes      ( int nNodes )
{
  assert ( pioinfo2 != NULL );
  pioinfo2->nodeInfo.nNodes = nNodes;
}


void     commDefNProcsColl  ( int nProcsColl )
{
  assert ( pioinfo2 != NULL );
  pioinfo2->nProcsColl = nProcsColl;
}


void     commEvalPhysNodes  ( void )
{
  nodeInfo_t * nodeInfo;
  MPI_Status status;
  int i, IOID, size, nNodes = CDI_UNDEFID;
  int ** p1, ** p2,  idx;

  assert ( pioinfo2 != NULL &&
           pioinfo2->root        != CDI_UNDEFID &&
           pioinfo2->nProcsIO    != CDI_UNDEFID &&
           pioinfo2->nProcsModel != CDI_UNDEFID &&
           pioinfo2->sizeGlob    != CDI_UNDEFID &&
           pioinfo2->rankGlob    != CDI_UNDEFID &&
           pioinfo2->commGlob    != MPI_COMM_NULL );
  xdebug();

  size = pioinfo2->nProcsIO * sizeNodeInfo;

  nodeInfo = xmalloc ( size * sizeof ( int ));

  if ( pioinfo2->rankGlob == pioinfo2->root )
    {
      assert ( pioinfo2->rankPio == pioinfo2->root );
 
      for ( i = pioinfo2->nProcsModel; i < pioinfo2->sizeGlob; i++ )
        {
          IOID = i - pioinfo2->nProcsModel;
          xmpi ( MPI_Recv ( nodeInfo + IOID, sizeNodeInfo, MPI_INTEGER, i, 
                            NODEINFO, pioinfo2->commGlob, &status ));
        }
    }

  xmpi ( MPI_Bcast ( nodeInfo, size, MPI_INTEGER, pioinfo2->root, 
                     pioinfo2->commPio ));

  // consistency check, count collectors
  for ( IOID = 0; IOID < pioinfo2->nProcsIO; IOID++ )
    {
      if ( IOID == 0 )
        {
          assert ( nodeInfo[IOID].nNodes > 0 &&
                   nodeInfo[IOID].nNodes <= pioinfo2->nProcsIO );
          nNodes = nodeInfo[IOID].nNodes;
        }
      else assert ( nodeInfo[IOID].nNodes == nNodes );         
      assert ( nodeInfo[IOID].hostID > 0 &&
               nodeInfo[IOID].hostID <= nNodes );
      if ( nodeInfo[IOID].isProcColl ) pioinfo2->nProcsColl++;
    }

  // define nodeSizes
  pioinfo2->nodeInfo.nNodes = nNodes; 
  pioinfo2->nodeSizes = xmalloc ( pioinfo2->nodeInfo.nNodes * 
                                  sizeof ( pioinfo2->nodeSizes[0] ));
  for ( IOID = 0; IOID < pioinfo2->nProcsIO; IOID++ )
    if ( nodeInfo[IOID].isProcColl )
      pioinfo2->nodeSizes[nodeInfo[IOID].hostID - 1]++;

  // define nodeMap
  pioinfo2->nodeMap   = xmalloc ( pioinfo2->nProcsColl * 
                                  sizeof ( pioinfo2->nodeMap[0] ));
  // helpers
  p1 = xmalloc ( pioinfo2->nodeInfo.nNodes * sizeof ( p1[0] ));
  p2 = xmalloc ( pioinfo2->nodeInfo.nNodes * sizeof ( p2[0] ));
  idx = 0;
  for ( i = 0; i < pioinfo2->nodeInfo.nNodes; i++ )
    {
      assert ( idx >= 0 && idx < pioinfo2->nProcsColl );
      p1[i] = &pioinfo2->nodeMap[idx];
      p2[i] = p1[i] + pioinfo2->nodeSizes[i]; 
      idx += pioinfo2->nodeSizes[i];
    }
  assert ( idx == pioinfo2->nProcsColl );
    
  for ( IOID = 0; IOID < pioinfo2->nProcsIO; IOID++ )
    {
      assert ( p1[nodeInfo[IOID].hostID - 1] <  p2[nodeInfo[IOID].hostID - 1] );
      if ( nodeInfo[IOID].isProcColl )
        * p1[nodeInfo[IOID].hostID - 1]++ = IOID + pioinfo2->nProcsModel;
    }

  if ( pioinfo2->rankGlob < pioinfo2->nProcsIO )
    xmpi ( MPI_Send ( pioinfo2->nodeMap, pioinfo2->nProcsColl, MPI_INTEGER, 
                    pioinfo2->rankGlob + pioinfo2->nProcsModel, 
                      NODEMAP, pioinfo2->commGlob ));

  free ( p2 );
  free ( p1 );
  free ( nodeInfo );
  xdebug();
}

// collective call
void     commDefCommsIO     ( void )
{
  MPI_Group groupGlob, currGroupIO;
  int IOID, * ranks, i, currIORank;
  char name[MAXCOMMIONAME];

  pioinfo2->commsIO = xmalloc ( pioinfo2->nProcsColl * 
                                sizeof ( pioinfo2->commsIO[0] ));
  strncpy ( name, "COMMSIO_", 8 ); 
  name[MAXCOMMIONAME - 1] = '\0';
  
  ranks = xmalloc (( pioinfo2->nProcsModel + 1 ) * sizeof ( ranks[0] ));
  for ( i = 0; i < pioinfo2->nProcsModel; i++ )
    * ( ranks + i ) = i;
  
  xmpi ( MPI_Comm_group ( pioinfo2->commGlob, &groupGlob )); 

  for ( IOID = 0; IOID < pioinfo2->nProcsColl; IOID++ )
    {
      currIORank = pioinfo2->nodeMap[IOID];
      * ( ranks + pioinfo2->nProcsModel ) = currIORank;	  
      xmpi ( MPI_Group_incl ( groupGlob, pioinfo2->nProcsModel + 1, 
                              ranks, &currGroupIO ));
      xmpi ( MPI_Comm_create ( pioinfo2->commGlob, currGroupIO,  
                               pioinfo2->commsIO + IOID ));
      if ( pioinfo2->rankGlob == currIORank )
	pioinfo2->commCalc = pioinfo2->commsIO[IOID];

      if ( pioinfo2->rankGlob < pioinfo2->nProcsModel || 
           pioinfo2->rankGlob == currIORank )
	{
	  sprintf ( &name[8], "%d", IOID ); 
	  xmpi ( MPI_Comm_set_name ( pioinfo2->commsIO[IOID], name ));
	}
    }

  if ( ddebug >= 2 )
    {

      if ( pioinfo2->rankGlob < pioinfo2->nProcsModel )
	for ( IOID = 0; IOID < pioinfo2->nProcsColl; IOID++ ) 
          xdebugComm ( pioinfo2->commsIO + IOID );
      else if ( pioinfo2->nodeInfo.isProcColl )
	xdebugComm ( &pioinfo2->commCalc );
    }

  xmpi ( MPI_Group_free ( &currGroupIO ));
  xmpi ( MPI_Group_free ( &groupGlob ));
  free ( ranks );

  commPrint ( stdout );
}


void commPrint ( FILE * fp )
{
  int i;

  assert ( pioinfo2 != NULL );

  fprintf ( fp, "\n" );
  fprintf ( fp, "######## pioinfo PE%d ###########\n", pioinfo2->rankGlob );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# IOModus     = %d\n", pioinfo2->IOModus );
  fprintf ( fp, "# nProcsIO    = %d\n", pioinfo2->nProcsIO );
  fprintf ( fp, "# nProcsModel = %d\n", pioinfo2->nProcsModel );
  fprintf ( fp, "# isProcIO    = %d\n", pioinfo2->isProcIO );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commGlob    = %d\n", pioinfo2->commGlob );
  fprintf ( fp, "# sizeGlob    = %d\n", pioinfo2->sizeGlob );
  fprintf ( fp, "# rankGlob    = %d\n", pioinfo2->rankGlob );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commPio     = %d\n", pioinfo2->commPio );
  fprintf ( fp, "# sizePio     = %d\n", pioinfo2->sizePio );
  fprintf ( fp, "# rankPio     = %d\n", pioinfo2->rankPio );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commNode    = %d\n", pioinfo2->commNode );
  fprintf ( fp, "# sizeNode    = %d\n", pioinfo2->sizeNode );
  fprintf ( fp, "# rankNode    = %d\n", pioinfo2->rankNode );
  fprintf ( fp, "# hostname    = %s\n", pioinfo2->hostname[0] == 0 ? "nn" : 
	    pioinfo2->hostname );
  fprintf ( fp, "# specialRankNode = %d\n", pioinfo2->specialRankNode );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commColl    = %d\n", pioinfo2->commColl );
  fprintf ( fp, "# sizeColl    = %d\n", pioinfo2->sizeColl );
  fprintf ( fp, "# rankColl    = %d\n", pioinfo2->rankColl );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commCalc    = %d\n", pioinfo2->commCalc );
  fprintf ( fp, "# sizeCalc    = %d\n", pioinfo2->sizeCalc );
  fprintf ( fp, "# rankCalc    = %d\n", pioinfo2->rankCalc );
  if ( pioinfo2->commsIO != NULL )
    {
      fprintf ( fp, "#\n" );
      for ( i = 0; i < pioinfo2->nProcsIO; i++ )
	fprintf ( fp, "# commsIO[%d]    = %d\n", i, pioinfo2->commsIO[i] );
    }
  else
    fprintf ( fp, "# commsIO     = NULL\n" );
  fprintf ( fp, "# hostID      = %d\n", pioinfo2->nodeInfo.hostID );
  fprintf ( fp, "# isProcColl  = %d\n", pioinfo2->nodeInfo.isProcColl );
  fprintf ( fp, "# nNodes      = %d\n", pioinfo2->nodeInfo.nNodes );
  fprintf ( fp, "# nProcsColl  = %d\n", pioinfo2->nProcsColl );
  if ( pioinfo2->nodeSizes != NULL )
    {
      fprintf ( fp, "# nodeSizes   = " );
      assert ( pioinfo2->nodeInfo.nNodes != CDI_UNDEFID );
      for ( i = 0; i < pioinfo2->nodeInfo.nNodes; i++ )
        fprintf ( fp, "%d ", pioinfo2->nodeSizes[i] );
      fprintf ( fp, "\n" );
    }
  else     
    fprintf ( fp, "# nodeSizes   = NULL\n" );
  if ( pioinfo2->nodeMap != NULL )
    {
      fprintf ( fp, "# nodeMap     = " );
      assert ( pioinfo2->nProcsIO != CDI_UNDEFID );
      for ( i = 0; i < pioinfo2->nProcsIO; i++ )
        fprintf ( fp, "%d ", pioinfo2->nodeMap[i] );
      fprintf ( fp, "\n" );
    }
  else     
    fprintf ( fp, "# nodeMap     = NULL\n" );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "############################\n" );
  fprintf ( fp, "\n" );
}

#endif


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
