/* 
   error with 1 node and 32 tasks: invalid communicator
              2 nodes and 16 tasks: -"-

	      ( when 99% of the data is written, pioinfo->color of the guard 
	      thread is set to 0 while calling MPI_Probe())

   todo 
   build in control, for consistance of pairs filename / filenumber 
   ( pioOpenFile member name, recv in tmpbuffer, if(!uniqueName(q,v,n))abort )

   change condition to refuse of message ( without hard coding )
*/

#ifndef NOMPI

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "mpi.h"
#include "pio.h"
#include "pio_impl.h"

int maxNFiles = 100;
extern int tagKey;

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

typedef struct 
{
  char *name;
  size_t size;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  int fd;
  IO_Server_command command;
} aFiledataPFTR;

typedef struct 
{
  long offset;
  bool finished;
  bool *nfinished;
} bFiledataPFTR;

static queue *bibAFiledataPFTR;

pthread_t thread;
pthread_attr_t attr;

/***************************************************************/
  
static aFiledataPFTR *initAFiledataPFTR( const char *key, size_t bs)
{
  aFiledataPFTR *afd;
  size_t len;
  int status;

  afd = ( aFiledataPFTR * ) malloc ( sizeof ( aFiledataPFTR ));
  memset ( afd, 0, sizeof ( aFiledataPFTR ));
  len = strlen ( key );
  afd->name = ( char * ) malloc (( len ) * sizeof ( char ));
  strcpy ( afd->name, key );
  afd->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d in initAFiledataPF(): name=%s, init output buffer\n", 
	      pioinfo->rank, pioinfo->color, afd->name );
   
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

  /* open file */ 
  
  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d in initAFiledataPF(): name=%s, open file\n", 
	      pioinfo->rank, pioinfo->color, afd->name );

  if ( ( afd->fd = open ( afd->name, O_CREAT | O_WRONLY, 0666 )) == -1 ) 
    {
      fprintf ( stderr, "Failed to open %s:\n", afd->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  
  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d in initAFiledataPF() has opened file %s\n", 
	      pioinfo->rank, pioinfo->color, afd->name );

  afd->command = IO_Open_file;
  
  return afd;
}

/***************************************************************/

bFiledataPFTR * initBFiledataPFTR ( int key, int nc )
{
  bFiledataPFTR *bfd;
  int i;

  bfd = ( bFiledataPFTR * ) malloc ( sizeof ( bFiledataPFTR ));
  memset ( bfd, 0, sizeof ( bFiledataPFTR ));
  bfd->offset = 0;
  bfd->finished = false;
  bfd->nfinished = ( bool * ) malloc ( nc * sizeof ( bool ));

  for ( i = 0; i < nc; i++ )
    *( bfd->nfinished + i ) = true;

  return bfd;
}

/***************************************************************/

int destroyAFiledataPFTR ( void *v )
{
  int iret = 0;
  aFiledataPFTR *afd = ( aFiledataPFTR * ) v;
  
  /* close file */
  
  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d in destroyAFiledataPFTR(): name=%s, close file\n", 
	      pioinfo->rank, pioinfo->color, afd->name );
       
  if (( iret = close ( afd->fd )) == -1 )
    {
      fprintf ( stderr, "Failed to close file %s:\n", afd->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }  

  /* file closed, cleanup */

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d in destroyAFiledataPFTR(): name=%s, file closed, cleanup ...\n", 
	      pioinfo->rank, pioinfo->color, afd->name );
  
  dbuffer_cleanup ( &( afd->db1 ));
  dbuffer_cleanup ( &( afd->db2 ));

  free ( afd->name );
  free ( afd );

  return iret;
}

/***************************************************************/

int destroyBFiledataPFTR ( void *v )
{
  int iret = 0;
  bFiledataPFTR *bfd = (bFiledataPFTR * ) v;
  
  free ( bfd->nfinished );
  free ( bfd );

  return iret;
}

/***************************************************************/

bool compareNamesAPFTR ( void *v1, void *v2 )
{
  aFiledataPFTR *afd1, *afd2;
  size_t len;
  bool ret;

  afd1 = ( aFiledataPFTR * ) v1;
  afd2 = ( aFiledataPFTR * ) v2;

  len = strlen ( afd1->name );
  ret = ( len == strlen ( afd2->name ) && 
	  memcmp ( afd1->name, afd2->name, len ) == 0 );

  return ret;
}

/***************************************************************/

