#ifndef NOMPI

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "mpi.h"

#include "cdi.h"
#include "pio.h"
#include "pio_impl.h"

bool ddebug = false;

long initial_buffer_size = 16 * 1024 * 1024; 
/* 16 * 1024 * 1024, 4 KB <= x < 256 MB */

double startTime;
double accumProbe   = 0.0;
double accumRecv    = 0.0;
double accumSend    = 0.0;
double accumSuspend = 0.0;
double accumWait    = 0.0;
double accumWrite   = 0.0;

pioInfo *pioinfo;

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
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fwPOSIXFPGUARDSENDRECV ( id, tsId, buffer, len );
      break;
    case PIO_POSIX_ASYNCH:
      iret = fwPOSIXASYNCH ( id, tsId, buffer, len );
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
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fcPOSIXFPGUARDSENDRECV ( id );
      break;
    case PIO_POSIX_ASYNCH:
      iret = fcPOSIXASYNCH ( id );
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
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fowPOSIXFPGUARDSENDRECV ( filename );
      break;
    case PIO_POSIX_ASYNCH:
      iret = fowPOSIXASYNCH ( filename );
      break;
    }
  
  return iret;
}

/***************************************************************/

int pioInit ( int ptype, int comm, int *ncollectors )
{
  int collectingData;
  int size;

  pioinfo = ( pioInfo * ) malloc ( sizeof ( pioInfo ));

  pioinfo->type = ptype;
  pioinfo->comm  = ( MPI_Comm ) comm;
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

  if ( ddebug && pioinfo->rank == 0 )
    fprintf ( stdout, 
	      "pe%d in pioDefPtype(), ptype=%d, initial_buffer_size=%ld: init pioinfo ...\n", 
	      pioinfo->rank, pioinfo->type, initial_buffer_size );

  switch ( pioinfo->type )
    {
    case PIO_POSIX_FPGUARD_SENDRECV:
      collectingData = initPOSIXFPGUARDSENDRECV ( ncollectors );
      break;
    case PIO_POSIX_ASYNCH:
      collectingData = initPOSIXASYNCH ( ncollectors );
      break;
    }

  return collectingData;
}

/***************************************************************/

void pioFinalize ()
{
  free ( pioinfo );

  return;
}

#endif
