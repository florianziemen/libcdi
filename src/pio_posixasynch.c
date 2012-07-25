/* 
   todo  
   README: specialRank Pe closes down, when all output files are closed    
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#ifdef USE_MPI
#ifndef _SX

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <aio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mpi.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_impl.h"
#include "pio_util.h"

extern char * command2charP[6];

extern long initial_buffersize;
extern char *token;
extern int maxErrorString; 

extern double accumProbe;
extern double accumRecv;
extern double accumSend;
extern double accumSuspend;
extern double accumWait;
extern double accumWrite;


typedef struct 
{
  char *name;
  size_t size;
  int tsID;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  IO_Server_command command;
  MPI_Request request;
} aFiledataPA;

typedef struct 
{
  char *name;
  size_t size;
  struct dBuffer *fb;
  int handle;
  struct aiocb *ctrlBlks;
  off_t offset;
  int currOpIndex;
  int nextOpIndex;
  int prefIndex;
  bool finished;
  bool *nfinished;
} bFiledataPA;

static queue_t *bibAFiledataPA;

int nPrefStreams = 4;

/***************************************************************/

static aFiledataPA *initAFiledataPA ( const char *filename, size_t bs )
{
  aFiledataPA *afd;
  size_t len;
  int iret;

  xdebug ( "filename=%s, buffersize=%zu, in", filename, bs );

  afd = xmalloc ( sizeof ( aFiledataPA ));

  len = strlen ( filename );
  afd->name = xmalloc (( len + 1) * sizeof ( afd->name[0] ));
  strcpy ( afd->name, filename );
  afd->size = bs;
  afd->tsID = 0;
  
  /* init output buffer */
  
  xdebug ( "filename=%s, init output buffer",  afd->name );

  iret = dbuffer_init ( &( afd->db1 ), afd->size );
  iret += dbuffer_init ( &( afd->db2 ), afd->size );

  if ( iret > 0 ) xabort ( "dbuffer_init did not succeed" );

  afd->db = afd->db1;

  afd->command = IO_Open_file;
  afd->request = MPI_REQUEST_NULL;

  xdebug ( "enqueued name=%s, return", afd->name );
  
  return afd;
}

/***************************************************************/ 

static bFiledataPA *initBFiledataPA ( char *filename, 
                                      size_t bs, int nc )
{
  bFiledataPA *bfd;
  int i;
  char errorString[maxErrorString];

  xdebug ( "filename=%s, buffersize=%zu, ncollectors=%d, nPrefetchStreams=%d",
           filename, bs, nc, nPrefStreams );

  bfd = xmalloc ( sizeof ( bfd[0] ));

  bfd->name = xmalloc (( strlen ( filename ) + 1) * sizeof ( bfd->name[0] ));
  strcpy ( bfd->name, filename );
  bfd->size = bs;

  if (( bfd->handle = open ( bfd->name, O_CREAT | O_WRONLY, 0666 )) == -1 )
    {
      sprintf ( errorString, "Failed to open %s", bfd->name );
      xabort ( errorString );
    }

  dbuffer_init ( &( bfd->fb ), ( size_t )( nPrefStreams * bfd->size ));

  bfd->ctrlBlks = xcalloc(nPrefStreams, sizeof (bfd->ctrlBlks[0]));

  for ( i = 0; i < nPrefStreams; i++ )
    {
      bfd->ctrlBlks[i].aio_fildes     = bfd->handle;
      bfd->ctrlBlks[i].aio_buf        = bfd->fb->buffer + ( i * bfd->size );
      bfd->ctrlBlks[i].aio_reqprio    = 0;
      bfd->ctrlBlks[i].aio_sigevent.sigev_notify = SIGEV_NONE;   
    }
  
  bfd->nextOpIndex = 0;
  bfd->prefIndex = 0; 
  bfd->offset = 0;
  bfd->finished = false;
  bfd->nfinished = xmalloc ( nc * sizeof ( bfd->nfinished[0] ));

  for ( i = 0; i < nc; i++ )
    *( bfd->nfinished + i ) = true;

  xdebug ( "filename=%s, opened file, return", bfd->name );

  return bfd;
}

/***************************************************************/

int destroyAFiledataPA ( void *v )
{
  aFiledataPA *afd = (aFiledataPA * ) v;
  MPI_Status status;

  xdebug ( "filename=%s, cleanup, in", afd->name );

  xmpiStat ( MPI_Wait ( &( afd->request ), &status ), &status );
  
  dbuffer_cleanup ( &( afd->db1 ));
  dbuffer_cleanup ( &( afd->db2 ));

  free ( afd->name );
  free ( afd );

  xdebug("%s", "cleaned up, return");

  return 0;
}

