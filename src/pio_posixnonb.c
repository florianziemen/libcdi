#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mpi.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_impl.h"
#include "pio_util.h"

extern char * command2charP[6];

extern char *token;

typedef struct
{
  size_t size;
  struct dBuffer *fb;
  FILE *fp;
  int fileID;
  bool finished;
  bool *nfinished;
  char name[];
} bFiledataP;

static int
fileIDTest(void *a, void *fileID)
{
  return ((bFiledataP *)a)->fileID == (int)(intptr_t)fileID;
}

/***************************************************************/

static bFiledataP *
initBFiledataP(char *filename, size_t bs, int nc, int fileID)
{
  bFiledataP * bfp;
  int i;

  xdebug ( "filename=%s, buffersize=%lu, ncollectors=%d", filename, bs, nc );

  bfp = xmalloc(sizeof (*bfp) + strlen(filename) + 1);
  strcpy(bfp->name, filename);
  bfp->size = bs;

  if (( bfp->fp = fopen ( filename, "w" )) == NULL ) 
    xabort("Failed to open %s", bfp->name);
  int fd = fileno(bfp->fp);
  ftruncate(fd, (off_t)0);
  dbuffer_init ( & ( bfp->fb ), ( size_t )( bfp->size ));

  bfp->finished = false;
  bfp->nfinished = xmalloc ( nc * sizeof ( bfp->nfinished[0] ));

  for ( i = 0; i < nc; i++ )
    bfp->nfinished[i] = false;

  bfp->fileID = fileID;

  xdebug ( "filename=%s, opened file, return", bfp->name );

  return bfp;
}

/***************************************************************/

static int
destroyBFiledataP(void *v)
{
  int iret = 0;
  bFiledataP *bfp = ( bFiledataP * ) v;

  xdebug ( "filename=%s, cleanup, in",  bfp->name );

  /* close file */
  if (( iret = fclose ( bfp->fp )) == EOF )
    xabort("Failed to close %s", bfp->name);

  /* file closed, cleanup */

  dbuffer_cleanup ( &( bfp->fb ));

  free ( bfp->nfinished );
  free ( bfp );

  xdebug("%s", "cleaned up, return");

  return iret;
}

/***************************************************************/

static bool
compareNamesBP(void *v1, void *v2)
{
  bFiledataP *bfd1 = v1, *bfd2 = v2;

  return !strcmp(bfd1->name, bfd2->name);
}

/***************************************************************/

static void
writeP(bFiledataP *bfd, long amount)
{
  long written;

  xdebug ( "filename=%s, amount=%ld, in", bfd->name, amount );

  if (( written = fwrite ( bfd->fb->buffer, 1, amount,
			   bfd->fp )) != amount )
    xabort("did not succeed writing buffer in %s", bfd->name);

  xdebug ( "filename=%s, written=%ld, amount=%ld, return",
           bfd->name, written, amount );
}

/***************************************************************/

// TODO: unify in IOModi, encapsulate in pio_queue.c
static void
elemCheck(void *q, void *nm)
{
  bFiledataP *bfd = q;
  const char *name = nm;

  if (!strcmp(name, bfd->name))
    xabort("Filename %s is already enqueued\n", name);
}

