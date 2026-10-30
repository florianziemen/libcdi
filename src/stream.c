#include "cdi_limits.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifdef HAVE_LIBNETCDF
#include <netcdf.h>  // NC_FORMAT_64BIT_DATA
#endif

#ifdef HAVE_TENSORSTORE
#include "tstore.h"
#endif

#ifdef HAVE_LIBFDB5
#include "cdi_fdb.h"
#endif

#include <sys/stat.h>  // struct stat
#include <ctype.h>
#include <string.h>

#include "binary.h"
#include "cdi.h"
#include "cdi_query.h"
#include "cdi_across.h"
#include "cdi_int.h"
#include "cdi_cksum.h"
#include "cdf.h"
#include "dmemory.h"
#include "error.h"
#include "stream_pack.h"
#include "stream_cgribex.h"
#include "stream_grb.h"
#include "stream_cdf.h"
#include "stream_srv.h"
#include "stream_ext.h"
#include "stream_ieg.h"
#include "file.h"
#include "cgribex.h"
#include "gribapi.h"
#include "vlist.h"
#include "namespace.h"
#include "async_worker.h"

static stream_t *stream_new_entry(int resH);
static int stream_compare(void *streamP1, void *streamP2);
static void stream_destroy(void *streamP);
static void stream_print(void *streamP, FILE *fp);

const resOps streamOps = { stream_compare, stream_destroy, stream_print, streamGetPackSize, streamPack, streamTxCode };

static int
getByteorder(int byteswap)
{
  switch (HOST_ENDIANNESS)
  {
    case CDI_BIGENDIAN: return byteswap ? CDI_LITTLEENDIAN : CDI_BIGENDIAN;
    case CDI_LITTLEENDIAN: return byteswap ? CDI_BIGENDIAN : CDI_LITTLEENDIAN;
    /* FIXME: does not currently adjust for PDP endianness */
    case CDI_PDPENDIAN:
    default: Error("unhandled endianness");
  }

  return -1;
}

static int
cdi_get_protocol(const char *uri, const char **filename)
{
  const char *pos = strstr(uri, "://");
  if (pos == NULL)
  {
    *filename = uri;
    return CDI_PROTOCOL_FILE;
  }

  size_t protocollen = (size_t) (pos - uri);
  *filename = pos + 3;

  // if (strncmp(uri, "file", protocollen) == 0) return CDI_PROTOCOL_FILE; // file is already used in NetCDF
  if (strncmp(uri, "fdb", protocollen) == 0) return CDI_PROTOCOL_FDB;
  if (strncmp(uri, "across", protocollen) == 0) return CDI_PROTOCOL_ACROSS;

  *filename = uri;

  return CDI_PROTOCOL_OTHER;
}

int
get_filetype_from_name(const char **path)
{
  const char *uri = *path;
  if (uri[0] == 'z' && uri[1] == 'a' && uri[2] == 'r' && uri[3] == 'r')
  {
    if (uri[4] == '2' && uri[5] == ':' && uri[6])
    {
      *path = uri + 6;
      return CDI_FILETYPE_ZARR2;
    }
    if (uri[4] == '3' && uri[5] == ':' && uri[6])
    {
      *path = uri + 6;
      return CDI_FILETYPE_ZARR3;
    }
  }

  return CDI_EUFTYPE;
}

// used also in CDO
int
cdiGetFiletype(const char *uri, int *byteorder)
{
  int filetype = get_filetype_from_name(&uri);
  if (filetype != CDI_EUFTYPE) return filetype;

  const char *filename;
  int protocol = cdi_get_protocol(uri, &filename);
  switch (protocol)
  {
    case CDI_PROTOCOL_ACROSS: return CDI_FILETYPE_GRB2;
    case CDI_PROTOCOL_FDB: return CDI_FILETYPE_GRB2;
    case CDI_PROTOCOL_OTHER: return CDI_FILETYPE_NC4;  // support for NetCDF remote types and ESDM
    case CDI_PROTOCOL_FILE: break;                     // handled below;
  }

  int fileID = fileOpen(filename, "r");

  // clang-format off
  if      (fileID == CDI_UNDEFID) return CDI_ESYSTEM;
  else if (fileID == -2)          return CDI_ETMOF;

  int swap = 0;
  int version;
  long recpos;
  char buffer[8];
  if (fileRead(fileID, buffer, 8) != 8)
  {
    struct stat buf;
    if (stat(filename, &buf) == 0)
    {
      if (buf.st_size == 0)      return CDI_EISEMPTY;
      if (buf.st_mode & S_IFDIR) return CDI_EISDIR;
    }

    return CDI_EUFTYPE;
  }

  fileRewind(fileID);

  if (memcmp(buffer, "GRIB", 4) == 0)
  {
    version = buffer[7];
    if      (version <= 1) { filetype = CDI_FILETYPE_GRB; }
    else if (version == 2) { filetype = CDI_FILETYPE_GRB2; }
  }
  else if (memcmp(buffer, "CDF\001", 4) == 0) { filetype = CDI_FILETYPE_NC; }
  else if (memcmp(buffer, "CDF\002", 4) == 0) { filetype = CDI_FILETYPE_NC2; }
  else if (memcmp(buffer, "CDF\005", 4) == 0) { filetype = CDI_FILETYPE_NC5; }
  else if (memcmp(buffer + 1, "HDF", 3) == 0) { filetype = CDI_FILETYPE_NC4; }
#ifdef HAVE_LIBSERVICE
  else if (srvCheckFiletype(fileID, &swap))   { filetype = CDI_FILETYPE_SRV; }
#endif
#ifdef HAVE_LIBEXTRA
  else if (extCheckFiletype(fileID, &swap))   { filetype = CDI_FILETYPE_EXT; }
#endif
#ifdef HAVE_LIBIEG
  else if (iegCheckFiletype(fileID, &swap))   { filetype = CDI_FILETYPE_IEG; }
#endif
#ifdef HAVE_LIBGRIB
  else if (gribCheckSeek(fileID, &recpos, &version) == 0)
  {
    if      (version <= 1) { filetype = CDI_FILETYPE_GRB; }
    else if (version == 2) { filetype = CDI_FILETYPE_GRB2; }
  }
#endif
  // clang-format on

  if (CDI_Debug && filetype != CDI_EUFTYPE) Message("found %s file = %s", strfiletype(filetype), filename);

  fileClose(fileID);

  *byteorder = getByteorder(swap);

  return filetype;
}

/*
@Function  streamInqFiletype
@Title     Get the filetype

@Prototype int streamInqFiletype(int streamID)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenRead} or @fref{streamOpenWrite}.

@Description
The function @func{streamInqFiletype} returns the filetype of a stream.

@Result
@func{streamInqFiletype} returns the type of the file format,
one of the set of predefined CDI file format types.
The valid CDI file format types are @func{CDI_FILETYPE_GRB}, @func{CDI_FILETYPE_GRB2}, @func{CDI_FILETYPE_NC},
@func{CDI_FILETYPE_NC2}, @func{CDI_FILETYPE_NC4}, @func{CDI_FILETYPE_NC4C}, @func{CDI_FILETYPE_NC5},
@func{CDI_FILETYPE_NCZARR}, @func{CDI_FILETYPE_SRV}, @func{CDI_FILETYPE_EXT} and @func{CDI_FILETYPE_IEG}.
@EndFunction
*/
int
streamInqFiletype(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);
  return streamP->filetype;
}

int
getByteswap(int byteorder)
{
  switch (byteorder)
  {
    case CDI_BIGENDIAN:
    case CDI_LITTLEENDIAN:
    case CDI_PDPENDIAN: return (HOST_ENDIANNESS != byteorder);
    case -1: break;
    default: Error("unexpected byteorder %d query!", byteorder);
  }

  return -1;
}

void
streamDefMaxSteps(int streamID, int maxSteps)
{
  if (maxSteps >= 0)
  {
    stream_t *streamP = stream_to_pointer(streamID);
    streamP->maxSteps = maxSteps;
  }
}

int
streamInqNumSteps(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);

  long ntsteps = streamP->ntsteps;
  if (ntsteps == (long) CDI_UNDEFID)
  {
    int tsID = 0;
    while (streamInqTimestep(streamID, tsID++)) { ntsteps = streamP->ntsteps; }
  }

  return (int) ntsteps;
}

