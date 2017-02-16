/*
   todo
   README: specialRank Pe closes down, when all output files are closed
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif



#include <unistd.h>

#ifdef _POSIX_ASYNCHRONOUS_IO
#include <aio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <mpi.h>

#include "pio.h"
#include "pio_comm.h"
#include "pio_impl.h"
#include "pio_util.h"
#include "dmemory.h"

struct fileFunnelAIO
{
  char *name;
  struct dBuffer *fb;
  struct aiocb *ctrlBlks;
  off_t offset;
  unsigned short currOpIndex, aioQueueDepth;
  unsigned numQueuedWrites;
  int activeCollectors;
  int handle, fileID;
};

static int
fileIDTest(void *a, void *fileID)
{
  return ((struct fileFunnelAIO *)a)->fileID == (int)(intptr_t)fileID;
}

/***************************************************************/

static struct fileFunnelAIO *
initBFiledataPA(char *filename, struct cdiPioConf *conf, int nc)
{
  struct fileFunnelAIO *bfd;

  size_t bufSize = conf->writeAggBufLim;
  size_t aioQueueDepth = conf->aioQueueDepth;
  xdebug ( "filename=%s, buffersize=%zu, ncollectors=%d, AIO queue depth=%zu",
           filename, bufSize, nc, aioQueueDepth );

  bfd = (struct fileFunnelAIO*) Malloc( sizeof (*bfd));
  {
    size_t filenameSize = strlen(filename) + 1;
    char *name = bfd->name = Malloc(filenameSize);
    strcpy(name, filename);
  }

  if (( bfd->handle = open ( bfd->name, O_CREAT | O_WRONLY, 0666 )) == -1 )
    xabort("Failed to open %s", bfd->name);

  dbuffer_init(&(bfd->fb), aioQueueDepth * bufSize);

  struct aiocb *ctrlBlks
    = bfd->ctrlBlks = Calloc(aioQueueDepth, sizeof (*ctrlBlks));

  for ( size_t i = 0; i < aioQueueDepth; i++ )
    {
      ctrlBlks[i].aio_fildes = bfd->handle;
      ctrlBlks[i].aio_buf = bfd->fb->buffer + (size_t)i * bufSize;
      ctrlBlks[i].aio_reqprio = 0;
      ctrlBlks[i].aio_sigevent.sigev_notify = SIGEV_NONE;
    }

  bfd->currOpIndex = (unsigned short)(aioQueueDepth - 1);
  bfd->numQueuedWrites = 0;
  bfd->offset = 0;
  bfd->activeCollectors = nc;

  xdebug ( "filename=%s, opened file, return", bfd->name );

  return bfd;
}

/***************************************************************/

static int
destroyBFiledataPA ( void *v )
{
  struct fileFunnelAIO *bfd = (struct fileFunnelAIO * ) v;
  const struct aiocb *ccBP[1];
  int iret = 0;
  struct cdiPioConf *conf = cdiPioGetConf();
  size_t aioQueueDepth = conf->aioQueueDepth;
  size_t numQueuedWrites = bfd->numQueuedWrites;
  size_t nextOpIndex
    = ((size_t)bfd->currOpIndex + 1 ) % aioQueueDepth;
  size_t nextFinishOp
    = (nextOpIndex - numQueuedWrites + aioQueueDepth) % aioQueueDepth;

  xdebug ( "filename=%s, cleanup and close file", bfd->name );

  /* close file */
  struct aiocb *ctrlBlks = bfd->ctrlBlks;
  for (; numQueuedWrites > 0 ; --numQueuedWrites)
    {
      xdebug("file: %s, numQueuedWrites=%zu", bfd->name, numQueuedWrites);
      ccBP[0] = ctrlBlks + nextFinishOp;
      do
	{
	  iret = aio_suspend(ccBP, 1, NULL);
	  if ( iret < 0 && errno != EINTR ) xabort ( "aio_suspend () failed" );
	}
      while ( iret != 0 );

      iret = aio_error(ctrlBlks + nextFinishOp);
      ssize_t ssiret;
      if (( ssiret = aio_return(ctrlBlks + nextFinishOp)) == -1 )
	xabort("aio_return () failed: %s", strerror(iret));

      nextFinishOp = ( nextFinishOp + 1 ) % aioQueueDepth;
    }
  bfd->numQueuedWrites = (unsigned)numQueuedWrites;

  if ((iret = ftruncate(bfd->handle, bfd->offset)) == -1)
    xabort("failed to truncate file %s: %s", bfd->name, strerror(errno));
  if (( iret = close ( bfd->handle )) == -1 )
    xabort("failed to close %s", bfd->name);

  /* file closed, cleanup */

  dbuffer_cleanup ( &( bfd->fb ));
  Free(bfd->name);
  bfd->name = NULL;
  Free(bfd->ctrlBlks);
  Free(bfd);

  xdebug("%s", "closed file and cleaned up, return");

  return iret;
}