void* fpgPOSIXFPGUARDTHREADREFUSE ( void* ncoll )
{
  int i, source, iret;
  rTag *rtag;
  MPI_Status status;
  bFiledataPFTR *bfd; 
  queue *bibBFiledataPFTR;
  long amount;

  int ncollectors;

  ncollectors = *(( int *) ncoll );

  if ( ddebug )
    fprintf(stdout, "#### pe%d, color=%d,  thread, in fpGuard(), ncollectors=%d\n", 
	    pioinfo->rank, pioinfo->color,  ncollectors);
 
  bibBFiledataPFTR = queueInit ( destroyBFiledataPFTR, NULL );
  
  for ( ;; )
    {

      fprintf ( stdout, "#### pe%d, color=%d, before probe\n", pioinfo->comm, pioinfo->color );

      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, pioinfo->comm, &status );

      fprintf ( stdout, "#### pe%d, color=%d, after probe\n", pioinfo->comm, pioinfo->color );

      source = status.MPI_SOURCE;
      rtag   = getTag ( status.MPI_TAG ); 
      if ( rtag->id >= maxNFiles ) continue;

      if ( ddebug )
	fprintf ( stdout, 
		  "#### pe%d, color=%d, in fpGuard (): receive message from source=%d, id=%d, command=%d ( %s )\n",
		  pioinfo->rank, pioinfo->color, source, rtag->id, rtag->command, 
		  command2charP [ rtag->command ]);
      
      switch ( rtag->command )
      	{
      	case IO_Open_file:

	  if ( !(bfd = ( bFiledataPFTR * ) queueIdx2val ( bibBFiledataPFTR, 
							     rtag->id )))
	    {
	      bfd = ( bFiledataPFTR * ) initBFiledataPFTR ( rtag->id, 
							       ncollectors );

	      if (( iret = queuePush ( bibBFiledataPFTR, bfd, 1, rtag->id )) != rtag->id )
	      {
		fprintf ( stderr, "pe%d, color%d, fileID=%d not unique\n",
			  pioinfo->rank, pioinfo->color, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      } 
	    }

	  *( bfd->nfinished + source ) = false;
	  
	  if ( ddebug )
	    fprintf ( stdout, "#### pe%d, color=%d, in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, pioinfo->color,  rtag->id, rtag->command, 
		      command2charP [ rtag->command ], bfd->offset );
	 
	  MPI_Recv (  &amount, 1, MPI_LONG, source,  status.MPI_TAG, pioinfo->comm, &status );
	  MPI_Send (  &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG + maxNFiles * tagKey , pioinfo->comm );

	  bfd->offset += amount; 

	  if ( ddebug )
	    fprintf ( stdout, 
		      "#### pe%d, color=%d, in fpguard(), id=%d, command=%d ( %s ), received amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, pioinfo->color,  rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Set_fp:
	  
	  if ( ! ( bfd = ( bFiledataPFTR * ) queueIdx2val ( bibBFiledataPFTR,  
							  rtag->id )))
	      {
		fprintf ( stderr, "pe%d, color%d, id=%d not in file queue\n",
			  pioinfo->rank, pioinfo->color, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }

	  if ( ddebug )
	    fprintf ( stdout, "#### pe%d, color=%d, in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, pioinfo->color,  rtag->id, rtag->command, 
		      command2charP [ rtag->command ], bfd->offset );

	  MPI_Recv (  &amount, 1, MPI_LONG, source,  status.MPI_TAG, pioinfo->comm, &status );
	  MPI_Send (  &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG + maxNFiles * tagKey , pioinfo->comm );

	  bfd->offset += amount;

	  if ( ddebug )
	    fprintf ( stdout, 
		      "#### pe%d, color=%d, in fpguard(), id=%d, command=%d ( %s ), received amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, pioinfo->color,  rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Close_file:
	  
	  if ( ! ( bfd = ( bFiledataPFTR * ) queueIdx2val ( bibBFiledataPFTR,  
							  rtag->id )))
	      {
		fprintf ( stderr, "pe%d, color%d, id=%d not in file queue\n",
			  pioinfo->rank, pioinfo->color, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }

	  if ( ddebug )
	    fprintf ( stdout, 
		      "#### pe%d, color=%d, in fpGuard(): id=%d, command=%d ( %s )), send offset=%ld\n", 
		      pioinfo->rank, pioinfo->color,  rtag->id, rtag->command,
		      command2charP [ rtag->command ], bfd->offset );

	  MPI_Recv (  &amount, 1, MPI_LONG, source,  status.MPI_TAG, pioinfo->comm, &status );

	  if ( ddebug )
	    fprintf ( stdout, "####### pe%d, color=%d, received amount, send offset\n", 
		      pioinfo->rank, pioinfo->color  );

	  MPI_Send (  &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG + maxNFiles * tagKey , pioinfo->comm );

	  bfd->offset += amount;

	  if ( ddebug )
	    fprintf ( stdout, 
		      "#### pe%d, color=%d, in fpguard(), id=%d, command=%d ( %s ), received amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, pioinfo->color,  rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, bfd->offset );

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
	      queueDelNode ( bibBFiledataPFTR, rtag->id );
	      if ( !bibBFiledataPFTR->head )
		{
		  queueDestroy ( bibBFiledataPFTR );
		  if ( ddebug )
		    fprintf ( stdout, "#### pe%d, color%d, returns\n", 
			      pioinfo->rank, pioinfo->color );
		  return;
		}
	    }	      
	}
    }
}   

/*******************************************************/
 
void writePFTR ( aFiledataPFTR *afd, int id )
{
  long amountC = 0L, written;
  long offsetC = 0L, error;
  int tagC;
  MPI_Status statusC;
  
  /* send buffersize, recv offsetC */

  amountC = dbuffer_data_size ( afd->db );
  tagC = setTag ( id, afd->command );

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 1 a, tagC=%d, amountC=%d\n", 
	      pioinfo->rank, pioinfo->color, tagC, amountC );

  MPI_Send (  &amountC, 1, MPI_LONG, pioinfo->specialRank, tagC, pioinfo->comm );

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d, color%d in writePFTR (), id=%d, command=%d, send amountC=%zu\n", 
	      pioinfo->rank, pioinfo->color, id, afd->command, amountC ); 

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 1 b, tagC+maxNFiles * tagKey=%d\n", 
	      pioinfo->rank, pioinfo->color, tagC+maxNFiles * tagKey );

  MPI_Recv (  &offsetC, 1, MPI_LONG, pioinfo->specialRank, tagC + maxNFiles * tagKey, 
	      pioinfo->comm, &statusC ); 

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 1 c\n", 
	      pioinfo->rank, pioinfo->color );
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d, color%d in writePFTR (), id=%d, command=%d, send amountC=%zu, received offsetC=%ld\n", 
	      pioinfo->rank, pioinfo->color, id, afd->command, amountC, offsetC );      
  
  /* write buffer */
  
  if (( error = lseek ( afd->fd, offsetC, SEEK_SET )) != offsetC )
    {
      fprintf ( stdout, "pe%d, color%d error=%Ld\n", error );
      perror ( "Error seeking fd" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 1 d\n", 
	      pioinfo->rank, pioinfo->color );	  

  if (( written = 
	write ( afd->fd, afd->db->buffer, amountC )) !=
      amountC )
    {
      fprintf ( stdout, 
		"pe%d, color%d, id=%d, expect to write %16d byte, written %16d byte.\n", 
		pioinfo->rank, pioinfo->color, id, amountC, written );
      perror ( "Error writing data" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d, color%d in writePFTR (), written %d bytes in file %d with offsetC %ld\n", 
	      pioinfo->rank, pioinfo->color, written, id, offsetC ); 
  
  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
  
  if ( afd->db == afd->db1 )
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d, color%d in writePFTR (): id=%d, change to buffer 2 ...\n", 
		  pioinfo->rank, pioinfo->color, id );
      afd->db =  afd->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d, color%d in writePFTR (): id=%d, change to buffer 1 ...\n", 
		  pioinfo->rank, pioinfo->color, id );
      afd->db =  afd->db1;
    }
  
  afd->command = IO_Set_fp;
}


