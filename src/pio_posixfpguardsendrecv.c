/*
   todo
   build in control, for consistance of pairs filename / filenumber
   ( pioOpenFile member name, recv in tmpbuffer, if(!uniqueName(q,v,n))abort )
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _XOPEN_SOURCE 500

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mpi.h>

#include "cdi.h"
#include "dmemory.h"
#include "namespace.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_conf.h"
#include "pio_impl.h"
#include "pio_util.h"

enum {
  msgNumWords = 3,
  collGuardTag = 777,
};

/*
 * Represents file opened by multiple writers and the corresponding
 * buffer(s) for accumulation of timestep data.
 */
struct mwFileBuf
{
  char *name;
  struct dBuffer *db;
  int fd, tsID;
  enum IO_Server_command command;
};

static struct mwFileBuf *openFiles;
static unsigned openFilesSize, openFilesFill;

/***************************************************************/

static void
newMultiwriterFileBuf(struct mwFileBuf *afd,
                      const char *filename, size_t bs)
{
  {
    size_t nameSize = strlen(filename) + 1;
    char *name = afd->name = Malloc(nameSize);
    memcpy(name, filename, nameSize);
  }
  afd->tsID = 0;

  /* init output buffer */

  xdebug ( " name=%s, init output buffer",  afd->name );

  int iret = dbuffer_init(&afd->db, bs);

  if ( iret > 0 )
    xabort("dbuffer_init did not succeed");

  /* open file */
  xdebug ( "name=%s, open file",  afd->name );

  if ( (afd->fd = open(afd->name, O_CREAT | O_WRONLY, 0666)) ==  -1 )
    xabort("Failed to open %s", afd->name);

  afd->command = IO_Open_file;
}

static int
deleteMultiwriterFileBuf(struct mwFileBuf *afd)
{
  int iret;

  /* close file */
  xdebug("name=%s, close file", afd->name);
  if ((iret = close(afd->fd)) == -1)
    xabort("Failed to close %s", afd->name);

  /* file closed, cleanup */
  xdebug("name=%s, file closed, cleanup ...",  afd->name);
  dbuffer_cleanup(&afd->db);
  Free(afd->name);
  afd->name = NULL;

  return iret;
}


//*******************************************************
#ifndef HAVE_PWRITE
static ssize_t
pwrite(int fd, const void *buf, size_t count, off_t offset)
{
  off_t pos;
  ssize_t written;
  if ( (pos = lseek(fd, offset, SEEK_SET)) == (off_t)-1 )
    xabort("did not succeed seeking file: %s", strerror(errno));
  if (( written = write(fd, buf, count)) != amount )
    xabort("fileId=%d, expect to write %zu byte, written %zd byte",
           id, count, written);
  return written;
}
#endif

static void
writePF(struct mwFileBuf *afd)
{
  ssize_t written;
  MPI_Status status;
  /* FIXME: pretend there's only one special rank for now */
  int fileID = (int)(afd - openFiles);
  int specialRank = commInqSizePio() - 1;
  MPI_Comm commPio = commInqCommPio();

  /* send buffersize, recv offset */
  size_t amount = dbuffer_data_size(afd->db);

  long query[msgNumWords] = { fileID, afd->command, (long)amount };
  long offset;
  xmpiStat(MPI_Sendrecv(query, msgNumWords, MPI_LONG, specialRank, collGuardTag,
                        &offset, 1, MPI_LONG, specialRank, collGuardTag,
                        commPio, &status), &status);
  xdebug("id=%d, command=%d, amount=%zu, sent amount=%ld, recv offset=%ld",
         fileID, afd->command, amount, (long)amount , offset );

  bool doTruncate = offset < 0;
  offset = labs(offset) - (offset < 0);
  /* write buffer */
  if ( (written = pwrite(afd->fd, afd->db->buffer, amount, offset) )
       != (ssize_t)amount )
    xabort("fileId=%d, expect to write %zu byte, written %zu byte",
           fileID, amount, written);
  xdebug("written %zu bytes in file %d with offset %ld",
         written, fileID, offset);
  if (doTruncate)
    ftruncate(afd->fd, offset + (off_t)amount);
  /* change outputBuffer */
  dbuffer_reset(afd->db);

  afd->command = IO_Set_fp;
}


/***************************************************************/

static void
defTimestepPF(struct mwFileBuf *afd, int tsID)
{
  assert(afd != NULL && tsID >= 0 && tsID == afd->tsID + 1);
  afd->tsID = tsID;
}


/***************************************************************/

static void
flushOp(struct mwFileBuf *a, int tsID)
{
  writePF(a);
  defTimestepPF(a, tsID);
}


