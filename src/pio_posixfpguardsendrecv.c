/* 
   todo 
   build in control, for consistance of pairs filename / filenumber 
   ( pioOpenFile member name, recv in tmpbuffer, if(!uniqueName(q,v,n))abort )
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "mpi.h"
#include "pio.h"
#include "pio_impl.h"
#include "pio_util.h"


extern char * command2charP[6];

extern long initial_buffersize;
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
  FILE *fp;
  IO_Server_command command;
} aFiledataPF;

typedef struct 
{
  long offset;
  bool finished;
  bool *nfinished;
} bFiledataPF;

static queue *bibAFiledataPF;

/***************************************************************/
  
static aFiledataPF *initAFiledataPF ( const char *key, size_t bs)
{
  aFiledataPF *afd;
  size_t len;
  int iret;
  char errorString[maxErrorString];

  afd = ( aFiledataPF * ) xmalloc ( sizeof ( aFiledataPF ));
  memset ( afd, 0, sizeof ( aFiledataPF ));
  len = strlen ( key );
  afd->name = ( char * ) xmalloc (( len ) * sizeof ( char ));
  strcpy ( afd->name, key );
  afd->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stderr, "pe%d in initAFiledataPF(): name=%s, init output buffer\n", 
	      pioinfo->rank, afd->name );
   
  iret = dbuffer_init ( &( afd->db1 ), afd->size );
  iret += dbuffer_init ( &( afd->db2 ), afd->size );

  if ( iret > 0 )
    xabort ( "dbuffer_init did not succeed" );

  afd->db = afd->db1;

  /* open file */ 
  
  if ( ddebug )
    fprintf ( stderr, "pe%d in initAFiledataPF(): name=%s, open file\n", 
	      pioinfo->rank, afd->name );

  if ( ( afd->fp = fopen ( afd->name, "w" )) == NULL ) 
    {
      sprintf ( errorString, "Failed to open %s", afd->name );
      xabort ( errorString );
    }

  afd->command = IO_Open_file;
  
  return afd;
}

/***************************************************************/

bFiledataPF * initBFiledataPF ( int key, int nc )
{
  bFiledataPF *bfd;
  int i;

  bfd = ( bFiledataPF * ) xmalloc ( sizeof ( bFiledataPF ));
  memset ( bfd, 0, sizeof ( bFiledataPF ));
  bfd->offset = 0;
  bfd->finished = false;
  bfd->nfinished = ( bool * ) xmalloc ( nc * sizeof ( bool ));

  for ( i = 0; i < nc; i++ )
    *( bfd->nfinished + i ) = true;

  return bfd;
}

/***************************************************************/

int destroyAFiledataPF ( void *v )
{
  int iret = 0;
  aFiledataPF *afd = ( aFiledataPF * ) v;
  char errorString[maxErrorString];
  
  /* close file */
  
  if ( ddebug )
    fprintf ( stderr, "pe%d in destroyAFiledataPF(): name=%s, close file\n", 
	      pioinfo->rank, afd->name );
       
  if (( iret = fclose ( afd->fp )) == EOF )
    {
      sprintf ( errorString, "Failed to close %s", afd->name );
      xabort ( errorString );
    }  

  /* file closed, cleanup */

  if ( ddebug )
    fprintf ( stderr, "pe%d in destroyAFiledataPF(): name=%s, file closed, cleanup ...\n", 
	      pioinfo->rank, afd->name );
  
  dbuffer_cleanup ( &( afd->db1 ));
  dbuffer_cleanup ( &( afd->db2 ));

  free ( afd->name );
  free ( afd );

  return iret;
}

/***************************************************************/

int destroyBFiledataPF ( void *v )
{
  int iret = 0;
  bFiledataPF *bfd = (bFiledataPF * ) v;
  
  free ( bfd->nfinished );
  free ( bfd );

  return iret;
}

/***************************************************************/

bool compareNamesAPF ( void *v1, void *v2 )
{
  aFiledataPF *afd1, *afd2;
  size_t len;
  bool ret;

  afd1 = ( aFiledataPF * ) v1;
  afd2 = ( aFiledataPF * ) v2;

  len = strlen ( afd1->name );
  ret = ( len == strlen ( afd2->name ) && 
	  memcmp ( afd1->name, afd2->name, len ) == 0 );

  return ret;
}

/***************************************************************/

