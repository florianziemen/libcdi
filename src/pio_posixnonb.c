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

extern long initial_buffersize;
extern char *token;

extern double accumProbe;
extern double accumRecv;
extern double accumSend;
extern double accumSuspend;
extern double accumWait;
extern double accumWrite;


typedef struct 
{
  char *name;
  int tsID;
  size_t size;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  IO_Server_command command;
  MPI_Request request;
} aFiledataP;

typedef struct 
{
  char *name;
  size_t size;
  struct dBuffer *fb;
  FILE *fp;
  bool finished;
  bool *nfinished;
} bFiledataP;

static queue_t * bibAFiledataP;


/***************************************************************/

static aFiledataP *initAFiledataP ( const char *filename, size_t bs )
{
  aFiledataP *afp;
  size_t len;
  int iret;

  xdebug ( "filename=%s, buffersize=%lu, in", filename, bs );

  afp = xmalloc ( sizeof ( aFiledataP ));

  len = strlen ( filename );
  afp->name = xmalloc (( len + 1) * sizeof ( afp->name[0] ));
  strcpy ( afp->name, filename );
  afp->size = bs;
  afp->tsID = 0;

  /* init output buffer */

  xdebug ( "filename=%s, init output buffer",  afp->name );

  iret = dbuffer_init ( &( afp->db1 ), afp->size );
  iret += dbuffer_init ( &( afp->db2 ), afp->size );

  if ( iret > 0 ) xabort ( "dbuffer_init did not succeed" );

  afp->db = afp->db1;

  afp->command = IO_Open_file;
  afp->request = MPI_REQUEST_NULL;

  xdebug ( "enqueued name=%s, return", afp->name );
  
  return afp;
}

/***************************************************************/

static bFiledataP * initBFiledataP ( char *filename, 
				     size_t bs, int nc )
{
  bFiledataP * bfp;
  int i;

  xdebug ( "filename=%s, buffersize=%lu, ncollectors=%d", filename, bs, nc );

  bfp = xmalloc ( sizeof (  bfp[0] ));

  bfp->name = xmalloc (( strlen ( filename ) + 1 ) * sizeof ( bfp->name[0] ));
  strcpy ( bfp->name, filename );
  bfp->size = bs;

  if (( bfp->fp = fopen ( filename, "w" )) == NULL ) 
    xabort("Failed to open %s", bfp->name);

  dbuffer_init ( & ( bfp->fb ), ( size_t )( bfp->size ));

  bfp->finished = false;
  bfp->nfinished = xmalloc ( nc * sizeof ( bfp->nfinished[0] ));

  for ( i = 0; i < nc; i++ )
    *( bfp->nfinished + i ) = true;


  xdebug ( "filename=%s, opened file, return", bfp->name );

  return bfp;
}

/***************************************************************/

int destroyAFiledataP ( void *v )
{
  aFiledataP *afp = ( aFiledataP * ) v;
  MPI_Status status;

  xdebug ( "filename=%s, cleanup, in", afp->name );

  xmpiStat ( MPI_Wait ( &( afp->request ), &status ), &status );
  
  dbuffer_cleanup ( &( afp->db1 ));
  dbuffer_cleanup ( &( afp->db2 ));

  free ( afp->name );
  free ( afp );

  xdebug("%s", "cleaned up, return");

  return 0;
}

/***************************************************************/

int destroyBFiledataP ( void *v )
{
  int iret = 0;
  bFiledataP *bfp = ( bFiledataP * ) v;

  xdebug ( "filename=%s, cleanup, in",  bfp->name );

  /* close file */

  if (( iret = fclose ( bfp->fp )) == EOF )
    xabort("Failed to close %s", bfp->name);

  /* file closed, cleanup */

  dbuffer_cleanup ( &( bfp->fb ));

  free ( bfp->name );
  free ( bfp->nfinished );
  free ( bfp );

  xdebug("%s", "cleaned up, return");

  return iret;
}

/***************************************************************/

