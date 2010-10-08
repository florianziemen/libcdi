#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef NOMPI

#include "mpi.h"
#include "cdi.h"
#include "pio.h"
#include "pio_impl.h"

#endif

bool ddebug = false;

#ifndef NOMPI

char * command2charP[6] = {"IO_Open_file", "IO_Close_file",
			   "IO_Get_fp","IO_Set_fp",
			   "IO_Send_buffer", "IO_Finalize"};

long initial_buffersize = 16 * 1024 * 1024;
/*  4 KB <= x < 256 MB */ 
/* 16 * 1024 * 1024; */
/* 16 * 1024; */
/* 4 * 1024; */

int maxPtype = 4;
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

void * xmalloc ( size_t size )
{
  void * value = malloc ( size );

  if ( value == NULL )
    {
      fprintf ( stderr, "ERROR: malloc didn't succeed, memory exausted\n" );
      perror ( "xmalloc" );
      MPI_Abort ( MPI_COMM_WORLD, 1 );
    }
  return value;
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

  rtag = ( rTag * ) xmalloc ( sizeof ( rTag ));
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
			  int *nnodes )
{
  int size, rank, len, npes_node, key, test, i, j;

  char *myHost, **allHosts, *allHosts0, *curr;
  char hostname [ MPI_MAX_PROCESSOR_NAME + 1 ];

  check_mpi ( __LINE__, 
	    MPI_Comm_size ( commF2C, &size ));
  check_mpi ( __LINE__, 
	    MPI_Comm_rank ( commF2C, &rank ));

  myHost = ( char * ) xmalloc ( MPI_MAX_PROCESSOR_NAME * sizeof ( char ));

  memset ( myHost, 0, MPI_MAX_PROCESSOR_NAME * sizeof ( char ));

  check_mpi( __LINE__, 
	    MPI_Get_processor_name ( myHost, &len ));

  if ( ddebug )
    {
      strncpy ( hostname, myHost, len );
      hostname [ len ] = '\0';
      fprintf ( stdout, "pe%d: myHost = %s\n", rank,  hostname );
    }

  
  allHosts = ( char ** ) xmalloc ( size * sizeof ( char * ));
  allHosts[0] = ( char * ) xmalloc ( size * MPI_MAX_PROCESSOR_NAME * 
				     sizeof ( char ));
  allHosts0 = allHosts[0];

  for ( i = 1; i < size; i++ )
    allHosts[i] = allHosts[0] + i * MPI_MAX_PROCESSOR_NAME;
  
  check_mpi ( __LINE__, 
	    MPI_Allgather ( myHost, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 
			    & ( allHosts[0][0] ), MPI_MAX_PROCESSOR_NAME, 
			    MPI_CHAR, commF2C ));

  qsort ( allHosts, size, sizeof ( char * ), cmpr );

  *color = 0;
  i = 0;
  j = 0;
  while ( i < size )
    {
      curr = allHosts[i];
      j++;
      if (( test = strcmp ( myHost, curr )) == 0 ) *color = j;
      while (( i < size ) &&
	     (( test = strcmp ( allHosts[i], curr )) == 0))
	i++;
    }

  *nnodes = j;
      
  if ( *color == 0 )
    {
      fprintf ( stderr, "Color is not set correctly\n" );
      MPI_Abort ( commF2C, 1 );
    }
  
  npes_node = size / ( * nnodes );
  key = rank % npes_node;

  check_mpi(__LINE__, 
	    MPI_Comm_split ( commF2C, *color, key, myComm));
  check_mpi(__LINE__, 
	    MPI_Errhandler_set ( *myComm, MPI_ERRORS_RETURN ));

  free ( allHosts0 );
  free ( allHosts );
  free ( myHost );

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in setPioCommunicator, color=%d, before return\n", 
	      rank, *color );
  
  return;
}

#endif

/***************************************************************/

int pioInit ( int ptype, int commF, int *color, int *nnodes, int *pioCollComm2F )
{

  int collectingData = 1;
  *nnodes = 1;
  *color = 1;

#ifndef NOMPI

  MPI_Comm comm;
  int size, rank;
  
  comm = MPI_COMM_NULL;

  if (( comm = MPI_Comm_f2c (( MPI_Fint ) commF )) == NULL )
    {
      fprintf ( stderr, 
		"2. arg of pioInit is no valid Fortran handle "
		"to a mpi communicator\n");
      MPI_Abort ( comm, 1 );
    }

  check_mpi ( __LINE__, MPI_Comm_size ( comm, &size ));

  check_mpi ( __LINE__, MPI_Comm_rank ( comm, &rank ));

  if ( ptype < 0 || ptype > maxPtype )
    {
      if ( rank == 0 )
	fprintf ( stderr, "1. arg of pioInit is no valid modus\n");
      MPI_Abort ( comm, 1 );
    }

  pioinfo = ( pioInfo * ) xmalloc ( sizeof ( pioInfo ));

  pioinfo->type = ptype; 
  setPioCommunicator ( & ( pioinfo->comm ), comm, color, nnodes );

  if ( ptype == PIO_NONE && 
       ( size != 1 || *nnodes != 1 ))
    {   
      if ( rank == 0 )
	fprintf ( stderr, 
		  "\n\npioInit(): combination of "
		  "arg(1) PTYPE=%d, NNODES=%d and npe=%d is not valid.\n"
		  "Possible modus is:\n"						
		  "ptype = PIO_NONE (:=0), \t\t nnodes = 1,\t\t  npe = 1\n\n",
		  ptype, *nnodes, size );
      MPI_Abort ( comm, 1 );
    }

  pioinfo->color = *color;
  MPI_Comm_rank ( pioinfo->comm, &( pioinfo->rank ));
  MPI_Comm_size ( pioinfo->comm, &( pioinfo->size ));

  if ( ddebug && pioinfo->rank == 0 )
    fprintf ( stdout, 
	      "pe%d in pioDefPtype(), ptype=%d, initial_buffersize=%ld: "
	      "after init pioinfo ...\n", 
	      pioinfo->rank, pioinfo->type, initial_buffersize );

  pioinfo->collectorComm = MPI_COMM_NULL;

  switch ( pioinfo->type )
    {
    case PIO_NONE:
      collectingData = 1;
      MPI_Comm_dup ( pioinfo->comm, &( pioinfo->collectorComm ));
      break;
    case PIO_MPI_NONB:
      collectingData = initMPINONB ();
      break;
    case PIO_POSIX_ASYNCH:
      collectingData = initPOSIXASYNCH ();
      break;
    case PIO_POSIX_FPGUARD_SENDRECV:
      collectingData = initPOSIXFPGUARDSENDRECV ();
      break;
    case PIO_POSIX_FPGUARD_THREAD:
      collectingData = initPOSIXFPGUARDTHREAD ();
      break;
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      collectingData = initPOSIXFPGUARDTHREADREFUSE ();
      break;
    case PIO_POSIX_NONB:      
      collectingData = initPOSIXNONB ();
      break;
    }

  *pioCollComm2F  = 0;
  if (( *pioCollComm2F = MPI_Comm_c2f ( pioinfo->collectorComm )) == 0 )
    {
      fprintf ( stderr, "mpi_comm_c2f didn't succeed\n");
      MPI_Abort ( comm, 1 );
    }

#endif
  
  if ( ddebug )
    fprintf ( stdout, "pe in pioinit out\n" );
      
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

