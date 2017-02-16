#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#include "cdi.h"
#include "dmemory.h"

#include "pio.h"
#include "pio_comm.h"
#include "pio_conf.h"
#include "pio_impl.h"
#include "pio_util.h"

struct fileFunnelStdio
{
  char *name;
  FILE *fp;
  int fileID;
  int activeCollectors;
};

static int
fileIDTest(void *a, void *fileID)
{
  return ((struct fileFunnelStdio *)a)->fileID == (int)(intptr_t)fileID;
}

/***************************************************************/

static struct fileFunnelStdio *
initBFiledataP(char *filename, size_t bs, int nc, int fileID)
{
  struct fileFunnelStdio * bfp;

  xdebug ( "filename=%s, buffersize=%lu, ncollectors=%d", filename, bs, nc );

  bfp = Malloc(sizeof (*bfp));
  {
    size_t nameSize = strlen(filename) + 1;
    char *name = bfp->name = Malloc(nameSize);
    memcpy(name, filename, nameSize);
  }

  if (( bfp->fp = fopen ( filename, "w" )) == NULL )
    xabort("Failed to open %s", bfp->name);
  int fd = fileno(bfp->fp);
  ftruncate(fd, (off_t)0);

  bfp->activeCollectors = nc;

  bfp->fileID = fileID;

  xdebug ( "filename=%s, opened file, return", bfp->name );

  return bfp;
}

/***************************************************************/

static int
destroyBFiledataP(void *v)
{
  int iret = 0;
  struct fileFunnelStdio *bfp = ( struct fileFunnelStdio * ) v;

  xdebug ( "filename=%s, cleanup, in",  bfp->name );

  /* close file */
  if (( iret = fclose ( bfp->fp )) == EOF )
    xabort("Failed to close %s", bfp->name);

  /* file closed, cleanup */
  Free(bfp->name);
  Free(bfp);

  xdebug("%s", "cleaned up, return");

  return iret;
}

/***************************************************************/

static bool
compareNamesBP(void *v1, void *v2)
{
  struct fileFunnelStdio *bfd1 = v1, *bfd2 = v2;

  return !strcmp(bfd1->name, bfd2->name);
}

/***************************************************************/

static void
writeP(struct fileFunnelStdio *bfd, void *buffer, size_t amount)
{
  size_t written;

  xdebug ( "filename=%s, amount=%ld, in", bfd->name, amount );

  if ( (written = fwrite(buffer, 1, amount, bfd->fp)) != amount )
    xabort("did not succeed writing buffer to %s", bfd->name);

  xdebug ( "filename=%s, written=%ld, amount=%ld, return",
           bfd->name, written, amount );
}

/***************************************************************/
static void
elemCheck(void *q, void *nm)
{
  struct fileFunnelStdio *bfd = q;
  const char *name = nm;

  if (!strcmp(name, bfd->name))
    xabort("Filename %s has already been added to the set\n", name);
}