bool compareNamesAP  ( void *v1, void *v2 )
{
  aFiledataP *afd1, *afd2;
  size_t len;
  bool bret;

  afd1 = ( aFiledataP * ) v1;
  afd2 = ( aFiledataP * ) v2;

  len = strlen ( afd1->name );
  bret = ( len == strlen ( afd2->name ) && 
	  memcmp ( afd1->name, afd2->name, len ) == 0 );

  return bret;
}

/***************************************************************/

bool compareNamesBP  ( void *v1, void *v2 )
{
  bFiledataP *bfd1, *bfd2;
  size_t len;
  bool bret;

  bfd1 = ( bFiledataP * ) v1;
  bfd2 = ( bFiledataP * ) v2;

  len = strlen ( bfd1->name );
  bret = ( len == strlen ( bfd2->name ) && 
	  memcmp ( bfd1->name, bfd2->name, len ) == 0 );

  return bret;
}

/***************************************************************/

static void
writeP(bFiledataP *bfd, long amount)
{
  long written;

  xdebug ( "filename=%s, amount=%ld, in", bfd->name, amount );

  if (( written = fwrite ( bfd->fb->buffer, sizeof ( char ), amount,
			   bfd->fp )) != amount )
    xabort("did not succeed writing buffer in %s", bfd->name);

  xdebug ( "filename=%s, written=%ld, amount=%ld, return",
           bfd->name, written, amount );
}

/***************************************************************/

// TODO: unify in IOModi, encapsulate in pio_queue.c
void queueCheckP ( queue_t *q, char *name )
{
  listElem_t *curr;
  bFiledataP *bfd;
  size_t len;

  len = strlen ( name );
  curr = q->head;   
      
  while ( curr )
    { 
      bfd = ( bFiledataP * ) curr->val;
      if ( len == strlen ( bfd->name ) &&
	   memcmp ( name, bfd->name, len ) == 0 )
        xabort("Filename %s is already enqueued\n", name);

      curr = curr->next;
    }  
}

/***************************************************************/