static long
get_max_global_recs(stream_t *streamP)
{
  long maxGlobalRecs = -1;
  const tsteps_t *tsteps = streamP->tsteps;
  if (tsteps)
  {
    maxGlobalRecs = tsteps[0].nrecs;
    long numSteps = streamP->ntsteps;
    if (numSteps > 1) maxGlobalRecs += tsteps[1].nrecs * (numSteps - 1);
  }
  return maxGlobalRecs;
}

void
streamDefNumWorker(int streamID, int numWorker)
{
  if (numWorker == 0) return;

  stream_t *streamP = stream_to_pointer(streamID);
  int filetype = streamP->filetype;

  if (streamP->filemode != 'r') return;

  if (cdiBaseFiletype(filetype) == CDI_FILETYPE_GRIB)
  {
    (void) streamInqNumSteps(streamID);
    streamP->maxGlobalRecs = get_max_global_recs(streamP);
  }
#ifdef HAVE_LIBNETCDF
  else if (filetype == CDI_FILETYPE_NCZARR || (CDI_Test && cdiBaseFiletype(filetype) == CDI_FILETYPE_NETCDF))
  {
    streamP->maxGlobalRecs = get_max_global_recs(streamP);
    if (CDI_Test) Message("numWorker=%d", numWorker);
    if (CDI_Test) Message("maxGlobalRecs=%ld", streamP->maxGlobalRecs);
    if (streamP->maxGlobalRecs == -1) xabort("Internal error: number of timesteps missing!");
    if (streamP->maxGlobalRecs == 1) numWorker = 0;
    if (numWorker > streamP->maxGlobalRecs) numWorker = (int) streamP->maxGlobalRecs;
    CdfInfo *cdfInfo = &(streamP->cdfInfo);
    if (cdfInfo->chunkSizeDimT > 1 && numWorker > streamP->nvars) numWorker = streamP->nvars;
    if (cdfInfo->chunkSizeDimZ > 1) numWorker = 0;
    if (CDI_Test) Message("chunkSizeTdim=%d chunkSizeZdim=%d", cdfInfo->chunkSizeDimT, cdfInfo->chunkSizeDimZ);
  }
#endif
  else
  {
    numWorker = 0;
  }

  streamP->numWorker = numWorker;
  if (CDI_Debug || CDI_Test) Message("Number of asynchronous worker: %d", numWorker);
}

/*
@Function  streamDefByteorder
@Title     Define the byte order

@Prototype void streamDefByteorder(int streamID, int byteorder)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenWrite}.
    @Item  byteorder The byte order of a dataset, one of the CDI constants @func{CDI_BIGENDIAN} and
                     @func{CDI_LITTLEENDIAN}.

@Description
The function @func{streamDefByteorder} defines the byte order of a binary dataset
with the file format type @func{CDI_FILETYPE_SRV}, @func{CDI_FILETYPE_EXT} or @func{CDI_FILETYPE_IEG}.

@EndFunction
*/
void
streamDefByteorder(int streamID, int byteorder)
{
  stream_t *streamP = stream_to_pointer(streamID);
  streamP->byteorder = byteorder;
  int filetype = streamP->filetype;

  switch (filetype)
  {
#ifdef HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV:
    {
      srvrec_t *srvp = (srvrec_t *) streamP->record->objectp;
      srvp->byteswap = getByteswap(byteorder);
      break;
    }
#endif
#ifdef HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT:
    {
      extrec_t *extp = (extrec_t *) streamP->record->objectp;
      extp->byteswap = getByteswap(byteorder);
      break;
    }
#endif
#ifdef HAVE_LIBIEG
    case CDI_FILETYPE_IEG:
    {
      iegrec_t *iegp = (iegrec_t *) streamP->record->objectp;
      iegp->byteswap = getByteswap(byteorder);
      break;
    }
#endif
  }

  reshSetStatus(streamID, &streamOps, RESH_DESYNC_IN_USE);
}

/*
@Function  streamInqByteorder
@Title     Get the byte order

@Prototype int streamInqByteorder(int streamID)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenRead} or @fref{streamOpenWrite}.

@Description
The function @func{streamInqByteorder} returns the byte order of a binary dataset
with the file format type @func{CDI_FILETYPE_SRV}, @func{CDI_FILETYPE_EXT} or @func{CDI_FILETYPE_IEG}.

@Result
@func{streamInqByteorder} returns the type of the byte order.
The valid CDI byte order types are @func{CDI_BIGENDIAN} and @func{CDI_LITTLEENDIAN}

@EndFunction
*/
int
streamInqByteorder(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);
  return streamP->byteorder;
}

const char *
streamFilesuffix(int filetype)
{
  static const char noSuffix[] = "";
  static const char ncSuffix[] = ".nc";
  static const char grbSuffix[] = ".grb";
  static const char srvSuffix[] = ".srv";
  static const char extSuffix[] = ".ext";
  static const char iegSuffix[] = ".ieg";

  switch (cdiBaseFiletype(filetype))
  {
    case CDI_FILETYPE_GRIB: return grbSuffix;
    case CDI_FILETYPE_SRV: return srvSuffix;
    case CDI_FILETYPE_EXT: return extSuffix;
    case CDI_FILETYPE_IEG: return iegSuffix;
    case CDI_FILETYPE_NETCDF: return ncSuffix;
    default: return noSuffix;
  }
}

const char *
streamFilename(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);
  return streamP->filename;
}

static int
cdiInqContents(stream_t *streamP)
{
  if (streamP->lockIO) CDI_IO_LOCK();

  int status = 0;
  int filetype = streamP->filetype;

  switch (cdiBaseFiletype(filetype))
  {
#ifdef HAVE_LIBGRIB
    case CDI_FILETYPE_GRIB:
    {
      switch (streamP->protocol)
      {
#ifdef HAVE_LIBFDB5
        case CDI_PROTOCOL_FDB: status = fdbInqContents(streamP); break;
#endif
        case CDI_PROTOCOL_ACROSS:  // TODO read from ACROSS
        case CDI_PROTOCOL_OTHER:
        case CDI_PROTOCOL_FILE: status = grbInqContents(streamP); break;
      }
      break;
    }
#endif
#ifdef HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV: status = srvInqContents(streamP); break;
#endif
#ifdef HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT: status = extInqContents(streamP); break;
#endif
#ifdef HAVE_LIBIEG
    case CDI_FILETYPE_IEG: status = iegInqContents(streamP); break;
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NETCDF: status = cdfInqContents(streamP); break;
#endif
#ifdef HAVE_TENSORSTORE
    case CDI_FILETYPE_ZARR: status = ts_inq_contents(streamP); break;
#endif
    default:
    {
      status = CDI_ELIBNAVAIL;
      if (CDI_Debug) Message("%s support not compiled in!", strfiletype(filetype));
    }
  }

  if (streamP->lockIO) CDI_IO_UNLOCK();

  if (status == 0)
  {
    int taxisID = vlistInqTaxis(streamP->vlistID);
    if (taxisID != CDI_UNDEFID)
    {
      taxis_t *taxisP1 = &streamP->tsteps[0].taxis;
      taxis_t *taxisP2 = taxis_to_pointer(taxisID);
      ptaxisCopy(taxisP2, taxisP1);
    }
  }

  return status;
}

