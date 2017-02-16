#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

#include "cdipio.h"
#include "dmemory.h"
#include "namespace.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_id_set.h"
#include "pio_impl.h"
#include "pio_util.h"

struct remoteFileBuf
{
  char *name;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  enum IO_Server_command command;
  MPI_Request request;
  int tsID;
};

static struct remoteFileBuf *openRemoteFiles;
static unsigned openRemoteFilesSize, openRemoteFilesFill;

static void
initRemoteFileBuf(struct remoteFileBuf *rfile, const char *filename, size_t bs)
{

  xdebug ( "filename=%s, buffersize=%zu, in", filename, bs );

  size_t len = strlen(filename);
  rfile->name = Malloc(len + 1);
  memcpy(rfile->name, filename, len + 1);
  rfile->tsID = 0;

  /* init output buffer */

  xdebug ( "filename=%s, init output buffer",  filename );

  int iret = dbuffer_init(&rfile->db1, bs);
  iret += dbuffer_init(&rfile->db2, bs);

  if ( iret > 0 ) xabort ( "dbuffer_init did not succeed" );

  rfile->db = rfile->db1;

  rfile->command = IO_Open_file;
  rfile->request = MPI_REQUEST_NULL;

  xdebug ( "added name=%s, return", rfile->name );
}

static int
destroyRemoteFileBuf(struct remoteFileBuf *rfile)
{
  MPI_Status status;

  xdebug ( "filename=%s, cleanup, in", rfile->name );

  xmpiStat(MPI_Wait(&rfile->request, &status), &status);
  dbuffer_cleanup(&rfile->db1);
  dbuffer_cleanup(&rfile->db2);
  Free(rfile->name);
  rfile->name = NULL;

  xdebug("%s", "cleaned up, return");

  return 0;
}

/***************************************************************/
/* send buffer to writer and swap buffer for filling */
static void
sendP(struct remoteFileBuf *rfile)
{
  MPI_Status status;

  size_t amount = dbuffer_data_size ( rfile->db );
  int fileID = (int)(rfile - openRemoteFiles);
  int tag = encodeFileOpTag(fileID, rfile->command);

  xdebug("send buffer for %s, size: %zu bytes, command=%s, in",
         rfile->name, amount, cdiPioCmdStrTab[rfile->command]);

  xmpiStat(MPI_Wait(&(rfile->request), &status), &status);

  /* FIXME: amount > INT_MAX unhandled */
  xmpi(MPI_Issend(rfile->db->buffer, (int)amount, MPI_UNSIGNED_CHAR,
                  commInqSizePio() - 1, tag, commInqCommPio(),
                  &rfile->request));

  /* change outputBuffer */
  dbuffer_reset ( rfile->db );
  if ( rfile->db == rfile->db1 )
    {
      xdebug("%s", "Change to buffer 2 ...");
      rfile->db =  rfile->db2;
    }
  else
    {
      xdebug("%s", "Change to buffer 1 ...");
      rfile->db =  rfile->db1;
    }
  rfile->command = IO_Send_buffer;
}

static void
defTimestep(struct remoteFileBuf *rfile, int tsID)
{
  if ( rfile == NULL || tsID != rfile->tsID + 1 )
    xabort ( " defTimestepPA () didn't succeed." );
  rfile->tsID = tsID;
}

static void
flushOp(struct remoteFileBuf *fb, int tsID)
{
  sendP(fb);
  defTimestep(fb, (int)(intptr_t)tsID);
}


static size_t
pioSendWrite(int fileID, const void *buffer, size_t len, int tsID)
{
  assert(fileID >= 0 && (size_t)fileID < openRemoteFilesSize
         && openRemoteFiles[fileID].name);
  int error = 0;
  int filled;

  struct remoteFileBuf *rfile = openRemoteFiles + fileID;

  bool flush = tsID != rfile->tsID;

  if ( flush )
    {
      xdebug("tsID = %d, flush buffer for fileID=%d", tsID, fileID);

      flushOp(rfile, tsID);
      {
        MPI_Status status;
        xmpiStat(MPI_Wait(&(rfile->request), &status), &status);
      }
      xmpi(MPI_Barrier(commInqCommColl()));
    }

  filled = dbuffer_push(rfile->db, buffer, len);

  xdebug ( "id = %d, tsID = %d, pushed %lu byte data on buffer, filled = %d",
           fileID, tsID, len, filled);

  if (filled == 1)
    {
      if ( flush )
        error = filled;
      else
        {
          sendP(rfile);
          error = dbuffer_push(rfile->db, buffer, len);
        }
    }

  if ( error == 1 )
    xabort("did not succeed filling output buffer, id=%d", fileID);

  return len;
}