/***************************************************************/

size_t fwPOSIXFPGUARDTHREADREFUSE ( int id, int tsId, const void *buffer, size_t len )
{
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataPFTR *afd;
  node *curr;

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d, color%d in fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, pioinfo->color, tsId );
      curr = bibAFiledataPFTR->head;
      while ( curr ) 
	{
	  writePFTR (( aFiledataPFTR *) curr->val, curr->idx );
	  curr = curr->next;
	}
      oldTsId = tsId;
      MPI_Barrier ( pioinfo->collectorComm ); 
    }                                          

  afd = ( aFiledataPFTR * ) queueIdx2val ( bibAFiledataPFTR, id );
  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d, color%d in fileWriter (): id = %d, tsId = %d, pushed data on buffer, filled = %d\n", 
	      pioinfo->rank, pioinfo->color, id, tsId, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  writePFTR ( afd, id );
     
	  error = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );
	}
    }
  
  if ( error == 1 )
    {
      fprintf ( stderr, "pe%d, color%d, id=%d, problem filling output buffer ...\n",
		pioinfo->rank, pioinfo->color, id);
      MPI_Abort ( pioinfo->comm, 1 );
    }
  
  return len;
}

/***************************************************************/

int fcPOSIXFPGUARDTHREADREFUSE ( int id )
{
  aFiledataPFTR *afd;
  int iret;

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d in fileClose (%d): write buffer, close file and cleanup\n", 
	      pioinfo->rank, pioinfo->color, id );

  afd = ( aFiledataPFTR * ) queueIdx2val ( bibAFiledataPFTR, id );

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 1\n", 
	      pioinfo->rank, pioinfo->color );

  afd->command = IO_Close_file;

  writePFTR ( afd, id );

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 2\n", 
	      pioinfo->rank, pioinfo->color );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataPFTR, id );

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 3\n", 
	      pioinfo->rank, pioinfo->color );

  if ( !bibAFiledataPFTR->head ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d, color%d in fileClose (): cleanup queue \n", 
		  pioinfo->rank, pioinfo->color );
      queueDestroy ( bibAFiledataPFTR );
    }

  if ( ddebug )
    fprintf ( stdout, "pe%d, color%d hallo 4\n", 
	      pioinfo->rank, pioinfo->color );

  return iret;
}

