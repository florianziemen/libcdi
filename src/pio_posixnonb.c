//pio_posixnonb.c:693: warning: pointer targets in passing argument 2 of ‘strncpy’ differ in signedness

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
#include "pio_impl.h"

extern bool ddebug;
extern char * command2charP[6];

extern long initial_buffersize;
extern char *token;
extern int maxErrorString; 

extern double startTime;
extern double accumProbe;
extern double accumRecv;
extern double accumSend;
extern double accumSuspend;
extern double accumWait;
extern double accumWrite;

extern pioInfo *pioinfo;


typedef struct 
{
  char *name;
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

static queue * bibAFiledataP;


int errclass, ierr, resultlen;
char err_buffer [ MPI_MAX_ERROR_STRING ];

/***************************************************************/

static aFiledataP *initAFiledataP ( const char *filename, size_t bs )
{
  aFiledataP *afp;
  size_t len;
  int iret;

  if ( ddebug )
    fprintf ( stderr, "pe%d initFiledataAP (): filename=%s, buffersize=%lu, in\n",
	pioinfo->rank, filename, bs );

  afp = ( aFiledataP * ) xmalloc ( sizeof ( aFiledataP ));
  memset ( afp, 0, sizeof ( aFiledataP ));

  len = strlen ( filename );
  afp->name = ( char * ) xmalloc (( len + 1) * sizeof ( char ));
  strcpy ( afp->name, filename );
  afp->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stderr, "pe%d initOpenFileCollector(): name=%s, init output buffer\n", 
	      pioinfo->rank, afp->name );

  iret = dbuffer_init ( &( afp->db1 ), afp->size );
  iret += dbuffer_init ( &( afp->db2 ), afp->size );

  if ( iret > 0 )
    errorPIO ( "dbuffer_init did not succeed", __FILE__, __LINE__, pioinfo->rank );

  afp->db = afp->db1;

  afp->command = IO_Open_file;
  afp->request = MPI_REQUEST_NULL;

  if ( ddebug )
    fprintf ( stderr, "pe%d initFiledataAP (), enqueued name=%s, return\n",
	pioinfo->rank, afp->name );
  
  return afp;
}

/***************************************************************/

static bFiledataP * initBFiledataP ( char *filename, 
				     size_t bs, int nc )
{
  bFiledataP * bfp;
  int i;
  char errorString[maxErrorString];

  if ( ddebug )
       fprintf ( stderr, "### pe%d initFiledataBP (): filename=%s, buffersize=%lu, ncollectors=%d, in\n",
	      pioinfo->rank, filename, bs, nc );

  bfp = ( bFiledataP * ) xmalloc ( sizeof (  bFiledataP ));
  memset ( bfp, 0, sizeof (  bFiledataP ));

  bfp->name = ( char * ) xmalloc (( strlen ( filename ) + 1 ) * sizeof ( char ));
  strcpy ( bfp->name, filename );
  bfp->size = bs;

  if (( bfp->fp = fopen ( filename, "w" )) == NULL ) 
    {
      sprintf ( errorString, "Failed to open %s", bfp->name );
      errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
    }

  dbuffer_init ( & ( bfp->fb ), ( size_t )( bfp->size ));

  bfp->finished = false;
  bfp->nfinished = ( bool * ) xmalloc ( nc * sizeof ( bool ));

  for ( i = 0; i < nc; i++ )
    *( bfp->nfinished + i ) = true;


  if ( ddebug )
    fprintf ( stderr, "### pe%d initFiledataBP (), name=%s, opened file, return\n",
	pioinfo->rank, bfp->name );

  return bfp;
}

/***************************************************************/

int destroyAFiledataP ( void *v )
{
  int iret = 0;
  aFiledataP *afp = ( aFiledataP * ) v;
  MPI_Status status;

  if ( ddebug )
    fprintf ( stderr, "pe%d destroyAFiledataP(): name=%s, cleanup, in\n", 
	      pioinfo->rank, afp->name );

  checkMPIstat ( MPI_Wait ( &( afp->request ), &status ), __FILE__, __LINE__, 
		 pioinfo->rank, &status );
  
  dbuffer_cleanup ( &( afp->db1 ));
  dbuffer_cleanup ( &( afp->db2 ));

  free ( afp->name );
  free ( afp );

  if ( ddebug )
    fprintf ( stderr, "pe%d return from destroyAFiledataP(), cleaned up, return\n", 
	      pioinfo->rank );

  return iret;
}

