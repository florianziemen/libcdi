/* 
   todo 
   does mpi_barrier work with threads?
   mutexes for each filedatas offset
   mutex for queue
*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

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
#include "pio_util.h"

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
} aFiledataPFT;

typedef struct 
{
  long offset;
  bool finished;
  bool *nfinished;
} bFiledataPFT;

static queue *bibAFiledataPFT;
queue *bibBFiledataPF;

pthread_t thread;
pthread_attr_t attr;
pthread_mutex_t offsetMutex;

/***************************************************************/
  
static aFiledataPFT *initAFiledataPFT( const char *key, size_t bs)
{
  aFiledataPFT *afd;
  size_t len;
  int status;

  afd = ( aFiledataPFT * ) xmalloc ( sizeof ( aFiledataPFT ));
  memset ( afd, 0, sizeof ( aFiledataPFT ));
  len = strlen ( key );
  afd->name = ( char * ) xmalloc (( len ) * sizeof ( char ));
  strcpy ( afd->name, key );
  afd->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in initAFiledataPF(): name=%s, init output buffer\n", 
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

  /* open file */ 
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in initAFiledataPF(): name=%s, open file\n", 
	      pioinfo->rank, afd->name );

  if ( ( afd->fd = open ( afd->name, O_CREAT | O_WRONLY, 0666 )) == -1 ) 
    {
      fprintf ( stderr, "Failed to open %s:\n", afd->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in initAFiledataPF() has opened file %s\n", 
	      pioinfo->rank, afd->name );

  afd->command = IO_Open_file;
  
  return afd;
}

/***************************************************************/

bFiledataPFT * initBFiledataPFT ( int key, int nc )
{
  bFiledataPFT *bfd;
  int i;

  bfd = ( bFiledataPFT * ) xmalloc ( sizeof ( bFiledataPFT ));
  memset ( bfd, 0, sizeof ( bFiledataPFT ));
  bfd->offset = 0;
  bfd->finished = false;
  bfd->nfinished = ( bool * ) xmalloc ( nc * sizeof ( bool ));

  for ( i = 0; i < nc; i++ )
    *( bfd->nfinished + i ) = true;

  return bfd;
}

/***************************************************************/

int destroyAFiledataPFT ( void *v )
{
  int iret = 0;
  aFiledataPFT *afd = ( aFiledataPFT * ) v;
  
  /* close file */
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in destroyAFiledataPFT(): name=%s, close file\n", 
	      pioinfo->rank, afd->name );
       
  if (( iret = close ( afd->fd )) == -1 )
    {
      fprintf ( stderr, "Failed to close file %s:\n", afd->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }  

  /* file closed, cleanup */

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in destroyAFiledataPFT(): name=%s, file closed, cleanup ...\n", 
	      pioinfo->rank, afd->name );
  
  dbuffer_cleanup ( &( afd->db1 ));
  dbuffer_cleanup ( &( afd->db2 ));

  free ( afd->name );
  free ( afd );

  return iret;
}

/***************************************************************/

int destroyBFiledataPFT ( void *v )
{
  int iret = 0;
  bFiledataPFT *bfd = (bFiledataPFT * ) v;
  
  free ( bfd->nfinished );
  free ( bfd );

  return iret;
}

/***************************************************************/

bool compareNamesAPFT ( void *v1, void *v2 )
{
  aFiledataPFT *afd1, *afd2;
  size_t len;
  bool ret;

  afd1 = ( aFiledataPFT * ) v1;
  afd2 = ( aFiledataPFT * ) v2;

  len = strlen ( afd1->name );
  ret = ( len == strlen ( afd2->name ) && 
	  memcmp ( afd1->name, afd2->name, len ) == 0 );

  return ret;
}

/***************************************************************/

