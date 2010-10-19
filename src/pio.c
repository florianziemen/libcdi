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
int maxErrorString = 100;

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

void errorPIO ( char *errorString, const char *filename, int line, int rank )
{
  fprintf ( stderr, "PIO ERROR, %s: line %d: pe%d, errorString: \"%s\"\n",
	    filename, line, rank, errorString );
  MPI_Finalize ();
  exit ( 1 );
}

/*****************************************************************************/

void errorMPI ( char *errorString, const char *filename, int line, int rank )
{
  fprintf ( stderr, "MPI ERROR, %s: line %d: pe%d, errorString: \"%s\"\n",
	    filename, line, rank, errorString );
  MPI_Finalize ();
  exit ( 1 );
}

/*****************************************************************************/

void checkMPI ( int iret, const char *filename, int line, int rank )
{
  char errorString[MPI_MAX_ERROR_STRING + 1];
  int len;

  if  ( iret != MPI_SUCCESS ) 
    {
      MPI_Error_string ( iret, errorString, &len ); 
      errorString[len] = '\0';
      fprintf ( stderr,"MPI ERROR, %s: line %d, pe%d, return value: %d,"
		" MPI_Error_string: \"%s\"\n", 
		filename, line, rank, iret, errorString ); 
      MPI_Finalize ();
      exit ( 1 );
    }

  return;
}

/*****************************************************************************/

void checkMPIstat ( int iret, const char *filename, int line, int rank,  
			MPI_Status *status )
{
  char errorString[MPI_MAX_ERROR_STRING + 1];
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
	  MPI_Error_string ( status->MPI_ERROR, errorString, &len );
	  errorString[len] = '\0';
	  fprintf ( stderr,"MPI ERROR in request, line %d, pe%d, return value: %d,"
		    "error_string: %s\n", line, iret, rank, errorString ); 
	  MPI_Finalize ();
	  exit ( 1 );
	}
    }
  else 
    checkMPI ( iret, filename, line, rank );
  
  return;
}

/***************************************************************/     

void * xmalloc ( size_t size )
{
  void * value = malloc ( size );

  if ( value == NULL )
    {
      fprintf ( stderr, "ERROR: malloc didn't succeed\n" );
      MPI_Finalize ();
      exit ( 1 );
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

  MPI_Comm_size ( commF2C, &size );
  MPI_Comm_rank ( commF2C, &rank );

  myHost = ( char * ) xmalloc ( MPI_MAX_PROCESSOR_NAME * sizeof ( char ));
  
  memset ( myHost, 0, MPI_MAX_PROCESSOR_NAME * sizeof ( char ));
  
  checkMPI (( MPI_Get_processor_name ( myHost, &len )), __FILE__, __LINE__, rank );

  if ( myHost[0] == '\0' ) 
    errorPIO ( "Hostname is not set", __FILE__, __LINE__ , rank);
  
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

  MPI_Allgather ( myHost, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 
			    & ( allHosts[0][0] ), MPI_MAX_PROCESSOR_NAME, 
			    MPI_CHAR, commF2C );

  qsort ( allHosts, size, sizeof ( char * ), cmpr );

  *color = 0;
  i = 0;
  j = 0;

  while ( i < size )
    {
      curr = allHosts[i];
      j++;
      if (( test = strcmp ( myHost, curr )) == 0 ) *color = j;
     
      while ( ++i < size )
	if (( test = strcmp ( allHosts[i], curr )) != 0)
	  break;
    }

  *nnodes = j;
      
  if ( *color == 0 ) errorPIO ( "Color is not set", __FILE__, __LINE__, rank );
  
  npes_node = size / ( * nnodes );
  key = rank % npes_node;

  checkMPI ( MPI_Comm_split ( commF2C, *color, key, myComm),
	     __FILE__, __LINE__, rank );

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
  *pioCollComm2F = 0;

#ifndef NOMPI

  MPI_Comm comm;
  int size, rank;
  
  comm = MPI_COMM_NULL;

  if (( comm = MPI_Comm_f2c (( MPI_Fint ) commF )) == NULL )
    errorMPI ( "MPI_Comm_f2c didn't succeed", __FILE__, __LINE__, -1 );

  MPI_Comm_set_errhandler ( comm, MPI_ERRORS_RETURN );
  MPI_Comm_size ( comm, &size );
  MPI_Comm_rank ( comm, &rank );

  if ( ptype < 0 || ptype > maxPtype ) 
    errorPIO ( "PIOTYPE is no valid modus", __FILE__, __LINE__, rank );

  pioinfo = ( pioInfo * ) xmalloc ( sizeof ( pioInfo ));

  pioinfo->type = ptype; 
  setPioCommunicator ( & ( pioinfo->comm ), comm, color, nnodes );

  if (( pioinfo->type == PIO_NONE ) && ( *nnodes != size ))
    errorPIO ( "PIOTYPE, NNODES: not a valid combination", __FILE__, __LINE__, rank );

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
      MPI_Comm_dup ( pioinfo->comm, &( pioinfo->collectorComm ));
      collectingData = 1;
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
    errorMPI ( "MPI_Comm_c2f didn't succeed\n", __FILE__, __LINE__ , rank);

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