/***************************************************************/

int destroyBFiledataP ( void *v )
{
  int iret = 0;
  bFiledataP *bfp = ( bFiledataP * ) v;
  char errorString[maxErrorString];

  if ( ddebug )
    fprintf ( stderr, "pe%d destroyBFiledataP(): name=%s, cleanup, in\n", 
	      pioinfo->rank, bfp->name );

  /* close file */

  if (( iret = fclose ( bfp->fp )) == EOF )
    {
      sprintf ( errorString, "Failed to close %s", bfp->name );
      errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
    }

  /* file closed, cleanup */

  dbuffer_cleanup ( &( bfp->fb ));

  free ( bfp->name );
  free ( bfp->nfinished );
  free ( bfp );

  if ( ddebug )
    fprintf ( stderr, "pe%d return from destroyBFiledataP(), cleaned up, return\n", 
	      pioinfo->rank );

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

void writeP ( bFiledataP *bfd, long amount )
{
  long written;
  char errorString[maxErrorString];

  if ( ddebug )
    fprintf ( stderr, "### pe%d writeP, name=%s, amount=%ld, in\n", 
	      pioinfo->rank, bfd->name, amount );
	  
  if (( written = fwrite ( bfd->fb->buffer, sizeof ( char ), amount, 
			   bfd->fp )) != amount )
    {
      sprintf ( errorString, "did not succeed writing buffer in %s", bfd->name );
      errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
    }

  if ( ddebug )
    fprintf ( stderr, "### pe%d writeP (), name=%s, written=%ld, amount=%ld, return\n",
	      pioinfo->rank, bfd->name, written, amount );

  return;
}

/***************************************************************/
// baustelle, in modi vereinheitlichen, kapseln in pio_queue.c
void queueCheckP ( queue *q, char *name )
{
  node *curr;
  bFiledataP *bfd;
  size_t len;
  char errorString[maxErrorString];
 
  len = strlen ( name );
  curr = q->head;   
      
  while ( curr )
    { 
      bfd = ( bFiledataP * ) curr->val;
      if ( len == strlen ( bfd->name ) &&
	   memcmp ( name, bfd->name, len ) == 0 )
	{
	  sprintf ( errorString, "Filename %s is already enqueued\n", name );
	  errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
	  return;
	}
      
      curr = curr->next;
    }  

  return;
}
//

void pwPOSIXNONB ( int ncollectors )
{
  bFiledataP *bfd; 
  queue * bibBFiledataP;
  long amount, buffersize;
  char *messageBuffer = NULL;
  char *pMB, *filename, *temp;
  int messagesize, source, tag, i, len, id;
  rTag *rtag = NULL;
  MPI_Status status;
  char errorString[maxErrorString];

  if ( ddebug )
    fprintf(stderr, "### pe%d pwPOSIXNONB (), ncollectors=%d, in\n", 
	    pioinfo->rank, ncollectors);
 
  bibBFiledataP = queueInit ( destroyBFiledataP, compareNamesBP );
  
  for ( ;; )
    {    

      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, pioinfo->comm, &status );

      source = status.MPI_SOURCE;
      tag    = status.MPI_TAG;

      if ( rtag ) ungetTag ( rtag );
      if (( rtag   = getTag ( tag )) == NULL )
	errorPIO ( "getTag () failed", __FILE__, __LINE__, pioinfo->rank );

      MPI_Get_count ( &status, MPI_CHAR, &messagesize );

      if ( ddebug )
	fprintf ( stderr, 
		  "### pe%d pwPOSIXNONB (): receive message from source=%d, id=%d, command=%d ( %s ), messagesize=%d\n",
		  pioinfo->rank, source, rtag->id, rtag->command, 
		  command2charP [ rtag->command ], messagesize);

      switch ( rtag->command )
	{
      	case IO_Open_file:

	  messageBuffer = ( char *) xmalloc ( messagesize  * sizeof ( char ));
    	  pMB = messageBuffer;

	  MPI_Recv ( messageBuffer, messagesize, MPI_CHAR, source, 
					 tag, pioinfo->comm, &status );

	  if ( ddebug )
	    fprintf ( stderr, "### pe%d pwPOSIXNONB (), after recv, in loop\n",
		      pioinfo->rank ); 
	  
	  filename = strtok ( pMB, token );
	  pMB += ( strlen ( filename ) + 1 );
	  temp =  strtok ( pMB, token );
	  buffersize =  strtol ( temp, NULL, 16 );
	  pMB += ( strlen ( temp ) + 1 );
	  amount = ( long ) ( messageBuffer + messagesize - pMB );
	  
	  if ( ddebug ) 
	    fprintf ( stderr,
		      "### pe%d pwPOSIXNONB () case %s, filename=%s, buffersize=%ld, amount=%ld\n",
		      pioinfo->rank, command2charP [ rtag->command ], filename, 
		      buffersize, amount ); 
	  
	  
	  if ( !(bfd = ( bFiledataP * ) queueIdx2val ( bibBFiledataP, 
						       rtag->id )))
	    {
	      queueCheckP (  bibBFiledataP, filename );
	      bfd = ( bFiledataP * ) initBFiledataP ( filename, buffersize, ncollectors );
	      
	      if (( id = queuePush ( bibBFiledataP, bfd, 1, rtag->id )) != rtag->id )
		{   
		  sprintf ( errorString, "fileID=%d not unique", rtag->id );
		  errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
		}	      
	    }
	  else
	    if ((( len = strlen ( filename )) != ( int ) strlen ( bfd->name )) || 
		memcmp ( filename, bfd->name, len ) != 0 )
	      {
		  sprintf ( errorString, "filename is not consistant, fileID=%d", rtag->id );
		  errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
	      }

	  *( bfd->nfinished + source ) = false;

	  memcpy ( bfd->fb->buffer, pMB, ( int ) amount );

	  writeP ( bfd, amount );
	  
	  free ( messageBuffer );

	  break;

	case IO_Send_buffer:
	  
	  if ( ! ( bfd = ( bFiledataP * ) queueIdx2val ( bibBFiledataP,  
							 rtag->id )))
	      {
		sprintf ( errorString, "fileID=%d is not in queue", rtag->id );
		errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
	      }

	  amount = messagesize;

	  if ( ddebug )
	    fprintf ( stderr, "### pe%d, command: %s, id=%d, name=%s\n", 
		      pioinfo->rank, command2charP [ rtag->command ], 
		      rtag->id, bfd->name );
	  
	  MPI_Recv (  bfd->fb->buffer, amount, MPI_CHAR, source, tag, 
		      pioinfo->comm, &status );

	  writeP ( bfd, amount );
	  
	  break;

	case IO_Close_file:
	  
	  if ( ddebug )
	    fprintf ( stderr, 
		      "### pe%d, case %s,  file%d, source%d\n", 
		      pioinfo->rank, command2charP [ rtag->command ], 
		      rtag->id, source );
	  
	  if ( ! ( bfd = ( bFiledataP * ) queueIdx2val ( bibBFiledataP,  
							 rtag->id )))
	    {
	      sprintf ( errorString, "fileID=%d is not in queue", rtag->id );
	      errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
	    }

	  amount = messagesize;

	  if ( ddebug )
	    fprintf ( stderr, "### pe%d, command %s, id=%d, name=%s, amount=%ld\n", 
		      pioinfo->rank, command2charP [ rtag->command ], 
		      rtag->id, bfd->name, amount );
	  
	  MPI_Recv (  bfd->fb->buffer, amount, MPI_CHAR, source,  tag, 
						 pioinfo->comm, &status );	

	  writeP ( bfd, amount );

	  *( bfd->nfinished + source ) = true;	  
	  bfd->finished          = true;
	  
	  for ( i = 0; i < ncollectors; i++ )
	    {
	      if ( *( bfd->nfinished + i ) == false )
		{
		  bfd->finished = false;
		  break;
		}
	    }

	  if ( bfd->finished )
	    {
	      if ( ddebug )
		fprintf ( stderr, "### pe%d: all are finished with file %d, delete node\n",
			  pioinfo->rank, rtag->id );
	      queueDelNode ( bibBFiledataP, rtag->id );
	      if ( !bibBFiledataP->head )
		{
		  if ( ddebug )
		    fprintf ( stderr, "### pe%d: all files are finished, destroy queue, return\n", 
			      pioinfo->rank );
		  queueDestroy ( bibBFiledataP );
		  ungetTag ( rtag );
		  return;
		}
	    }	      
	}
    }
}