int
cdiStreamOpenDefaultDelegate(const char *uri, char filemode, int filetype, stream_t *streamP, int recordBufIsToBeCreated)
{
  int fileID;
  char temp[2] = { filemode, 0 };

  const char *filename;
  streamP->protocol = cdi_get_protocol(uri, &filename);
  switch (streamP->protocol)
  {
    case CDI_PROTOCOL_ACROSS:
#if defined(HAVE_ACROSS) && defined(HAVE_LIBGRIB_API)
      if (filetype != CDI_FILETYPE_GRB2)
      {
        Warning("ACROSS needs to be used with GRIB2");
        return CDI_EUFTYPE;
      }
      fileID = across_connect(filename, filemode, streamP);
      if (fileID >= 0)
      {
        streamP->filetype = filetype;
        if (recordBufIsToBeCreated)
        {
          streamP->record = (Record *) Malloc(sizeof(Record));
          streamP->record->buffer = NULL;
        }
      }
      return fileID;
#else
#ifdef HAVE_ACROSS
      Warning("ecCodes support not compiled in (Needed for ACROSS)!");
#else
      Warning("ACROSS support not compiled in!");
#endif
      return CDI_ELIBNAVAIL;
#endif

    case CDI_PROTOCOL_FDB:
#if defined(HAVE_LIBFDB5) && defined(HAVE_LIBGRIB_API)

      if (filetype != CDI_FILETYPE_GRB && filetype != CDI_FILETYPE_GRB2)
      {
        Warning("FDB5 needs to be used with GRIB or GRIB2");
        return CDI_EUFTYPE;
      }

      check_fdb_error(fdb_initialise());
      check_fdb_error(fdb_new_handle((fdb_handle_t **) &(streamP->protocolData)));
      streamP->filetype = filetype;
      if (recordBufIsToBeCreated)
      {
        streamP->record = (Record *) Malloc(sizeof(Record));
        streamP->record->buffer = NULL;
      }
      return 88;

#else  // !(defined(HAVE_LIBFDB5) && defined(HAVE_LIBGRIB_API))

#ifdef HAVE_LIBFDB5
      Warning("ecCodes support not compiled in (Needed for FDB5)!");
#else
      Warning("FDB5 support not compiled in!");
#endif

      return CDI_ELIBNAVAIL;
#endif

    case CDI_PROTOCOL_OTHER:
    case CDI_PROTOCOL_FILE:
      // handled below;
      break;
  }

  switch (filetype)
  {
#if defined(HAVE_LIBGRIB) && (defined(HAVE_LIBCGRIBEX) || defined(HAVE_LIBGRIB_API))
    case CDI_FILETYPE_GRB:
    {
      fileID = gribOpen(filename, temp);
      if (fileID < 0) return CDI_ESYSTEM;
      if (recordBufIsToBeCreated)
      {
        streamP->record = (Record *) Malloc(sizeof(Record));
        streamP->record->buffer = NULL;
#ifdef HAVE_LIBCGRIBEX
        streamP->record->objectp = cgribexNew();
#else
        streamP->record->objectp = NULL;
#endif
      }
      break;
    }
#ifdef HAVE_LIBGRIB_API
    case CDI_FILETYPE_GRB2:
    {
      fileID = gribOpen(filename, temp);
      if (fileID < 0) return CDI_ESYSTEM;
      if (recordBufIsToBeCreated)
      {
        streamP->record = (Record *) Malloc(sizeof(Record));
        streamP->record->buffer = NULL;
      }
      break;
    }
#endif
#endif
#ifdef HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV:
    {
      fileID = fileOpen(filename, temp);
      if (fileID < 0) return CDI_ESYSTEM;
      if (recordBufIsToBeCreated)
      {
        streamP->record = (Record *) Malloc(sizeof(Record));
        streamP->record->buffer = NULL;
        streamP->record->objectp = srvNew();
      }
      break;
    }
#endif
#ifdef HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT:
    {
      fileID = fileOpen(filename, temp);
      if (fileID < 0) return CDI_ESYSTEM;
      if (recordBufIsToBeCreated)
      {
        streamP->record = (Record *) Malloc(sizeof(Record));
        streamP->record->buffer = NULL;
        streamP->record->objectp = extNew();
      }
      break;
    }
#endif
#ifdef HAVE_LIBIEG
    case CDI_FILETYPE_IEG:
    {
      fileID = fileOpen(filename, temp);
      if (fileID < 0) return CDI_ESYSTEM;
      if (recordBufIsToBeCreated)
      {
        streamP->record = (Record *) Malloc(sizeof(Record));
        streamP->record->buffer = NULL;
        streamP->record->objectp = iegNew();
      }
      break;
    }
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NC:
    case CDI_FILETYPE_NC2:
    case CDI_FILETYPE_NC5:
    {
      fileID = cdfOpen(filename, temp, filetype);
      break;
    }
    case CDI_FILETYPE_NC4:
    case CDI_FILETYPE_NC4C:
    case CDI_FILETYPE_NCZARR:
    {
      fileID = cdf4Open(filename, temp, &filetype);
      break;
    }
#endif
#ifdef HAVE_TENSORSTORE
    case CDI_FILETYPE_ZARR2:
    case CDI_FILETYPE_ZARR3:
    {
      fileID = ts_open(filename, temp, filetype);
      break;
    }
#endif
    default:
    {
      if (CDI_Debug) Message("%s support not compiled in!", strfiletype(filetype));
      return CDI_ELIBNAVAIL;
    }
  }

  streamP->filetype = filetype;

  return fileID;
}

static int
stream_create_vlist(stream_t *streamP, struct CdiQuery *query)
{
  int vlistID = vlistCreate();
  if (vlistID < 0) return CDI_ELIMIT;

  cdiVlistMakeInternal(vlistID);
  streamP->vlistID = vlistID;

  if (query) { streamP->query = query; }

  int status = cdiInqContents(streamP);
  if (status >= 0)
  {
    vlist_t *vlistP = vlist_to_pointer(streamP->vlistID);
    vlistP->ntsteps = streamP->ntsteps;
    cdiVlistMakeImmutable(vlistID);
  }

  return status;
}

int
streamOpenID(const char *filename, char filemode, int filetype, int resH)
{
  if (CDI_Debug) Message("Open %s mode %c file %s", strfiletype(filetype), filemode, filename ? filename : "(NUL)");

  if (!filename || filetype < 0) return CDI_EINVAL;

  stream_t *streamP = stream_new_entry(resH);
  int streamID = CDI_ESYSTEM;

#ifndef HAVE_NC4HDF5_THREADSAFE
  if (CDI_Threadsafe)
  {
#ifdef HAVE_LIBPTHREAD
    if (filetype == CDI_FILETYPE_NC4 || filetype == CDI_FILETYPE_NC4C || filetype == CDI_FILETYPE_NCZARR) streamP->lockIO = true;
#else
    CDI_WARN_ONCE("CDI threadsafe failed, pthread support not compiled in!");
#endif
  }
#endif

  if (streamP->lockIO) CDI_IO_LOCK();

  int (*streamOpenDelegate)(const char *filename, char filemode, int filetype, stream_t *streamP, int recordBufIsToBeCreated)
      = (int (*)(const char *, char, int, stream_t *, int)) namespaceSwitchGet(NSSWITCH_STREAM_OPEN_BACKEND).func;

  int fileID = streamOpenDelegate(filename, filemode, filetype, streamP, 1);
  if (fileID < 0)
  {
    streamID = fileID;
    FreeAndNull(streamP->record);
    reshRemove(streamP->self, &streamOps);
    if (streamP->lockIO) CDI_IO_UNLOCK();
    FreeAndNull(streamP);
  }
  else
  {
    streamID = streamP->self;
    if (streamID < 0)
    {
      FreeAndNull(streamP->record);
      reshRemove(streamP->self, &streamOps);
      if (streamP->lockIO) CDI_IO_UNLOCK();
      FreeAndNull(streamP);
      return CDI_ELIMIT;
    }

    streamP->filemode = filemode;
    streamP->filename = strdup(filename);
    streamP->fileID = fileID;
    if (streamP->lockIO) CDI_IO_UNLOCK();
  }

  return streamID;
}

int
streamOpenNCMem(int ncidp, char mode)
{
  cdi_initialize();
  stream_t *streamP = stream_new_entry(CDI_UNDEFID);
  int streamID = CDI_ESYSTEM;

#ifdef NC_FORMAT_64BIT_DATA  // so that we ran reliably use NC5 filetype
                             // Not doing this can result in an error message
                             // that falsely states a NC3 file is being used
  streamID = streamP->self;
  if (streamID < 0) return CDI_ELIMIT;

  size_t max_file_path_length = 128;
  size_t actual_length = 0;
  char *filename = (char *) malloc(max_file_path_length);

  nc_inq_path(ncidp, &actual_length, filename);

  streamP->filename = strdup(filename);
  free(filename);
  streamP->filemode = mode;
  streamP->filetype = CDI_FILETYPE_NC5;
  streamP->fileID = ncidp;

  if (mode == 'r')
  {
    streamP->groupID = ncidp;
    int status = stream_create_vlist(streamP, NULL);
    if (status < 0)
    {
      streamID = status;
      FreeAndNull(streamP->record);
      reshRemove(streamP->self, &streamOps);
    }
  }
#else
  Error("tried to use in memory capabilities without Netcdf4 enabled");
#endif

  return streamID;
}

static int
streamOpen(const char *filename, const char *filemode, int filetype)
{
  if (!filemode || strlen(filemode) != 1) return CDI_EINVAL;
  return streamOpenID(filename, (char) tolower(filemode[0]), filetype, CDI_UNDEFID);
}