/***************************************************************/

static bool
compareNamesBPA(void *v1, void *v2)
{
  struct fileFunnelAIO *bfd1 = v1, *bfd2 = v2;

  return !strcmp(bfd1->name, bfd2->name);
}

/***************************************************************/

static void
writePA(struct fileFunnelAIO *bfd, size_t amount, struct cdiPioConf *conf)
{
  const struct aiocb *ccBP;
  size_t aioQueueDepth = conf->aioQueueDepth;

  xdebug ( "file %s, in", bfd->name );

  size_t currOpIndex = (size_t)bfd->currOpIndex;
  bfd->ctrlBlks[currOpIndex].aio_nbytes = amount;
  bfd->ctrlBlks[currOpIndex].aio_offset = bfd->offset;

  xdebug ( " before aio_write(), file %s, aio_nbytes=%zu, aio_offset=%zu",
           bfd->name, bfd->ctrlBlks[currOpIndex].aio_nbytes,
           bfd->ctrlBlks[currOpIndex].aio_offset );

  ssize_t iret = aio_write ( bfd->ctrlBlks + currOpIndex );

  xdebug ( "after aio_write(), file %s, aio_nbytes=%zu, aio_offset=%zu,"
           "iret=aio_write()=%d",
           bfd->name, bfd->ctrlBlks[currOpIndex].aio_nbytes,
           bfd->ctrlBlks[currOpIndex].aio_offset, (int)iret );

  if ( iret == -1 )
    {
      SysError ( "did not succeed writing buffer" );
    }
  else
    xdebug ( "buffer written to %s",  bfd->name );

  bfd->offset += (off_t)amount;
  size_t numQueuedWrites = bfd->numQueuedWrites + 1;

  if ( numQueuedWrites >= aioQueueDepth )
    {
      size_t nextOpIndex
        = ((size_t)bfd->currOpIndex + 1 ) % aioQueueDepth;
      ccBP = bfd->ctrlBlks + nextOpIndex;
      do
	{
	  iret = aio_suspend(&ccBP, 1, NULL);
	  if ( iret < 0 && errno != EINTR )
	    xabort ( "aio_suspend () failed" );
	} while ( iret != 0 );

      if (( iret = aio_return ( bfd->ctrlBlks + nextOpIndex )) == -1 )
	xabort ( "aio_return () failed" );

      --numQueuedWrites;
    }
  bfd->numQueuedWrites = (unsigned)numQueuedWrites;

  xdebug ( "filename=%s, numQueuedWrites=%zu, return", bfd->name, numQueuedWrites );
}

/***************************************************************/
static void
elemCheck(void *q, void *nm)
{
  struct fileFunnelAIO *bfd = q;
  const char *name = nm;

  if (!strcmp(name, bfd->name))
    xabort("Filename %s has already been inserted\n", name);
}

/***************************************************************/

