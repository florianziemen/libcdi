/*
   todo
   build in control, for consistance of pairs filename / filenumber
   ( pioOpenFile member name, recv in tmpbuffer, if(!uniqueName(q,v,n))abort )
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _XOPEN_SOURCE 500

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
#include "pio_impl.h"
#include "pio_util.h"

typedef struct
{
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  int fd;
  enum IO_Server_command command;
  int tsID, fileID;
  char name[];
} aFiledataPF;

static int
fileIDTestA(void *a, void *fileID)
{
  return ((aFiledataPF *)a)->fileID == (int)(intptr_t)fileID;
}

typedef struct
{
  long offset;
  int fileID;
  int unfinished;
} bFiledataPF;

static int
fileIDTestB(void *a, void *fileID)
{
  return ((bFiledataPF *)a)->fileID == (int)(intptr_t)fileID;
}

static bool
fileIDCmpB(void *a, void *b)
{
  return ((bFiledataPF *)a)->fileID == ((bFiledataPF *)b)->fileID;
}

static listSet *bibAFiledataPF;

/***************************************************************/

static aFiledataPF *initAFiledataPF ( const char *key, size_t bs)
{
  aFiledataPF *afd;
  size_t len;
  int iret;

  len = strlen(key);
  afd = Calloc(1, sizeof (*afd) + len + 1);
  strcpy(afd->name, key);
  afd->tsID = 0;

  /* init output buffer */

  xdebug ( " name=%s, init output buffer",  afd->name );

  iret = dbuffer_init(&(afd->db1), bs);
  iret += dbuffer_init(&(afd->db2), bs);

  if ( iret > 0 )
    xabort("dbuffer_init did not succeed");

  afd->db = afd->db1;

  /* open file */
  xdebug ( "name=%s, open file",  afd->name );

  if ( (afd->fd = open(afd->name, O_CREAT | O_WRONLY, 0666)) ==  -1 )
    xabort("Failed to open %s", afd->name);

  afd->command = IO_Open_file;
  return afd;
}

/***************************************************************/
static bFiledataPF *
initBFiledataPF(int fileID, int nProcsColl)
{
  bFiledataPF *bfd = Malloc(sizeof (*bfd));
  bfd->offset = 0;
  bfd->fileID = fileID;
  bfd->unfinished = (int)nProcsColl;

  return bfd;
}

/***************************************************************/

static int
destroyAFiledataPF(void *v)
{
  int iret = 0;
  aFiledataPF *afd = ( aFiledataPF * ) v;

  /* close file */
  xdebug("name=%s, close file", afd->name);
  if ((iret = close(afd->fd)) == -1)
    xabort("Failed to close %s", afd->name);

  /* file closed, cleanup */
  xdebug("name=%s, file closed, cleanup ...",  afd->name);
  dbuffer_cleanup(&(afd->db1));
  dbuffer_cleanup(&(afd->db2));

  Free(afd);

  return iret;
}

/***************************************************************/

static int
destroyBFiledataPF(void *v)
{
  int iret = 0;
  bFiledataPF *bfd = (bFiledataPF * ) v;

  Free ( bfd );

  return iret;
}

/***************************************************************/

static bool
compareNamesAPF(void *v1, void *v2)
{
  aFiledataPF *afd1 = v1, *afd2 = v2;

  return !strcmp(afd1->name, afd2->name);
}

/***************************************************************/
/* the rank running this message loop responds to queries of the form
 * long query[3] = { file_id, operation code, amount }
 * where the answer depends on the operation code:
 * IO_Open_file, IO_Set_fp or IO_Close_file:
 * long answer = offset to write to
 * IO_Finalize: no answer
 */
enum {
  msgNumWords = 3,
  collGuardTag = 777,
};