static int
streamOpenA(const char *filename, const char *filemode, int filetype)
{
  if (CDI_Debug) Message("Open %s file (mode=%c); filename: %s", strfiletype(filetype), (int) *filemode, filename);
  if (CDI_Debug) printf("streamOpenA: %s\n", filename);  // seg fault without this line on thunder/squall with "cdo cat x y"

  if (!filename || !filemode || filetype < 0) return CDI_EINVAL;

  stream_t *streamP = stream_new_entry(CDI_UNDEFID);
  int fileID = CDI_UNDEFID;

  {
    int (*streamOpenDelegate)(const char *filename, char filemode, int filetype, stream_t *streamP, int recordBufIsToBeCreated)
        = (int (*)(const char *, char, int, stream_t *, int)) namespaceSwitchGet(NSSWITCH_STREAM_OPEN_BACKEND).func;

    fileID = streamOpenDelegate(filename, 'r', filetype, streamP, 1);
  }

  if (fileID == CDI_UNDEFID || fileID == CDI_ELIBNAVAIL || fileID == CDI_ESYSTEM) return fileID;

  int streamID = streamP->self;

  streamP->filemode = tolower(*filemode);
  streamP->filename = strdup(filename);
  streamP->fileID = fileID;
  streamP->groupID = fileID;

  streamP->vlistID = vlistCreate();
  cdiVlistMakeInternal(streamP->vlistID);
  // cdiReadByteorder(streamID);
  long status = cdiInqContents(streamP);
  if (status < 0) return (int) status;
  vlist_t *vlistP = vlist_to_pointer(streamP->vlistID);
  vlistP->ntsteps = streamInqNumSteps(streamID);

  // Needed for NetCDF4
  for (int varID = 0; varID < vlistP->nvars; ++varID) streamP->vars[varID].defmiss = true;

  if (str_is_equal(filemode, "r")) cdiVlistMakeImmutable(streamP->vlistID);

  {
    void (*streamCloseDelegate)(stream_t *streamP, int recordBufIsToBeDeleted)
        = (void (*)(stream_t *, int)) namespaceSwitchGet(NSSWITCH_STREAM_CLOSE_BACKEND).func;

    streamCloseDelegate(streamP, 0);
  }

  switch (filetype)
  {
#if defined(HAVE_LIBGRIB) && (defined(HAVE_LIBCGRIBEX) || defined(HAVE_LIBGRIB_API))
    case CDI_FILETYPE_GRB:
#ifdef HAVE_LIBGRIB_API
    case CDI_FILETYPE_GRB2:
#endif
    {
      fileID = gribOpen(filename, filemode);
      if (fileID != CDI_UNDEFID) gribContainersNew(streamP);
      break;
    }
#endif
#ifdef HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV:
    {
      fileID = fileOpen(filename, filemode);
      break;
    }
#endif
#ifdef HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT:
    {
      fileID = fileOpen(filename, filemode);
      break;
    }
#endif
#ifdef HAVE_LIBIEG
    case CDI_FILETYPE_IEG:
    {
      fileID = fileOpen(filename, filemode);
      break;
    }
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NC:
    case CDI_FILETYPE_NC2:
    case CDI_FILETYPE_NC5:
    {
      fileID = cdfOpen(filename, filemode, filetype);
      streamP->ncmode = 2;
      break;
    }
    case CDI_FILETYPE_NC4:
    case CDI_FILETYPE_NC4C:
    {
      fileID = cdf4Open(filename, filemode, &filetype);
      streamP->ncmode = 2;
      break;
    }
    case CDI_FILETYPE_NCZARR:
    {
      Warning("%s not available in append mode!", strfiletype(filetype));
      return CDI_ELIBNAVAIL;
    }
#endif
    default:
    {
      if (CDI_Debug) Message("%s support not compiled in!", strfiletype(filetype));
      return CDI_ELIBNAVAIL;
    }
  }

  if (fileID == CDI_UNDEFID) return CDI_UNDEFID;

  streamP->fileID = fileID;
  streamP->groupID = fileID;

  return streamID;
}

/*
@Function  streamOpenRead
@Title     Open a dataset for reading

@Prototype int streamOpenRead(const char *path)
@Parameter
    @Item  path  The name of the dataset to be read.

@Description
The function @func{streamOpenRead} opens an existing dataset for reading.

@Result
Upon successful completion @func{streamOpenRead} returns an identifier to the
open stream. Otherwise, a negative number with the error status is returned.

@Errors
@List
   @Item  CDI_ESYSTEM     Operating system error.
   @Item  CDI_EINVAL      Invalid argument.
   @Item  CDI_EUFILETYPE  Unsupported file type.
   @Item  CDI_ELIBNAVAIL  Library support not compiled in.
@EndList

@Example
Here is an example using @func{streamOpenRead} to open an existing NetCDF
file named @func{foo.nc} for reading:

@Source
#include "cdi.h"
   ...
int streamID;
   ...
streamID = streamOpenRead("foo.nc");
if ( streamID < 0 ) handle_error(streamID);
   ...
@EndSource
@EndFunction
*/
int
streamOpenRead(const char *filename)
{
  cdi_initialize();

  int byteorder = 0;
  int filetype = cdiGetFiletype(filename, &byteorder);
  if (filetype < 0) return filetype;

  int streamID = streamOpen(filename, "r", filetype);
  if (streamID >= 0)
  {
    stream_t *streamP = stream_to_pointer(streamID);
    streamP->byteorder = byteorder;

    streamP->groupID = streamP->fileID;
    long status = stream_create_vlist(streamP, NULL);
    if (status < 0)
    {
      streamID = (int) status;
      FreeAndNull(streamP->record);
      reshRemove(streamP->self, &streamOps);
    }
  }

  return streamID;
}

int
streamOpenTensorstore(const char *path)
{
  cdi_initialize();

  /*int byteorder = 0;
  int filetype = cdiGetFiletype(filename, &byteorder);
  if (filetype < 0) return filetype;

  int streamID = streamOpen(filename, "r", filetype);
  if (streamID >= 0)
  {
    stream_t *streamP = stream_to_pointer(streamID);
    streamP->byteorder = byteorder;

    streamP->groupID = streamP->fileID;
    long status = stream_create_vlist(streamP, NULL);
    if (status < 0)
    {
      streamID = (int) status;
      FreeAndNull(streamP->record);
      reshRemove(streamP->self, &streamOps);
    }
  }

  return streamID;*/
  return -1;
}

static int
process_group_query(stream_t *streamP, struct CdiQuery *query)
{
  int groupID = -1;
#ifdef HAVE_LIBNETCDF
  int fileID = streamP->fileID;
  int filetype = streamP->filetype;
  if (cdiBaseFiletype(filetype) == CDI_FILETYPE_NETCDF)
  {
    // process var query information if available
    if (query && cdiQueryNumGroups(query) > 0)
    {
      const char *groupName = cdiQueryGetGroup(query, 0);
      groupID = cdf_inq_group(fileID, groupName);
    }
  }
#endif
  return groupID;
}

int
streamOpenReadQuery(const char *filename, struct CdiQuery *query)
{
  cdi_initialize();

  int byteorder = 0;
  int filetype = cdiGetFiletype(filename, &byteorder);
  if (filetype < 0) return filetype;

  if (cdiBaseFiletype(filetype) != CDI_FILETYPE_NETCDF && filetype != CDI_FILETYPE_GRB2) return CDI_EQNAVAIL;

  int streamID = streamOpen(filename, "r", filetype);
  if (streamID >= 0)
  {
    stream_t *streamP = stream_to_pointer(streamID);
    streamP->byteorder = byteorder;

    int groupID = process_group_query(streamP, query);
    if (groupID < -1)
    {
      streamClose(streamID);
      return groupID;
    }

    streamP->groupID = (groupID > -1) ? groupID : streamP->fileID;

    int status = stream_create_vlist(streamP, query);
    if (status < 0)
    {
      streamID = status;
      FreeAndNull(streamP->record);
      reshRemove(streamP->self, &streamOps);
    }
  }

  return streamID;
}

int
streamOpenAppend(const char *filename)
{
  cdi_initialize();

  int byteorder = 0;
  int filetype = cdiGetFiletype(filename, &byteorder);
  if (filetype < 0) return filetype;

  int streamID = streamOpenA(filename, "a", filetype);
  if (streamID >= 0)
  {
    stream_t *streamP = stream_to_pointer(streamID);
    streamP->byteorder = byteorder;
  }

  return streamID;
}