long rmwOffset ( long *bfdOffset, long amount, long *offsetNew, int tid )
{
  long offsetOld;

  if ( ddebug )
    {
      if ( tid )
        fprintf ( stdout, "#### pe%d, thread, in rmwOffset, amount=%ld\n",
                  pioinfo->rank, amount );
      else
        fprintf ( stdout, "pe%d in rmwOffset, amount=%ld\n",
                  pioinfo->rank, amount );
    }

  pthread_mutex_lock ( &offsetMutex );
  offsetOld = *bfdOffset;
  *bfdOffset += amount;
  *offsetNew = *bfdOffset;
  pthread_mutex_unlock ( &offsetMutex );

  if ( ddebug )
    {
      if ( tid )
        fprintf ( stdout,
                  "#### pe%d, thread, in rmwOffset, offsetOld=%ld,"
                  " offsetNew=%ld\n",
                  pioinfo->rank, offsetOld, *offsetNew );
      else
        fprintf ( stdout,
                  "pe%d in rmwOffset, offsetOld=%ld, offsetNew=%ld\n",
                  pioinfo->rank, offsetOld, *offsetNew );
    }

  return offsetOld;
}

/***************************************************************/

void communicateOffset ( MPI_Status *statusF, long *bfdOffset )
{
  long amount, offsetOld, offsetNew;
  int source, tag;
  rTag *rtagF;

  source = statusF->MPI_SOURCE;
  tag = statusF->MPI_TAG;
  rtagF = getTag ( tag );
  
  MPI_Recv (  &amount, 1, MPI_LONG, source,  tag, pioinfo->comm, statusF );
	  
  if ( ddebug )
    fprintf ( stdout, 
	      "#### pe%d, thread, in communicateOffset(), id=%d "
	      "command=%d ( %s ), received amount=%ld from pe%d\n", 
	      pioinfo->rank, rtagF->id, rtagF->command, 
	      command2charP [ rtagF->command ], amount, source );

  offsetOld = rmwOffset ( bfdOffset, amount, &offsetNew, 1 );
  
  MPI_Send (  &offsetOld, 1, MPI_LONG, source, tag, pioinfo->comm );

  if ( ddebug )
    fprintf ( stdout, 
	      "#### pe%d, thread, in communicateOffset(), id=%d, "
	      "command=%d ( %s ), sent offset=%ld to pe%d, set offset=%ld\n", 
	      pioinfo->rank, rtagF->id, rtagF->command, 
	      command2charP [ rtagF->command ], offsetOld,source, offsetNew );
  
  return;
}

/***************************************************************/