/***************************************************************/

int fowPOSIXFPGUARDTHREADREFUSE ( const char *filename )
{
  int bcastRank, id; 
  MPI_Comm bcastComm;
  static aFiledataPFTR *afd;
  static bool firstOpen = true;
  static long buffersize = 0;

  /* broadcast buffersize to collectors */
  
  if ( firstOpen )
    {
      if ( ddebug && pioinfo->rank == 0)
	fprintf ( stdout, 
		  "pe%d, color%d in pioFileOpenW(): name=%s, broadcast buffersize to collectors ...\n", 
		  pioinfo->rank, pioinfo->color, filename );
      
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
  
        firstOpen = false;
      }

  /* init and enqueue file element */ 

  afd = initAFiledataPFTR( filename, buffersize );
  if (( id = queuePush ( bibAFiledataPFTR, afd, 0 )) < 0 )
    {   
      fprintf ( stderr, "Filename %s not unique\n", afd->name );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d, color%d in pioFileOpenW(): name=%s, init and enqueued aFiledataPFTR, return id = %d\n", 
	      pioinfo->rank, pioinfo->color, filename, id );
  
  return id;
}

/***************************************************************/

int initPOSIXFPGUARDTHREADREFUSE ( int *ncollectors )
{
  int collectingData = 1;
  MPI_Group streamGroup, collectorGroup;
  int ioRanks[1];
  int rc = 0;

  if ( pioinfo->size < 2 )
    {
      fprintf ( stderr, 
		"PTYPE PIO_POSIX_B  or PIO_MPI_FPGUARD should be used with at least 2 pes.\n" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in initPOSIXFPGUARDSENDREC()\n", pioinfo->rank );

  *ncollectors = pioinfo->size;
  pioinfo->specialRank = pioinfo->size - 1;

  pioinfo->collectorComm = pioinfo->comm;


  if ( pioinfo->rank == pioinfo->specialRank ) 
    {
      if ( ddebug )
	fprintf ( stdout, 
		  "pe%d in initPOSIXFPGUARDTHREAD(): create thread with fpGuard(), ncollectors = %d ...\n", 
		  pioinfo->rank, *ncollectors );
      pthread_attr_init(&attr );
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

      rc = pthread_create ( &thread, NULL, &fpgPOSIXFPGUARDTHREADREFUSE, 
			    ( void *) ncollectors );
      if (rc)
	{
	  fprintf ( stderr, "ERROR; return code from pthread_create() is %d\n", rc);
	  MPI_Abort ( pioinfo->comm, 1 );
	}
    }

  bibAFiledataPFTR = queueInit ( destroyAFiledataPFTR, compareNamesAPFTR );  

  return collectingData;
}


#endif
