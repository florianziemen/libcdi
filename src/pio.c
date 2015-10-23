#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <ctype.h>
#include <yaxt.h>

#include "file.h"
#include "cdi_int.h"
#include "namespace.h"

#include "pio.h"
#include "cdi.h"
#include "cdipio.h"
#include "pio_comm.h"
#include "pio_impl.h"
#include "pio_interface.h"
#include "pio_rpc.h"
#include "pio_util.h"

const char *const cdiPioCmdStrTab[] = {
  "IO_Open_file",
  "IO_Close_file",
  "IO_Get_fp",
  "IO_Set_fp",
  "IO_Send_buffer",
  "IO_Finalize"
};

extern const char *token;
const char *token = "%";

/***************************************************************/

void cdiPioFileWritingInit(void)
{
  int IOMode = commInqIOMode ();

  xassert ( IOMode != PIO_NONE  || commInqSizeGlob() == 1 );

  switch ( IOMode )
    {
    case PIO_NONE:
      break;
    case PIO_MPI:
      initMPINONB();
      break;
#ifndef _SX
    case PIO_ASYNCH:
#endif
    case PIO_WRITER:
      pioSendInitialize();
      break;
    case PIO_FPGUARD:
      initPOSIXFPGUARDSENDRECV();
      break;
    }
}

/***************************************************************/

void cdiPioFileWritingFinalize(void)
{
  int IOMode = commInqIOMode ();
  switch ( IOMode )
    {
    case PIO_NONE:
      break;
    case PIO_MPI:
      finalizeMPINONB ();
      break;
#ifndef _SX
    case PIO_ASYNCH:
#endif
    case PIO_WRITER:
      pioSendFinalize();
      break;
    case PIO_FPGUARD:
      finalizePOSIXFPGUARDSENDRECV ();
      break;
    default:
      xdebug("%s", " BACKENDCLEANUP FUNCTION NOT IMPLEMENTED YET.");
    }
}

/***************************************************************/

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