void* fpgPOSIXFPGUARDTHREAD ( void* ncoll )
{
  int i, source, iret;
  rTag *rtag;
  MPI_Status status;
  bFiledataPFT *bfd; 

  int ncollectors;

  ncollectors = *(( int *) ncoll );

  if ( ddebug )
    fprintf(stdout, "#### pe%d, thread, in fpGuard(), ncollectors=%d\n", 
	    pioinfo->rank, ncollectors);
 
  bibBFiledataPF = queueInit ( destroyBFiledataPFT, NULL );
  
  for ( ;; )
    {
      if ( ddebug )
	fprintf ( stdout, "#### pe%d, thread, in fpGuard () before MPI_Probe () \n",
		  pioinfo->rank );
      
      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, pioinfo->comm, &status );
      source = status.MPI_SOURCE;
      rtag   = getTag ( status.MPI_TAG ); 

      if ( ddebug )
	fprintf ( stdout, 
		  "#### pe%d, thread, in fpGuard (): "
		  "before receive message from source=%d, id=%d, command=%d ( %s )\n",
		  pioinfo->rank, source, rtag->id, rtag->command, 
		  command2charP [ rtag->command ]);
      
      switch ( rtag->command )
      	{
      	case IO_Open_file:

	  if ( !(bfd = ( bFiledataPFT * ) queueIdx2val ( bibBFiledataPF, 
							     rtag->id )))
	    {
	      bfd = ( bFiledataPFT * ) initBFiledataPFT ( rtag->id, 
							       ncollectors );

	      if (( iret = queuePush ( bibBFiledataPF, bfd, 1, rtag->id )) != rtag->id )
	      {
		fprintf ( stderr, "#### pe%d, thread, in fpGuard (): "
			  "fileID=%d not unique\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      } 
	    }

	  *( bfd->nfinished + source ) = false;

	  communicateOffset ( &status, &( bfd->offset ));

	  break;

	case IO_Set_fp:
	  
	  if ( ! ( bfd = ( bFiledataPFT * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		fprintf ( stderr, "#### pe%d, thread, in fpGuard: "
			  "id=%d not in file queue\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }

	  communicateOffset ( &status, &( bfd->offset ));

	  break;

	case IO_Close_file:
	  
	  if ( ! ( bfd = ( bFiledataPFT * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		fprintf ( stderr, "#### pe%d, thread, in fpGuard: "
			  "id=%d not in file queue\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }

	  communicateOffset ( &status, &( bfd->offset ));

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
	      queueDelNode ( bibBFiledataPF, rtag->id );
	      if ( !bibBFiledataPF->head )
		{
		  queueDestroy ( bibBFiledataPF );
		  return 0;
		}
	    }

	  break;

	case IO_Finalize:

	  return 0;

	  break;
	}
    }
}

//*******************************************************/   
      
long bibBFiledataPFAccess ( aFiledataPFT* afd, int id )
{ 
  int i, iret;
  bFiledataPFT *bfd; 
  long amount, offsetF = 0, offsetNew;

  int ncollectors;

  ncollectors = pioinfo->size;
  amount = dbuffer_data_size ( afd->db );

  if ( ddebug )
    fprintf ( stdout,
	      "pe%d in bibBFiledataPFAccess(), id=%d, amount=%ld, "
	      "command=%d(%s)\n",
	      pioinfo->rank, id, amount, afd->command,
	      command2charP [ afd->command ]);
	      

  switch ( afd->command )
    {
    case IO_Open_file:
      
      if ( !(bfd = ( bFiledataPFT * ) queueIdx2val ( bibBFiledataPF, 
						     id )))
	{
	  bfd = ( bFiledataPFT * ) initBFiledataPFT ( id, 
						      ncollectors );
	  
	  if (( iret = queuePush ( bibBFiledataPF, bfd, 1, id )) != id )
	    {
	      fprintf ( stderr, "pe%d fileID=%d not unique\n",
			pioinfo->rank, id);
	      MPI_Abort ( pioinfo->comm, 1 );
	    } 
	}
      
      *( bfd->nfinished + pioinfo->rank ) = false;
      
      offsetF =  rmwOffset ( &( bfd->offset ), amount, &offsetNew, 0 );
      
      break;
      
    case IO_Set_fp:
      
      if ( ! ( bfd = ( bFiledataPFT * ) queueIdx2val ( bibBFiledataPF,  
						       id )))
	{
	  fprintf ( stderr, "pe%d id=%d not in file queue\n",
		    pioinfo->rank, id);
	  MPI_Abort ( pioinfo->comm, 1 );
	}
      
      offsetF =  rmwOffset ( &( bfd->offset ), amount, &offsetNew, 0 );
      
      break;
      
    case IO_Close_file:
      
      if ( ! ( bfd = ( bFiledataPFT * ) queueIdx2val ( bibBFiledataPF,  
						       id )))
	{
	  fprintf ( stderr, "pe%d id=%d not in file queue\n",
		    pioinfo->rank, id);
	  MPI_Abort ( pioinfo->comm, 1 );
	}
      
      offsetF =  rmwOffset ( &( bfd->offset ), amount, &offsetNew, 0 );
      
      bfd->nfinished[pioinfo->rank] = true;	  
      bfd->finished          = true;
      
      for ( i = 0; i < ncollectors; i++ )
	if ( !( bfd->nfinished[i] ))
	  {
	    bfd->finished = false;
	    break;
	  }
      
      if ( bfd->finished )
	{
	  queueDelNode ( bibBFiledataPF, id );
	  if ( !bibBFiledataPF->head )
	    {
	      queueDestroy ( bibBFiledataPF );
	      MPI_Send (  &amount, 1, MPI_LONG, pioinfo->specialRank,
			  IO_Finalize, pioinfo->comm );
	    }
	}	      
    }
  return offsetF;
}

//*******************************************************/

void writePFT ( aFiledataPFT *afd, int id )
{
  long amountC = 0L, written;
  long offsetC = 0L, error;
  int tagC;
  MPI_Status statusC;
  
  /* send buffersize, recv offsetC */

  amountC = dbuffer_data_size ( afd->db );
  tagC = setTag ( id, afd->command );

  if ( pioinfo->rank == pioinfo->specialRank )
    {
      offsetC = bibBFiledataPFAccess ( afd, id );
    }
  else
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in writePFT before send amount=%ld for id%d\n",
		  pioinfo->rank, amountC, id );

      MPI_Send (  &amountC, 1, MPI_LONG, pioinfo->specialRank, tagC, 
		  pioinfo->comm ); 
      if ( ddebug )
	fprintf ( stdout, "pe%d in writePFT before recv offset\n",
		  pioinfo->rank );

      MPI_Recv (  &offsetC, 1, MPI_LONG, pioinfo->specialRank, tagC, 
		  pioinfo->comm, &statusC );
      if ( ddebug )
	fprintf ( stdout, 
		  "pe%d in writePFT (), id=%d, command=%d, send amountC=%ld, "
		  "received offsetC=%ld\n", 
		  pioinfo->rank, id, afd->command, amountC, offsetC );      
    }

  /* write buffer */
  
  if (( error = lseek ( afd->fd, offsetC, SEEK_SET )) != offsetC )
    {
      fprintf ( stderr, "pe%d error=%ld\n", 
		pioinfo->rank, error );
      perror ( "Error seeking fd" );
      MPI_Abort ( pioinfo->comm, 1 );
    }	  

  if (( written = 
	write ( afd->fd, afd->db->buffer, amountC )) !=
      amountC )
    {
      fprintf ( stderr, 
		"pe%d id=%d, expect to write %ld byte, written %ld byte.\n", 
		pioinfo->rank, id, amountC, written );
      perror ( "Error writing data" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in writePFT (), written %ld bytes in file %d with offsetC %ld\n", 
	      pioinfo->rank, written, id, offsetC ); 
  
  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
  
  if ( afd->db == afd->db1 )
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in writePFT (): id=%d, change to buffer 2 ...\n", 
		  pioinfo->rank, id );
      afd->db =  afd->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in writePFT (): id=%d, change to buffer 1 ...\n", 
		  pioinfo->rank, id );
      afd->db =  afd->db1;
    }
  
  afd->command = IO_Set_fp;
}


/***************************************************************/

size_t fwPOSIXFPGUARDTHREAD( int id, int tsId, const void *buffer, size_t len )
{
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataPFT *afd;
  node *curr;

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, tsId );

      curr = bibAFiledataPFT->head;
      while ( curr ) 
	{
	  writePFT (( aFiledataPFT *) curr->val, curr->idx );
	  curr = curr->next;
	}
      oldTsId = tsId;

      if ( ddebug )
	fprintf ( stdout, "pe%d in fileWriter (): tsId = %d, before barrier\n", 
		  pioinfo->rank, tsId );

      MPI_Barrier ( pioinfo->collectorComm ); 
    }                                          

  afd = ( aFiledataPFT * ) queueIdx2val ( bibAFiledataPFT, id );
  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in fileWriter (): id = %d, tsId = %d, "
	      "pushed data on buffer, filled = %d\n", 
	      pioinfo->rank, id, tsId, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  writePFT ( afd, id );
     
	  error = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );
	}
    }
  
  if ( error == 1 )
    {
      fprintf ( stderr, "pe%d id=%d, problem filling output buffer ...\n",
		pioinfo->rank, id);
      MPI_Abort ( pioinfo->comm, 1 );
    }
  
  return len;
}

