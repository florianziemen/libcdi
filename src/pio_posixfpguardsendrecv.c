/*
   todo
   build in control, for consistance of pairs filename / filenumber
   ( pioOpenFile member name, recv in tmpbuffer, if(!uniqueName(q,v,n))abort )
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
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

enum
{
  msgNumWords = 3,
  collGuardTag = 777,
};

/*
 * Represents file opened by multiple writers and the corresponding
 * buffer(s) for accumulation of timestep data.
 */
struct mwFileBuf
{
  struct dBuffer *db;
  int fd;
  enum IO_Server_command command;
  int tsID, fileID;
  char name[];
};

static int
lookupMwFBFileID(void *a, void *fileID)
{
  return ((struct mwFileBuf *) a)->fileID == (int) (intptr_t) fileID;
}

static listSet *openedFiles;

/***************************************************************/

static struct mwFileBuf *
newMultiwriterFileBuf(const char *key, size_t bs)
{
  size_t len = strlen(key);
  struct mwFileBuf *afd = Calloc(1, sizeof(*afd) + len + 1);
  strcpy(afd->name, key);
  afd->tsID = 0;

  /* init output buffer */

  xdebug(" name=%s, init output buffer", afd->name);

  int iret = dbuffer_init(&afd->db, bs);

  if (iret > 0) xabort("dbuffer_init did not succeed");

  /* open file */
  xdebug("name=%s, open file", afd->name);

  if ((afd->fd = open(afd->name, O_CREAT | O_WRONLY, 0666)) == -1) xabort("Failed to open %s", afd->name);

  afd->command = IO_Open_file;
  return afd;
}

static int
deleteMultiwriterFileBuf(void *v)
{
  int iret = 0;
  struct mwFileBuf *afd = (struct mwFileBuf *) v;

  /* close file */
  xdebug("name=%s, close file", afd->name);
  if ((iret = close(afd->fd)) == -1) xabort("Failed to close %s", afd->name);

  /* file closed, cleanup */
  xdebug("name=%s, file closed, cleanup ...", afd->name);
  dbuffer_cleanup(&afd->db);

  Free(afd);

  return iret;
}

static bool
compareNamesAPF(void *v1, void *v2)
{
  struct mwFileBuf *afd1 = v1, *afd2 = v2;

  return !strcmp(afd1->name, afd2->name);
}


//*******************************************************
#ifndef HAVE_PWRITE
static ssize_t
pwrite(int fd, const void *buf, size_t count, off_t offset)
{
  off_t pos;
  ssize_t written;
  if ((pos = lseek(fd, offset, SEEK_SET)) == (off_t) -1) xabort("did not succeed seeking file: %s", strerror(errno));
  if ((written = write(fd, buf, count)) != amount)
    xabort("fileId=%d, expect to write %zu byte, written %zd byte", id, count, written);
  return written;
}
#endif

static void
writePF(struct mwFileBuf *afd)
{
  ssize_t written;
  MPI_Status status;
  /* FIXME: pretend there's only one special rank for now */
  int file_id = afd->fileID;
  int specialRank = commInqSizePio() - 1;
  MPI_Comm commPio = commInqCommPio();

  /* send buffersize, recv offset */
  size_t amount = dbuffer_data_size(afd->db);

  long query[msgNumWords] = { file_id, afd->command, (long) amount };
  long offset;
  xmpiStat(MPI_Sendrecv(query, msgNumWords, MPI_LONG, specialRank, collGuardTag, &offset, 1, MPI_LONG, specialRank, collGuardTag,
                        commPio, &status),
           &status);
  xdebug("id=%d, command=%d, amount=%zu, sent amount=%ld, recv offset=%ld", file_id, afd->command, amount, (long) amount, offset);

  bool doTruncate = offset < 0;
  offset = labs(offset) - (offset < 0);
  /* write buffer */
  if ((written = pwrite(afd->fd, afd->db->buffer, amount, offset)) != (ssize_t) amount)
    xabort("fileId=%d, expect to write %zu byte, written %zu byte", file_id, amount, written);
  xdebug("written %zu bytes in file %d with offset %ld", written, file_id, offset);
  if (doTruncate) ftruncate(afd->fd, offset + (off_t) amount);
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
  int error = 0;
  struct mwFileBuf *afd = listSetGet(openedFiles, lookupMwFBFileID, (void *) (intptr_t) fileID);

  bool flush = tsID != afd->tsID;

  if (flush)
    {
      xdebug("fileID %d, tsID = %d, flush buffer", fileID, tsID);
      flushOp(afd, tsID);
      xmpi(MPI_Barrier(commInqCommColl()));
    }

  int filled = dbuffer_push(afd->db, buffer, len);

  xdebug("fileID = %d, tsID = %d, pushed data on buffer, filled = %d", fileID, tsID, filled);

  if (filled == 1)
    {
      if (flush)
        error = filled;
      else
        {
          writePF(afd);

          error = dbuffer_push(afd->db, (unsigned char *) buffer, len);
        }
    }

  if (error == 1) xabort("did not succeed filling output buffer, fileID=%d", fileID);

  return len;
}