/***************************************************************/

int destroyBFiledataPA ( void *v )
{
  bFiledataPA *bfd = (bFiledataPA * ) v;
  const struct aiocb *ccBP[1];
  int iret = 0;
  ssize_t ssiret;
  int nextFinishOp = (bfd->nextOpIndex - bfd->prefIndex + nPrefStreams)
    % nPrefStreams;
  double startTime;

  xdebug ( "filename=%s, cleanup and close file", bfd->name );

  /* close file */

  for (; bfd->prefIndex > 0 ; bfd->prefIndex --)
    {
      xdebug("file: %s, prefIndex=%d", bfd->name, (int)bfd->prefIndex);
      ccBP[0] = ( bfd->ctrlBlks + nextFinishOp );

      if ( ddebug )
	startTime = MPI_Wtime ();

      do
	{
	  iret = aio_suspend ( ccBP, 1, NULL );
	  if ( iret < 0 && errno != EINTR ) xabort ( "aio_suspend () failed" );
	}
      while ( iret != 0 );

      if ( ddebug )
	accumSuspend += ( MPI_Wtime () - startTime);

      iret = aio_error(bfd->ctrlBlks + nextFinishOp);
      if (( ssiret = aio_return ( bfd->ctrlBlks + nextFinishOp )) == -1 )
	xabort("aio_return () failed: %s", strerror(iret));

      nextFinishOp = ( nextFinishOp + 1 ) % nPrefStreams;
    }

  if (( iret = close ( bfd->handle )) == -1 )
    xabort("failed to close %s", bfd->name);

  /* file closed, cleanup */

  dbuffer_cleanup ( &( bfd->fb ));

  free ( bfd->nfinished );
  free ( bfd->ctrlBlks );
  free ( bfd->name );
  free ( bfd );

  xdebug("%s", "closed file and cleaned up, return");

  return iret;
}

/***************************************************************/

bool compareNamesAPA  ( void *v1, void *v2 )
{
  aFiledataPA *afd1, *afd2;
  size_t len;
  bool bret;

  afd1 = ( aFiledataPA * ) v1;
  afd2 = ( aFiledataPA * ) v2;

  len = strlen ( afd1->name );
  bret = ( len == strlen ( afd2->name ) && 
	  memcmp ( afd1->name, afd2->name, len ) == 0 );

  return bret;
}

/***************************************************************/

bool compareNamesBPA  ( void *v1, void *v2 )
{
  bFiledataPA *bfd1, *bfd2;
  size_t len;
  bool bret;

  bfd1 = ( bFiledataPA * ) v1;
  bfd2 = ( bFiledataPA * ) v2;

  len = strlen ( bfd1->name );
  bret = ( len == strlen ( bfd2->name ) && 
	  memcmp ( bfd1->name, bfd2->name, len ) == 0 );

  return bret;
}

/***************************************************************/

static void
writePA(bFiledataPA *bfd, long amount)
{
  const struct aiocb *ccBP[1];
  int iret;
  double startTime;

  xdebug ( "file %s, in", bfd->name );
  
  bfd->ctrlBlks[bfd->currOpIndex].aio_nbytes = amount;
  bfd->ctrlBlks[bfd->currOpIndex].aio_offset = bfd->offset;

  xdebug ( " before aio_write(), file %s, aio_nbytes=%zu, aio_offset=%zu",
           bfd->name, bfd->ctrlBlks[bfd->currOpIndex].aio_nbytes,
           bfd->ctrlBlks[bfd->currOpIndex].aio_offset );

  if ( ddebug ) startTime = MPI_Wtime ();

  iret = aio_write ( bfd->ctrlBlks + bfd->currOpIndex );

  if ( ddebug ) accumWrite += ( MPI_Wtime () - startTime);

  xdebug ( "after aio_write(), file %s, aio_nbytes=%zu, aio_offset=%zu,"
           "iret=aio_write()=%d",
           bfd->name, bfd->ctrlBlks[bfd->currOpIndex].aio_nbytes,
           bfd->ctrlBlks[bfd->currOpIndex].aio_offset, iret );
   
  if ( iret == -1 ) 
    {
      xabort ( "did not succeed writing buffer" );
    }
  else
    xdebug ( "buffer written to %s",  bfd->name );
     
  bfd->offset += ( off_t ) amount;
  bfd->prefIndex ++;

  if ( bfd->prefIndex >= nPrefStreams ) 
    {
      ccBP[0] = ( bfd->ctrlBlks + bfd->nextOpIndex );

      if ( ddebug )
	startTime = MPI_Wtime ();

      do
	{
	  iret = aio_suspend ( ccBP, 1, NULL );
	  if ( iret < 0 && errno != EINTR )
	    xabort ( "aio_suspend () failed" );
	} while ( iret != 0 );

      if ( ddebug )
	accumSuspend += ( MPI_Wtime () - startTime);
	      
      if (( iret = aio_return ( bfd->ctrlBlks + bfd->nextOpIndex )) == -1 ) 
	xabort ( "aio_return () failed" );

      bfd->prefIndex --;
    }

  xdebug ( "filename=%s, prefIndex=%d, return", bfd->name, bfd->prefIndex );
}

