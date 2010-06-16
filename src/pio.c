#ifndef NOMPI

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

#include "cdi.h"
#include "pio.h"
#include "pio_impl.h"

bool ddebug = false;

const char *token_NODE = "p";

long initial_buffersize = 16 * 1024 * 1024;
/*  4 KB <= x < 256 MB */ 
/* 16 * 1024 * 1024; */
/* 256 * 1024; */
/* 4 * 1024; */

double startTime;
double accumProbe   = 0.0;
double accumRecv    = 0.0;
double accumSend    = 0.0;
double accumSuspend = 0.0;
double accumWait    = 0.0;
double accumWrite   = 0.0;

pioInfo *pioinfo;

MPI_Request request = MPI_REQUEST_NULL;

char *token = "%";

/*****************************************************************************/

void check_mpi ( int line, int iret )
{
  char error_string[MPI_MAX_ERROR_STRING+1];
  int len;

  if  ( iret != MPI_SUCCESS ) 
    {
      MPI_Error_string ( iret, error_string, &len ); 
      error_string[len] = '\0';
      fprintf ( stderr,"\nLine %8d MPI error %4d: %s\n\n", line, iret, 
		error_string ); 
    }

  return;
}

/***************************************************************/

int setTag ( int ID, int sc )
{
  return ID * 1000 + sc;
}

/***************************************************************/

rTag * getTag ( int tag )
{
  rTag *rtag;

  rtag = ( rTag * ) malloc ( sizeof ( rTag ));
  rtag->id = tag / 1000;
  rtag->command = tag % 1000;

  return rtag;
}

/***************************************************************/

size_t pioFileWrite ( int id, int tsId, const void *buffer, size_t len )
{
  size_t iret;

  switch ( pioinfo->type )
    {
    case PIO_MPI_NONB:
      iret = fwMPINONB ( id, tsId, buffer, len );
      break;
    case PIO_POSIX_ASYNCH:
      iret = fwPOSIXASYNCH ( id, tsId, buffer, len );
      break;
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fwPOSIXFPGUARDSENDRECV ( id, tsId, buffer, len );
      break;
    case PIO_POSIX_NONB:
      iret = fwPOSIXNONB ( id, tsId, buffer, len );
      break;
    }

  return iret;
}

/***************************************************************/

int pioFileClose ( int id )
{
  int iret;
  
  switch ( pioinfo->type )
    {
    case PIO_MPI_NONB:
      iret = fcMPINONB ( id );
      break;
    case PIO_POSIX_ASYNCH:
      iret = fcPOSIXASYNCH ( id );
      break;
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fcPOSIXFPGUARDSENDRECV ( id );
      break;
    case PIO_POSIX_NONB:
      iret = fcPOSIXNONB ( id );
      break;
    }

  return iret;
}

/***************************************************************/

int pioFileOpenW ( const char *filename )
{
  int iret;
  
  switch ( pioinfo->type )
    {
    case PIO_MPI_NONB:
      iret = fowMPINONB ( filename );
      break;
    case PIO_POSIX_ASYNCH:
      iret = fowPOSIXASYNCH ( filename );
      break;
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fowPOSIXFPGUARDSENDRECV ( filename );
      break;
    case PIO_POSIX_NONB:
      iret = fowPOSIXNONB ( filename );
      break;
    }
  
  return iret;
}

/***************************************************************/

int pioInit ( int ptype, int comm, int *color, int *colors, int nnodes, int *pioComm, int *ncollectors )
{
  int collectingData = 1;
  int key, rank, size, npes_node;
  int *allcolors;
  int i,j,k,curr;

  char *cpuName, *temp;


  pioinfo = ( pioInfo * ) malloc ( sizeof ( pioInfo ));
  pioinfo->type = ptype;

  cpuName = (char * ) malloc ( 80 * sizeof(char));
  gethostname(cpuName,80);
  temp = strtok ( cpuName, token_NODE );
  *color = atoi ( temp );
  MPI_Comm_size (( MPI_Comm ) comm, &size );
  MPI_Comm_rank (( MPI_Comm ) comm, &rank );
  npes_node = size/nnodes;
  key = rank % npes_node;
  MPI_Comm_split (( MPI_Comm ) comm, *color, key, ( MPI_Comm *) pioComm );

  pioinfo->comm  = ( MPI_Comm ) *pioComm;
  MPI_Comm_rank ( pioinfo->comm, &( pioinfo->rank ));
  MPI_Comm_size ( pioinfo->comm, &( pioinfo->size ));

  if ( pioinfo->type == PIO_NONE && pioinfo->size != 1 )
    {
      fprintf ( stderr, 
		"PTYPE should be set to a parallel I/O type or npe should be 1." );
      fprintf ( stderr, "PTYPE = %d, npe = %d\n", 
		pioinfo->type , pioinfo->size );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf (stdout, 
	     "pe%d in pioDefPtype(), size=%d, nnodes=%d, cpuName=%s, color=%d,npes_node=%d, key=%d\n", 
	     rank,size, nnodes,cpuName, *color, npes_node, key);

  allcolors = (int * ) malloc ( size * sizeof(int));
  MPI_Allgather ( color, 1, MPI_INTEGER, allcolors, 1, MPI_INTEGER, ( MPI_Comm )comm );

  k = 0;
  for ( i = 0; i < size; i++ )
    {
      curr = allcolors[i] ;
      if ( curr == -1 ) 
	continue;
      else
	{
	  *( colors + k ) = allcolors[i];
	  k++;
	  if ( k == nnodes ) break;
	}
      for ( j = i + 1; j < size; j++ )
	if ( allcolors[j] == curr ) allcolors[j] = -1;
    }

  if ( ddebug && pioinfo->rank == 0 )
    fprintf ( stdout, 
	      "pe%d in pioDefPtype(), ptype=%d, initial_buffersize=%ld: init pioinfo ...\n", 
	      pioinfo->rank, pioinfo->type, initial_buffersize );

  switch ( pioinfo->type )
    {
    case PIO_MPI_NONB:
      collectingData = initMPINONB ( ncollectors );
      break;
    case PIO_POSIX_ASYNCH:
      collectingData = initPOSIXASYNCH ( ncollectors );
      break;
    case PIO_POSIX_FPGUARD_SENDRECV:
      collectingData = initPOSIXFPGUARDSENDRECV ( ncollectors );
      break;
    case PIO_POSIX_NONB:
      collectingData = initPOSIXNONB ( ncollectors );
      break;
    }

  free ( allcolors );
  free ( cpuName );

  return collectingData;
}

/***************************************************************/

void pioFinalize ()
{
  free ( pioinfo );

  return;
}

#endif

