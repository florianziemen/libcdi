#ifndef NOMPI

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mpi.h"
#include "cdi.h"
#include "pio.h"
#include "pio_impl.h"

bool ddebug = false;

char * command2charP[6] = {"IO_Open_file", "IO_Close_file",
			   "IO_Get_fp","IO_Set_fp",
			   "IO_Send_buffer", "IO_Finalize"};

long initial_buffersize = 16 * 1024 * 1024;
/*  4 KB <= x < 256 MB */ 
/* 16 * 1024 * 1024; */
/* 16 * 1024; */
/* 4 * 1024; */

int maxPtype = 6;
int maxNnodes  = 249;
int tagKey = 100;

double startTime;
double accumProbe   = 0.0;
double accumRecv    = 0.0;
double accumSend    = 0.0;
double accumSuspend = 0.0;
double accumWait    = 0.0;
double accumWrite   = 0.0;

pioInfo *pioinfo;

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

/*****************************************************************************/

void check_mpi_status ( int line, MPI_Comm *comm, MPI_Status *status, int iret )
{
  char error_string[MPI_MAX_ERROR_STRING];
  int len;

  if ( iret == MPI_ERR_IN_STATUS )
    {
      switch ( status->MPI_ERROR )
	{
	  fprintf ( stdout, "------- checking error in request ----------\n" );
	case MPI_SUCCESS :
	  fprintf ( stdout, "-------- mpi_success -----------\n" );
	  break;
	case MPI_ERR_PENDING:
	  fprintf ( stdout, "-------- mpi_err_pending ----------\n");
	  break;
	default:
	  MPI_Error_string ( status->MPI_ERROR, error_string, &len );
	  fprintf ( stdout, "---------- error in request, error string: --------\n");
	  fprintf ( stdout, error_string );
	  MPI_Abort ( *comm, 1 );
	}
    }
  else 
    check_mpi ( line, iret );
  
  return;
}

/***************************************************************/

int setTag ( int ID, int sc )
{
  return ID * tagKey + sc;
}

/***************************************************************/

rTag * getTag ( int tag )
{
  rTag *rtag;

  rtag = ( rTag * ) malloc ( sizeof ( rTag ));
  rtag->id = tag / tagKey;
  rtag->command = tag % tagKey;

  return rtag;
}

/***************************************************************/

void ungetTag ( rTag *rtag )
{
  free ( rtag );
  return;
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
    case PIO_POSIX_FPGUARD_THREAD:
      iret = fwPOSIXFPGUARDTHREAD ( id, tsId, buffer, len );
      break;
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      iret = fwPOSIXFPGUARDTHREADREFUSE ( id, tsId, buffer, len );
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
    case PIO_POSIX_FPGUARD_THREAD:
      iret = fcPOSIXFPGUARDTHREAD ( id );
      break;
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      iret = fcPOSIXFPGUARDTHREADREFUSE ( id );
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
    case PIO_POSIX_FPGUARD_THREAD:
      iret = fowPOSIXFPGUARDTHREAD ( filename );
      break;
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      iret = fowPOSIXFPGUARDTHREADREFUSE ( filename );
      break;
    case PIO_POSIX_NONB:
      iret = fowPOSIXNONB ( filename );
      break;
    }
  
  return iret;
}

/***************************************************************/

static int cmpr ( const void *a, const void *b ) 
{ 
  return strcmp ( *( char ** ) a, *( char ** ) b);
}

/***************************************************************/

void setPioCommunicator ( MPI_Comm *myComm, MPI_Comm commF2C, int *color, 
			  int nnodes, int * myComm2F )
{
  int size, rank, len, npes_node, key, test, i, j;

  char *myHost, **allHosts, *curr;
  char hostname [ MPI_MAX_PROCESSOR_NAME + 1 ];

  MPI_Comm_size ( commF2C, &size );
  MPI_Comm_rank ( commF2C, &rank );

  myHost = (char * ) malloc (( MPI_MAX_PROCESSOR_NAME + 1 ) * sizeof( char ));

  MPI_Get_processor_name ( myHost, &len );

  if ( ddebug )
    {
      strncpy ( hostname, myHost, len );
      hostname [ len ] = '\0';
      fprintf ( stdout, "pe%d: myHost = %s\n", rank,  hostname );
    }


  allHosts = ( char ** ) malloc ( size * sizeof ( char * ));
  allHosts[0] = ( char * ) malloc 
    ( size * MPI_MAX_PROCESSOR_NAME * sizeof ( char ));
  for ( i = 1; i < size; i++ )
    allHosts[i] = allHosts[0] + i * MPI_MAX_PROCESSOR_NAME;

  MPI_Allgather ( myHost, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 
		  &( allHosts[0][0] ), MPI_MAX_PROCESSOR_NAME, MPI_CHAR, commF2C );
  
  if ( ddebug )
    for ( i = 0; i < size; i++ )
      {
	strcpy ( hostname, &( allHosts[i][0] ));
	fprintf ( stdout, "pe%d: before qsort, allHosts[%d][0] = %s\n", 
		  rank, i, hostname );
      }  

  qsort ( allHosts, size, sizeof ( char * ), cmpr );
  
  if ( ddebug )
    for ( i = 0; i < size; i++ )
      {
	strcpy ( hostname, &( allHosts[i][0] ));
	fprintf ( stdout, "pe%d: after qsort, allHosts[%d][0] = %s\n", 
		  rank, i, hostname );
      }

  *color = 0;
  i = 0;
  j = 0;
  while ( ++j <= nnodes )
    {
      curr = allHosts[i];
      if (( test = strcmp ( myHost, curr )) == 0 ) *color = j;
      while (( ++i < size ) && 
	     (( test = strcmp ( allHosts[i], curr )) == 0));;    
    } 

  if ( *color <= 0 || *color > nnodes )
    {
      fprintf ( stderr, "color not set correctly\n" );
      MPI_Abort ( commF2C, 1 );
    }
  
  npes_node = size/nnodes;
  key = rank % npes_node;

  MPI_Comm_split ( commF2C, *color, key, myComm);
  MPI_Errhandler_set ( *myComm, MPI_ERRORS_RETURN );
  
  if ( ddebug )
    fprintf ( stdout, "pe%d, after mpi_comm_split, color=%d\n", 
	      rank, *color );

  *myComm2F = 0;
  if (( *myComm2F = MPI_Comm_c2f ( *myComm )) == 0 )
    {
      fprintf ( stderr, "mpi_comm_c2f didn't succeed\n");
      MPI_Abort ( commF2C , 1 );
    }
  
  if ( ddebug )
    fprintf ( stdout, "pe%d, after mpi_comm_c2f, color=%d\n", 
	      rank, *color );


  free ( allHosts[0] );
  free ( allHosts );
  free ( myHost );
  
  return;
}