void pioWriterAIO(void)
{
  struct fileFunnelAIO *bfd;
  listSet * bibBFiledataPA;
  MPI_Comm commPio = commInqCommPio ();
  int nProcsColl = commInqSizeColl ();
  struct cdiPioConf *conf = cdiPioGetConf();
  size_t aioQueueDepth = conf->aioQueueDepth;

  assert(aioQueueDepth >= 1);
  xdebug ( "nProcsColl=%d ", nProcsColl );

  bibBFiledataPA = listSetNew(destroyBFiledataPA, compareNamesBPA);
  bool *sentFinalize = Calloc((size_t)nProcsColl, sizeof (sentFinalize[0]));

  for ( ;; )
    {
      MPI_Status status;
      xmpiStat(MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, commPio,
                         &status ), &status);

      int source = status.MPI_SOURCE;
      int tag = status.MPI_TAG;
      struct fileOpTag rtag = decodeFileOpTag(tag);

      int messagesize;
      xmpi(MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &messagesize));

      xdebug ( "receive message from source=%d, id=%d, command=%d ( %s ), "
               "messagesize=%d", source, rtag.id, rtag.command,
               cdiPioCmdStrTab[rtag.command], messagesize);

      switch (rtag.command)
	{
      	case IO_Open_file:
          {
            unsigned char *messageBuffer = Malloc((size_t)messagesize
                                                   * sizeof (messageBuffer[0])),
              *pMB = messageBuffer;

            xmpi(MPI_Recv(messageBuffer, messagesize, MPI_UNSIGNED_CHAR, source,
                          tag, commPio, &status ));

            char *filename = (char *)pMB;
            pMB += (strlen(filename) + 1);
            size_t amount = (size_t)(messageBuffer + messagesize - pMB);

            xdebug("command  %s, filename=%s, amount=%zd",
                   cdiPioCmdStrTab[rtag.command], filename, amount);

            if (!(bfd = listSetGet(bibBFiledataPA, fileIDTest,
                                   (void *)(intptr_t)rtag.id)))
              {
                listSetForeach(bibBFiledataPA, elemCheck, filename);
                bfd = initBFiledataPA(filename, conf, nProcsColl);
                int id;
                if ((id = listSetAdd(bibBFiledataPA, bfd)) < 0)
                  xabort("fileID=%d not unique", rtag.id);
                bfd->fileID = id;
              }
            else
              if (strcmp(filename, bfd->name) != 0)
                xabort("filename is not consistent, fileID=%d", rtag.id);

            size_t currOpIndex
              = ((size_t)bfd->currOpIndex + 1 ) % aioQueueDepth;
            bfd->currOpIndex = (unsigned short)currOpIndex;

            memcpy((void *)bfd->ctrlBlks[currOpIndex].aio_buf,
                   pMB, (size_t)amount);

            writePA(bfd, amount, conf);

            Free(messageBuffer);
          }
	  break;

	case IO_Send_buffer:
          {
            if (!(bfd = listSetGet(bibBFiledataPA, fileIDTest,
                                   (void *)(intptr_t)rtag.id)))
              xabort("fileID=%d is not in set", rtag.id);

            xdebug("command: %s, id=%d, name=%s",
                   cdiPioCmdStrTab[rtag.command], rtag.id, bfd->name );

            size_t currOpIndex
              = ((size_t)bfd->currOpIndex + 1 ) % aioQueueDepth;
            bfd->currOpIndex = (unsigned short)currOpIndex;

            xmpi(MPI_Recv((void *)bfd->ctrlBlks[currOpIndex].aio_buf,
                          messagesize, MPI_UNSIGNED_CHAR, source, tag, commPio,
                          &status));
            writePA(bfd, (size_t)messagesize, conf);
          }
	  break;

	case IO_Close_file:
          {
            if (!(bfd = listSetGet(bibBFiledataPA, fileIDTest,
                                   (void *)(intptr_t)rtag.id)))
              xabort("fileID=%d is not in set", rtag.id);

            xdebug(" command %s, id=%d, name=%s",
                   cdiPioCmdStrTab[rtag.command], rtag.id, bfd->name);

            size_t currOpIndex
              = ((size_t)bfd->currOpIndex + 1 ) % aioQueueDepth;
            bfd->currOpIndex = (unsigned short)currOpIndex;

            xmpi(MPI_Recv((void *)bfd->ctrlBlks[currOpIndex].aio_buf,
                          messagesize, MPI_UNSIGNED_CHAR,
                          source, tag, commPio, &status));

            writePA(bfd, (size_t)messagesize, conf);

            if ( ! --(bfd->activeCollectors))
              {
                xdebug ( "all are finished with file %d, delete entry", rtag.id);
                listSetRemove(bibBFiledataPA, fileIDTest,
                              (void *)(intptr_t)rtag.id);
              }
          }
          break;
        case IO_Finalize:
          {
            int buffer, collID;

            xmpi ( MPI_Recv ( &buffer, 1, MPI_INT, source, tag, commPio, &status ));
            sentFinalize[source] = true;
            bool doFinalize = true;
            for ( collID = 0; collID < nProcsColl; collID++ )
              doFinalize &= sentFinalize[collID];
            if ( doFinalize )
              {
                if (!listSetIsEmpty(bibBFiledataPA))
                  xabort("Set bibBfiledataP is not empty.");
                else
                  {
                    xdebug("%s", "all files are finished, destroy set,"
                           " return");
                    listSetDelete(bibBFiledataPA);
                  }
                Free(sentFinalize);
                return;
              }
          }

          break;
        default:
          xabort ( "COMMAND NOT IMPLEMENTED" );
	}
    }
}



/***************************************************************/


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