void
pioWriterStdIO(void)
{
  struct fileFunnelStdio *bfd;
  listSet * bibBFiledataP;
  int messageSize, source, tag, id;
  struct fileOpTag rtag;
  MPI_Status status;
  MPI_Comm commPio = commInqCommPio ();
  int nProcsColl = commInqSizeColl ();
  bool * sentFinalize, doFinalize;

  xdebug ( "ncollectors=%d", nProcsColl );

  bibBFiledataP = listSetNew(destroyBFiledataP, compareNamesBP);
  sentFinalize = Calloc((size_t)nProcsColl, sizeof (sentFinalize[0]));
  char *messageBuffer = NULL;
  size_t messageBufferSize = 0;
  struct cdiPioConf *conf = cdiPioGetConf();

  for ( ;; )
    {

      xmpiStat ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commPio,
                             &status ), &status );


      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;

      rtag = decodeFileOpTag(tag);

      xmpi(MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &messageSize));

      xdebug ( "RECEIVE MESSAGE FROM SOURCE=%d, ID=%d, COMMAND=%d ( %s ),"
               "MESSAGESIZE=%d", source, rtag.id, rtag.command,
               cdiPioCmdStrTab[rtag.command], messageSize);

      if ((size_t)messageSize > messageBufferSize)
        {
          messageBuffer = Realloc(messageBuffer, (size_t)messageSize);
          messageBufferSize = (size_t)messageSize;
        }

      switch (rtag.command)
	{
      	case IO_Open_file:
          {
            char *pMB = messageBuffer;
            xmpi(MPI_Recv(messageBuffer, messageSize, MPI_UNSIGNED_CHAR,
                          source, tag, commPio, &status));

            xdebug("%s", "after recv, in loop");

            char *filename = pMB;
            pMB += (strlen(filename) + 1);
            size_t amount = (size_t)(messageBuffer + messageSize - pMB);

            size_t buffersize = conf->writeAggBufLim;
            xdebug("command %s, filename=%s, buffersize=%zu, amount=%zu",
                   cdiPioCmdStrTab[rtag.command], filename, buffersize, amount);


            if (!(bfd = listSetGet(bibBFiledataP, fileIDTest,
                                   (void *)(intptr_t)rtag.id)))
              {
                listSetForeach(bibBFiledataP, elemCheck, filename);
                bfd = initBFiledataP(filename, buffersize, nProcsColl, rtag.id);
                if ((id = listSetAdd(bibBFiledataP, bfd)) < 0)
                  xabort("fileID=%d not unique", rtag.id);
                bfd->fileID = id;
              }
            else
              if (strcmp(filename, bfd->name) != 0)
                xabort("filename is not consistent, fileID=%d", rtag.id);
            writeP(bfd, pMB, amount);
          }
	  break;

	case IO_Send_buffer:
          {
            if (!(bfd = listSetGet(bibBFiledataP, fileIDTest,
                                   (void *)(intptr_t)rtag.id)))
              xabort("fileID=%d is not in set", rtag.id );
            size_t amount = (size_t)messageSize;

            xdebug("COMMAND %s, ID=%d, NAME=%s", cdiPioCmdStrTab[rtag.command],
                   rtag.id, bfd->name);

            xmpi(MPI_Recv(messageBuffer, messageSize, MPI_UNSIGNED_CHAR,
                          source, tag, commPio, &status));

            writeP(bfd, messageBuffer, amount);
          }
          break;

	case IO_Close_file:
          {
            xdebug("COMMAND %s,  FILE%d, SOURCE%d",
                   cdiPioCmdStrTab[rtag.command], rtag.id, source);

            if (!(bfd = listSetGet(bibBFiledataP, fileIDTest,
                                   (void *)(intptr_t)rtag.id)))
              xabort("fileID=%d is not in set", rtag.id);
            size_t amount = (size_t)messageSize;
            xdebug("COMMAND %s, ID=%d, NAME=%s, AMOUNT=%zu",
                   cdiPioCmdStrTab[rtag.command], rtag.id, bfd->name, amount);

            xmpi(MPI_Recv(messageBuffer, messageSize, MPI_UNSIGNED_CHAR,
                          source, tag, commPio, &status));

            writeP(bfd, messageBuffer, amount);

            if ( ! --(bfd->activeCollectors))
              {
                xdebug("all are finished with file %d, delete node", rtag.id);
                listSetRemove(bibBFiledataP, fileIDTest,
                              (void *)(intptr_t)rtag.id);
              }
          }
          break;
        case IO_Finalize:
          {
            int buffer = CDI_UNDEFID, collID;

            xmpi ( MPI_Recv ( &buffer, 1, MPI_INT, source, tag, commPio, &status ));

            sentFinalize[source] = true;
            doFinalize = true;

            for ( collID = 0; collID < nProcsColl; collID++ )
              if ( !sentFinalize[collID] )
                {
                  doFinalize = false;
                  break;
                }

            if ( doFinalize )
              {
                if (!listSetIsEmpty(bibBFiledataP))
                  xabort("set bibBfiledataP is not empty.");
                else
                  {
                    xdebug("%s", "all files are finished, destroy file set,"
                           " return");
                    listSetDelete(bibBFiledataP);
                  }
                Free(sentFinalize);
                Free(messageBuffer);
                return;
              }
          }
          break;
        default:
          xabort ( "COMMAND NOT IMPLEMENTED" );
	}
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