/***************************************************************/

// TODO: unify in IOModi, encapsulate in pio_queue.c
void queueCheckPA ( queue_t *q, char *name )
{
  listElem_t *curr;
  bFiledataPA *bfd;
  size_t len;
  char errorString[maxErrorString];
 
  len = strlen ( name );
  curr = q->head;   
      
  while ( curr )
    { 
      bfd = ( bFiledataPA * ) curr->val;
      if ( len == strlen ( bfd->name ) &&
	   memcmp ( name, bfd->name, len ) == 0 )
	{
	  sprintf ( errorString, "Filename %s is already enqueued\n", name );
	  xabort ( errorString );
	  return;
	}
      
      curr = curr->next;
    }  
}

/***************************************************************/

void pwPOSIXASYNCH ( void )
{
  bFiledataPA *bfd; 
  queue_t * bibBFiledataPA;
  long amount, buffersize;
  char *messageBuffer, *pMB, *filename, *temp;
  int messagesize, source, tag, i, id;
  tag_t *rtag;
  MPI_Status status;
  MPI_Comm commNode = commInqCommNode ();
  int nProcsCollNode = commInqSizeNode () - commInqSizeColl ();
  bool * sentFinalize, doFinalize;

  xdebug ( "nProcsCollNode=%d on this node", nProcsCollNode );
 
  bibBFiledataPA = queueInit ( destroyBFiledataPA, compareNamesBPA );
  sentFinalize = xmalloc ( nProcsCollNode * sizeof ( sentFinalize ));
  
  for ( ;; )
    {   
      xmpiStat ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commNode, 
                             &status ), &status );

      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      rtag   = getTag ( tag );

      xmpi ( MPI_Get_count ( &status, MPI_CHAR, &messagesize ));

      xdebug ( "receive message from source=%d, id=%d, command=%d ( %s ), "
               "messagesize=%d",
               source, rtag->id, rtag->command, 
               command2charP [ rtag->command ], messagesize);

      switch ( rtag->command )
	{
      	case IO_Open_file:

	  messageBuffer = ( char *) xmalloc ( messagesize * 
                                              sizeof ( messageBuffer[0] ));
	  pMB = messageBuffer;

	  xmpi ( MPI_Recv ( messageBuffer, messagesize, MPI_CHAR, source, 
                            tag, commNode, &status ));

	  filename = strtok ( pMB, token );
	  pMB += ( strlen ( filename ) + 1 );
	  temp =  strtok ( pMB, token );
	  buffersize =  strtol ( temp, NULL, 16 );
	  pMB += ( strlen ( temp ) + 1 );
	  amount = ( long ) ( messageBuffer + messagesize - pMB );

	  xdebug ( "command  %s, filename=%s, buffersize=%ld, amount=%ld",
                   command2charP [ rtag->command ], filename, 
                   buffersize, amount ); 

	  
	  if ( !(bfd = ( bFiledataPA * ) queueIdx2val ( bibBFiledataPA, 
							     rtag->id )))
	    {
	      queueCheckPA (  bibBFiledataPA, filename );
	      bfd = ( bFiledataPA * ) initBFiledataPA ( filename, 
								 buffersize,  
							       nProcsCollNode );
	      if (( id = queuePush ( bibBFiledataPA, bfd, 1, rtag->id )) != rtag->id )
                xabort("fileID=%d not unique", rtag->id);
	    }
	  else
	    if (strcmp(filename, bfd->name) != 0)
              xabort("filename is not consistent, fileID=%d", rtag->id);

	  *( bfd->nfinished + source ) = false;

	  bfd->currOpIndex = bfd->nextOpIndex;
	  bfd->nextOpIndex = ( bfd->nextOpIndex + 1 ) % nPrefStreams;

	  memcpy ( bfd->fb->buffer + ( bfd->currOpIndex * bfd->size ),
                   pMB, ( int ) amount );

	  writePA ( bfd, amount );

	  free ( messageBuffer );

	  break;

	case IO_Send_buffer:

	  if (!(bfd = (bFiledataPA *)queueIdx2val(bibBFiledataPA, rtag->id)))
            xabort("fileID=%d is not in queue", rtag->id);

	  amount = messagesize;

	  xdebug ( "command: %s, id=%d, name=%s", 
                   command2charP [ rtag->command ], rtag->id, bfd->name );

	  bfd->currOpIndex = bfd->nextOpIndex;
	  bfd->nextOpIndex = ( bfd->nextOpIndex + 1 ) % nPrefStreams;
	  
	  xmpi(MPI_Recv((void *)bfd->ctrlBlks[bfd->currOpIndex].aio_buf,
                        amount, MPI_CHAR, source, tag, commNode, &status ));

	  writePA ( bfd, amount );
	  
	  break;

	case IO_Close_file:

	  if (!(bfd = (bFiledataPA *)queueIdx2val(bibBFiledataPA, rtag->id)))
            xabort("fileID=%d is not in queue", rtag->id );

	  amount = messagesize;

	  xdebug ( " command %s, id=%d, name=%s", 
                   command2charP [ rtag->command ], rtag->id, bfd->name );

	  bfd->currOpIndex = bfd->nextOpIndex;

	  bfd->nextOpIndex = ( bfd->nextOpIndex + 1 ) % nPrefStreams;

	  MPI_Recv (  bfd->fb->buffer + ( bfd->currOpIndex * bfd->size ), 
		      amount, MPI_CHAR, source,  tag, commNode, &status );

	  writePA ( bfd, amount );

	  bfd->nfinished[source] = true;	  
	  bfd->finished          = true;
	  
	  for ( i = 0; i < nProcsCollNode; i++ )
	    if ( !( bfd->nfinished[i] ))
	      {
		bfd->finished = false;
		break;
	      }
	  
	  if ( bfd->finished )
	    {
              xdebug ( "all are finished with file %d, delete node", rtag->id );
	      queueDelNode ( bibBFiledataPA, rtag->id );
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
                if ( bibBFiledataPA->head != NULL )
                  {
                    xabort ( "Queue bibBfiledataP is not empty." );
                  }
                else
                  {
                    xdebug("%s", "all files are finished, destroy queue,"
                           " return");
                    queueDestroy ( bibBFiledataPA );
                  }
                if ( rtag ) ungetTag ( rtag );
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
/* send buffer to writer and swap buffer for filling */
static void
sendPA(aFiledataPA *afd, int id)
{
  int tag;
  long amount;
  MPI_Status status;
  double startTime;

  amount = ( long ) dbuffer_data_size ( afd->db );
  tag = setTag ( id, afd->command );

  xdebug ( "send buffer for %s, size: %zu bytes, command=%s, in", 
           afd->name, amount, command2charP [ afd->command ] );
  
  if ( ddebug ) startTime = MPI_Wtime ();

  xmpiStat ( MPI_Wait ( &( afd->request ), &status ), &status );

  if ( ddebug ) accumWait +=  ( MPI_Wtime () - startTime );

  xmpi ( MPI_Isend ( afd->db->buffer, amount, MPI_CHAR, commInqSpecialRankNode (), 
                     tag, commInqCommNode (), &afd->request )); 

  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
  
  if ( afd->db == afd->db1 )
    {
      xdebug("%s", "Change to buffer 2 ...");
      afd->db =  afd->db2;
    }
  else 
    {
      xdebug("%s", "Change to buffer 1 ...");
      afd->db =  afd->db1;
    }
  
  afd->command = IO_Send_buffer;
  
  return;
}

 
/***************************************************************/ 

void defTimestepPA ( aFiledataPA * afd, int tsID )
{
  if ( afd == NULL || tsID != afd->tsID + 1 ) 
    xabort ( " defTimestepPA () didn't succeed." );
  afd->tsID = tsID;
}
 

/***************************************************************/

size_t fwPOSIXASYNCH( int id, int tsID, const void *buffer, size_t len )
{
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataPA *afd;
  listElem_t *curr;
  char errorString[maxErrorString];

  afd = ( aFiledataPA * ) queueIdx2val ( bibAFiledataPA, id );
  flush = ( tsID != afd->tsID ) ? 1 : 0;
  if ( flush == 1 ) 
    {
      xdebug ( "fileWriter (): tsID = %d, flush buffer", tsID );

      curr = bibAFiledataPA->head;

      while ( curr ) 
	{
	  sendPA (( aFiledataPA *) curr->val, curr->idx );
          defTimestepPA (( aFiledataPA * ) curr->val, tsID );
	  curr = curr->next;
	}
      xmpi ( MPI_Barrier ( commInqCommColl ())); 
    }                                          

  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  xdebug ( "id = %d, tsID = %d, pushed %zu byte data on buffer, filled = %d", 
           id, tsID, len, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  sendPA ( afd, id );
     
	  error = dbuffer_push ( afd->db, ( unsigned char * )buffer, len );
	}
    }

  if ( error == 1 )
    xabort("did not succeed filling output buffer, id=%d", id);

  return len;
}

/***************************************************************/

int fcPOSIXASYNCH ( int id )
{
  double accumWaitMax;
  aFiledataPA *afd;
  int iret, root = 0;

  xdebug ( "fileID %d: send buffer, close file and cleanup", id );

  afd = ( aFiledataPA * ) queueIdx2val ( bibAFiledataPA, id );

  afd->command = IO_Close_file;

  sendPA ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataPA, id );

  /* timer output */

  if ( ddebug )
    {
      xmpi ( MPI_Reduce ( &accumWait, &accumWaitMax, 
                          1, MPI_DOUBLE, MPI_MAX, root, commInqCommColl ()));
      xdebug ( "Wait time %15.10lf s", accumWait );
      if ( commInqRankColl () == root )
	xdebug ( "Max wait time %15.10lf s", accumWaitMax );
    }
  
  return iret;
}