/***************************************************************/

static int
fcPOSIXFPGUARDSENDRECV(int id)
{
  xdebug("write buffer, close file %d and cleanup", id);

  struct mwFileBuf *afd = listSetGet(openedFiles, lookupMwFBFileID, (void *) (intptr_t) id);

  afd->command = IO_Close_file;

  writePF(afd);

  /* remove file element */
  int iret = listSetRemove(openedFiles, lookupMwFBFileID, (void *) (intptr_t) id);
  /* make sure the file is closed on all collectors before proceeding */
  xmpi(MPI_Barrier(commInqCommColl()));
  return iret;
}

/***************************************************************/
static void
lookupMwFBFilename(void *q, void *nm)
{
  struct mwFileBuf *afd = q;
  const char *name = nm;

  if (!strcmp(name, afd->name)) xabort("Filename %s has already been added to set\n", name);
}

static int
fowPOSIXFPGUARDSENDRECV(const char *filename, const char *mode)
{
  if ((mode[0] != 'w' && mode[0] != 'W') || mode[1] != 0) xabort("Unsupported mode \"%s\" in parallel file open.", mode);

  struct cdiPioConf *conf = cdiPioGetConf();

  listSetForeach(openedFiles, lookupMwFBFilename, (void *) filename);
  /* init and add file element */
  struct mwFileBuf *afd = newMultiwriterFileBuf(filename, conf->writeAggBufLim);

  int file_id;
  if ((afd->fileID = file_id = listSetAdd(openedFiles, afd)) < 0) xabort("filename %s not unique", afd->name);
  xdebug("name=%s, init and add struct mwFileBuf, return id = %d", filename, file_id);
  long offset;
  int specialRank = commInqSpecialRank();
  MPI_Status status;
  MPI_Comm commPio = commInqCommPio();
  long query[msgNumWords] = { file_id, afd->command, 0L };
  xmpiStat(MPI_Sendrecv(query, msgNumWords, MPI_LONG, specialRank, collGuardTag, &offset, 1, MPI_LONG, specialRank, collGuardTag,
                        commPio, &status),
           &status);
  afd->command = IO_Set_fp;
  return file_id;
}

/***************************************************************/

static void
finalizePOSIXFPGUARDSENDRECV(void)
{
  long query[msgNumWords] = { 0, IO_Finalize, 0L };
  xmpi(MPI_Send(query, msgNumWords, MPI_LONG, commInqSpecialRank(), collGuardTag, commInqCommPio()));

  if (!listSetIsEmpty(openedFiles))
    xabort("files still open at CDI-PIO finalization");
  else
    {
      xdebug("%s", "destroy set");
      listSetDelete(openedFiles);
    }
}

/***************************************************************/

static void fpgPOSIXFPGUARDSENDRECV(void);

void
initPOSIXFPGUARDSENDRECV(void)
{
  int numIOServers = commInqSizePio();
  if (numIOServers < 2) xabort("error: # of I/O processes must be >= 2 for mode, but is %d", numIOServers);

  int isCollector = commInqRankColl() != -1;
  if (!isCollector)
    fpgPOSIXFPGUARDSENDRECV();
  else
    {
      namespaceSwitchSet(NSSWITCH_FILE_OPEN, NSSW_FUNC(fowPOSIXFPGUARDSENDRECV));
      namespaceSwitchSet(NSSWITCH_FILE_CLOSE, NSSW_FUNC(fcPOSIXFPGUARDSENDRECV));
      namespaceSwitchSet(NSSWITCH_FILE_WRITE, NSSW_FUNC(fwPOSIXFPGUARDSENDRECV));
      namespaceSwitchSet(cdiPioExtraNSKeys[cdiPioEKFileWritingFinalize], NSSW_FUNC(finalizePOSIXFPGUARDSENDRECV));
      openedFiles = listSetNew(deleteMultiwriterFileBuf, compareNamesAPF);
    }
}