/*
@Function  streamOpenWrite
@Title     Create a new dataset

@Prototype int streamOpenWrite(const char *path, int filetype)
@Parameter
    @Item  path      The name of the new dataset.
    @Item  filetype  The type of the file format, one of the set of predefined CDI file format types.
                     The valid CDI file format types are @func{CDI_FILETYPE_GRB}, @func{CDI_FILETYPE_GRB2}, @func{CDI_FILETYPE_NC},
                     @func{CDI_FILETYPE_NC2}, @func{CDI_FILETYPE_NC4}, @func{CDI_FILETYPE_NC4C}, @func{CDI_FILETYPE_NC5},
                     @func{CDI_FILETYPE_NCZARR}, @func{CDI_FILETYPE_SRV}, @func{CDI_FILETYPE_EXT} and @func{CDI_FILETYPE_IEG}.

@Description
The function @func{streamOpenWrite} creates a new datset.
@Result
Upon successful completion @func{streamOpenWrite} returns an identifier to the
open stream. Otherwise, a negative number with the error status is returned.

@Errors
@List
   @Item  CDI_ESYSTEM     Operating system error.
   @Item  CDI_EINVAL      Invalid argument.
   @Item  CDI_EUFILETYPE  Unsupported file type.
   @Item  CDI_ELIBNAVAIL  Library support not compiled in.
@EndList

@Example
Here is an example using @func{streamOpenWrite} to create a new NetCDF file named @func{foo.nc} for writing:

@Source
#include "cdi.h"
   ...
int streamID;
   ...
streamID = streamOpenWrite("foo.nc", CDI_FILETYPE_NC);
if ( streamID < 0 ) handle_error(streamID);
   ...
@EndSource
@EndFunction
*/
int
streamOpenWrite(const char *filename, int filetype)
{
  cdi_initialize();

  return streamOpen(filename, "w", filetype);
}

static void
streamDefaultValue(stream_t *streamP)
{
  streamP->self = CDI_UNDEFID;
  streamP->accesstype = CDI_UNDEFID;
  streamP->accessmode = 0;
  streamP->filetype = CDI_FILETYPE_UNDEF;
  streamP->byteorder = CDI_UNDEFID;
  streamP->fileID = 0;
  streamP->groupID = -1;
  streamP->filemode = 0;
  streamP->numvals = 0;
  streamP->filename = NULL;
  streamP->record = NULL;
  streamP->query = NULL;
  streamP->varsAllocated = 0;
  streamP->nrecs = 0;
  streamP->nvars = 0;
  streamP->vars = NULL;
  streamP->ncmode = 0;
  streamP->curTsID = CDI_UNDEFID;
  streamP->rtsteps = 0;
  streamP->ntsteps = CDI_UNDEFID;
  streamP->maxSteps = CDI_UNDEFID;
  streamP->tsteps = NULL;
  streamP->tstepsTableSize = 0;
  streamP->tstepsNextID = 0;
  streamP->vlistID = CDI_UNDEFID;
  streamP->globalatts = 0;
  streamP->localatts = 0;
  streamP->unreduced = cdiDataUnreduced;
  streamP->have_missval = cdiHaveMissval;
  streamP->comptype = CDI_COMPRESS_NONE;
  streamP->complevel = 0;
  streamP->shuffle = 0;
  streamP->sortname = cdiSortName;
  streamP->lockIO = CDI_Lock_IO;
  // netcdf4/HDF5 filter
  streamP->filterSpec = NULL;

  basetimeInit(&streamP->basetime);

#ifdef HAVE_LIBNETCDF
  CdfInfo *cdfInfo = &(streamP->cdfInfo);

  for (int i = 0; i < MAX_GRIDS_PS; i++)
  {
    cdfInfo->cdfGridList[i].start = CDI_UNDEFID;
    cdfInfo->cdfGridList[i].count = CDI_UNDEFID;
    cdfInfo->cdfGridList[i].gridID = CDI_UNDEFID;
    for (size_t j = 0; j < CDF_SIZE_NCID; ++j) cdfInfo->cdfGridList[i].ncIdList[j] = CDI_UNDEFID;
  }

  for (int i = 0; i < MAX_ZAXES_PS; i++)
  {
    cdfInfo->cdfZaxisList[i].start = CDI_UNDEFID;
    cdfInfo->cdfZaxisList[i].count = CDI_UNDEFID;
    cdfInfo->cdfZaxisList[i].zaxisID = CDI_UNDEFID;
  }

  cdfInfo->chunkSizeDimT = 0;
  cdfInfo->chunkSizeDimZ = 0;

  for (int i = 0; i < MAX_DIMS_PS; i++) cdfInfo->ncDimLenList[i] = 0;
  for (int i = 0; i < MAX_ZAXES_PS; i++) cdfInfo->zaxisIdList[i] = CDI_UNDEFID;
  for (int i = 0; i < MAX_ZAXES_PS; i++) cdfInfo->ncZvarIdList[i] = CDI_UNDEFID;
  for (int i = 0; i < MAX_DIMS_PS; i++) cdfInfo->ncDimIdList[i] = CDI_UNDEFID;

  cdfInfo->ncNumDims = 0;
  cdfInfo->complexFloatId = CDI_UNDEFID;
  cdfInfo->complexDoubleId = CDI_UNDEFID;

  cdfInfo->vct.ilev = 0;
  cdfInfo->vct.mlev = 0;
  cdfInfo->vct.ilevID = CDI_UNDEFID;
  cdfInfo->vct.mlevID = CDI_UNDEFID;
#endif
  streamP->maxGlobalRecs = CDI_UNDEFID;

  streamP->gribContainers = NULL;

  streamP->numWorker = 0;
  streamP->nextGlobalRecId = 0;
  streamP->cachedTsID = -1;
  streamP->jobs = NULL;
  streamP->jobManager = NULL;

  streamP->protocolData = NULL;

#ifdef HAVE_LIBFDB5
  streamP->fdbNumItems = 0;
  streamP->fdbKeyValueList = NULL;
#endif
}

static stream_t *
stream_new_entry(int resH)
{
  cdi_initialize(); /* ***************** make MT version !!! */

  stream_t *streamP = (stream_t *) Malloc(sizeof(stream_t));
  streamDefaultValue(streamP);

  if (resH == CDI_UNDEFID) { streamP->self = reshPut(streamP, &streamOps); }
  else
  {
    streamP->self = resH;
    reshReplace(resH, streamP, &streamOps);
  }

  return streamP;
}

void
cdiStreamCloseDefaultDelegate(stream_t *streamP, int recordBufIsToBeDeleted)
{
  int fileID = streamP->fileID;
  int filetype = streamP->filetype;

  FreeAndNull(streamP->filterSpec);

  switch (streamP->protocol)
  {
    case CDI_PROTOCOL_ACROSS:
#ifdef HAVE_ACROSS
      if (fileID) across_disconnect(fileID);
      if (streamP->protocolData)
      {
        Free(((across_info_t *) streamP->protocolData)->expid);
        FreeAndNull(streamP->protocolData);
      }
#endif
      return;

    case CDI_PROTOCOL_FDB:
#ifdef HAVE_LIBFDB5
      if (streamP->protocolData) check_fdb_error(fdb_delete_handle(streamP->protocolData));
      streamP->protocolData = NULL;
#endif
      return;

    case CDI_PROTOCOL_OTHER:
    case CDI_PROTOCOL_FILE:
      // handled below;
      break;
  }

  if (fileID == CDI_UNDEFID)
  {
    Warning("File %s not open!", streamP->filename);
    return;
  }

  switch (cdiBaseFiletype(filetype))
  {
#if defined(HAVE_LIBGRIB) && (defined(HAVE_LIBCGRIBEX) || defined(HAVE_LIBGRIB_API))
    case CDI_FILETYPE_GRIB:
      if (filetype == CDI_FILETYPE_GRB)
      {
        gribClose(fileID);
        if (recordBufIsToBeDeleted) gribContainersDelete(streamP);
#ifdef HAVE_LIBCGRIBEX
        if (recordBufIsToBeDeleted) cgribexDelete(streamP->record->objectp);
#endif
      }
      else if (filetype == CDI_FILETYPE_GRB2)
      {
        gribClose(fileID);
        if (recordBufIsToBeDeleted) gribContainersDelete(streamP);
      }
      break;
#endif
#ifdef HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV:
    {
      fileClose(fileID);
      if (recordBufIsToBeDeleted) srvDelete(streamP->record->objectp);
      break;
    }
#endif
#ifdef HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT:
    {
      fileClose(fileID);
      if (recordBufIsToBeDeleted) extDelete(streamP->record->objectp);
      break;
    }
#endif
#ifdef HAVE_LIBIEG
    case CDI_FILETYPE_IEG:
    {
      fileClose(fileID);
      if (recordBufIsToBeDeleted) iegDelete(streamP->record->objectp);
      break;
    }
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NETCDF:
    {
      cdfClose(fileID);
      if (streamP->ntsteps == 0 && streamP->tsteps != NULL)
      {
        tsteps_t *tstep = &(streamP->tsteps[0]);
        FreeAndNull(tstep->recinfo);
        FreeAndNull(tstep->records);
        FreeAndNull(tstep->recIDs);
      }
      break;
    }
#endif
#ifdef HAVE_TENSORSTORE
    case CDI_FILETYPE_ZARR:
    {
      ts_close(fileID);
      if (streamP->ntsteps == 0 && streamP->tsteps != NULL)
      {
        tsteps_t *tstep = &(streamP->tsteps[0]);
        FreeAndNull(tstep->recinfo);
        FreeAndNull(tstep->records);
        FreeAndNull(tstep->recIDs);
      }
      break;
    }
#endif
    default:
    {
      Error("%s support not compiled in (fileID = %d)!", strfiletype(filetype), fileID);
      break;
    }
  }
}

