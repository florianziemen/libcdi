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
  p->IOModus         = CDI_UNDEFID;
  p->nProcsIO        = CDI_UNDEFID;
  p->nProcsModel     = CDI_UNDEFID;
  p->role            = CDI_UNDEFID;

  p->commGlob        = MPI_COMM_NULL;
  p->sizeGlob        = CDI_UNDEFID;
  p->rankGlob        = CDI_UNDEFID;

  p->commPio         = MPI_COMM_NULL;
  p->sizePio         = CDI_UNDEFID;
  p->rankPio         = CDI_UNDEFID;

  p->commModel       = MPI_COMM_NULL;
  p->sizeModel       = CDI_UNDEFID;
  p->rankModel       = CDI_UNDEFID;

  p->commNode        = MPI_COMM_NULL;
  p->sizeNode        = CDI_UNDEFID;
  p->rankNode        = CDI_UNDEFID;
  p->hostname[0]     = 0;
  p->hostID          = CDI_UNDEFID;
  p->specialRankNode = CDI_UNDEFID;
     
  p->commColl        = MPI_COMM_NULL;
  p->sizeColl        = CDI_UNDEFID;
  p->rankColl        = CDI_UNDEFID;
     
  p->commCalc        = MPI_COMM_NULL;
  p->sizeCalc        = CDI_UNDEFID;
  p->rankCalc        = CDI_UNDEFID;
     
  p->commsIO         = NULL;
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


int      commInqRole     ( void )
{
  assert ( pioinfo2 != NULL &&
           pioinfo2->role != CDI_UNDEFID );
  return pioinfo2->role;
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

  pioinfo2->role = pioinfo2->rankGlob >= nProcsModel ? ISPROCIO : ISPROCMODEL;

  xmpi ( MPI_Comm_split ( pioinfo2->commGlob, pioinfo2->role, 0, 
                          &pioinfo2->commPio ));
  xmpi ( MPI_Comm_size ( pioinfo2->commPio, &pioinfo2->sizePio ));
  xmpi ( MPI_Comm_rank ( pioinfo2->commPio, &pioinfo2->rankPio )); 

  pioinfo2->commModel = pioinfo2->commPio;
  pioinfo2->sizeModel = pioinfo2->sizePio;
  pioinfo2->rankModel = pioinfo2->rankPio;
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
           pioinfo2->commModel != MPI_COMM_NULL );
  return pioinfo2->commModel;
}


void commPrint ( FILE * fp )
{
  int i;

  assert ( pioinfo2 != NULL );

  fprintf ( fp, "\n" );
  fprintf ( fp, "######## pioinfo ###########\n" );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# IOModus     = %d\n", pioinfo2->IOModus );
  fprintf ( fp, "# nProcsIO    = %d\n", pioinfo2->nProcsIO );
  fprintf ( fp, "# nProcsModel = %d\n", pioinfo2->nProcsModel );
  fprintf ( fp, "# role        = %d\n", pioinfo2->role );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commGlob    = %d\n", pioinfo2->commGlob );
  fprintf ( fp, "# sizeGlob    = %d\n", pioinfo2->sizeGlob );
  fprintf ( fp, "# rankGlob    = %d\n", pioinfo2->rankGlob );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commPio     = %d\n", pioinfo2->commPio );
  fprintf ( fp, "# sizePio     = %d\n", pioinfo2->sizePio );
  fprintf ( fp, "# rankPio     = %d\n", pioinfo2->rankPio );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commModel   = %d\n", pioinfo2->commModel );
  fprintf ( fp, "# sizeModel   = %d\n", pioinfo2->sizeModel );
  fprintf ( fp, "# rankModel   = %d\n", pioinfo2->rankModel );
  fprintf ( fp, "#\n" );
  fprintf ( fp, "# commNode    = %d\n", pioinfo2->commNode );
  fprintf ( fp, "# sizeNode    = %d\n", pioinfo2->sizeNode );
  fprintf ( fp, "# rankNode    = %d\n", pioinfo2->rankNode );
  fprintf ( fp, "# hostname    = %s\n", pioinfo2->hostname[0] == 0 ? "nn" : 
	    pioinfo2->hostname );
  fprintf ( fp, "# hostID      = %d\n", pioinfo2->hostID );
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