/***************************************************************/

int fcPOSIXFPGUARDTHREAD ( int id )
{
  aFiledataPFT *afd;
  int iret;

  if ( ddebug )
    fprintf ( stdout, "pe%d in fileClose (%d): write buffer, close file and cleanup\n", 
	      pioinfo->rank, id );

  afd = ( aFiledataPFT * ) queueIdx2val ( bibAFiledataPFT, id );

  afd->command = IO_Close_file;

  writePFT ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataPFT, id );

  if ( !bibAFiledataPFT->head ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      queueDestroy ( bibAFiledataPFT );
    }

  return iret;
}

/***************************************************************/

int fowPOSIXFPGUARDTHREAD ( const char *filename )
{
  int bcastRank, id; 
  MPI_Comm bcastComm;
  static aFiledataPFT *afd;
  static bool firstOpen = true;
  static long buffersize = 0;

  /* broadcast buffersize to collectors */
  
  if ( firstOpen )
    {
      if ( ddebug && pioinfo->rank == 0)
	fprintf ( stdout, 
		  "pe%d in pioFileOpenW(): name=%s, "
		  "broadcast buffersize to collectors ...\n", 
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
  
        firstOpen = false;
      }

  /* init and enqueue file element */ 

  afd = initAFiledataPFT( filename, buffersize );
  if (( id = queuePush ( bibAFiledataPFT, afd, 0 )) < 0 )
    {   
      fprintf ( stderr, "Filename %s not unique\n", afd->name );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in pioFileOpenW(): name=%s, "
	      "init and enqueued aFiledataPFT, return id = %d\n", 
	      pioinfo->rank, filename, id );
  
  return id;
}

/***************************************************************/

int initPOSIXFPGUARDTHREAD ()
{
  int collectingData = 1, ncollectors;
  int rc = 0;

  if ( pioinfo->size < 2 )
    {
      fprintf ( stderr, 
		"PTYPE PIO_POSIX_B  or PIO_MPI_FPGUARD "
		"should be used with at least 2 pes.\n" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in initPOSIXFPGUARDSENDREC()\n", 
	      pioinfo->rank );

  ncollectors = pioinfo->size;
  pioinfo->specialRank = pioinfo->size - 1;

  pioinfo->collectorComm = pioinfo->comm;


  if ( pioinfo->rank == pioinfo->specialRank ) 
    {
      if ( ddebug )
	fprintf ( stdout, 
		  "pe%d in initPOSIXFPGUARDTHREAD(): "
		  "create thread with fpGuard(), ncollectors = %d ...\n", 
		  pioinfo->rank, ncollectors );
      
      pthread_mutex_init( &offsetMutex, NULL ); 
      pthread_attr_init(&attr );
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

      rc = pthread_create ( &thread, NULL, &fpgPOSIXFPGUARDTHREAD, 
			    ( void *) &ncollectors );
      if (rc)
	{
	  fprintf ( stderr, 
		    "ERROR; return code from pthread_create() is %d\n", rc);
	  MPI_Abort ( pioinfo->comm, 1 );
	}
    }

  bibAFiledataPFT = queueInit ( destroyAFiledataPFT, compareNamesAPFT );  

  return collectingData;
}

 
void finalizePOSIXFPGUARDTHREAD ()
{
  if ( pioinfo->rank == pioinfo->specialRank )
    {
      pthread_attr_destroy ( &attr );
      pthread_join ( thread, NULL );
      pthread_mutex_destroy( &offsetMutex ); 
    }

  MPI_Barrier ( pioinfo->comm );

  return;
}

#endif