#endif

/***************************************************************/

int pioInit ( int ptype, int commF, int *color, int nnodes, int *pioComm, int *ncollectors )
{

  int collectingData = 1;
  int rank;

#ifndef NOMPI

  MPI_Comm comm;

  if ( ptype < 0 || ptype > maxPtype )
    {
      fprintf ( stderr, "1. arg of pioInit is no valid modus\n");
      MPI_Abort ( MPI_COMM_WORLD, 1 );
    }

  if (( comm = MPI_Comm_f2c (( MPI_Fint ) commF )) == NULL )
    {
      fprintf ( stderr, "2. arg of pioInit is no valid Fortran handle to a mpi communicator\n");
      MPI_Abort ( MPI_COMM_WORLD, 1 );
    }

  if ( nnodes < 1 || nnodes > maxNnodes )
    {
      fprintf ( stderr, "4. arg of pioInit is no valid no for nnodes\n");
      MPI_Abort ( MPI_COMM_WORLD, 1 );
    }


  if ( !(( ptype == PIO_NONE && nnodes == 1 ) ||
	 ( ptype == PIO_MPI_NONB ) ||
	 ( ptype == PIO_POSIX_FPGUARD_SENDRECV && nnodes == 1 ) ||
	 ( ptype == PIO_POSIX_NONB && nnodes == 1 )))
    {
      MPI_Comm_rank ( comm, &rank );     
      if ( rank == 0 )
	fprintf ( stderr, "\n\npioInit(): arg(1) PTYPE=%d and arg(4) NNODES=%d are not valid. \
\nPossible combinations are:\
\nptype = PIO_NONE (:=0), \t\t  nnodes = 1\
\nptype = PIO_MPI_NONB (:=1)\
\nptype = PIO_POSIX_FPGUARD_SENDRECV (:=3), \t nnodes = 1\
\nptype = PIO_POSIX_NONB (:=6), \t\t nnodes = 1\n\n\n",
		ptype, nnodes );
      MPI_Abort ( comm, 1 );
    }

  if (( pioinfo = ( pioInfo * ) malloc ( sizeof ( pioInfo ))) == NULL )
    {
      fprintf ( stderr, "malloc of pioInfo didn't succeed\n");
      MPI_Abort ( MPI_COMM_WORLD, 1 );
    }

  pioinfo->type = ptype; 
  pioinfo->collectorComm = MPI_COMM_NULL;
  setPioCommunicator ( & ( pioinfo->comm ), comm, color, nnodes, pioComm );
  pioinfo->color = *color;
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
    case PIO_POSIX_FPGUARD_THREAD:
      collectingData = initPOSIXFPGUARDTHREAD ( ncollectors );
      break;
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      collectingData = initPOSIXFPGUARDTHREADREFUSE ( ncollectors );
      break;
    case PIO_POSIX_NONB:
      collectingData = initPOSIXNONB ( ncollectors );
      break;
    }

#endif

  return collectingData;
}

/***************************************************************/

void pioFinalize ()
{
#ifndef NOMPI
  switch ( pioinfo->type )
    {
    case PIO_POSIX_FPGUARD_THREAD:
      finalizePOSIXFPGUARDTHREAD ();
      break;
    }

  if ( pioinfo->collectorComm != MPI_COMM_NULL ) 
    MPI_Comm_free ( &( pioinfo->collectorComm ));
  
  if ( pioinfo->comm != MPI_COMM_NULL ) 
    MPI_Comm_free ( &( pioinfo->comm ));

  free ( pioinfo );
#endif

  return;
}