void
pioWriterStdIO(void)
{
  bFiledataP *bfd; 
  queue_t * bibBFiledataP;
  size_t amount, buffersize;
  char *messageBuffer = NULL;
  char *pMB, *filename, *temp;
  int messagesize, source, tag, i, id;
  struct fileOpTag rtag;
  MPI_Status status;
  MPI_Comm commNode = commInqCommNode ();
  int nProcsCollNode = commInqSizeNode () - commInqSizeColl ();
  bool * sentFinalize, doFinalize;

  xdebug ( "ncollectors=%d on this node", nProcsCollNode );

  bibBFiledataP = queueInit(destroyBFiledataP, compareNamesBP);
  sentFinalize = xmalloc ( nProcsCollNode * sizeof ( sentFinalize ));
  
  for ( ;; )
    {  
        
      xmpiStat ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commNode, 
                             &status ), &status );
      
      
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      
      rtag = getTag(tag);
      
      xmpi ( MPI_Get_count ( &status, MPI_CHAR, &messagesize ));

      xdebug ( "RECEIVE MESSAGE FROM SOURCE=%d, ID=%d, COMMAND=%d ( %s ),"
               "MESSAGESIZE=%d", source, rtag.id, rtag.command,
               command2charP[rtag.command], messagesize);

      switch (rtag.command)
	{
      	case IO_Open_file:

	  messageBuffer = xmalloc ( messagesize  * sizeof ( messageBuffer[0] ));
    	  pMB = messageBuffer;

	  xmpi ( MPI_Recv ( messageBuffer, messagesize, MPI_CHAR, source, 
                            tag, commNode, &status ));

	  xdebug("%s", "after recv, in loop");
	  
	  filename = strtok ( pMB, token );
	  pMB += ( strlen ( filename ) + 1 );
	  temp =  strtok ( pMB, token );
          buffersize =  strtol ( temp, NULL, 16 );
	  pMB += ( strlen ( temp ) + 1 );
	  amount = (size_t)(messageBuffer + messagesize - pMB);
	  
	  xdebug("command %s, filename=%s, buffersize=%zu, amount=%zu",
                 command2charP[rtag.command], filename, buffersize, amount);
	  
	  
          if (!(bfd = queueGet(bibBFiledataP, fileIDTest,
                               (void *)(intptr_t)rtag.id)))
	    {
	      queueForeach(bibBFiledataP, elemCheck, filename);
	      bfd = initBFiledataP(filename, buffersize, nProcsCollNode,
                                   rtag.id);
	      
	      if ((id = queueAdd(bibBFiledataP, bfd)) < 0)
                xabort("fileID=%d not unique", rtag.id);
	    }
	  else
	    if (strcmp(filename, bfd->name) != 0)
              xabort("filename is not consistent, fileID=%d", rtag.id);

	  memcpy(bfd->fb->buffer, pMB, amount);

	  writeP(bfd, amount);
	  
	  free ( messageBuffer );

	  break;

	case IO_Send_buffer:

          if (!(bfd = queueGet(bibBFiledataP, fileIDTest,
                               (void *)(intptr_t)rtag.id)))
            xabort("fileID=%d is not in queue", rtag.id );

	  amount = messagesize;

	  xdebug("COMMAND %s, ID=%d, NAME=%s", command2charP[rtag.command],
                 rtag.id, bfd->name);
	  
	  xmpi ( MPI_Recv (  bfd->fb->buffer, amount, MPI_CHAR, source, tag, 
                             commNode, &status ));

	  writeP ( bfd, amount );
	  
	  break;

	case IO_Close_file:
	  
	  xdebug("COMMAND %s,  FILE%d, SOURCE%d",
                 command2charP[rtag.command], rtag.id, source);

          if (!(bfd = queueGet(bibBFiledataP, fileIDTest,
                               (void *)(intptr_t)rtag.id)))
            xabort("fileID=%d is not in queue", rtag.id);

          amount = messagesize;

	  xdebug("COMMAND %s, ID=%d, NAME=%s, AMOUNT=%zu",
                 command2charP[rtag.command], rtag.id, bfd->name, amount);
	  
	  xmpi(MPI_Recv(bfd->fb->buffer, amount, MPI_CHAR, source, tag,
                        commNode, &status ));

	  writeP ( bfd, amount );

	  bfd->nfinished[source] = true;
	  bfd->finished          = true;
	  
	  for ( i = 0; i < nProcsCollNode; i++ )
	    {
	      if ( bfd->nfinished[i] == false )
		{
		  bfd->finished = false;
		  break;
		}
	    }

	  if ( bfd->finished )
	    {
	      xdebug("all are finished with file %d, delete node", rtag.id);
	      queueDelNode(bibBFiledataP, fileIDTest,
                           (void *)(intptr_t)rtag.id);
	    }
          break;
        case IO_Finalize:
          {
            int buffer = CDI_UNDEFID, collID;

            xmpi ( MPI_Recv ( &buffer, 1, MPI_INT, source, tag, commNode, &status ));
            
            sentFinalize[source] = true;
            doFinalize = true;
            
            for ( collID = 0; collID < nProcsCollNode; collID++ )
              if ( !sentFinalize[collID] ) 
                {
                  doFinalize = false;
                  break;
                }
            
            if ( doFinalize )
              {
                if (!queueIsEmpty(bibBFiledataP))
                  xabort("Queue bibBfiledataP is not empty.");
                else
                  {
                    xdebug("%s", "all files are finished, destroy queue,"
                           " return");
                    queueDestroy ( bibBFiledataP );
                  }
                return;
              }
          }
          break;
        default:
          xabort ( "COMMAND NOT IMPLEMENTED" );
	}
    }
}

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