/***************************************************************/

int fowPOSIXASYNCH ( const char *filename )
{
  static aFiledataPA *afd;
  static long buffersize = 0; 
  int root = 0, iret, id, messageLength = 32;
  char message[messageLength];
  char errorString[maxErrorString];

  /* broadcast buffersize to collectors */
             
  if ( !buffersize )
    {
      
      if  ( commInqRankColl () == root )
	{ 
	  if ( getenv( "BUFSIZE" ) != NULL )
	    buffersize = atol ( getenv ( "BUFSIZE" ));
	  if ( buffersize < initial_buffersize )
	    buffersize = initial_buffersize;
          xdebug("filename=%s, broadcast buffersize=%ld to collectors ...",
                 filename, buffersize);
	}
      xmpi ( MPI_Bcast ( &buffersize, 1, MPI_LONG, root, commInqCommColl ()));
    }

  /* init and enqueue aFiledataPA */

  afd = initAFiledataPA ( filename, buffersize );
  if (( id = queuePush ( bibAFiledataPA, afd, 0 )) < 0 )
    {  
      sprintf ( errorString, "filename %s is not unique", afd->name );
      xabort ( errorString );  
    }

  xdebug ( "filename=%s, init and enqueued aFiledataPA, return id = %d", 
           filename, id );

  /* put filename, id and buffersize on buffer */ 

  iret = dbuffer_push ( afd->db, ( unsigned char *) filename, 
                        strlen ( filename ));
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1);  
  sprintf ( message,"%lX", buffersize); 
  iret = dbuffer_push ( afd->db,  ( unsigned char *) message, 
                        strlen ( message ));
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1);

  if ( ddebug )
    {
      size_t l = strlen(filename) + strlen(message) + 2;
      char *temp = xmalloc(l + 1);
      strncpy(temp, (char *)afd->db->buffer, l);
      temp[l]  = '\0';
      xdebug ( "filename=%s, put Open file message on buffer:\n%s,\t return",
           filename, temp);
      free(temp);
    }

  return id;
}