void fpgPOSIXFPGUARDSENDRECV ( int ncollectors )
{
  int i, source, iret;
  rTag *rtag;
  MPI_Status status;
  bFiledataPF *bfd; 
  queue *bibBFiledataPF;
  long amount;
  char errorString[maxErrorString];

  if ( ddebug )
    fprintf(stderr, "pe%d in fpGuard(), ncollectors=%d\n", 
	    pioinfo->rank, ncollectors);

  bibBFiledataPF = queueInit ( destroyBFiledataPF, NULL );

  for ( ;; )
    {
      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, pioinfo->comm, &status );
      source = status.MPI_SOURCE;
      rtag   = getTag ( status.MPI_TAG ); 

      if ( ddebug )
	fprintf ( stderr, 
		  "pe%d in fpGuard (): receive message from source=%d, id=%d, command=%d ( %s )\n",
		  pioinfo->rank, source, rtag->id, rtag->command, 
		  command2charP [ rtag->command ]);
      
      switch ( rtag->command )
      	{
      	case IO_Open_file:
	  if ( !(bfd = ( bFiledataPF * ) queueIdx2val ( bibBFiledataPF, 
							     rtag->id )))
	    {
	      bfd = ( bFiledataPF * ) initBFiledataPF ( rtag->id, 
							       ncollectors );

	      if (( iret = queuePush ( bibBFiledataPF, bfd, 1, rtag->id )) != rtag->id )
	      {
		sprintf ( errorString, "fileID=%d not unique", rtag->id );
		xabort ( errorString);
	      } 
	    }

	  *( bfd->nfinished + source ) = false;

	  if ( ddebug )
	    fprintf ( stderr, "pe%d in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], bfd->offset );
	  
	  MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  bfd->offset += amount; 

	  if ( ddebug )
	    fprintf ( stderr, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Set_fp:
	  
	  if ( ! ( bfd = ( bFiledataPF * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		sprintf ( errorString, "fileId=%d not in queue", rtag->id );
		xabort ( errorString );
	      }

	  if ( ddebug )
	    fprintf ( stderr, "pe%d in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], bfd->offset );

	  MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  bfd->offset += amount;

	  if ( ddebug )
	    fprintf ( stderr, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Close_file:
	  
	  if ( ! ( bfd = ( bFiledataPF * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		sprintf ( errorString, "fileId=%d not in queue", rtag->id );
		xabort ( errorString );
	      }

	  if ( ddebug )
	    fprintf ( stderr, "pe%d in fpGuard(): id=%d, command=%d ( %s )), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command,
		      command2charP [ rtag->command ], bfd->offset );

	  MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  bfd->offset += amount;

	  if ( ddebug )
	    fprintf ( stderr, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
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
	      queueDelNode ( bibBFiledataPF, rtag->id );
	      if ( !bibBFiledataPF->head )
		{
		  queueDestroy ( bibBFiledataPF );
		  return;
		}
	    }      
	}
    }
}   

//*******************************************************

void writePF ( aFiledataPF *afd, int id )
{
  size_t amount, written;
  long offset;
  long amountL;
  int error, tag;
  MPI_Status status;
  char errorString[maxErrorString];
  
  /* send buffersize, recv offset */

  amount = dbuffer_data_size ( afd->db );
  amountL = ( long ) amount;

  tag = setTag ( id, afd->command );
  
  MPI_Sendrecv ( &amountL, 1, MPI_LONG, pioinfo->specialRank, tag,
		 &offset, 1, MPI_LONG, pioinfo->specialRank, tag,
		 pioinfo->comm, &status );
  
  if ( ddebug )
    fprintf ( stderr, 
	      "pe%d in writePF (), id=%d, command=%d, amount=%llu, send amountL=%ld, recv offset=%ld\n", 
	      pioinfo->rank, id, afd->command, (unsigned long long)amount,
              amountL, offset );
  
  /* write buffer */
  
  if (( error = fseek ( afd->fp, offset, SEEK_SET )) != 0 )
    xabort ( "did not succeed seeking fp" );

  if (( written = 
	fwrite ( afd->db->buffer, sizeof ( char ), amount, afd->fp )) !=
      amount )
    {
      sprintf ( errorString, 
		"fileId=%d, expect to write %llu byte, written %llu byte", 
		id, (unsigned long long)amount,
                (unsigned long long)written );
      xabort ( errorString );
    }

  if ( ddebug )
    fprintf ( stderr, 
	      "pe%d in writePF (), written %llu bytes in file %d with offset %ld\n", 
	      pioinfo->rank, (unsigned long long)written,
              id, offset ); 
  
  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
  
  if ( afd->db == afd->db1 )
    {
      if ( ddebug ) 
	fprintf ( stderr, "pe%d in writePF (): id=%d, change to buffer 2 ...\n", 
		  pioinfo->rank, id );
      afd->db =  afd->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stderr, "pe%d in writePF (): id=%d, change to buffer 1 ...\n", 
		  pioinfo->rank, id );
      afd->db =  afd->db1;
    }
  
  afd->command = IO_Set_fp;
}


/***************************************************************/

size_t fwPOSIXFPGUARDSENDRECV( int id, int tsId, const void *buffer, size_t len )
{
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataPF *afd;
  node *curr;
  char errorString[maxErrorString];

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stderr, "pe%d in fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, tsId );
      curr = bibAFiledataPF->head;
      while ( curr ) 
	{
	  writePF (( aFiledataPF *) curr->val, curr->idx );
	  curr = curr->next;
	}
      oldTsId = tsId;
      MPI_Barrier ( pioinfo->collectorComm ); 
    }                                          

  afd = ( aFiledataPF * ) queueIdx2val ( bibAFiledataPF, id );
  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  if ( ddebug )
    fprintf ( stderr, "pe%d in fileWriter (): id = %d, tsId = %d, pushed data on buffer, filled = %d\n", 
	      pioinfo->rank, id, tsId, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  writePF ( afd, id );
     
	  error = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );
	}
    }
  
  if ( error == 1 )
    {
      sprintf ( errorString,"did not succeed filling output buffer, id=%d", id );
      xabort ( errorString );
    }
  
  return len;
}

/***************************************************************/

int fcPOSIXFPGUARDSENDRECV ( int id )
{
  aFiledataPF *afd;
  int iret;

  if ( ddebug )
    fprintf ( stderr, "pe%d in fileClose (%d ): write buffer, close file and cleanup\n", 
	      pioinfo->rank, id );

  afd = ( aFiledataPF * ) queueIdx2val ( bibAFiledataPF, id );

  afd->command = IO_Close_file;

  writePF ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataPF, id );

  if ( !bibAFiledataPF->head ) 
    {
      if ( ddebug )
	fprintf ( stderr, "pe%d in fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      queueDestroy ( bibAFiledataPF );
    }

  return iret;
}

/***************************************************************/
// baustelle, in modi vereinheitlichen, kapseln in pio_queue.c
void queueCheckFP ( queue *q, const char *name )
{
  node *curr;
  aFiledataPF *afd;
  size_t len;
  char errorString[maxErrorString];
 
  len = strlen ( name );
  curr = q->head;   
      
  while ( curr )
    { 
      afd = ( aFiledataPF * ) curr->val;
      if ( len == strlen ( afd->name ) &&
	   memcmp ( name, afd->name, len ) == 0 )
	{
	  sprintf ( errorString, "Filename %s is already enqueued\n", name );
	  xabort ( errorString );
	  return;
	}
      
      curr = curr->next;
    }  

  return;
}
//

int fowPOSIXFPGUARDSENDRECV ( const char *filename )
{
  int bcastRank, id; 
  MPI_Comm bcastComm;
  static aFiledataPF *afd;
  static bool firstOpen = true;
  static long buffersize = 0;
  char errorString[maxErrorString];

  /* broadcast buffersize to collectors */
  
  if ( firstOpen )
    {
      if ( ddebug && pioinfo->rank == 0)
	fprintf ( stderr, 
		  "pe%d in pioFileOpenW(): name=%s, broadcast buffersize to collectors ...\n", 
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
  
  queueCheckFP (  bibAFiledataPF, filename );

  afd = initAFiledataPF ( filename, buffersize );

  if (( id = queuePush ( bibAFiledataPF, afd, 0 )) < 0 )
    {  
      sprintf ( errorString, "filename %s not unique", afd->name );
      xabort ( errorString ); 
    }

  if ( ddebug )
    fprintf ( stderr, 
	      "pe%d in pioFileOpenW(): name=%s, init and enqueued aFiledataPF, return id = %d\n", 
	      pioinfo->rank, filename, id );
  
  return id;
}

/***************************************************************/

int initPOSIXFPGUARDSENDRECV ()
{
  int collectingData = 1, ncollectors;
  MPI_Group streamGroup, collectorGroup;
  int ioRanks[1];

  if ( pioinfo->size < 2 ) xabort ( "usage: #pes/#nodes >= 2" );
  
  if ( ddebug )
    fprintf ( stderr, 
	      "pe%d in initPOSIXFPGUARDSENDRECV()\n", pioinfo->rank );

  ncollectors = pioinfo->size - 1;
  pioinfo->specialRank = pioinfo->size - 1;
  
  ioRanks[0] = pioinfo->specialRank;
  MPI_Comm_group ( pioinfo->comm, &streamGroup );
  MPI_Group_excl ( streamGroup, 1, ioRanks, &collectorGroup );
  MPI_Comm_create ( pioinfo->comm, collectorGroup, &
		    pioinfo->collectorComm );
  
  if ( pioinfo->rank == pioinfo->specialRank ) 
    {
      if ( ddebug )
	fprintf ( stderr, 
		  "pe%d in initPOSIXFPGUARDSENDREC(): Call fpGuard(), ncollectors = %d ...\n", 
		  pioinfo->rank, ncollectors );
      fpgPOSIXFPGUARDSENDRECV ( ncollectors );

      collectingData = 0;
    }
  else
    bibAFiledataPF = queueInit ( destroyAFiledataPF, compareNamesAPF );
    

  return collectingData;
}

#endif
