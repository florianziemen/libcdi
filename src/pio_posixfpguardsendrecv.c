/* 
   todo 
   build in control, for consistance of pairs filename / filenumber 
   ( pioOpenFile member name, recv in tmpbuffer, if(!uniqueName(q,v,n))abort )
*/

#ifndef NOMPI

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

extern long initial_buffersize; 

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
  int status;

  afd = ( aFiledataPF * ) malloc ( sizeof ( aFiledataPF ));
  memset ( afd, 0, sizeof ( aFiledataPF ));
  len = strlen ( key );
  afd->name = ( char * ) malloc (( len ) * sizeof ( char ));
  strcpy ( afd->name, key );
  afd->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stdout, "pe%d in initAFiledataPF(): name=%s, init output buffer\n", 
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
    fprintf ( stdout, "pe%d in initAFiledataPF(): name=%s, open file\n", 
	      pioinfo->rank, afd->name );

  if ( ( afd->fp = fopen ( afd->name, "w" )) == NULL ) 
    {
      fprintf ( stderr, "Failed to open %s:\n", afd->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  afd->command = IO_Open_file;
  
  return afd;
}

/***************************************************************/

bFiledataPF * initBFiledataPF ( int key, int nc )
{
  bFiledataPF *bfd;
  int i;

  bfd = ( bFiledataPF * ) malloc ( sizeof ( bFiledataPF ));
  memset ( bfd, 0, sizeof ( bFiledataPF ));
  bfd->offset = 0;
  bfd->finished = false;
  bfd->nfinished = ( bool * ) malloc ( nc * sizeof ( bool ));

  for ( i = 0; i < nc; i++ )
    *( bfd->nfinished + i ) = false;

  return bfd;
}

/***************************************************************/

int destroyAFiledataPF ( void *v )
{
  int iret = 0;
  aFiledataPF *afd = ( aFiledataPF * ) v;
  
  /* close file */
  
  if ( ddebug )
    fprintf ( stdout, "pe%d in destroyAFiledataPF(): name=%s, close file\n", 
	      pioinfo->rank, afd->name );
       
  if (( iret = fclose ( afd->fp )) == EOF )
    {
      fprintf ( stderr, "Failed to close file:\n" );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }  

  /* file closed, cleanup */

  if ( ddebug )
    fprintf ( stdout, "pe%d in destroyAFiledataPF(): name=%s, file closed, cleanup ...\n", 
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

  if ( ddebug )
    fprintf(stdout, "pe%d in fpGuard(), ncollectors=%d\n", 
	    pioinfo->rank, ncollectors);
 
  bibBFiledataPF = queueInit ( destroyBFiledataPF, NULL );
  
  for ( ;; )
    {
      MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, pioinfo->comm, &status );
      source = status.MPI_SOURCE;
      rtag   = getTag ( status.MPI_TAG ); 

      if ( ddebug )
	fprintf ( stdout, 
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
		fprintf ( stderr, "pe%d, fileID=%d not unique\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      } 
	    }

	  if ( ddebug )
	    fprintf ( stdout, "pe%d in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], bfd->offset );
	  
	  MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  bfd->offset += amount; 

	  if ( ddebug )
	    fprintf ( stdout, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Set_fp:
	  
	  if ( ! ( bfd = ( bFiledataPF * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		fprintf ( stderr, "pe%d, id=%d not in file queue\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }

	  if ( ddebug )
	    fprintf ( stdout, "pe%d in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], bfd->offset );

	  MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  bfd->offset += amount;

	  if ( ddebug )
	    fprintf ( stdout, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Close_file:
	  
	  if ( ! ( bfd = ( bFiledataPF * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		fprintf ( stderr, "pe%d, id=%d not in file queue\n",
			  pioinfo->rank, rtag->id);
		MPI_Abort ( pioinfo->comm, 1 );
	      }

	  if ( ddebug )
	    fprintf ( stdout, "pe%d in fpGuard(): id=%d, command=%d ( %s )), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command,
		      command2charP [ rtag->command ], bfd->offset );

	  MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  bfd->offset += amount;

	  if ( ddebug )
	    fprintf ( stdout, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%zu, set offset=%ld\n", 
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
		  free ( bibBFiledataPF );
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
  int error, tag;
  MPI_Status status;
  
  /* send buffersize, recv offset */

  amount = dbuffer_data_size ( afd->db );
  
  tag = setTag ( id, afd->command );
  
  MPI_Sendrecv ( &amount, 1, MPI_LONG, pioinfo->specialRank, tag,
		 &offset, 1, MPI_LONG, pioinfo->specialRank, tag,
		 pioinfo->comm, &status );
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in writePF (), id=%d, command=%d, send amount=%zu, recv offset=%ld\n", 
	      pioinfo->rank, id, afd->command, amount, offset );      
  
  /* write buffer */
  
  if (( error = fseek ( afd->fp, offset, SEEK_SET )) != 0 )
    {
      perror ( "Error seeking fp" );
      MPI_Abort ( pioinfo->comm, 1 );
    }	  
  if (( written = 
	fwrite ( afd->db->buffer, sizeof ( char ), amount, afd->fp )) !=
      amount )
    {
      fprintf ( stdout, 
		"pe%d, id=%d, expect to write %16d byte, written %16d byte.\n", 
		pioinfo->rank, id, amount, written );
      perror ( "Error writing data" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in writePF (), written %d bytes in file %d with offset %ld\n", 
	      pioinfo->rank, written, id, offset ); 
  
  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
  
  if ( afd->db == afd->db1 )
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in writePF (): id=%d, change to buffer 2 ...\n", 
		  pioinfo->rank, id );
      afd->db =  afd->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in writePF (): id=%d, change to buffer 1 ...\n", 
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

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileWriter (): tsId = %d, flush buffer\n", 
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
    fprintf ( stdout, "pe%d in fileWriter (): id = %d, tsId = %d, pushed data on buffer, filled = %d\n", 
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
      fprintf ( stderr, "pe%d, id=%d, problem filling output buffer ...\n",
		pioinfo->rank, id);
      MPI_Abort ( pioinfo->comm, 1 );
    }
  
  return len;
}

/***************************************************************/

int fcPOSIXFPGUARDSENDRECV ( int id )
{
  aFiledataPF *afd;
  int iret;

  if ( ddebug )
    fprintf ( stdout, "pe%d in fileClose (%id ): write buffer, close file and cleanup\n", 
	      pioinfo->rank, id );

  afd = ( aFiledataPF * ) queueIdx2val ( bibAFiledataPF, id );

  afd->command = IO_Close_file;

  writePF ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataPF, id );

  if ( !bibAFiledataPF->head ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      queueDestroy ( bibAFiledataPF );
    }

  return iret;
}

/***************************************************************/

int fowPOSIXFPGUARDSENDRECV ( const char *filename )
{
  int bcastRank, id; 
  MPI_Comm bcastComm;
  static aFiledataPF *afd;
  static bool firstOpen = true;
  static long buffersize = 0;

  /* broadcast buffersize to collectors */
  
  if ( firstOpen )
    {
      if ( ddebug && pioinfo->rank == 0)
	fprintf ( stdout, 
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

  afd = initAFiledataPF ( filename, buffersize );
  if (( id = queuePush ( bibAFiledataPF, afd, 0 )) < 0 )
    {   
      fprintf ( stderr, "Filename %s not unique\n", afd->name );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in pioFileOpenW(): name=%s, init and enqueued aFiledataPF, return id = %d\n", 
	      pioinfo->rank, filename, id );
  
  return id;
}

/***************************************************************/

int initPOSIXFPGUARDSENDRECV ( int *ncollectors )
{
  int collectingData = 1;
  MPI_Group streamGroup, collectorGroup;
  int ioRanks[1];

  if ( pioinfo->size < 2 )
    {
      fprintf ( stderr, 
		"PTYPE PIO_POSIX_B  or PIO_MPI_FPGUARD should be used with at least 2 pes.\n" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in initPOSIXFPGUARDSENDREC()\n", pioinfo->rank );

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
		  "pe%d in initPOSIXFPGUARDSENDREC(): Call fpGuard(), ncollectors = %d ...\n", 
		  pioinfo->rank, *ncollectors );
      fpgPOSIXFPGUARDSENDRECV ( *ncollectors );

      collectingData = 0;
    }
  else
    bibAFiledataPF = queueInit ( destroyAFiledataPF, compareNamesAPF );
    

  return collectingData;
}

#endif