static size_t
fwPOSIXFPGUARDSENDRECV(int fileID, const void *buffer, size_t len, int tsID)
{
  assert(fileID >= 0 && (size_t)fileID < openFilesSize
         && openFiles[fileID].name);
  struct mwFileBuf *afd = openFiles + fileID;

  bool flush = tsID != afd->tsID;

  if (flush)
    {
      xdebug("fileID %d, tsID = %d, flush buffer", fileID, tsID);
      flushOp(afd, tsID);
      xmpi(MPI_Barrier(commInqCommColl()));
    }

  int filled = dbuffer_push(afd->db, buffer, len);

  xdebug("fileID = %d, tsID = %d, pushed data on buffer, filled = %d",
         fileID, tsID, filled);

  int error = 0;
  if ( filled == 1 )
    {
      if ( flush )
	error = filled;
      else
	{
	  writePF(afd);

	  error = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );
	}
    }

  if ( error == 1 )
    xabort("did not succeed filling output buffer, fileID=%d", fileID);

  return len;
}

/***************************************************************/

static int
fcPOSIXFPGUARDSENDRECV(int fileID)
{
  assert(fileID >= 0 && (size_t)fileID < openFilesSize
         && openFiles[fileID].name);
  struct mwFileBuf *afd = openFiles + fileID;

  xdebug("write buffer, close file %d and cleanup", fileID);

  afd->command = IO_Close_file;

  writePF(afd);

  /* remove file element */
  int iret = deleteMultiwriterFileBuf(afd);
  /* make sure the file is closed on all collectors before proceeding */
  xmpi(MPI_Barrier(commInqCommColl()));
  return iret;
}

/***************************************************************/

static int
fowPOSIXFPGUARDSENDRECV(const char *filename, const char *mode)
{
  if ((mode[0] != 'w' && mode[0] != 'W') || mode[1] != 0)
    xabort("Unsupported mode \"%s\" in parallel file open.", mode);

  struct cdiPioConf *conf = cdiPioGetConf();

  for (size_t i = 0; i < openFilesSize; ++i)
    if (openFiles[i].name && !strcmp(openFiles[i].name, filename))
      {
        Warning("filename %s is already open!"
                " CDI-PIO does not support concurrent access"
                " through different filehandles.", filename);
        return CDI_EINVAL;
      }

  size_t fileID = SIZE_MAX;
  if (openFilesSize == openFilesFill)
    {
      fileID = openFilesSize;
      if (openFilesSize == (size_t)INT_MAX + 1)
        return CDI_ELIMIT;
      openFilesSize = openFilesSize ? openFilesSize * 2 : 4;
      if (openFilesSize > (size_t)INT_MAX + 1)
        openFilesSize = (size_t)INT_MAX + 1;
      openFiles = Realloc(openFiles, sizeof (*openFiles)
                                * openFilesSize);
      for (size_t i = fileID; i < openFilesSize; ++i)
        openFiles[i].name = NULL;
    }
  else
    {
      for (size_t i = 0; i < openFilesSize; ++i)
        if (openFiles[i].name == NULL)
          {
            fileID = i;
            break;
          }
    }
  struct mwFileBuf *afd = openFiles + fileID;

  newMultiwriterFileBuf(afd, filename, conf->writeAggBufLim);

  xdebug("name=%s, init and add struct mwFileBuf, return id = %zu",
         filename, fileID);
  long offset;
  int specialRank = commInqSpecialRank ();
  MPI_Status status;
  MPI_Comm commPio = commInqCommPio();
  long query[msgNumWords] = { (int)fileID, afd->command, 0L };
  xmpiStat(MPI_Sendrecv(query, msgNumWords, MPI_LONG, specialRank, collGuardTag,
                        &offset, 1, MPI_LONG, specialRank, collGuardTag,
                        commPio, &status), &status);
  afd->command = IO_Set_fp;
  return (int)fileID;
}

/***************************************************************/

static void
finalizePOSIXFPGUARDSENDRECV(void)
{
  long query[msgNumWords] = { 0, IO_Finalize, 0L };
  xmpi(MPI_Send(query, msgNumWords, MPI_LONG, commInqSpecialRank(),
                collGuardTag, commInqCommPio()));

  if (openFilesFill)
    xabort("files still open at CDI-PIO finalization");
  else
    Free(openFiles);
}

/***************************************************************/

static void
fpgPOSIXFPGUARDSENDRECV(void);

