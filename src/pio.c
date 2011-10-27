#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#ifdef USE_MPI

#include "pio.h"
#include "cdi.h"
#include "pio_comm.h"
#include "pio_impl.h"
#include "pio_util.h"

char * command2charP[6] = {"IO_Open_file", "IO_Close_file",
                           "IO_Get_fp","IO_Set_fp",
                           "IO_Send_buffer", "IO_Finalize"};

long initial_buffersize = 16 * 1024 * 1024;
/*  4 KB <= x < 256 MB */
/* 16 * 1024 * 1024; */
/* 16 * 1024; */
/* 4 * 1024; */

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

  switch ( commInqIOMode ())
    {
    case PIO_MPI:
      iret = fwMPINONB ( fileID, tsID, buffer, len );
      break;
#ifndef _SX
    case PIO_ASYNCH:
      iret = fwPOSIXASYNCH ( fileID, tsID, buffer, len );
      break;
#endif      
    case PIO_FPGUARD:
      iret = fwPOSIXFPGUARDSENDRECV ( fileID, tsID, buffer, len );
      break;
    case PIO_WRITER:
      iret = fwPOSIXNONB ( fileID, tsID, buffer, len );
      break;
    }

  return iret;
}

/***************************************************************/

int pioFileClose ( int id )
{
  int iret = CDI_UNDEFID;
  switch ( commInqIOMode ())
    {
    case PIO_MPI:
      iret = fcMPINONB ( id );
      break;
#ifndef _SX
    case PIO_ASYNCH:
      iret = fcPOSIXASYNCH ( id );
      break;
#endif
    case PIO_FPGUARD:
      iret = fcPOSIXFPGUARDSENDRECV ( id );
      break;
    case PIO_WRITER:
      iret = fcPOSIXNONB ( id );
      break;
    }

  return iret;
}

/***************************************************************/

int pioFileOpenW ( const char *filename )
{
  int iret = CDI_UNDEFID;

  switch ( commInqIOMode ())
    {
    case PIO_MPI:
      iret = fowMPINONB ( filename );
      break;
#ifndef _SX
    case PIO_ASYNCH:
      iret = fowPOSIXASYNCH ( filename );
      break;
#endif
    case PIO_FPGUARD:
      iret = fowPOSIXFPGUARDSENDRECV ( filename );
      break;
    case PIO_WRITER:
      iret = fowPOSIXNONB ( filename );
      break;
    }

  return iret;
}

/***************************************************************/

void backendInit ( void )
{
  int IOMode = commInqIOMode ();

  commDefCommNode ();

  xassert ( IOMode != PIO_NONE  || commInqSizeNode () == 1 );

  switch ( IOMode )
    {
    case PIO_NONE:
      commDefCommColl ( 1 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      break;
    case PIO_MPI:
      initMPINONB ();
      break;
#ifndef _SX
    case PIO_ASYNCH:
      initPOSIXASYNCH ();
      break;
#endif
    case PIO_FPGUARD:
      initPOSIXFPGUARDSENDRECV ();
      break;
    case PIO_WRITER:
      initPOSIXNONB ();
      break;
    }
}

/***************************************************************/

void backendCleanup ( void )
{
  int IOMode = commInqIOMode ();
  switch ( IOMode )
    {
    case PIO_NONE:
      break;
    case PIO_MPI:
      finalizeMPINONB ();
      break;
    case PIO_ASYNCH:
      finalizePOSIXASYNCH ();
      break;
    case PIO_FPGUARD:
      finalizePOSIXFPGUARDSENDRECV ();
      break;
    case PIO_WRITER:
      finalizePOSIXNONB ();
      break;
    default:
      xdebug ( " BACKENDCLEANUP FUNCTION NOT IMPLEMENTED YET." );
    }
}

/***************************************************************/

void backendFinalize ( void )
{
  commDestroy ();
  MPI_Finalize ();
  exit ( 0 );
}
#endif
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
