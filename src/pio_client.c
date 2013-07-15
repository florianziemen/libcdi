#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include <ctype.h>

#include <yaxt.h>

#include "namespace.h"
#include "pio.h"
#include "pio_client.h"
#include "pio_comm.h"
#include "pio_interface.h"
#include "pio_rpc.h"
#include "pio_util.h"

static int
cdiPioClientStreamOpen(const char *filename, const char *filemode,
                       int filetype, stream_t *streamptr,
                       int recordBufIsToBeCreated)
{
  if ( tolower ( * filemode ) == 'w' )
    {
      statusCode nspStatus = namespaceInqResStatus ();
      switch ( nspStatus )
        {
        case STAGE_DEFINITION:
          break;
        case STAGE_TIMELOOP:
          pioBufferFuncCall(STREAMOPEN, 2, filename, filetype);
          break;
        case STAGE_CLEANUP:
          xabort ( "TRANSITION TO IO PROCESSES ALREADY FINISHED." );
          break;
        default:
          xabort ( "INTERNAL ERROR" );
        }
    }
  else
    Error("cdiPIO read support not implemented");
  return 1;
}

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

static void
cdiPioClientStreamWriteVarChunk_(int streamID, int varID, int memtype,
                                 const int rect[][2],
                                 const void *data, int nmiss)
{
  int vlistID = streamInqVlist(streamID);
  int size = vlistInqVarSize(vlistID, varID),
    varShape[3];
  unsigned ndims = (unsigned)cdiPioQueryVarDims(varShape, vlistID, varID);
  Xt_int varShapeXt[3], chunkShape[3] = { 1, 1, 1 }, origin[3] = { 0, 0, 0 };
  /* FIXME: verify xt_int ranges are good enough */
  for (unsigned i = 0; i < 3; ++i)
    varShapeXt[i] = varShape[i];
  for (unsigned i = 0; i < ndims; ++i)
    chunkShape[i] = rect[i][1] - rect[i][0] + 1;
  int varSize = varShape[0] * varShape[1] * varShape[2];
  xassert(varSize == size);
  Xt_idxlist chunkDesc
    = xt_idxsection_new(0, ndims, varShapeXt, chunkShape, origin);
  pioBufferPartData(streamID, varID, data, nmiss, chunkDesc);
  xt_idxlist_delete(chunkDesc);
}

#if defined HAVE_LIBNETCDF
static void
cdiPioCdfDefTimestepNOP(stream_t *streamptr, int tsID)
{
}
#endif

static void
cdiPioClientStreamNOP(stream_t *streamptr)
{
}


static void
cdiPioClientStreamClose(stream_t *streamptr, int recordBufIsToBeDeleted)
{
  statusCode nspStatus = namespaceInqResStatus ();
  switch ( nspStatus )
    {
    case STAGE_DEFINITION:
      break;
    case STAGE_TIMELOOP:
      pioBufferFuncCall(STREAMCLOSE, 1, streamptr->self);
      break;
    case STAGE_CLEANUP:
      break;
    default:
      xabort ( "INTERNAL ERROR" );
    }
}


void
cdiPioClientSetup(int *pioNamespace_, int *pioNamespace)
{
  commEvalPhysNodes ();
  commDefCommsIO ();
  *pioNamespace_ = *pioNamespace = namespaceNew(0);
  int callerCDINamespace = namespaceGetActive();
  pioNamespaceSetActive(*pioNamespace_);
  serializeSetMPI();
  namespaceSwitchSet(NSSWITCH_STREAM_OPEN_BACKEND, cdiPioClientStreamOpen);
  namespaceSwitchSet(NSSWITCH_STREAM_DEF_VLIST_, cdiPioClientStreamDefVlist_);
  namespaceSwitchSet(NSSWITCH_STREAM_WRITE_VAR_, cdiPioClientStreamWriteVar_);
  namespaceSwitchSet(NSSWITCH_STREAM_WRITE_VAR_CHUNK_,
                     cdiPioClientStreamWriteVarChunk_);
  namespaceSwitchSet(NSSWITCH_STREAM_CLOSE_BACKEND, cdiPioClientStreamClose);
  namespaceSwitchSet(NSSWITCH_STREAM_SYNC, cdiPioClientStreamNOP);
#ifdef HAVE_LIBNETCDF
  namespaceSwitchSet(NSSWITCH_CDF_DEF_TIMESTEP, cdiPioCdfDefTimestepNOP);
  namespaceSwitchSet(NSSWITCH_CDF_STREAM_SETUP, cdiPioClientStreamNOP);
#endif
  pioNamespaceSetActive(callerCDINamespace);
}


#endif