/***************************************************************/


void sendP ( aFiledataP *afd, int id )
{
  int tag;
  long amount;
  MPI_Status status;

  amount = ( long ) dbuffer_data_size ( afd->db );
  tag = setTag ( id, afd->command );

  if ( ddebug ) 
    fprintf ( stderr,
	      "pe%d sendP (): send buffer for %s, size: %l bytes, command=%s, in\n", 
	      pioinfo->rank, afd->name, amount, 
	      command2charP [ afd->command ] );

  if ( ddebug ) startTime = MPI_Wtime ();

  checkMPIstat (  MPI_Wait ( &( afd->request ), &status ), 
		  __FILE__,  __LINE__, pioinfo->rank, &status );

  if ( ddebug )
    accumWait +=  ( MPI_Wtime () - startTime );

  MPI_Isend ( afd->db->buffer, amount, MPI_CHAR, pioinfo->specialRank, 
			  tag, pioinfo->comm, &( afd->request )); 

  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
 
  if ( afd->db == afd->db1 )
    {
      if ( ddebug ) 
	fprintf ( stderr, "pe%d sendP (): Change to buffer 2 ...\n", 
		  pioinfo->rank );
      afd->db =  afd->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stderr, "pe%d sendP (): Change to buffer 1 ...\n", 
		  pioinfo->rank );
      afd->db =  afd->db1;
    }
  
  afd->command = IO_Send_buffer;

  return;
} 

