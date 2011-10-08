#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <assert.h>

#include "pio.h"
#include "cdi.h"
#include "pio_util.h"

#ifdef USE_MPI
#include "pio_comm.h"
#include "pio_impl.h"
#endif

#ifdef USE_MPI

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

char *token = "%";

/***************************************************************/

int setTag ( int ID, int sc )
{
  return ID * tagKey + sc;
}

/***************************************************************/

tag_t * getTag ( int tag )
{
  tag_t *rtag;

  rtag = ( tag_t * ) xmalloc ( sizeof ( tag_t ));
  rtag->id = tag / tagKey;
  rtag->command = tag % tagKey;

  return rtag;
}

/***************************************************************/

void ungetTag ( tag_t *rtag )
{
  free ( rtag );
  return;
}

/***************************************************************/

size_t pioFileWrite ( int fileID, int tsID, const void *buffer, size_t len )
{
  size_t iret = CDI_UNDEFID;

  switch ( commInqIOModus ())
    {
    case PIO_MPI_NONB:
      iret = fwMPINONB ( fileID, tsID, buffer, len );
      break;
#ifndef _SX
/*
    case PIO_POSIX_ASYNCH:
      iret = fwPOSIXASYNCH ( fileID, tsID, buffer, len );
      break;
*/
#endif
      
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fwPOSIXFPGUARDSENDRECV ( fileID, tsID, buffer, len );
      break;
      
/*
    case PIO_POSIX_FPGUARD_THREAD:
      iret = fwPOSIXFPGUARDTHREAD ( fileID, tsID, buffer, len );
      break;
*/
/*
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      iret = fwPOSIXFPGUARDTHREADREFUSE ( fileID, tsID, buffer, len );
      break;
*/
    case PIO_POSIX_NONB:
      iret = fwPOSIXNONB ( fileID, tsID, buffer, len );
      break;
    }

  return iret;
}

/***************************************************************/

int pioFileClose ( int id )
{
  int iret = CDI_UNDEFID;
  switch ( commInqIOModus ())
    {
    case PIO_MPI_NONB:
      iret = fcMPINONB ( id );
      break;
#ifndef _SX
/*
    case PIO_POSIX_ASYNCH:
      iret = fcPOSIXASYNCH ( id );
      break;
*/
#endif
      
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fcPOSIXFPGUARDSENDRECV ( id );
      break;
      
/*
    case PIO_POSIX_FPGUARD_THREAD:
      iret = fcPOSIXFPGUARDTHREAD ( id );
      break;
*/
/*
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      iret = fcPOSIXFPGUARDTHREADREFUSE ( id );
      break;
*/
    case PIO_POSIX_NONB:
      iret = fcPOSIXNONB ( id );
      break;
    }

  return iret;
}

/***************************************************************/

int pioFileOpenW ( const char *filename )
{
  int iret = CDI_UNDEFID;

  switch ( commInqIOModus ())
    {
    case PIO_MPI_NONB:
      iret = fowMPINONB ( filename );
      break;
#ifndef _SX
/*
    case PIO_POSIX_ASYNCH:
      iret = fowPOSIXASYNCH ( filename );
      break;
*/
#endif
      
    case PIO_POSIX_FPGUARD_SENDRECV:
      iret = fowPOSIXFPGUARDSENDRECV ( filename );
      break;
      
/*
    case PIO_POSIX_FPGUARD_THREAD:
      iret = fowPOSIXFPGUARDTHREAD ( filename );
      break;
*/
/*
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      iret = fowPOSIXFPGUARDTHREADREFUSE ( filename );
      break;
*/
    case PIO_POSIX_NONB:
      iret = fowPOSIXNONB ( filename );
      break;
    }

  return iret;
}
#endif

/***************************************************************/

void backendInit ( void )
{
  int IOModus = commInqIOModus ();

#ifdef USE_MPI
  commDefCommNode ();

  if (( IOModus == PIO_NONE ) && ( commInqSizeNode () != 1 ))
    xabort ( "PIOTYPE, NNODES: not a valid combination" );
 

  switch ( IOModus )
    {
    case PIO_NONE:
      commDefCommColl ( 1 );
      break;
    case PIO_MPI_NONB:
      initMPINONB ();
      break;
#ifndef _SX
/*
    case PIO_POSIX_ASYNCH:
      collectingData = initPOSIXASYNCH ();
      break;
*/
#endif
      
    case PIO_POSIX_FPGUARD_SENDRECV:
      initPOSIXFPGUARDSENDRECV ();
      break;
      
/*
    case PIO_POSIX_FPGUARD_THREAD:
      collectingData = initPOSIXFPGUARDTHREAD ();
      break;
*/
/*
    case PIO_POSIX_FPGUARD_THREAD_REFUSE:
      collectingData = initPOSIXFPGUARDTHREADREFUSE ();
      break;
*/
    case PIO_POSIX_NONB:
      initPOSIXNONB ();
      break;
    }
#endif
}

/***************************************************************/

void backendFinalize ( void )
{
#ifdef USE_MPI
  commDestroy ();
  MPI_Finalize ();
  exit ( 0 );
#endif
}
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