/***************************************************************/

void finalizePOSIXASYNCH ( void )
{  
  int buffer = 0, tag, specialRank = commInqSpecialRankNode ();
  MPI_Comm commNode = commInqCommNode ();
  
  tag = setTag ( 0, IO_Finalize );
  
  xmpi ( MPI_Send ( &buffer, 1, MPI_INT, specialRank, tag, commNode ));
  
  if ( bibAFiledataPA->head != NULL ) 
    {
      xabort ( "Queue bibAFiledataP not empty." );
    }
  else
    {
      xdebug("%s", "cleanup queue");
      queueDestroy ( bibAFiledataPA );
    }
}

/***************************************************************/

void initPOSIXASYNCH ( void )  
{
  if ( commInqSizeNode () < 2 ) 
    xabort ( "USAGE: # IO PROCESSES ON A PHYSICAL NODE >= 2" );

  if ( nPrefStreams < 1 ) xabort ( "USAGE: # PREFETCH STREAMS >= 1" );

  if ( commInqRankNode () == commInqSpecialRankNode ())
    {
      commDefCommColl ( 0 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      pwPOSIXASYNCH ();
    }
  else
    {
      commDefCommColl ( 1 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      bibAFiledataPA = queueInit ( destroyAFiledataPA, 
                                   compareNamesAPA );
    }
}

#endif
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