static int
pioSendClose(int fileID)
{
  assert(fileID >= 0 && (size_t)fileID < openRemoteFilesSize
         && openRemoteFiles[fileID].name);

  struct remoteFileBuf *rfile = openRemoteFiles + fileID;

  xdebug("fileID %d: send buffer, close file and cleanup", fileID);

  rfile->command = IO_Close_file;

  sendP(rfile);
  /* wait for other collectors to also close the file
   * this prevents one process from re-using the file ID before
   * another has sent the close */
  xmpi(MPI_Barrier(commInqCommColl()));

  /* remove file element */
  destroyRemoteFileBuf(rfile);
  return 0;
}

static int
pioSendOpen(const char *filename, const char *mode)
{
  if ((mode[0] != 'w' && mode[0] != 'W') || mode[0] == 0 || mode[1] != 0)
    xabort("Unsupported mode \"%s\" in parallel file open.", mode);

  struct cdiPioConf *conf = cdiPioGetConf();

  /* init and add struct remoteFileBuf */
  for (size_t i = 0; i < openRemoteFilesSize; ++i)
    if (openRemoteFiles[i].name && !strcmp(openRemoteFiles[i].name, filename))
      {
        Warning("filename %s is already open! CDI-PIO does not support concurrent access through different filehandles.", filename);
        return CDI_EINVAL;
      }

  int fileID = CDI_ELIMIT;
  if (openRemoteFilesSize == openRemoteFilesFill)
    {
      fileID = (int)openRemoteFilesSize;
      if (openRemoteFilesSize == (size_t)INT_MAX + 1)
        return CDI_ELIMIT;
      openRemoteFilesSize = openRemoteFilesSize ? openRemoteFilesSize * 2 : 4;
      if (openRemoteFilesSize > (size_t)INT_MAX + 1)
        openRemoteFilesSize = (size_t)INT_MAX + 1;
      openRemoteFiles = Realloc(openRemoteFiles, sizeof (*openRemoteFiles)
                                * openRemoteFilesSize);
      for (size_t i = (size_t)fileID; i < openRemoteFilesSize; ++i)
        openRemoteFiles[i].name = NULL;
    }
  else
    {
      for (size_t i = 0; i < openRemoteFilesSize; ++i)
        if (openRemoteFiles[i].name == NULL)
          {
            fileID = (int)i;
            break;
          }
    }
  struct remoteFileBuf *rfile = openRemoteFiles + fileID;
  initRemoteFileBuf(rfile, filename, conf->writeAggBufLim);

  xdebug("filename=%s, init and added remoteFileBuf, return fileID = %d",
         filename, fileID);

  /* put filename, id and buffersize on buffer */
  int iret = dbuffer_push(rfile->db, filename, strlen(filename) + 1);
  xassert(iret == 0);

  sendP(rfile);
  MPI_Comm commCollectors = commInqCommColl();
  xmpi(MPI_Barrier(commCollectors));
  return fileID;
}

static void
pioSendFinalize(void)
{
  int buffer = 0, tag, specialRank = commInqSizePio() - 1;
  MPI_Comm commPio = commInqCommPio();

  tag = encodeFileOpTag(0, IO_Finalize);

  xmpi(MPI_Send(&buffer, 1, MPI_INT, specialRank, tag, commPio));
  xdebug("%s", "SENT MESSAGE WITH TAG \"IO_FINALIZE\" TO SPECIAL PROCESS");

  if (openRemoteFilesFill)
    xabort("files still open.");
  else
    {
      xdebug("%s", "destroy set");
      Free(openRemoteFiles);
    }
}

void
pioSendInitialize(void)
{
  int numIOServers = commInqSizePio();
  if (numIOServers < 2)
    xabort("error: # of I/O processes must be >= 2 for mode, but is %d",
           numIOServers);

  int isCollector = commInqRankColl() != -1;
  if (!isCollector)
    switch ( commInqIOMode ())
      {
      case PIO_WRITER:
        pioWriterStdIO();
        break;
#ifdef _POSIX_ASYNCHRONOUS_IO
      case PIO_ASYNCH:
        pioWriterAIO();
        break;
#endif
      }
  else
    {
      namespaceSwitchSet(NSSWITCH_FILE_OPEN, NSSW_FUNC(pioSendOpen));
      namespaceSwitchSet(NSSWITCH_FILE_CLOSE, NSSW_FUNC(pioSendClose));
      namespaceSwitchSet(NSSWITCH_FILE_WRITE, NSSW_FUNC(pioSendWrite));
      namespaceSwitchSet(cdiPioExtraNSKeys[cdiPioEKFileWritingFinalize],
                         NSSW_FUNC(pioSendFinalize));
    }
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