void pwPOSIXNONB ( void )
{
  bFiledataP *bfd; 
  queue_t * bibBFiledataP;
  size_t amount, buffersize;
  char *messageBuffer = NULL;
  char *pMB, *filename, *temp;
  int messagesize, source, tag, i, len, id;
  tag_t *rtag = NULL;
  MPI_Status status;
  MPI_Comm commNode = commInqCommNode ();
  int nProcsCollNode = commInqSizeNode () - commInqSizeColl ();
  bool * sentFinalize, doFinalize;

  xdebug ( "ncollectors=%d on this node", nProcsCollNode );

  bibBFiledataP = queueInit ( destroyBFiledataP, compareNamesBP );
  sentFinalize = xmalloc ( nProcsCollNode * sizeof ( sentFinalize ));
  
  for ( ;; )
    {  
        
      xmpiStat ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commNode, 
                             &status ), &status );
      
      
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      
      if ( rtag ) ungetTag ( rtag );
      if (( rtag   = getTag ( tag )) == NULL )
	xabort ( "getTag () failed" );
      
      xmpi ( MPI_Get_count ( &status, MPI_CHAR, &messagesize ));

      xdebug ( "RECEIVE MESSAGE FROM SOURCE=%d, ID=%d, COMMAND=%d ( %s ),"
               "MESSAGESIZE=%d",
               source, rtag->id, rtag->command, 
               command2charP [ rtag->command ], messagesize);

      switch ( rtag->command )
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
                 command2charP [ rtag->command ], filename,
                 buffersize, amount);
	  
	  
	  if ( !(bfd = ( bFiledataP * ) queueIdx2val ( bibBFiledataP, 
						       rtag->id )))
	    {
	      queueCheckP (  bibBFiledataP, filename );
	      bfd = ( bFiledataP * ) initBFiledataP ( filename, buffersize, nProcsCollNode );
	      
	      if (( id = queuePush ( bibBFiledataP, bfd, 1, rtag->id )) != rtag->id )
                xabort("fileID=%d not unique", rtag->id);
	    }
	  else
	    if (strcmp(filename, bfd->name) != 0)
              xabort("filename is not consistent, fileID=%d", rtag->id);

	  *( bfd->nfinished + source ) = false;

	  memcpy(bfd->fb->buffer, pMB, amount);

	  writeP(bfd, amount);
	  
	  free ( messageBuffer );

	  break;

	case IO_Send_buffer:

	  if (!(bfd = (bFiledataP *)queueIdx2val(bibBFiledataP, rtag->id)))
            xabort("fileID=%d is not in queue", rtag->id );

	  amount = messagesize;

	  xdebug ( "COMMAND %s, ID=%d, NAME=%s", 
                   command2charP [ rtag->command ], 
                   rtag->id, bfd->name );
	  
	  xmpi ( MPI_Recv (  bfd->fb->buffer, amount, MPI_CHAR, source, tag, 
                             commNode, &status ));

	  writeP ( bfd, amount );
	  
	  break;

	case IO_Close_file:
	  
	  xdebug ( "COMMAND %s,  FILE%d, SOURCE%d", 
                   command2charP [ rtag->command ],  rtag->id, source );

	  if (!(bfd = (bFiledataP *)queueIdx2val(bibBFiledataP, rtag->id)))
            xabort("fileID=%d is not in queue", rtag->id);

          amount = messagesize;

	  xdebug("COMMAND %s, ID=%d, NAME=%s, AMOUNT=%zu",
                 command2charP [ rtag->command ], rtag->id, bfd->name,
                 amount);
	  
	  xmpi ( MPI_Recv (  bfd->fb->buffer, amount, MPI_CHAR, source,  tag, 
                             commNode, &status ));	

	  writeP ( bfd, amount );

	  *( bfd->nfinished + source ) = true;	  
	  bfd->finished          = true;
	  
	  for ( i = 0; i < nProcsCollNode; i++ )
	    {
	      if ( *( bfd->nfinished + i ) == false )
		{
		  bfd->finished = false;
		  break;
		}
	    }

	  if ( bfd->finished )
	    {
	      xdebug ( "all are finished with file %d, delete node", rtag->id );
	      queueDelNode ( bibBFiledataP, rtag->id );
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
                if ( bibBFiledataP->head != NULL )
                  {
                    xabort ( "Queue bibBfiledataP is not empty." );
                  }
                else
                  {
                    xdebug("%s", "all files are finished, destroy queue,"
                           " return");
                    queueDestroy ( bibBFiledataP );
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
sendP(aFiledataP *afd, int id)
{
  int tag;
  size_t amount;
  MPI_Status status;
  double startTime;

  amount = dbuffer_data_size ( afd->db );
  tag = setTag ( id, afd->command );

  xdebug ( "send buffer for %s,"
           " size: %llu bytes, command=%s, in",
           afd->name, (unsigned long long)amount,
           command2charP [ afd->command ] );

  if ( ddebug ) startTime = MPI_Wtime ();

  xmpiStat (  MPI_Wait ( &( afd->request ), &status ), &status );

  if ( ddebug ) accumWait +=  ( MPI_Wtime () - startTime );

  xmpi ( MPI_Isend ( afd->db->buffer, amount, MPI_CHAR, commInqSpecialRankNode (), 
                     tag, commInqCommNode (), &( afd->request ))); 

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

void defTimestepP ( aFiledataP *afd, int tsID)
{
  if ( afd == NULL || tsID != afd->tsID + 1 ) 
    xabort ( " defTimestepP () didn't succeed." );
  afd->tsID = tsID;
}


/***************************************************************/

size_t fwPOSIXNONB ( int id, int tsID, const void *buffer, size_t len )
{
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataP *afd;
  listElem_t *curr;

  afd = ( aFiledataP * ) queueIdx2val ( bibAFiledataP, id );

  flush = ( tsID != afd->tsID ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      xdebug ( "tsID = %d, flush buffer", tsID );

      curr = bibAFiledataP->head;

      while ( curr ) 
	{
	  sendP (( aFiledataP *) curr->val, curr->idx );
          defTimestepP (( aFiledataP * ) curr->val, tsID );
	  curr = curr->next;
	}
      xmpi ( MPI_Barrier ( commInqCommColl ())); 
    }                                          

  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  xdebug ( "id = %d, tsID = %d, pushed %lu byte data on buffer, filled = %d", 
           id, tsID, len, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  sendP ( afd, id );
     
	  error = dbuffer_push ( afd->db, ( unsigned char * )buffer, len );
	}
    }

  if ( error == 1 )
    xabort("did not succeed filling output buffer, id=%d", id);


  return len;
}

/***************************************************************/

int fcPOSIXNONB ( int id )
{
  double accumWaitMax;
  aFiledataP *afd;
  int iret, root = 0;

  xdebug ( "fileID %d: send buffer, close file and cleanup",id );

  afd = ( aFiledataP * ) queueIdx2val ( bibAFiledataP, id );

  afd->command = IO_Close_file;

  sendP ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataP, id );

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

int fowPOSIXNONB ( const char *filename )
{
  static aFiledataP *afd;
  static long buffersize = 0; 
  int root = 0, id, iret, messageLength = 32;
  char message[messageLength];
  size_t l;
  char *temp;

  /* broadcast buffersize to collectors */
  
  if ( ! buffersize )
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

  /* init and enqueue aFiledataP */

  afd = initAFiledataP ( filename, buffersize );
  if (( id = queuePush ( bibAFiledataP, afd, 0 )) < 0 )
    xabort("filename %s is not unique", afd->name);

  xdebug ( "filename=%s, init and enqueued aFiledataP, return id = %d", 
	    filename, id );

  /* put filename, id and buffersize on buffer */ 

  iret = dbuffer_push ( afd->db, ( unsigned char *) filename, strlen ( filename ));
  xassert(iret == 0);
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1);  
  xassert(iret == 0);
  sprintf ( message,"%lX", buffersize);
  iret = dbuffer_push ( afd->db,  ( unsigned char *) message, strlen ( message ));
  xassert(iret == 0);
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1);
  xassert(iret == 0);

  if ( ddebug )
    {
      l = strlen ( filename ) + strlen ( message ) + 2; 
      temp = xmalloc (( l + 1 ) * sizeof ( char ));
      strncpy ( temp, ( char * ) afd->db->buffer, ( size_t ) l );
      temp[l]  = '\0';
      xdebug("filename=%s, put Open file message on buffer:\n%s,\t return",
               filename, temp);
      free ( temp );
    }

  return id;
}