/***************************************************************/

size_t fwPOSIXNONB ( int id, int tsId, const void *buffer, size_t len )
{
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataP *afd;
  node *curr;
  char errorString[maxErrorString];

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stderr, "pe%d fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, tsId );

      curr = bibAFiledataP->head;

      while ( curr ) 
	{
	  sendP (( aFiledataP *) curr->val, curr->idx );
	  curr = curr->next;
	}

      oldTsId = tsId;
      MPI_Barrier ( pioinfo->collectorComm ); 
    }                                          

  afd = ( aFiledataP * ) queueIdx2val ( bibAFiledataP, id );
  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  if ( ddebug )
    fprintf ( stderr, 
	      "pe%d fileWrite (): id = %d, tsId = %d, pushed %lu byte data on buffer, filled = %d\n", 
	      pioinfo->rank, id, tsId, len, filled ); 

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
    {
      sprintf ( errorString,"did not succeed filling output buffer, id=%d", id );
      errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );
    }

  return len;
}

/***************************************************************/

int fcPOSIXNONB ( int id )
{
  double accumWaitMax;
  aFiledataP *afd;
  int iret;

  if ( ddebug )
    fprintf ( stderr, "pe%d fileClose (%d ): send buffer, close file and cleanup\n", 
	      pioinfo->rank, id );

  afd = ( aFiledataP * ) queueIdx2val ( bibAFiledataP, id );

  afd->command = IO_Close_file;

  sendP ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataP, id );

  if ( !bibAFiledataP->head ) 
    {
      if ( ddebug )
	fprintf ( stderr, "pe%d fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      queueDestroy ( bibAFiledataP );
    }

  /* timer output */
  	    
  if ( ddebug )
    {
      MPI_Reduce ( &accumWait, &accumWaitMax, 
		   1, MPI_DOUBLE, MPI_MAX, 0, pioinfo->collectorComm );
      fprintf ( stderr, "pe%d fileClose(): Wait time %15.10lf s\n",
		pioinfo->rank, accumWait );
      if ( pioinfo->rank == 0 )
	fprintf ( stderr, 
		  "pe%d fileClose(): Max wait time %15.10lf s\n",
		  pioinfo->rank, accumWaitMax );
    }
      
  return iret;
}