/***************************************************************/
/* the rank running the message loop below responds to queries of the form
 * long query[3] = { file_id, operation code, amount }
 * where the answer depends on the operation code:
 * IO_Open_file, IO_Set_fp or IO_Close_file:
 * long answer = offset to write to
 * IO_Finalize: no answer
 *
 * In essence this is an implementation of a shared file pointer.
 */

struct sharedFP
{
  long offset;
  int fileID;
  int unfinished;
};

static int
lookupSharedFP(void *a, void *fileID)
{
  return ((struct sharedFP *) a)->fileID == (int) (intptr_t) fileID;
}

static bool
compareSharedFPFileID(void *a, void *b)
{
  return ((struct sharedFP *) a)->fileID == ((struct sharedFP *) b)->fileID;
}

static struct sharedFP *
newSharedFP(int fileID, int nProcsColl)
{
  struct sharedFP *bfd = Malloc(sizeof(*bfd));
  bfd->offset = 0;
  bfd->fileID = fileID;
  bfd->unfinished = (int) nProcsColl;

  return bfd;
}

static int
deleteSharedFP(void *v)
{
  int iret = 0;
  struct sharedFP *bfd = (struct sharedFP *) v;

  Free(bfd);

  return iret;
}

static void
fpgPOSIXFPGUARDSENDRECV(void)
{
  int source, iret;
  MPI_Status status;
  struct sharedFP *bfd;
  listSet *bibBFiledataPF;
  MPI_Comm commPio = commInqCommPio();
  size_t nProcsColl = (size_t) (commInqSizeColl()), sentFinalize = nProcsColl;
  long(*msgWords)[msgNumWords] = Malloc(sizeof(*msgWords) * nProcsColl);
  MPI_Request *msgReq = Malloc(sizeof(*msgReq) * nProcsColl);

  xdebug("ncollectors=%zu", nProcsColl);

  /* create empty set of files identified by id */
  bibBFiledataPF = listSetNew(deleteSharedFP, compareSharedFPFileID);

  for (size_t i = 0; i < nProcsColl; ++i)
    xmpi(MPI_Irecv(msgWords[i], msgNumWords, MPI_LONG, (int) i, collGuardTag, commPio, msgReq + i));

  for (;;)
    {
      xmpiStat(MPI_Waitany((int) nProcsColl, msgReq, &source, &status), &status);
      int file_id = (int) msgWords[source][0];
      int opcode = (int) msgWords[source][1];
      long amount = msgWords[source][2];
      /* re-instate listening */
      if (opcode != IO_Finalize) xmpi(MPI_Irecv(msgWords[source], 3, MPI_LONG, source, collGuardTag, commPio, msgReq + source));

      xdebug("receive message from source=%d, id=%d, command=%d (%s)", source, file_id, opcode, cdiPioCmdStrTab[opcode]);

      if (opcode >= IO_Open_file && opcode < IO_Finalize)
        {
          if (opcode == IO_Open_file)
            {
              if (!(bfd = listSetGet(bibBFiledataPF, lookupSharedFP, (void *) (intptr_t) file_id)))
                {
                  bfd = newSharedFP(file_id, (int) nProcsColl);
                  if ((iret = listSetAdd(bibBFiledataPF, bfd)) < 0) xabort("fileID=%d not unique", file_id);
                  bfd->fileID = iret;
                }
            }
          else
            {
              if (!(bfd = listSetGet(bibBFiledataPF, lookupSharedFP, (void *) (intptr_t) file_id)))
                xabort("fileId=%d not in set", file_id);
            }
          xdebug("id=%d, command=%d (%s), send offset=%ld", file_id, opcode, cdiPioCmdStrTab[opcode], bfd->offset);

          int unfinished = bfd->unfinished;
          if (opcode == IO_Close_file && unfinished == 1) bfd->offset = -bfd->offset - 1;
          xmpi(MPI_Send(&bfd->offset, 1, MPI_LONG, source, collGuardTag, commPio));

          xdebug("id=%d, command=%d (%s), recv amount=%ld, set offset=%ld", file_id, opcode, cdiPioCmdStrTab[opcode], amount,
                 bfd->offset);

          if (opcode == IO_Close_file && !(bfd->unfinished = --unfinished))
            listSetRemove(bibBFiledataPF, lookupSharedFP, (void *) (intptr_t) file_id);
          else
            bfd->offset += amount;
        }
      else if (opcode == IO_Finalize)
        {
          if (!--sentFinalize)
            {
              if (!listSetIsEmpty(bibBFiledataPF))
                xabort("set bibBFiledataM not empty");
              else
                {
                  xdebug("%s", "destroy set");
                  listSetDelete(bibBFiledataPF);
                }
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