void
initPOSIXFPGUARDSENDRECV(void)
{
  int numIOServers = commInqSizePio();
  if (numIOServers < 2)
    xabort("error: # of I/O processes must be >= 2 for mode, but is %d",
           numIOServers);

  int isCollector = commInqRankColl() != -1;
  if (!isCollector)
    fpgPOSIXFPGUARDSENDRECV ();
  else
    {
      namespaceSwitchSet(NSSWITCH_FILE_OPEN,
                         NSSW_FUNC(fowPOSIXFPGUARDSENDRECV));
      namespaceSwitchSet(NSSWITCH_FILE_CLOSE,
                         NSSW_FUNC(fcPOSIXFPGUARDSENDRECV));
      namespaceSwitchSet(NSSWITCH_FILE_WRITE,
                         NSSW_FUNC(fwPOSIXFPGUARDSENDRECV));
      namespaceSwitchSet(cdiPioExtraNSKeys[cdiPioEKFileWritingFinalize],
                         NSSW_FUNC(finalizePOSIXFPGUARDSENDRECV));
    }
}

/***************************************************************/
/* the rank running the message loop below responds to queries of the form
 * long query[3] = { fileID, operation code, amount }
 * where the answer depends on the operation code:
 * IO_Open_file, IO_Set_fp or IO_Close_file:
 * long answer = offset to write to
 * IO_Finalize: no answer
 *
 * In essence this is an implementation of a shared file pointer.
 */

struct sharedFP {
  long offset;
  int unfinished;
};

static inline void
initSharedFP(struct sharedFP *bfd, int nProcsColl)
{
  bfd->offset = 0;
  bfd->unfinished = nProcsColl;
}

static void
fpgPOSIXFPGUARDSENDRECV(void)
{
  int source;
  MPI_Status status;
  struct sharedFP *restrict sharedFPs = NULL;
  size_t sharedFPsSize = 0, sharedFPsFill = 0;
  MPI_Comm commPio = commInqCommPio();
  size_t nProcsColl = (size_t)(commInqSizeColl()),
    sentFinalize = nProcsColl;
  long (*msgWords)[msgNumWords] = Malloc(sizeof (*msgWords) * nProcsColl);
  MPI_Request *msgReq = Malloc(sizeof (*msgReq) * nProcsColl);

  xdebug("ncollectors=%zu", nProcsColl);

  for (size_t i = 0; i < nProcsColl; ++i)
    xmpi(MPI_Irecv(msgWords[i], msgNumWords, MPI_LONG, (int)i, collGuardTag,
                   commPio, msgReq + i));

  for ( ;; )
    {
      xmpiStat(MPI_Waitany((int)nProcsColl, msgReq, &source, &status), &status);
      int fileID = (int)msgWords[source][0];
      assert(fileID >= 0);
      int opcode = (int)msgWords[source][1];
      long amount = msgWords[source][2];
      /* re-instate listening */
      if (opcode != IO_Finalize)
        xmpi(MPI_Irecv(msgWords[source], 3, MPI_LONG, source, collGuardTag,
                       commPio, msgReq + source));

      xdebug("receive message from source=%d, id=%d, command=%d (%s)",
             source, fileID, opcode, cdiPioCmdStrTab[opcode]);

      if ( opcode >= IO_Open_file && opcode < IO_Finalize )
        {
          if (opcode == IO_Open_file)
            {
              if (sharedFPsSize <= (size_t)fileID)
                {
                  size_t oldSize = sharedFPsSize;
                  sharedFPsSize = sharedFPsSize ? sharedFPsSize * 2 : 2;
                  sharedFPs = Realloc(sharedFPs,
                                      sharedFPsSize * sizeof (*sharedFPs));
                  for (size_t i = oldSize; i < sharedFPsSize; ++i)
                    sharedFPs[i].offset = -1;
                }
              if (sharedFPs[fileID].offset < 0)
                {
                  initSharedFP(sharedFPs + fileID, (int)nProcsColl);
                  ++sharedFPsFill;
                }
            }
          xdebug("id=%d, command=%d (%s), send offset=%lld",
                 fileID, opcode, cdiPioCmdStrTab[opcode],
                 (long long)sharedFPs[fileID].offset);

          int unfinished = sharedFPs[fileID].unfinished;
          if (opcode == IO_Close_file && unfinished == 1)
            sharedFPs[fileID].offset = -sharedFPs[fileID].offset-1;
	  xmpi(MPI_Send(&sharedFPs[fileID].offset, 1, MPI_LONG, source,
                        collGuardTag, commPio));

          xdebug("id=%d, command=%d (%s), recv amount=%ld, set offset=%ld",
                 fileID, opcode, cdiPioCmdStrTab[opcode], amount,
                 sharedFPs[fileID].offset);

          if ( opcode == IO_Close_file
               && ! (sharedFPs[fileID].unfinished = --unfinished) )
            --sharedFPsFill;
          else
            sharedFPs[fileID].offset += amount;
        }
      else if ( opcode == IO_Finalize )
        {
          if ( !--sentFinalize )
            {
              if (sharedFPsFill)
                xabort("still files open");
              Free(sharedFPs);
              Free(msgReq);
              Free(msgWords);
              return;
            }
        }
      else
        xabort("COMMAND NOT IMPLEMENTED");
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