static void
deallocate_sleveltable_t(sleveltable_t *entry)
{
  FreeAndNull(entry->recordID);
  FreeAndNull(entry->lindex);
}

#ifdef HAVE_LIBNETCDF
static void
deallocate_cdfCache(CdfCache *cache)
{
  FreeAndNull(cache->buffer);
  FreeAndNull(cache);
}
#endif

static void
streamDestroyViaDelegate(stream_t *streamP, void (*streamCloseDelegate)(stream_t *streamP, int recordBufIsToBeDeleted))
{
  xassert(streamP);
  if (streamP->filetype != CDI_FILETYPE_UNDEF) streamCloseDelegate(streamP, 1);

  if (streamP->record)
  {
    Free(streamP->record->buffer);
    FreeAndNull(streamP->record);
  }

  streamP->filetype = CDI_FILETYPE_UNDEF;
  FreeAndNull(streamP->filename);
  FreeAndNull(streamP->filterSpec);

  if (streamP->vars)
  {
    for (int index = 0; index < streamP->nvars; index++)
    {
      sleveltable_t *pslev = streamP->vars[index].recordTable;
      unsigned nsub = streamP->vars[index].subtypeSize >= 0 ? (unsigned) streamP->vars[index].subtypeSize : 0U;
      for (size_t isub = 0; isub < nsub; isub++) deallocate_sleveltable_t(pslev + isub);
      Free(pslev);
#ifdef HAVE_LIBNETCDF
      if (streamP->vars[index].cdfCache) deallocate_cdfCache(streamP->vars[index].cdfCache);
      streamP->vars[index].cdfCache = NULL;
#endif
    }
    FreeAndNull(streamP->vars);
  }

  if (streamP->tsteps)
  {
    int maxSteps = streamP->tstepsNextID;
    for (int index = 0; index < maxSteps; ++index)
    {
      tsteps_t *tstep = &(streamP->tsteps[index]);
      FreeAndNull(tstep->recinfo);
      FreeAndNull(tstep->records);
      FreeAndNull(tstep->recIDs);
      taxisDestroyKernel(&(tstep->taxis));
    }

    FreeAndNull(streamP->tsteps);
  }

#ifdef HAVE_LIBFDB5
  if (streamP->fdbKeyValueList)
  {
    cdi_fdb_delete_kvlist(streamP->fdbNumItems, streamP->fdbKeyValueList);
    streamP->fdbNumItems = 0;
    streamP->fdbKeyValueList = NULL;
  }
#endif

  int vlistID = streamP->vlistID;
  if (vlistID != -1)
  {
    int taxisID = (streamP->filemode != 'w') ? vlistInqTaxis(vlistID) : -1;
    if (taxisID != -1) taxisDestroy(taxisID);
    void (*mycdiVlistDestroy_)(int, bool) = (void (*)(int, bool)) namespaceSwitchGet(NSSWITCH_VLIST_DESTROY_).func;
    mycdiVlistDestroy_(vlistID, true);
  }

  Free(streamP->jobs);
  if (streamP->jobManager) AsyncWorker_finalize((AsyncManager *) streamP->jobManager);

  Free(streamP);
}

static void
streamDestroy(stream_t *streamP)
{
  void (*streamCloseDelegate)(stream_t *streamP, int recordBufIsToBeDeleted)
      = (void (*)(stream_t *, int)) namespaceSwitchGet(NSSWITCH_STREAM_CLOSE_BACKEND).func;

  streamDestroyViaDelegate(streamP, streamCloseDelegate);
}

static void
stream_destroy(void *streamP)
{
  streamDestroy((stream_t *) streamP);
}

/*
@Function  streamClose
@Title     Close an open dataset

@Prototype  void streamClose(int streamID)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenRead} or @fref{streamOpenWrite}.

@Description
The function @func{streamClose} closes an open dataset.

@EndFunction
*/
void
streamClose(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);

  bool lockIO = streamP->lockIO;
  if (lockIO) CDI_IO_LOCK();

  if (CDI_Debug) Message("streamID = %d filename = %s", streamID, streamP->filename);
  streamDestroy(streamP);
  reshRemove(streamID, &streamOps);
  if (CDI_Debug) Message("Removed stream %d from stream list", streamID);

  if (lockIO) CDI_IO_UNLOCK();
}

void
cdiStreamSync_(stream_t *streamP)
{
  int fileID = streamP->fileID;
  int filetype = streamP->filetype;
  int vlistID = streamP->vlistID;
  int numVars = vlistNvars(vlistID);

  if (fileID == CDI_UNDEFID) { Warning("File %s not open!", streamP->filename); }
  else if (vlistID == CDI_UNDEFID) { Warning("Vlist undefined for file %s!", streamP->filename); }
  else if (numVars == 0) { Warning("No variables defined!"); }
  else
  {
    if (streamP->filemode == 'w' || streamP->filemode == 'a')
    {
      switch (cdiBaseFiletype(filetype))
      {
#ifdef HAVE_LIBNETCDF
        case CDI_FILETYPE_NETCDF:
        {
          void cdf_sync(int ncid);
          if (streamP->ncmode == 2) cdf_sync(fileID);
          break;
        }
#endif
        default:
        {
          fileFlush(fileID);
          break;
        }
      }
    }
  }
}

/*
@Function  streamSync
@Title     Synchronize an Open Dataset to Disk

@Prototype  void streamSync(int streamID)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenWrite}.

@Description
The function @func{streamSync} offers a way to synchronize the disk copy of a dataset with in-memory buffers.

@EndFunction
*/
void
streamSync(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);

  void (*myStreamSync_)(stream_t *streamP) = (void (*)(stream_t *)) namespaceSwitchGet(NSSWITCH_STREAM_SYNC).func;
  myStreamSync_(streamP);
}

int
cdiStreamDefTimestep_(stream_t *streamP, int tsID)
{
  stream_check_ptr(__func__, streamP);

  if (CDI_Debug) Message("streamID = %d  tsID = %d", streamP->self, tsID);

  int vlistID = streamP->vlistID;
  if (vlistID == CDI_UNDEFID)
    Error("Must not call streamDefTimestep for stream (ID=%d) with (not yet) defined vlist", streamP->self);

  if (tsID > 0)
  {
    int newtsID = tstepsNewEntry(streamP);
    if (tsID != newtsID) Error("Internal problem: tsID = %d newtsID = %d", tsID, newtsID);
  }

  int taxisID = vlistInqTaxis(vlistID);
  if (taxisID != CDI_UNDEFID) ptaxisCopy(&streamP->tsteps[tsID].taxis, taxis_to_pointer(taxisID));

  streamP->curTsID = tsID;
  streamP->ntsteps = tsID + 1;

  bool baseFiletypeIsNetCDF = (cdiBaseFiletype(streamP->filetype) == CDI_FILETYPE_NETCDF);
#ifdef HAVE_LIBNETCDF
  int timeIsVarying = vlistHasTime(vlistID);
  if (baseFiletypeIsNetCDF && timeIsVarying)
  {
    /* usually points to cdfDefTimestep in serial mode but
     * to cdiPioCdfDefTimestep on servers and to a null-op on
     * clients in client/server mode */
    void (*myCdfDefTimestep)(stream_t *streamP, int tsID, size_t)
        = (void (*)(stream_t *, int, size_t)) namespaceSwitchGet(NSSWITCH_CDF_DEF_TIMESTEP).func;
    myCdfDefTimestep(streamP, tsID, 1);
  }
#endif

  cdi_create_records(streamP, tsID, !baseFiletypeIsNetCDF);

  return (int) streamP->ntsteps;
}