/***************************************************************/

void finalizePOSIXNONB ( void )
{
  int buffer = 0, tag, specialRank = commInqSpecialRankNode ();
  MPI_Comm commNode = commInqCommNode ();

  tag = setTag ( 0, IO_Finalize );
  
  xmpi ( MPI_Send ( &buffer, 1, MPI_INT, specialRank, tag, commNode ));

  xdebug("%s", "SENT MESSAGE WITH TAG \"IO_FINALIZE\" TO SPECIAL PROCESS");

  if ( bibAFiledataP->head != NULL ) 
    {
      xabort ( "Queue bibAFiledataP not empty." );
    }
  else
    {
      xdebug("%s", "cleanup queue");
      queueDestroy ( bibAFiledataP );
    }
}

/***************************************************************/

void initPOSIXNONB ( void )  
{
  if ( commInqSizeNode () < 2 ) 
    xabort ( "USAGE: # IO PROCESSES ON A PHYSICAL NODE >= 2" );
  
  if ( commInqRankNode () == commInqSpecialRankNode ()) 
    {
      commDefCommColl ( 0 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      pwPOSIXNONB ();
    }
  else
    {
      commDefCommColl ( 1 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      bibAFiledataP = queueInit ( destroyAFiledataP, 
                                  compareNamesAP );
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
