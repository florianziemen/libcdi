#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include "namespace.h"
#include "pio.h"
#include "pio_client.h"
#include "pio_comm.h"

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
  namespaceSwitchSet(NSSWITCH_STREAM_DEF_VLIST_, cdiPioStreamDefVlist_);
  namespaceSwitchSet(NSSWITCH_STREAM_WRITE_VAR_, cdiPioStreamWriteVar_);
  namespaceSwitchSet(NSSWITCH_STREAM_WRITE_VAR_CHUNK_,
                     cdiPioStreamWriteVarChunk_);
  namespaceSwitchSet(NSSWITCH_STREAM_CLOSE_BACKEND, cdiPioStreamClose);
#ifdef HAVE_LIBNETCDF
  namespaceSwitchSet(NSSWITCH_CDF_DEF_TIMESTEP, cdiPioCdfDefTimestepNOP);
#endif
  pioNamespaceSetActive(callerCDINamespace);
}


#endif