/*
@Function  streamDefTimestep
@Title     Define a timestep

@Prototype int streamDefTimestep(int streamID, int tsID)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenWrite}.
    @Item  tsID      Timestep identifier.

@Description
The function @func{streamDefTimestep} defines a timestep of a stream by the identifier tsID.
The identifier tsID is the timestep index starting at 0 for the first timestep.
Before calling this function the functions @func{taxisDefVdate} and @func{taxisDefVtime} should be used
to define the timestamp for this timestep. All calls to write the data refer to this timestep.

@Result
@func{streamDefTimestep} returns the number of expected records of the timestep.

@EndFunction
*/
int
streamDefTimestep(int streamID, int tsID)
{
  stream_t *streamP = stream_to_pointer(streamID);

  if (streamP->lockIO) CDI_IO_LOCK();

  int (*myStreamDefTimestep_)(stream_t *streamP, int tsID)
      = (int (*)(stream_t *, int)) namespaceSwitchGet(NSSWITCH_STREAM_DEF_TIMESTEP_).func;
  int status = myStreamDefTimestep_(streamP, tsID);

  if (streamP->lockIO) CDI_IO_UNLOCK();

  return status;
}

int
streamInqCurTimestepID(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);
  return streamP->curTsID;
}

/*
@Function  streamInqTimestep
@Title     Get timestep information

@Prototype int streamInqTimestep(int streamID, int tsID)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenRead} or @fref{streamOpenWrite}.
    @Item  tsID      Timestep identifier.

@Description
The function @func{streamInqTimestep} sets the next timestep to the identifier tsID.
The identifier tsID is the timestep index starting at 0 for the first timestep.
After a call to this function the functions @func{taxisInqVdate} and @func{taxisInqVtime} can be used
to read the timestamp for this timestep. All calls to read the data refer to this timestep.

@Result
@func{streamInqTimestep} returns the number of records of the timestep or 0, if the end of the file is reached.

@EndFunction
*/
int
streamInqTimestep(int streamID, int tsID)
{
  int nrecs = 0;
  stream_t *streamP = stream_to_pointer(streamID);
  int vlistID = streamP->vlistID;

  if (tsID < streamP->ntsteps) { streamP->tsteps[tsID].curRecID = CDI_UNDEFID; }  // fix for netCDF
  if (tsID < streamP->rtsteps)
  {
    streamP->curTsID = tsID;
    nrecs = streamP->tsteps[tsID].nrecs;
    streamP->tsteps[tsID].curRecID = CDI_UNDEFID;
    int taxisID = vlistInqTaxis(vlistID);
    if (taxisID == -1) Error("Timestep undefined for fileID = %d", streamID);
    ptaxisCopy(taxis_to_pointer(taxisID), &streamP->tsteps[tsID].taxis);

    return nrecs;
  }

  if (tsID >= streamP->ntsteps && streamP->ntsteps != CDI_UNDEFID) return 0;

  int filetype = streamP->filetype;

  if (CDI_Debug) Message("streamID = %d  tsID = %d  filetype = %d", streamID, tsID, filetype);

  if (streamP->lockIO) CDI_IO_LOCK();

  switch (cdiBaseFiletype(filetype))
  {
#ifdef HAVE_LIBGRIB
    case CDI_FILETYPE_GRIB:
      switch (streamP->protocol)
      {
        case CDI_PROTOCOL_FDB: nrecs = fdbInqTimestep(streamP, tsID); break;
        case CDI_PROTOCOL_ACROSS:  // TODO read from ACROSS
        case CDI_PROTOCOL_OTHER:
        case CDI_PROTOCOL_FILE: nrecs = grbInqTimestep(streamP, tsID); break;
      }
      break;
#endif
#ifdef HAVE_LIBSERVICE
    case CDI_FILETYPE_SRV: nrecs = srvInqTimestep(streamP, tsID); break;
#endif
#ifdef HAVE_LIBEXTRA
    case CDI_FILETYPE_EXT: nrecs = extInqTimestep(streamP, tsID); break;
#endif
#ifdef HAVE_LIBIEG
    case CDI_FILETYPE_IEG: nrecs = iegInqTimestep(streamP, tsID); break;
#endif
#ifdef HAVE_LIBNETCDF
    case CDI_FILETYPE_NETCDF: nrecs = cdfInqTimestep(streamP, tsID); break;
#endif
#ifdef HAVE_TENSORSTORE
    case CDI_FILETYPE_ZARR: nrecs = ts_inq_timestep(streamP, tsID); break;
#endif
    default: Error("%s support not compiled in!", strfiletype(filetype)); break;
  }

  if (streamP->lockIO) CDI_IO_UNLOCK();

  int taxisID = vlistInqTaxis(vlistID);
  if (taxisID == -1) Error("Time axis undefined for fileID = %d", streamID);

  ptaxisCopy(taxis_to_pointer(taxisID), &streamP->tsteps[tsID].taxis);

  return nrecs;
}

// This function is used in CDO!
SizeType
streamNvals(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);
  return streamP->numvals;
}

/*
@Function  streamDefVlist
@Title     Define the variable list

@Prototype void streamDefVlist(int streamID, int vlistID)
@Parameter
    @Item  streamID Stream ID, from a previous call to @fref{streamOpenWrite}.
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}.

@Description
The function @func{streamDefVlist} defines the variable list of a stream.

To safeguard against errors by modifying the wrong vlist object,
this function makes the passed vlist object immutable.
All further vlist changes have to use the vlist object returned by streamInqVlist().

@EndFunction
*/
void
streamDefVlist(int streamID, int vlistID)
{
  void (*myStreamDefVlist)(int streamID, int vlistID) = (void (*)(int, int)) namespaceSwitchGet(NSSWITCH_STREAM_DEF_VLIST_).func;
  myStreamDefVlist(streamID, vlistID);
}

// The single image implementation of streamDefVlist
void
cdiStreamDefVlist_(int streamID, int vlistID)
{
  stream_t *streamP = stream_to_pointer(streamID);

  if (streamP->vlistID == CDI_UNDEFID)
  {
    if (streamP->lockIO) CDI_IO_LOCK();

    int vlistCopy = vlistDuplicate(vlistID);
    cdiVlistMakeInternal(vlistCopy);
    cdiVlistMakeImmutable(vlistID);
    cdiStreamSetupVlist(streamP, vlistCopy);

    if (streamP->lockIO) CDI_IO_UNLOCK();
  }
  else
    Warning("vlist already defined for %s!", streamP->filename);
}

/*
@Function  streamInqVlist
@Title     Get the variable list

@Prototype int streamInqVlist(int streamID)
@Parameter
    @Item  streamID  Stream ID, from a previous call to @fref{streamOpenRead} or @fref{streamOpenWrite}.

@Description
The function @func{streamInqVlist} returns the variable list of a stream.

@Result
@func{streamInqVlist} returns an identifier to the variable list.

@EndFunction
*/
int
streamInqVlist(int streamID)
{
  stream_t *s = stream_to_pointer(streamID);
  return s->vlistID;
}

void
streamDefShuffle(int streamID, int shuffle)
{
  stream_t *s = stream_to_pointer(streamID);
  if (s->shuffle != shuffle)
  {
    s->shuffle = shuffle;
    reshSetStatus(streamID, &streamOps, RESH_DESYNC_IN_USE);
  }
}

