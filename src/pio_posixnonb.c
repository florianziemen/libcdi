#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <assert.h>
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
  int activeCollectors;
};

/***************************************************************/

static void
initBFiledataP(struct fileFunnelStdio *bfp,
               const char *filename, size_t bs, int nc)
{
  xdebug ( "filename=%s, buffersize=%lu, ncollectors=%d", filename, bs, nc );

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

  xdebug ( "filename=%s, opened file, return", bfp->name );
}

/***************************************************************/

static int
destroyBFiledataP(struct fileFunnelStdio *bfp)
{

  xdebug ( "filename=%s, cleanup, in",  bfp->name );

  /* close file */
  int iret;
  if (( iret = fclose ( bfp->fp )) == EOF )
    xabort("Failed to close %s", bfp->name);

  /* file closed, cleanup */
  Free(bfp->name);
  bfp->name = NULL;

  xdebug("%s", "cleaned up, return");

  return iret;
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

void
pioWriterStdIO(void)
{
  struct fileFunnelStdio *openFileFunnels;
  size_t openFileFunnelsSize, openFileFunnelsFill;
  int messageSize, source, tag;
  struct fileOpTag rtag;
  MPI_Status status;
  MPI_Comm commPio = commInqCommPio ();
  int nProcsColl = commInqSizeColl ();
  bool * sentFinalize, doFinalize;

  xdebug ( "ncollectors=%d", nProcsColl );

  openFileFunnelsSize = 4;
  openFileFunnelsFill = 0;
  openFileFunnels = Malloc(sizeof (*openFileFunnels) * openFileFunnelsSize);
  for (size_t i = 0; i < openFileFunnelsSize; ++i)
    openFileFunnels[i].name = NULL;
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

            int fileID = rtag.id;
            size_t prevOpenFileFunnelsSize = openFileFunnelsSize;
            if (openFileFunnelsSize <= (unsigned)fileID)
              {
                while (openFileFunnelsSize <= (unsigned)fileID)
                  openFileFunnelsSize *= 2;
                if (openFileFunnelsSize > (unsigned)INT_MAX + 1)
                  openFileFunnelsSize = (unsigned)INT_MAX + 1;
                openFileFunnels
                  = Realloc(openFileFunnels,
                            sizeof (*openFileFunnels) * openFileFunnelsSize);
                for (size_t i = prevOpenFileFunnelsSize;
                     i < openFileFunnelsSize;
                     ++i)
                  openFileFunnels[i].name = NULL;
              }
            struct fileFunnelStdio *bfd = openFileFunnels + fileID;
            if (!bfd->name)
              {
                for (size_t i = 0; i < prevOpenFileFunnelsSize; ++i)
                  if (openFileFunnels[i].name
                      && !strcmp(openFileFunnels[i].name, filename))
                    xabort("filename %s is already open!"
                           " CDI-PIO does not support concurrent access"
                           " through different filehandles.", filename);
                initBFiledataP(bfd, filename, buffersize, nProcsColl);
                ++openFileFunnelsFill;
              }
            else if (strcmp(filename, bfd->name) != 0)
              xabort("filename is not consistent, fileID=%d,"
                     " \"%s\" vs. \"%s\"", fileID, filename, bfd->name);
            writeP(bfd, pMB, amount);
          }
	  break;

	case IO_Send_buffer:
          {
            int fileID = rtag.id;
            assert(fileID >= 0 && (size_t)fileID < openFileFunnelsSize
                   && openFileFunnels[fileID].name);
            struct fileFunnelStdio *bfd = openFileFunnels + fileID;
            size_t amount = (size_t)messageSize;

            xdebug("COMMAND %s, ID=%d, NAME=%s", cdiPioCmdStrTab[rtag.command],
                   fileID, bfd->name);

            xmpi(MPI_Recv(messageBuffer, messageSize, MPI_UNSIGNED_CHAR,
                          source, tag, commPio, &status));

            writeP(bfd, messageBuffer, amount);
          }
          break;

	case IO_Close_file:
          {
            int fileID = rtag.id;
            assert(fileID >= 0 && (size_t)fileID < openFileFunnelsSize
                   && openFileFunnels[fileID].name);
            struct fileFunnelStdio *bfd = openFileFunnels + fileID;

            xdebug("COMMAND %s,  FILE%d, SOURCE%d",
                   cdiPioCmdStrTab[rtag.command], fileID, source);

            size_t amount = (size_t)messageSize;
            xdebug("COMMAND %s, ID=%d, NAME=%s, AMOUNT=%zu",
                   cdiPioCmdStrTab[rtag.command], fileID, bfd->name, amount);

            xmpi(MPI_Recv(messageBuffer, messageSize, MPI_UNSIGNED_CHAR,
                          source, tag, commPio, &status));

            writeP(bfd, messageBuffer, amount);

            if ( ! --(bfd->activeCollectors))
              {
                xdebug("all are finished with file %d, delete node", rtag.id);
                destroyBFiledataP(bfd);
                --openFileFunnelsFill;
              }
          }
          break;
        case IO_Finalize:
          {
            int buffer, collID;

            xmpi(MPI_Recv(&buffer, 1, MPI_INT, source, tag, commPio, &status));

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
                if (openFileFunnelsFill)
                  xabort("some files still not closed.");
                else
                  {
                    xdebug("%s", "all files are finished, destroy file set,"
                           " return");
                  }
                Free(openFileFunnels);
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