/***************************************************************/

int fowPOSIXNONB ( const char *filename )
{
  static aFiledataP *afd;
  static long buffersize = 0; 
  int bcastRank, iret, id, messageLength = 32;
  char message[messageLength];
  MPI_Comm bcastComm;
  size_t l;
  char *temp;
  char errorString[maxErrorString];

  /* broadcast buffersize to collectors */
  
  if ( ! buffersize )
    {
      if ( ddebug )
	fprintf ( stderr, 
		  "pe%d fowPOSIXNONB(): Broadcast buffersize to collectors ...\n", 
		  pioinfo->rank );
      
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

  /* init and enqueue aFiledataP */

  afd = initAFiledataP ( filename, buffersize );
  if (( id = queuePush ( bibAFiledataP, afd, 0 )) < 0 )
    {
      sprintf ( errorString, "filename %s is not unique", afd->name );
      errorPIO ( errorString, __FILE__, __LINE__, pioinfo->rank );   
    }

  if ( ddebug )
    fprintf ( stderr, 
	      "pe%d fowPOSIXNONB(): name=%s, init and enqueued aFiledataP, return id = %d\n", 
	      pioinfo->rank, filename, id );

  /* put filename, id and buffersize on buffer */ 

  iret = dbuffer_push ( afd->db, ( unsigned char *) filename, strlen ( filename ));
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1);  
  sprintf ( message,"%lX", buffersize); 
  iret = dbuffer_push ( afd->db,  ( unsigned char *) message, strlen ( message ));
  iret = dbuffer_push ( afd->db,  ( unsigned char *) token, 1); 
  
  if ( ddebug )
    {
      l = strlen ( filename ) + strlen ( message ) + 2; 
      temp = ( char * ) xmalloc (( l + 1 ) * sizeof ( char ));
      strncpy ( temp, ( char * ) afd->db->buffer, ( size_t ) l );
      *( temp + l )  = '\0';
      fprintf ( stderr, 
		"pe%d fowPOSIXNONB(): name=%s, put Open file message on buffer:\n%s,\t return \n", 
		pioinfo->rank, filename, temp ); 
      free ( temp );
    }

  return id;
}

/***************************************************************/

int initPOSIXNONB ()  
{
  int collectingData = 1, ncollectors;
  MPI_Group ioGroup, collectorGroup;
  int specialRanks[1];

  if ( pioinfo->size < 2 )
    errorPIO ( "usage: #pes/#nodes >= 2", 
	       __FILE__, __LINE__, pioinfo->rank );

  pioinfo->specialRank = pioinfo->size - 1;
  ncollectors = pioinfo->size - 1;
  
  specialRanks[0] = pioinfo->specialRank;
  MPI_Comm_group ( pioinfo->comm, &ioGroup );
  MPI_Group_excl ( ioGroup, 1, specialRanks, &collectorGroup );
  MPI_Comm_create ( pioinfo->comm, collectorGroup, &
		    pioinfo->collectorComm );
  
  if ( pioinfo->rank == pioinfo->specialRank ) 
    {
      if ( ddebug )
	fprintf ( stderr, 
		  "### pe%d bInit (): Call posixWriter(), ncollectors = %d ...\n", 
		  pioinfo->rank, ncollectors );
      pwPOSIXNONB ( ncollectors );
      collectingData = 0;
    }
  else
    {
      bibAFiledataP = queueInit ( destroyAFiledataP, 
				compareNamesAP );
    }
      
  MPI_Group_free ( &collectorGroup );
  MPI_Group_free ( &ioGroup );

  return collectingData;
}

#endif
