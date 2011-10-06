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

void pioinfoInit ( pioInfo_t * p )
{
  p->IOModus         = CDI_UNDEFID;
  p->nProcsIO        = CDI_UNDEFID;

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


void pioInfoNew ( void )
{
  pioinfo2 = xmalloc ( sizeof ( pioInfo_t ));
  pioinfoInit ( pioinfo2 );
}


void pioInfoDestroy ( void )
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
  if ( pioinfo2->commModel != MPI_COMM_NULL )
    xmpi ( MPI_Comm_free ( &pioinfo2->commModel ));
  if ( pioinfo2->commPio != MPI_COMM_NULL )
    xmpi ( MPI_Comm_free ( &pioinfo2->commPio ));    

  free ( pioinfo2 );
}




#endif