void
streamDefFilter(int streamID, const char *filterSpec)
{
  stream_t *s = stream_to_pointer(streamID);
  if (filterSpec)
  {
    // if (s->filterSpec) Error("Filter spec already defined!");
    if (s->filterSpec == NULL)
    {
      s->filterSpec = strdup(filterSpec);
      reshSetStatus(streamID, &streamOps, RESH_DESYNC_IN_USE);
    }
  }
}

void
streamDefCompType(int streamID, int comptype)
{
  stream_t *s = stream_to_pointer(streamID);
  if (s->comptype != comptype)
  {
    s->comptype = comptype;
    reshSetStatus(streamID, &streamOps, RESH_DESYNC_IN_USE);
  }
}

void
streamDefCompLevel(int streamID, int complevel)
{
  stream_t *s = stream_to_pointer(streamID);
  if (s->complevel != complevel)
  {
    s->complevel = complevel;
    reshSetStatus(streamID, &streamOps, RESH_DESYNC_IN_USE);
  }
}

int
streamInqCompType(int streamID)
{
  stream_t *s = stream_to_pointer(streamID);
  return s->comptype;
}

int
streamInqCompLevel(int streamID)
{
  stream_t *s = stream_to_pointer(streamID);
  return s->complevel;
}

int
streamInqFileID(int streamID)
{
  stream_t *s = (stream_t *) reshGetVal(streamID, &streamOps);
  return s->fileID;
}

void
cdiStreamSetupVlist(stream_t *s, int vlistID)
{
  void (*myStreamSetupVlist)(stream_t *s, int vlistID)
      = (void (*)(stream_t *, int)) namespaceSwitchGet(NSSWITCH_STREAM_SETUP_VLIST).func;
  myStreamSetupVlist(s, vlistID);
}

void
cdiStreamSetupVlist_(stream_t *streamP, int vlistID)
{
  streamP->vlistID = vlistID;
  int numVars = vlistNvars(vlistID);
  for (int varID = 0; varID < numVars; ++varID)
  {
    int gridID = vlistInqVarGrid(vlistID, varID);
    int zaxisID = vlistInqVarZaxis(vlistID, varID);
    int tilesetID = vlistInqVarSubtype(vlistID, varID);
    stream_new_var(streamP, gridID, zaxisID, tilesetID);
    if (streamP->have_missval) vlistDefVarMissval(vlistID, varID, vlistInqVarMissval(vlistID, varID));
  }

  if (streamP->filemode == 'w')
  {
    tstepsNewEntry(streamP);  // timestep 0
    int vlistIDw = streamP->vlistID;
    int timeIsVarying = vlistHasTime(vlistIDw);
    if (timeIsVarying)
    {
      int taxisID = vlistInqTaxis(vlistIDw);
      if (taxisID == CDI_UNDEFID)
      {
        Warning("taxisID undefined for fileID = %d! Using absolute time axis.", streamP->self);
        taxisID = taxisCreate(TAXIS_ABSOLUTE);
        vlistDefTaxis(vlistIDw, taxisID);
      }

#ifdef HAVE_LIBNETCDF
      if (taxisInqType(taxisID) == TAXIS_RELATIVE)
        if (cdiBaseFiletype(streamP->filetype) == CDI_FILETYPE_NETCDF)
        {
          const taxis_t *taxisP = taxis_to_pointer(taxisID);
          if (cdiDateTime_isNull(taxisP->rDateTime))
          {
            int vdate = taxisInqVdate(taxisID);
            if (vdate == 0) vdate = 10101;
            taxisDefRdate(taxisID, vdate);
          }
        }
#endif
      ptaxisCopy(&streamP->tsteps[0].taxis, taxis_to_pointer(taxisID));
    }

    switch (cdiBaseFiletype(streamP->filetype))
    {
#ifdef HAVE_LIBNETCDF
      case CDI_FILETYPE_NETCDF:
      {
        /* calls cdfDefCoordinateVars in serial mode but
         * cdiPioClientStreamNOP (i.e. nothing) on client ranks
         * and cdiPioServerCdfDefVars on server ranks in parallel mode*/
        void (*myCdfDefVars)(stream_t *streamP) = (void (*)(stream_t *)) namespaceSwitchGet(NSSWITCH_CDF_STREAM_SETUP).func;
        myCdfDefVars(streamP);
      }
      break;
#endif
#ifdef HAVE_LIBGRIB
      case CDI_FILETYPE_GRIB: gribContainersNew(streamP); break;
#endif
      default:;
    }
  }
}

void
cdiStreamGetIndexList(unsigned numIDs, int *IDs)
{
  reshGetResHListOfType(numIDs, IDs, &streamOps);
}

int
streamInqNvars(int streamID)
{
  stream_t *s = (stream_t *) reshGetVal(streamID, &streamOps);
  return s->nvars;
}

static int
stream_compare(void *streamP1, void *streamP2)
{
  stream_t *s1 = (stream_t *) streamP1;
  stream_t *s2 = (stream_t *) streamP2;
  enum
  {
    differ = -1,
    equal = 0,
  };

  xassert(s1);
  xassert(s2);

  if (s1->filetype != s2->filetype) return differ;
  if (s1->byteorder != s2->byteorder) return differ;
  if (s1->comptype != s2->comptype) return differ;
  if (s1->complevel != s2->complevel) return differ;

  if (s1->filename)
  {
    if (!str_is_equal(s1->filename, s2->filename)) return differ;
  }
  else if (s2->filename)
    return differ;

  return equal;
}

void
stream_print(void *streamP, FILE *fp)
{
  stream_t *sp = (stream_t *) streamP;
  if (!sp) return;

  fprintf(fp,
          "#\n"
          "# streamID %d\n"
          "#\n"
          "self          = %d\n"
          "accesstype    = %d\n"
          "accessmode    = %d\n"
          "filetype      = %d\n"
          "byteorder     = %d\n"
          "fileID        = %d\n"
          "filemode      = %d\n"
          "filename      = %s\n"
          "nrecs         = %d\n"
          "nvars         = %d\n"
          "varsAllocated = %d\n"
          "curTsID       = %d\n"
          "rtsteps       = %d\n"
          "ntsteps       = %ld\n"
          "tstepsTableSize= %d\n"
          "tstepsNextID  = %d\n"
          "ncmode        = %d\n"
          "vlistID       = %d\n"
          "globalatts    = %d\n"
          "localatts     = %d\n"
          "unreduced     = %d\n"
          "sortname      = %d\n"
          "have_missval  = %d\n"
          "ztype         = %d\n"
          "zlevel        = %d\n",
          sp->self, sp->self, sp->accesstype, sp->accessmode, sp->filetype, sp->byteorder, sp->fileID, sp->filemode, sp->filename,
          sp->nrecs, sp->nvars, sp->varsAllocated, sp->curTsID, sp->rtsteps, sp->ntsteps, sp->tstepsTableSize, sp->tstepsNextID,
          sp->ncmode, sp->vlistID, sp->globalatts, sp->localatts, sp->unreduced, sp->sortname, sp->have_missval, sp->comptype,
          sp->complevel);
}

/* *
 * This function does not really close the memio,
 * this has to be done outside cdi to access the memory buffer*/
void
freePtrAfterNCMem(stream_t *streamP, int recordBufIsToBeDeleted)
{
  (void) recordBufIsToBeDeleted;
  int fileID = streamP->fileID;

  if (fileID == CDI_UNDEFID)
  {
    Warning("File %s not open!", streamP->filename);
    return;
  }

  if (streamP->ntsteps == 0 && streamP->tsteps != NULL)
  {
    tsteps_t *tstep = &(streamP->tsteps[0]);
    FreeAndNull(tstep->recinfo);
    FreeAndNull(tstep->records);
    FreeAndNull(tstep->recIDs);
  }
}

void
streamCloseNCMem(int streamID)
{
  stream_t *streamP = stream_to_pointer(streamID);

  bool lockIO = streamP->lockIO;
  if (lockIO) CDI_IO_LOCK();

  if (CDI_Debug) Message("streamID = %d filename = %s", streamID, streamP->filename);
  streamDestroyViaDelegate(streamP, freePtrAfterNCMem);
  reshRemove(streamID, &streamOps);
  if (CDI_Debug) Message("Removed stream %d from stream list", streamID);

  if (lockIO) CDI_IO_UNLOCK();
}

int
streamOpenReadNCMem(int ncidp)
{
  return streamOpenNCMem(ncidp, 'r');
}

int
streamOpenWriteNCMem(int ncidp)
{
  return streamOpenNCMem(ncidp, 'w');
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
