#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include "namespace.h"
#include "pio.h"
#include "pio_client.h"
#include "pio_comm.h"
#include "pio_interface.h"
#include "pio_rpc.h"

static void
cdiPioClientStreamDefVlist_(int streamID, int vlistID)
{
  statusCode nspStatus = namespaceInqResStatus ();
  switch ( nspStatus )
    {
    case STAGE_DEFINITION:
      break;
    case STAGE_TIMELOOP:
      pioBufferFuncCall(STREAMDEFVLIST, 2, streamID, vlistID);
      break;
    case STAGE_CLEANUP:
      xabort ( "TRANSITION TO IO PROCESSES ALREADY FINISHED." );
      break;
    default:
      xabort ( "INTERNAL ERROR" );
    }
  cdiStreamDefVlist_(streamID, vlistID);
}

static void
cdiPioClientStreamWriteVar_(int streamID, int varID, int memtype,
                            const void *data, int nmiss)
{
  xabort("parallel writing requires explicit partition information,"
         " use streamWriteVarPart!");
}

#if defined HAVE_LIBNETCDF
static void
cdiPioCdfDefTimestepNOP(stream_t *streamptr, int tsID)
{
}
#endif

void
cdiPioClientSetup(int *pioNamespace_, int *pioNamespace)
{
  commEvalPhysNodes ();
  commDefCommsIO ();
  *pioNamespace_ = *pioNamespace = namespaceNew(0);
  int callerCDINamespace = namespaceGetActive();
  pioNamespaceSetActive(*pioNamespace_);
  serializeSetMPI();
  namespaceSwitchSet(NSSWITCH_STREAM_OPEN_BACKEND, cdiPioStreamOpen);
  namespaceSwitchSet(NSSWITCH_STREAM_DEF_VLIST_, cdiPioClientStreamDefVlist_);
  namespaceSwitchSet(NSSWITCH_STREAM_WRITE_VAR_, cdiPioClientStreamWriteVar_);
  namespaceSwitchSet(NSSWITCH_STREAM_WRITE_VAR_CHUNK_,
                     cdiPioStreamWriteVarChunk_);
  namespaceSwitchSet(NSSWITCH_STREAM_CLOSE_BACKEND, cdiPioStreamClose);
#ifdef HAVE_LIBNETCDF
  namespaceSwitchSet(NSSWITCH_CDF_DEF_TIMESTEP, cdiPioCdfDefTimestepNOP);
#endif
  pioNamespaceSetActive(callerCDINamespace);
}


#endif
