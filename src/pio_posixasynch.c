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
#include <fcntl.h>

#include "mpi.h"
#include "pio.h"
#include "pio_impl.h"

extern bool ddebug;

extern double startTime;
extern double accumProbe;
extern double accumRecv;
extern double accumSend;
extern double accumSuspend;
extern double accumWait;
extern double accumWrite;

extern long initial_buffer_size; 

extern pioInfo *pioinfo;

typedef struct 
{
  char *name;
  size_t size;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  IO_Server_command command;
} pioOpenFileCollector;

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
} pioOpenFileWriter;

static queue *openFilesCollector;

MPI_Request request = MPI_REQUEST_NULL;

int nPrefStreams = 10;

char *token = "%";


/***************************************************************/

static pioOpenFileCollector *initOpenFileCollector ( const char *filename, size_t bs )
{
  pioOpenFileCollector *ofc;
  size_t len;
  int status;

  if ( ddebug )
    fprintf ( stdout, "pe%d initOpenFileCollector (): filename=%s, buffersize=%d, in\n",
	pioinfo->rank, filename, bs );

  ofc = ( pioOpenFileCollector * ) malloc ( sizeof ( pioOpenFileCollector ));
  memset ( ofc, 0, sizeof ( pioOpenFileCollector ));

  len = strlen ( filename );
  ofc->name = ( char * ) malloc (( len ) * sizeof ( char ));
  strcpy ( ofc->name, filename );
  ofc->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stdout, "pe%d initOpenFileCollector(): name=%s, init output buffer\n", 
	      pioinfo->rank, ofc->name );
   
  if (( status = dbuffer_init ( &( ofc->db1 ), ofc->size )) == 1 )
    {
      fprintf ( stderr, "dbuffer_init outputBuffer1: %d\n", status );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  if (( status = dbuffer_init ( &( ofc->db2 ), ofc->size )) == 1)
    {
      fprintf ( stderr, "dbuffer_init outputBuffer2: %d\n", status );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  ofc->db = ofc->db1;

  ofc->command = IO_Open_file;

  if ( ddebug )
    fprintf ( stdout, "pe%d initOpenFileCollector (), enqueued name=%s, return\n",
	pioinfo->rank, ofc->name );
  
  return ofc;
}
/***************************************************************/ 

static pioOpenFileWriter *initOpenFileWriter ( char *filename, 
					       size_t bs, int nc )
{
  pioOpenFileWriter *ofw;
  int i;

  if ( ddebug )
       fprintf ( stdout, "### pe%d initOpenFileWriter (): filename=%s, buffersize=%d, ncollectors=%d, nPrefetchStreams=%d, in\n",
	      pioinfo->rank, filename, bs, nc, nPrefStreams );

  ofw = ( pioOpenFileWriter * ) malloc ( sizeof ( pioOpenFileWriter ));
  memset ( ofw, 0, sizeof ( pioOpenFileWriter ));

  ofw->name = ( char * ) malloc (( strlen ( filename )) * sizeof ( char ));
  strcpy ( ofw->name, filename );
  ofw->size = bs;

  if (( ofw->handle = open ( ofw->name, O_CREAT | O_WRONLY, 0666 )) == -1 )
    {
      fprintf ( stderr, "Failed to open %s:\n", ofw->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  dbuffer_init ( &( ofw->fb ), ( size_t )( nPrefStreams * ofw->size ));

  if (( ofw->ctrlBlks = ( struct aiocb * ) malloc ( nPrefStreams * sizeof ( struct aiocb ))) == NULL )
    {
      fprintf ( stderr, "Cannot allocate AIO control blocks\n" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  for ( i = 0; i < nPrefStreams; i++ )
    {
      ofw->ctrlBlks[i].aio_fildes     = ofw->handle;
      ofw->ctrlBlks[i].aio_buf        = ofw->fb->buffer + ( i * ofw->size );
      ofw->ctrlBlks[i].aio_reqprio    = 0;
      ofw->ctrlBlks[i].aio_sigevent.sigev_notify = SIGEV_NONE;   
    }
  
  ofw->nextOpIndex = 0;
  ofw->prefIndex = 0; 
  ofw->offset = 0;
  ofw->finished = false;
  ofw->nfinished = ( bool * ) malloc ( nc * sizeof ( bool ));

  if ( ddebug )
    fprintf ( stdout, "### pe%d initOpenFileWriter (), name=%s, opened file, return\n",
	pioinfo->rank, ofw->name );

  return ofw;
}

/***************************************************************/

int destroyOpenFileCollector ( void *v )
{
  int iret = 0;
  pioOpenFileCollector *ofc = (pioOpenFileCollector * ) v;

  if ( ddebug )
    fprintf ( stdout, "pe%d destroyOpenFileCollector(): name=%s, cleanup, in\n", 
	      pioinfo->rank, ofc->name );
  
  dbuffer_cleanup ( &( ofc->db1 ));
  dbuffer_cleanup ( &( ofc->db2 ));

  free ( ofc->name );
  free ( ofc );

  if ( ddebug )
    fprintf ( stdout, "pe%d return from destroyOpenFileCollector(), cleaned up, return\n", 
	      pioinfo->rank );

  return iret;
}

/***************************************************************/

int destroyOpenFileWriter ( void *v )
{
  pioOpenFileWriter *ofw = (pioOpenFileWriter * ) v;
  const struct aiocb *ccBP[1];
  int iret = 0;

  if ( ddebug )
    fprintf ( stdout, "### pe%d destroyOpenFileWriter(): name=%s, cleanup and close file, in\n", 
	      pioinfo->rank, ofw->name );

  /* close file */

  while ( ofw->prefIndex > 0 ) 
    {
      ccBP[0] = ( ofw->ctrlBlks + ofw->nextOpIndex );

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
      
      if (( iret = aio_return ( ofw->ctrlBlks + ofw->nextOpIndex )) == -1 ) 
	{
	  fprintf ( stderr, "Error from AIO \n");
	  MPI_Abort ( pioinfo->comm, 1 );
	}
      ofw->nextOpIndex = ( ofw->nextOpIndex + 1 ) % nPrefStreams;
      ofw->prefIndex --;
    }

  if (( iret = close ( ofw->handle )) == -1 )
    {
      fprintf ( stderr, "Failed to close %s:\n", ofw->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  /* file closed, cleanup */

  dbuffer_cleanup ( &( ofw->fb ));

  free ( ofw->nfinished );
  free ( ofw->ctrlBlks );
  free ( ofw->name );
  free ( ofw );

  if ( ddebug )
    fprintf ( stdout, "### pe%d destroyOpenFileWriter(), closed file and cleaned up, return\n", 
	      pioinfo->rank );

  return iret;
}

/***************************************************************/

bool compareNamesDBPOSIXASYNCH  ( void *v1, void *v2 )
{
  pioOpenFileCollector *ofc1, *ofc2;
  size_t len;
  bool bret;

  ofc1 = ( pioOpenFileCollector * ) v1;
  ofc2 = ( pioOpenFileCollector * ) v2;

  len = strlen ( ofc1->name );
  bret = ( len == strlen ( ofc2->name ) && 
	  memcmp ( ofc1->name, ofc2->name, len ) == 0 );

  return bret;
}

/***************************************************************/

void writeDBPOSIXASYNCH ( pioOpenFileWriter *ofw, long amount )
{
  const struct aiocb *ccBP[1];
  int iret;

  if ( ddebug )
    fprintf ( stdout, "### pe%d writeDBPOSIXASYNCH, file %s, in\n", 
	      pioinfo->rank, ofw->name );
  
  ofw->ctrlBlks[ofw->currOpIndex].aio_nbytes = amount;
  ofw->ctrlBlks[ofw->currOpIndex].aio_offset = ofw->offset;

  if ( ddebug )
    fprintf ( stdout, "### pe%d writeDBPOSIXASYNCH before aio_write(), file %s, aio_nbytes=%d, aio_offset=%ld\n",
	      pioinfo->rank, ofw->name, ofw->ctrlBlks[ofw->currOpIndex].aio_nbytes,
	      ofw->ctrlBlks[ofw->currOpIndex].aio_offset );

  if ( ddebug )
    startTime = MPI_Wtime ();

  iret = aio_write ( ofw->ctrlBlks + ofw->currOpIndex );

  if ( ddebug )
    accumWrite += ( MPI_Wtime () - startTime);

  if ( ddebug )
    fprintf ( stdout, "### pe%d writeDBPOSIXASYNCH after aio_write(), file %s, aio_nbytes=%d, aio_offset=%ld, iret=aio_write()=%d\n",
	      pioinfo->rank, ofw->name, ofw->ctrlBlks[ofw->currOpIndex].aio_nbytes,
	      ofw->ctrlBlks[ofw->currOpIndex].aio_offset, iret );

  if ( iret == -1 )
    {
      perror ( "### Problems writing buffer" );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  else
    if ( ddebug )
      fprintf ( stdout, "### pe%d writeBuffer (), buffer written to %s\n", 
		pioinfo->rank, ofw->name );
	  
  ofw->offset += ( off_t ) amount;
  ofw->prefIndex ++;

  if ( ofw->prefIndex >= nPrefStreams ) 
    {
      ccBP[0] = ( ofw->ctrlBlks + ofw->nextOpIndex );

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
	      
      if (( iret = aio_return ( ofw->ctrlBlks + ofw->nextOpIndex )) == -1 ) 
	{
	  fprintf ( stderr, "Error from AIO \n");
	  MPI_Abort ( pioinfo->comm, 1 );
	}

      ofw->prefIndex --;
    }

  if ( ddebug )
    fprintf ( stdout, "### pe%d writeDBPOSIXASYNCH (), name=%s, prefIndex=%d, return \n",
	      pioinfo->rank, ofw->name, ofw->prefIndex );

  return;
}

/***************************************************************/

void pwPOSIXASYNCH ( int ncollectors )
{
  pioOpenFileWriter *ofw; 
  queue *openFilesWriter;
  long amount, buffersize;
  char *messageBuffer, *pMB, *filename, *temp;
  int messagesize, source, tag, i, len;
  rTag *rtag;
  MPI_Status status;

  if ( ddebug )
    fprintf(stdout, "### pe%d pwPOSIXASYNCH (), ncollectors=%d, in\n", 
	    pioinfo->rank, ncollectors);
 
  openFilesWriter = queueInit ( destroyOpenFileWriter, compareNamesDBPOSIXASYNCH );
  
  for ( ;; )
    {
      free ( messageBuffer );      
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
	  messageBuffer = ( char *) malloc ( messagesize * sizeof ( char ));
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
		      "### pe%d pwPOSIXASYNCH () case %s, filename=%s, buffersize=%d, amount=%d\n",
		      pioinfo->rank, command2charP [ rtag->command ], filename, 
		      buffersize, amount ); 

	  
	  if ( !(ofw = ( pioOpenFileWriter * ) queueIdx2val ( openFilesWriter, 
							     rtag->id )))
	    {
	      ofw = ( pioOpenFileWriter * ) initOpenFileWriter ( filename, 
								 buffersize,  
							       ncollectors );
	      queuePush ( openFilesWriter, ofw, 1, rtag->id );
	    }
	  else
	    if ((( len = strlen ( filename )) != strlen ( ofw->name )) || 
		memcmp ( filename, ofw->name, len ) != 0 )
	      {
		fprintf ( stderr, "pe%d, id=%d, filename not consistant\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }
		 

	  ofw->currOpIndex = ofw->nextOpIndex;
	  ofw->nextOpIndex = ( ofw->nextOpIndex + 1 ) % nPrefStreams;

	  memcpy ( ofw->fb->buffer + ( ofw->currOpIndex * ofw->size ), pMB, ( int ) amount );

	  writeDBPOSIXASYNCH ( ofw, amount );
	  
	  free ( messageBuffer );

	  break;

	case IO_Send_buffer:
	  
	  ofw = ( pioOpenFileWriter * ) queueIdx2val ( openFilesWriter,  
						       rtag->id );
	  amount = messagesize;

	  if ( ddebug )
	    fprintf ( stdout, "### pe%d, command: %s, id=%d, name=%s\n", 
		      pioinfo->rank, command2charP [ rtag->command ], 
		      rtag->id, ofw->name );

	  ofw->currOpIndex = ofw->nextOpIndex;
	  ofw->nextOpIndex = ( ofw->nextOpIndex + 1 ) % nPrefStreams;
	  
	  MPI_Recv (  ofw->fb->buffer + ( ofw->currOpIndex * ofw->size ), 
		      amount, MPI_CHAR, source, tag, pioinfo->comm, &status );

	  writeDBPOSIXASYNCH ( ofw, amount );
	  
	  break;

	case IO_Close_file:
	  
	  ofw = ( pioOpenFileWriter * ) queueIdx2val ( openFilesWriter,  
						       rtag->id );
	  amount = messagesize;

	  if ( ddebug )
	    fprintf ( stdout, "### pe%d, command %s, id=%d, name=%s\n", 
		      pioinfo->rank, command2charP [ rtag->command ], 
		      rtag->id, ofw->name );

	  ofw->currOpIndex = ofw->nextOpIndex;
	  ofw->nextOpIndex = ( ofw->nextOpIndex + 1 ) % nPrefStreams;
	  
	  MPI_Recv (  ofw->fb->buffer + ( ofw->currOpIndex * ofw->size ), 
		      amount, MPI_CHAR, source,  tag, pioinfo->comm, &status );

	  writeDBPOSIXASYNCH ( ofw, amount );

	  ofw->nfinished[source] = true;	  
	  ofw->finished          = true;
	  
	  for ( i = 0; i < ncollectors; i++ )
	    if ( !( ofw->nfinished[i] ))
	      {
		ofw->finished = false;
		break;
	      }
	  
	  if ( ofw->finished )
	    {
	      queueDelNode ( openFilesWriter, rtag->id );
	      if ( !openFilesWriter->head )
		{
		  queueDestroy ( openFilesWriter );
		  return;
		}
	    }	      
	}
    }
}


/***************************************************************/

void sendDBPOSIXASYNCH ( pioOpenFileCollector *ofc, int id )
{
  int iret, tag;
  long amount, written;
  MPI_Offset displ;
  MPI_Status status;

  amount = ( long ) dbuffer_data_size ( ofc->db );
  tag = setTag ( id, ofc->command );

  if ( ddebug ) 
    fprintf ( stdout,
	      "pe%d in sendDBPOSIXASYNCH (): send buffer for %s, size: %zu bytes, command=%s, in\n", 
	      pioinfo->rank, ofc->name, amount, 
	      command2charP [ ofc->command ] );
  if ( ddebug )
    startTime = MPI_Wtime ();

  MPI_Wait (&request, &status);

  if ( ddebug )
    accumWait +=  ( MPI_Wtime () - startTime );

  MPI_Isend ( ofc->db->buffer, amount, MPI_CHAR, pioinfo->specialRank, 
	      tag, pioinfo->comm, &request ); 

  /* change outputBuffer */
  
  dbuffer_reset ( ofc->db );
  
  if ( ofc->db == ofc->db1 )
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in sendDBPOSIXASYNCH (): Change to buffer 2 ...\n", 
		  pioinfo->rank );
      ofc->db =  ofc->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in sendDBPOSIXASYNCH (): Change to buffer 1 ...\n", 
		  pioinfo->rank );
      ofc->db =  ofc->db1;
    }
  
  ofc->command = IO_Send_buffer;
  
  return;
} 

/***************************************************************/

size_t fwPOSIXASYNCH( int id, int tsId, const void *buffer, size_t len )
{
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  pioOpenFileCollector *ofc;
  node *curr;

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, tsId );

      curr = openFilesCollector->head;

      while ( curr ) 
	{
	  sendDBPOSIXASYNCH (( pioOpenFileCollector *) curr->val, curr->idx );
	  curr = curr->next;
	}

      oldTsId = tsId;
      MPI_Barrier ( pioinfo->collectorComm ); 
    }                                          

  ofc = ( pioOpenFileCollector * ) queueIdx2val ( openFilesCollector, id );
  filled = dbuffer_push ( ofc->db, ( unsigned char * ) buffer, len );

  if ( ddebug )
    fprintf ( stdout, "pe%d fileWrite (): id = %d, tsId = %d, pushed %d byte data on buffer, filled = %d\n", 
	      pioinfo->rank, id, tsId, len, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  sendDBPOSIXASYNCH ( ofc, id );
     
	  error = dbuffer_push ( ofc->db, ( unsigned char * )buffer, len );
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
  pioOpenFileCollector *ofc;
  int iret;

  if ( ddebug )
    fprintf ( stdout, "pe%d fileClose (%id ): send buffer, close file and cleanup\n", 
	      pioinfo->rank, id );

  ofc = ( pioOpenFileCollector * ) queueIdx2val ( openFilesCollector, id );

  ofc->command = IO_Close_file;

  sendDBPOSIXASYNCH ( ofc, id );

  /* dequeue file element */

  iret = queueDelNode ( openFilesCollector, id );

  if ( !openFilesCollector->head ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      queueDestroy ( openFilesCollector );
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
  int status;
  int iret, id, len, messageLength = 32;
  char message[messageLength];
  int bcastRank; 
  MPI_Comm bcastComm;
  static pioOpenFileCollector *ofc;
  static long buffersize = 0;

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
	  if ( buffersize < initial_buffer_size )
	    buffersize = initial_buffer_size;
	}
      
      MPI_Bcast ( &buffersize, 1, MPI_LONG, 0, bcastComm );
    }

  ofc = initOpenFileCollector ( filename, buffersize );
  queuePush ( openFilesCollector, ofc, 0 );
  id = openFilesCollector->tail->idx;

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d fileOpenW(): name=%s, init and enqueued pioOpenFileCollector, return id = %d\n", 
	      pioinfo->rank, filename, id );

  /* put filename, id and buffersize on buffer */ 

  iret = dbuffer_push ( ofc->db, ( unsigned char *) filename, strlen ( filename ));
  iret = dbuffer_push ( ofc->db,  ( unsigned char *) token, 1);  
  sprintf ( message,"%lX", buffersize); 
  iret = dbuffer_push ( ofc->db,  ( unsigned char *) message, strlen ( message ));
  iret = dbuffer_push ( ofc->db,  ( unsigned char *) token, 1); 
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d fileOpenW(): name=%s, put Open file message on buffer:\n%s\n return \n", 
	      pioinfo->rank, filename, ofc->db->buffer ); 
  
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
    openFilesCollector = queueInit ( destroyOpenFileCollector, 
				     compareNamesDBPOSIXASYNCH );

  return collectingData;
}

#endif
