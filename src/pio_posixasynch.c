/* 
   todo  
   README: specialRank Pe closes down, when all output files are closed    
*/

#ifndef NOMPI

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <aio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "mpi.h"
#include "pio.h"
#include "pio_impl.h"

extern bool ddebug;
extern char * command2charP[6];

extern double startTime;
extern double accumProbe;
extern double accumRecv;
extern double accumSend;
extern double accumSuspend;
extern double accumWait;
extern double accumWrite;

extern long initial_buffersize; 

extern pioInfo *pioinfo;

extern char *token;

typedef struct 
{
  char *name;
  size_t size;
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

static queue *bibAFiledataPA;

int nPrefStreams = 5;

/***************************************************************/

static aFiledataPA *initAFiledataPA ( const char *filename, size_t bs )
{
  aFiledataPA *afd;
  size_t len;
  int status;

  if ( ddebug )
    fprintf ( stdout, "pe%d initAFiledataPA (): filename=%s, buffersize=%zu, in\n",
	pioinfo->rank, filename, bs );

  afd = ( aFiledataPA * ) xmalloc ( sizeof ( aFiledataPA ));
  memset ( afd, 0, sizeof ( aFiledataPA ));

  len = strlen ( filename );
  afd->name = ( char * ) xmalloc (( len ) * sizeof ( char ));
  strcpy ( afd->name, filename );
  afd->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stdout, "pe%d initAFiledataPA(): name=%s, init output buffer\n", 
	      pioinfo->rank, afd->name );
   
  if (( status = dbuffer_init ( &( afd->db1 ), afd->size )) == 1 )
    {
      fprintf ( stderr, "dbuffer_init outputBuffer1: %d\n", status );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  if (( status = dbuffer_init ( &( afd->db2 ), afd->size )) == 1)
    {
      fprintf ( stderr, "dbuffer_init outputBuffer2: %d\n", status );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  afd->db = afd->db1;

  afd->command = IO_Open_file;
  afd->request = MPI_REQUEST_NULL;

  if ( ddebug )
    fprintf ( stdout, "pe%d initAFiledataPA (), enqueued name=%s, return\n",
	pioinfo->rank, afd->name );
  
  return afd;
}
/***************************************************************/ 

static bFiledataPA *initBFiledataPA ( char *filename, 
					       size_t bs, int nc )
{
  bFiledataPA *bfd;
  int i;

  if ( ddebug )
       fprintf ( stdout, "### pe%d initBFiledataPA (): filename=%s, buffersize=%zu, ncollectors=%d, nPrefetchStreams=%d, in\n",
	      pioinfo->rank, filename, bs, nc, nPrefStreams );

  bfd = ( bFiledataPA * ) xmalloc ( sizeof ( bFiledataPA ));
  memset ( bfd, 0, sizeof ( bFiledataPA ));

  bfd->name = ( char * ) xmalloc (( strlen ( filename )) * sizeof ( char ));
  strcpy ( bfd->name, filename );
  bfd->size = bs;

  if (( bfd->handle = open ( bfd->name, O_CREAT | O_WRONLY, 0666 )) == -1 )
    {
      fprintf ( stderr, "Failed to open %s:\n", bfd->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  dbuffer_init ( &( bfd->fb ), ( size_t )( nPrefStreams * bfd->size ));

  bfd->ctrlBlks = ( struct aiocb * ) xmalloc ( nPrefStreams * sizeof ( struct aiocb ));

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
  bfd->nfinished = ( bool * ) xmalloc ( nc * sizeof ( bool ));

  for ( i = 0; i < nc; i++ )
    *( bfd->nfinished + i ) = true;


  if ( ddebug )
    fprintf ( stdout, "### pe%d initBFiledataPA (), name=%s, opened file, return\n",
	pioinfo->rank, bfd->name );

  return bfd;
}

/***************************************************************/

int destroyAFiledataPA ( void *v )
{
  int iret = 0;
  aFiledataPA *afd = (aFiledataPA * ) v;
  MPI_Status status;

  if ( ddebug )
    fprintf ( stdout, "pe%d destroyAFiledataPA(): name=%s, cleanup, in\n", 
	      pioinfo->rank, afd->name );

  MPI_Wait ( &( afd->request ), &status );
  
  dbuffer_cleanup ( &( afd->db1 ));
  dbuffer_cleanup ( &( afd->db2 ));

  free ( afd->name );
  free ( afd );

  if ( ddebug )
    fprintf ( stdout, "pe%d return from destroyAFiledataPA(), cleaned up, return\n", 
	      pioinfo->rank );

  return iret;
}

/***************************************************************/

int destroyBFiledataPA ( void *v )
{
  bFiledataPA *bfd = (bFiledataPA * ) v;
  const struct aiocb *ccBP[1];
  int iret = 0;

  if ( ddebug )
    fprintf ( stdout, "### pe%d destroyBFiledataPA(): name=%s, cleanup and close file, in\n", 
	      pioinfo->rank, bfd->name );

  /* close file */

  while ( bfd->prefIndex > 0 ) 
    {
      ccBP[0] = ( bfd->ctrlBlks + bfd->nextOpIndex );

      if ( ddebug )
	startTime = MPI_Wtime ();

      do
	{
	  iret = aio_suspend ( ccBP, 1, NULL );
	  if ( iret < 0 && errno != EINTR )
	    {
	      fprintf ( stderr, "aio_suspend() failed\n" );
	      MPI_Abort ( pioinfo->comm, 1 );
	    }
	} while ( iret != 0 );

      if ( ddebug )
	accumSuspend += ( MPI_Wtime () - startTime);
      
      if (( iret = aio_return ( bfd->ctrlBlks + bfd->nextOpIndex )) == -1 ) 
	{
	  fprintf ( stderr, "Error from AIO \n");
	  MPI_Abort ( pioinfo->comm, 1 );
	}
      bfd->nextOpIndex = ( bfd->nextOpIndex + 1 ) % nPrefStreams;
      bfd->prefIndex --;
    }

  if (( iret = close ( bfd->handle )) == -1 )
    {
      fprintf ( stderr, "Failed to close %s:\n", bfd->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  /* file closed, cleanup */

  dbuffer_cleanup ( &( bfd->fb ));

  free ( bfd->nfinished );
  free ( bfd->ctrlBlks );
  free ( bfd->name );
  free ( bfd );

  if ( ddebug )
    fprintf ( stdout, "### pe%d destroyBFiledataPA(), closed file and cleaned up, return\n", 
	      pioinfo->rank );

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

void writePA ( bFiledataPA *bfd, long amount )
{
  const struct aiocb *ccBP[1];
  int iret;

  if ( ddebug )
    fprintf ( stdout, "### pe%d writePA, file %s, in\n", 
	      pioinfo->rank, bfd->name );
  
  bfd->ctrlBlks[bfd->currOpIndex].aio_nbytes = amount;
  bfd->ctrlBlks[bfd->currOpIndex].aio_offset = bfd->offset;

  if ( ddebug )
    fprintf ( stdout, "### pe%d writePA before aio_write(), file %s, aio_nbytes=%zu, aio_offset=%zu\n",
	      pioinfo->rank, bfd->name, bfd->ctrlBlks[bfd->currOpIndex].aio_nbytes,
	      bfd->ctrlBlks[bfd->currOpIndex].aio_offset );

  if ( ddebug )
    startTime = MPI_Wtime ();

  iret = aio_write ( bfd->ctrlBlks + bfd->currOpIndex );

  if ( ddebug )
    accumWrite += ( MPI_Wtime () - startTime);

  if ( ddebug )
    fprintf ( stdout, "### pe%d writePA after aio_write(), file %s, aio_nbytes=%zu, aio_offset=%zu, iret=aio_write()=%d\n",
	      pioinfo->rank, bfd->name, bfd->ctrlBlks[bfd->currOpIndex].aio_nbytes,
	      bfd->ctrlBlks[bfd->currOpIndex].aio_offset, iret );

  if ( iret == -1 )
    {
      perror ( "### Problems writing buffer" );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  else
    if ( ddebug )
      fprintf ( stdout, "### pe%d writeBuffer (), buffer written to %s\n", 
		pioinfo->rank, bfd->name );
	  
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
	    {
	      fprintf ( stderr, "aio_suspend() failed\n" );
	      MPI_Abort ( pioinfo->comm, 1 );
	    }
	} while ( iret != 0 );

      if ( ddebug )
	accumSuspend += ( MPI_Wtime () - startTime);
	      
      if (( iret = aio_return ( bfd->ctrlBlks + bfd->nextOpIndex )) == -1 ) 
	{
	  fprintf ( stderr, "Error from AIO \n");
	  MPI_Abort ( pioinfo->comm, 1 );
	}

      bfd->prefIndex --;
    }

  if ( ddebug )
    fprintf ( stdout, "### pe%d writePA (), name=%s, prefIndex=%d, return \n",
	      pioinfo->rank, bfd->name, bfd->prefIndex );

  return;
}

/***************************************************************/

void pwPOSIXASYNCH ( int ncollectors )
{
  bFiledataPA *bfd; 
  queue * bibBFiledataPA;
  long amount, buffersize;
  char *messageBuffer, *pMB, *filename, *temp;
  int messagesize, source, tag, i, id;
  size_t len;
  rTag *rtag;
  MPI_Status status;

  if ( ddebug )
    fprintf(stdout, "### pe%d pwPOSIXASYNCH (), ncollectors=%d, in\n", 
	    pioinfo->rank, ncollectors);
 
  bibBFiledataPA = queueInit ( destroyBFiledataPA, compareNamesBPA );
  
  for ( ;; )
    {  
      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, pioinfo->comm, &status );
  
      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;
      rtag   = getTag ( tag );

      MPI_Get_count ( &status, MPI_CHAR, &messagesize );

      if ( ddebug )
	fprintf ( stdout, 
		  "### pe%d pwPOSIXASYNCH (): receive message from source=%d, id=%d, command=%d ( %s ), messagesize=%d\n",
		  pioinfo->rank, source, rtag->id, rtag->command, 
		  command2charP [ rtag->command ], messagesize);

      switch ( rtag->command )
	{
      	case IO_Open_file:

	  //messageBuffer = ( char *) xmalloc ( messagesize * sizeof ( char ));

	  if (( messageBuffer = ( char *) malloc ( messagesize  * sizeof ( char ))) == NULL )
	    {
	      perror ( "### malloc of messageBuffer failed" );
	      MPI_Abort ( pioinfo->comm, 1 );
	    }

	  pMB = messageBuffer;

	  MPI_Recv ( messageBuffer, messagesize, MPI_CHAR, source, 
		     tag, pioinfo->comm, &status );

	  filename = strtok ( pMB, token );
	  pMB += ( strlen ( filename ) + 1 );
	  temp =  strtok ( pMB, token );
	  buffersize =  strtol ( temp, NULL, 16 );
	  pMB += ( strlen ( temp ) + 1 );
	  amount = ( long ) ( messageBuffer + messagesize - pMB );

	  if ( ddebug ) 
	    fprintf ( stdout,
		      "### pe%d pwPOSIXASYNCH () case %s, filename=%s, buffersize=%ld, amount=%ld\n",
		      pioinfo->rank, command2charP [ rtag->command ], filename, 
		      buffersize, amount ); 

	  
	  if ( !(bfd = ( bFiledataPA * ) queueIdx2val ( bibBFiledataPA, 
							     rtag->id )))
	    {
	      bfd = ( bFiledataPA * ) initBFiledataPA ( filename, 
								 buffersize,  
							       ncollectors );
	      if (( id = queuePush ( bibBFiledataPA, bfd, 1, rtag->id )) != rtag->id )
		{
		  fprintf ( stderr, "pe%d, fileID=%d not unique\n",
			    pioinfo->rank, rtag->id);
		  MPI_Abort ( pioinfo->comm, 1 );
		} 	      
	    }
	  else
	    if ((( len = strlen ( filename )) != strlen ( bfd->name )) || 
		memcmp ( filename, bfd->name, len ) != 0 )
	      {
		fprintf ( stderr, "pe%d, id=%d, filename not consistant\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }

	  *( bfd->nfinished + source ) = false;
		 

	  bfd->currOpIndex = bfd->nextOpIndex;
	  bfd->nextOpIndex = ( bfd->nextOpIndex + 1 ) % nPrefStreams;

	  memcpy ( bfd->fb->buffer + ( bfd->currOpIndex * bfd->size ), pMB, ( int ) amount );

	  writePA ( bfd, amount );
	  
	  free ( messageBuffer );

	  break;

	case IO_Send_buffer:
	  
	  if ( ! ( bfd = ( bFiledataPA * ) queueIdx2val ( bibBFiledataPA,  
							  rtag->id )))

	      {
		fprintf ( stderr, "pe%d, file id=%d not in queue\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }
	  
	  amount = messagesize;

	  if ( ddebug )
	    fprintf ( stdout, "### pe%d, command: %s, id=%d, name=%s\n", 
		      pioinfo->rank, command2charP [ rtag->command ], 
		      rtag->id, bfd->name );

	  bfd->currOpIndex = bfd->nextOpIndex;
	  bfd->nextOpIndex = ( bfd->nextOpIndex + 1 ) % nPrefStreams;
	  
	  MPI_Recv (  bfd->fb->buffer + ( bfd->currOpIndex * bfd->size ), 
		      amount, MPI_CHAR, source, tag, pioinfo->comm, &status );

	  writePA ( bfd, amount );
	  
	  break;

	case IO_Close_file:
	  
	  if ( ! ( bfd = ( bFiledataPA * ) queueIdx2val ( bibBFiledataPA,  
							  rtag->id )))

	      {
		fprintf ( stderr, "pe%d, file id=%d not in queue\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }
	  
	  amount = messagesize;

	  if ( ddebug )
	    fprintf ( stdout, "### pe%d, command %s, id=%d, name=%s\n", 
		      pioinfo->rank, command2charP [ rtag->command ], 
		      rtag->id, bfd->name );

	  bfd->currOpIndex = bfd->nextOpIndex;

	  bfd->nextOpIndex = ( bfd->nextOpIndex + 1 ) % nPrefStreams;

	  MPI_Recv (  bfd->fb->buffer + ( bfd->currOpIndex * bfd->size ), 
		      amount, MPI_CHAR, source,  tag, pioinfo->comm, &status );

	  writePA ( bfd, amount );

	  bfd->nfinished[source] = true;	  
	  bfd->finished          = true;
	  
	  for ( i = 0; i < ncollectors; i++ )
	    if ( !( bfd->nfinished[i] ))
	      {
		bfd->finished = false;
		break;
	      }
	  
	  if ( bfd->finished )
	    {
	      queueDelNode ( bibBFiledataPA, rtag->id );
	      if ( !bibBFiledataPA->head )
		{
		  queueDestroy ( bibBFiledataPA );
		  return;
		}
	    }	      
	}
    }
}


/***************************************************************/

void sendPA ( aFiledataPA *afd, int id )
{
  int tag;
  long amount;
  MPI_Status status;

  amount = ( long ) dbuffer_data_size ( afd->db );
  tag = setTag ( id, afd->command );

  if ( ddebug ) 
    fprintf ( stdout,
	      "pe%d in sendPA (): send buffer for %s, size: %zu bytes, command=%s, in\n", 
	      pioinfo->rank, afd->name, amount, 
	      command2charP [ afd->command ] );
  if ( ddebug )
    startTime = MPI_Wtime ();

  MPI_Wait ( &( afd->request ), &status );

  if ( ddebug )
    accumWait +=  ( MPI_Wtime () - startTime );

  MPI_Isend ( afd->db->buffer, amount, MPI_CHAR, pioinfo->specialRank, 
	      tag, pioinfo->comm, &afd->request ); 

  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
  
  if ( afd->db == afd->db1 )
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in sendPA (): Change to buffer 2 ...\n", 
		  pioinfo->rank );
      afd->db =  afd->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in sendPA (): Change to buffer 1 ...\n", 
		  pioinfo->rank );
      afd->db =  afd->db1;
    }
  
  afd->command = IO_Send_buffer;
  
  return;
} 

/***************************************************************/

size_t fwPOSIXASYNCH( int id, int tsId, const void *buffer, size_t len )
{
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataPA *afd;
  node *curr;

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, tsId );

      curr = bibAFiledataPA->head;

      while ( curr ) 
	{
	  sendPA (( aFiledataPA *) curr->val, curr->idx );
	  curr = curr->next;
	}

      oldTsId = tsId;
      MPI_Barrier ( pioinfo->collectorComm ); 
    }                                          

  afd = ( aFiledataPA * ) queueIdx2val ( bibAFiledataPA, id );
  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  if ( ddebug )
    fprintf ( stdout, "pe%d fileWrite (): id = %d, tsId = %d, pushed %zu byte data on buffer, filled = %d\n", 
	      pioinfo->rank, id, tsId, len, filled ); 

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
    {
      fprintf ( stderr, "pe%d, id=%d, problem filling output buffer ...\n",
		pioinfo->rank, id);
      MPI_Abort ( pioinfo->comm, 1 );
    }
  
  return len;
}

/***************************************************************/

int fcPOSIXASYNCH ( int id )
{
  double accumWaitMax;
  aFiledataPA *afd;
  int iret;

  if ( ddebug )
    fprintf ( stdout, "pe%d fileClose (%d ): send buffer, close file and cleanup\n", 
	      pioinfo->rank, id );

  afd = ( aFiledataPA * ) queueIdx2val ( bibAFiledataPA, id );

  afd->command = IO_Close_file;

  sendPA ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataPA, id );

  if ( !bibAFiledataPA->head ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      queueDestroy ( bibAFiledataPA );
    }

  /* timer output */

  if ( ddebug )
    {
      MPI_Reduce ( &accumWait, &accumWaitMax, 
		   1, MPI_DOUBLE, MPI_MAX, 0, pioinfo->collectorComm );
      fprintf ( stdout, "pe%d fileClose(): Wait time %15.10lf s\n",
		pioinfo->rank, accumWait );
      if ( pioinfo->rank == 0 )
	fprintf ( stdout, 
		  "pe%d fileClose(): Max wait time %15.10lf s\n",
		  pioinfo->rank, accumWaitMax );
    }
  
  return iret;
}

/***************************************************************/

int fowPOSIXASYNCH ( const char *filename )
{
  static aFiledataPA *afd;
  static long buffersize = 0; 
  int bcastRank, iret, id, messageLength = 32;
  char message[messageLength];
  MPI_Comm bcastComm;

  /* broadcast buffersize to collectors */
  
  if ( !buffersize )
    {
      if ( ddebug && pioinfo->rank == 0)
	fprintf ( stdout, 
		  "pe%d fileOpenW(): name=%s, broadcast buffersize to collectors ...\n", 
		  pioinfo->rank, filename );
      
      bcastRank = 0;
      bcastComm = pioinfo->collectorComm;
      
      if  ( pioinfo->rank == bcastRank )
	{ 
	  if ( getenv( "BUFSIZE" ) != NULL )
	    buffersize = atol ( getenv ( "BUFSIZE" ));
	  if ( buffersize < initial_buffersize )
	    buffersize = initial_buffersize;
	}
      
      MPI_Bcast ( &buffersize, 1, MPI_LONG, 0, bcastComm );
    }

  /* init and enqueue aFiledataPA */

  afd = initAFiledataPA ( filename, buffersize );
  if (( id = queuePush ( bibAFiledataPA, afd, 0 )) < 0 )
    {   
      fprintf ( stderr, "Filename %s not unique\n", afd->name );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d fileOpenW(): name=%s, init and enqueued aFiledataPA, return id = %d\n", 
	      pioinfo->rank, filename, id );

  /* put filename, id and buffersize on buffer */ 

  iret = dbuffer_push ( afd->db, ( unsigned char *) filename, strlen ( filename ));
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1);  
  sprintf ( message,"%lX", buffersize); 
  iret = dbuffer_push ( afd->db,  ( unsigned char *) message, strlen ( message ));
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1); 
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d fileOpenW(): name=%s, put Open file message on buffer:\n%s\n return \n", 
	      pioinfo->rank, filename, afd->db->buffer ); 
  
  return id;
}

/***************************************************************/

int initPOSIXASYNCH ( int *ncollectors )  
{
  int collectingData = 1;
  MPI_Group streamGroup, collectorGroup;
  int ioRanks[1];

  if ( pioinfo->size < 2 )
    {
      fprintf ( stderr, 
		"PTYPE PIO_POSIX_*  or PIO_MPI_FPGUARD should be used with at least 2 pes.\n" );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  if ( nPrefStreams < 1 )
    {
      fprintf ( stderr, 
		"PTYPE PIO_POSIX_ASYNCH should be used with at least 1 prefetch stream.\n" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  *ncollectors = pioinfo->size - 1;
  pioinfo->specialRank = pioinfo->size - 1;
  
  ioRanks[0] = pioinfo->specialRank;
  MPI_Comm_group ( pioinfo->comm, &streamGroup );
  MPI_Group_excl ( streamGroup, 1, ioRanks, &collectorGroup );
  MPI_Comm_create ( pioinfo->comm, collectorGroup, &
		    pioinfo->collectorComm );
  
  if ( pioinfo->rank == pioinfo->specialRank ) 
    {
      if ( ddebug )
	fprintf ( stdout, 
		  "### pe%d pioInit (): Call posixWriter(), ncollectors = %d ...\n", 
		  pioinfo->rank, *ncollectors );
      pwPOSIXASYNCH ( *ncollectors );
      collectingData = 0;
    }
  else
    bibAFiledataPA = queueInit ( destroyAFiledataPA, 
				 compareNamesAPA );

  return collectingData;
}

#endif