static void
fpgPOSIXFPGUARDSENDRECV(void)
{
  int source, iret;
  MPI_Status status;
  bFiledataPF *bfd;
  listSet *bibBFiledataPF;
  MPI_Comm commPio = commInqCommPio();
  size_t nProcsColl = (size_t)(commInqSizeColl()),
    sentFinalize = nProcsColl;
  long (*msgWords)[msgNumWords] = Malloc(sizeof (*msgWords) * nProcsColl);
  MPI_Request *msgReq = Malloc(sizeof (*msgReq) * nProcsColl);

  xdebug("ncollectors=%zu", nProcsColl);

  /* create empty set of files identified by id */
  bibBFiledataPF = listSetNew(destroyBFiledataPF, fileIDCmpB);

  for (size_t i = 0; i < nProcsColl; ++i)
    xmpi(MPI_Irecv(msgWords[i], msgNumWords, MPI_LONG, (int)i, collGuardTag, commPio,
                   msgReq + i));

  for ( ;; )
    {
      xmpiStat(MPI_Waitany((int)nProcsColl, msgReq, &source, &status), &status);
      int file_id = (int)msgWords[source][0];
      int opcode = (int)msgWords[source][1];
      long amount = msgWords[source][2];
      /* re-instate listening */
      if (opcode != IO_Finalize)
        xmpi(MPI_Irecv(msgWords[source], 3, MPI_LONG, source, collGuardTag, commPio,
                       msgReq + source));

      xdebug("receive message from source=%d, id=%d, command=%d (%s)",
             source, file_id, opcode, cdiPioCmdStrTab[opcode]);

      if ( opcode >= IO_Open_file && opcode < IO_Finalize )
      	{
          if (opcode == IO_Open_file)
            {
              if (!(bfd = listSetGet(bibBFiledataPF, fileIDTestB,
                                     (void *)(intptr_t)file_id)))
                {
                  bfd = initBFiledataPF(file_id, (int)nProcsColl);
                  if ((iret = listSetAdd(bibBFiledataPF, bfd)) < 0)
                    xabort("fileID=%d not unique", file_id);
                  bfd->fileID = iret;
                }
            }
          else
            {
              if (!(bfd = listSetGet(bibBFiledataPF, fileIDTestB,
                                     (void *)(intptr_t)file_id)))
                xabort("fileId=%d not in set", file_id);
            }
          xdebug("id=%d, command=%d (%s), send offset=%ld",
                 file_id, opcode, cdiPioCmdStrTab[opcode], bfd->offset);

          int unfinished = bfd->unfinished;
          if (opcode == IO_Close_file && unfinished == 1)
            bfd->offset = -bfd->offset-1;
	  xmpi(MPI_Send(&bfd->offset, 1, MPI_LONG, source, collGuardTag,
                        commPio));

          xdebug("id=%d, command=%d (%s), recv amount=%ld, set offset=%ld",
                 file_id, opcode, cdiPioCmdStrTab[opcode], amount,
                 bfd->offset);

          if ( opcode == IO_Close_file && ! (bfd->unfinished = --unfinished) )
            listSetRemove(bibBFiledataPF, fileIDTestB,
                          (void *)(intptr_t)file_id);
          else
            bfd->offset += amount;
        }
      else if ( opcode == IO_Finalize )
        {
          if ( !--sentFinalize )
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
writePF(aFiledataPF *afd)
{
  ssize_t written;
  MPI_Status status;
  /* FIXME: pretend there's only one special rank for now */
  int file_id = afd->fileID;
  int specialRank = commInqSizePio() - 1;
  MPI_Comm commPio = commInqCommPio();

  /* send buffersize, recv offset */
  size_t amount = dbuffer_data_size(afd->db);

  long query[msgNumWords] = { file_id, afd->command, (long)amount };
  long offset;
  xmpiStat(MPI_Sendrecv(query, msgNumWords, MPI_LONG, specialRank, collGuardTag,
                        &offset, 1, MPI_LONG, specialRank, collGuardTag,
                        commPio, &status), &status);
  xdebug("id=%d, command=%d, amount=%zu, sent amount=%ld, recv offset=%ld",
         file_id, afd->command, amount, (long)amount , offset );

  bool doTruncate = offset < 0;
  offset = labs(offset) - (offset < 0);
  /* write buffer */
  if ( (written = pwrite(afd->fd, afd->db->buffer, amount, offset))
       != (ssize_t)amount )
    xabort("fileId=%d, expect to write %zu byte, written %zu byte",
           file_id, amount, written);
  xdebug("written %zu bytes in file %d with offset %ld",
         written, file_id, offset);
  if (doTruncate)
    ftruncate(afd->fd, offset + (off_t)amount);
  /* change outputBuffer */
  dbuffer_reset(afd->db);

  if ( afd->db == afd->db1 )
    {
      xdebug ( "id=%d, change to buffer 2 ...", file_id );
      afd->db =  afd->db2;
    }
  else
    {
      xdebug ( "id=%d, change to buffer 1 ...", file_id );
      afd->db =  afd->db1;
    }

  afd->command = IO_Set_fp;
}


/***************************************************************/

static void
defTimestepPF(aFiledataPF *afd, int tsID)
{
  if ( afd == NULL || tsID < 0 || tsID != afd->tsID + 1 )
    xabort ( " defTimestepPF() didn't succeed." );
  afd->tsID = tsID;
}


/***************************************************************/

static void
flushOp(aFiledataPF *a, int tsID)
{
  writePF(a);
  defTimestepPF(a, tsID);
}


static size_t
fwPOSIXFPGUARDSENDRECV(int fileID, const void *buffer, size_t len, int tsID)
{
  int error = 0;
  aFiledataPF *afd
    = listSetGet(bibAFiledataPF, fileIDTestA, (void *)(intptr_t)fileID);

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
fcPOSIXFPGUARDSENDRECV(int id)
{
  aFiledataPF *afd;
  int iret;

  xdebug("write buffer, close file %d and cleanup", id);

  afd = listSetGet(bibAFiledataPF, fileIDTestA, (void *)(intptr_t)id);

  afd->command = IO_Close_file;

  writePF(afd);

  /* remove file element */
  iret = listSetRemove(bibAFiledataPF, fileIDTestA, (void *)(intptr_t)id);
  /* make sure the file is closed on all collectors before proceeding */
  xmpi(MPI_Barrier(commInqCommColl()));
  return iret;
}

/***************************************************************/
static void
elemCheck(void *q, void *nm)
{
  aFiledataPF *afd = q;
  const char *name = nm;

  if (!strcmp(name, afd->name))
    xabort("Filename %s has already been added to set\n", name);
}

static int
fowPOSIXFPGUARDSENDRECV(const char *filename, const char *mode)
{
  enum {
    bcastRoot = 0
  };
  if ((mode[0] != 'w' && mode[0] != 'W') || mode[1] != 0)
    xabort("Unsupported mode \"%s\" in parallel file open.", mode);
  static unsigned long buffersize = 0;

  /* broadcast buffersize to collectors */
  if (!buffersize)
    {
      if (commInqRankColl() == bcastRoot)
        buffersize = findWriteAccumBufsize();
      xmpi(MPI_Bcast(&buffersize, 1, MPI_UNSIGNED_LONG, bcastRoot,
                     commInqCommColl()));
    }

  /* init and add file element */
  listSetForeach(bibAFiledataPF, elemCheck, (void *)filename);

  aFiledataPF *afd = initAFiledataPF(filename, (size_t)buffersize);

  int file_id;
  if ((file_id = listSetAdd(bibAFiledataPF, afd)) < 0)
    xabort("filename %s not unique", afd->name);
  afd->fileID = file_id;
  xdebug("name=%s, init and add aFiledataPF, return id = %d",
         filename, file_id);
  long offset;
  int specialRank = commInqSpecialRank ();
  MPI_Status status;
  MPI_Comm commPio = commInqCommPio();
  long query[msgNumWords] = { file_id, afd->command, 0L };
  xmpiStat(MPI_Sendrecv(query, msgNumWords, MPI_LONG, specialRank, collGuardTag,
                        &offset, 1, MPI_LONG, specialRank, collGuardTag,
                        commPio, &status), &status);
  afd->command = IO_Set_fp;
  return file_id;
}

/***************************************************************/

static void
finalizePOSIXFPGUARDSENDRECV(void)
{
  long query[msgNumWords] = { 0, IO_Finalize, 0L };
  xmpi(MPI_Send(query, msgNumWords, MPI_LONG, commInqSpecialRank(),
                collGuardTag, commInqCommPio()));

  if (!listSetIsEmpty(bibAFiledataPF))
    xabort("set bibAFiledataM not empty");
  else
    {
      xdebug("%s", "destroy set");
      listSetDelete(bibAFiledataPF);
    }
}

/***************************************************************/

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
      bibAFiledataPF = listSetNew(destroyAFiledataPF, compareNamesAPF);
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
