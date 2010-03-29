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

extern long initial_buffer_size; 

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
} pioOpenFile;

typedef struct 
{
  int ID;
  long offset;
  bool finished;
  bool *nfinished;
} pioOpenFileGuard;

static queue *openFiles;

/***************************************************************/
  
static pioOpenFile *initOpenFile ( const char *key, size_t bs)
{
  pioOpenFile *of;
  size_t len;
  int status;

  of = ( pioOpenFile * ) malloc ( sizeof ( pioOpenFile ));
  memset ( of, 0, sizeof ( pioOpenFile ));
  len = strlen ( key );
  of->name = ( char * ) malloc (( len ) * sizeof ( char ));
  strcpy ( of->name, key );
  of->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stdout, "pe%d in initOpenFile(): name=%s, init output buffer\n", 
	      pioinfo->rank, of->name );
   
  if (( status = dbuffer_init ( &( of->db1 ), of->size )) == 1 )
    {
      fprintf ( stderr, "dbuffer_init outputBuffer1: %d\n", status );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  if (( status = dbuffer_init ( &( of->db2 ), of->size )) == 1)
    {
      fprintf ( stderr, "dbuffer_init outputBuffer2: %d\n", status );
      MPI_Abort ( pioinfo->comm, 1 );
    }
  of->db = of->db1;

  /* open file */ 
  
  if ( ddebug )
    fprintf ( stdout, "pe%d in initOpenFile(): name=%s, open file\n", 
	      pioinfo->rank, of->name );

  if ( ( of->fp = fopen ( of->name, "w" )) == NULL ) 
    {
      fprintf ( stderr, "Failed to open %s:\n", of->name );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  of->command = IO_Open_file;
  
  return of;
}

/***************************************************************/

pioOpenFileGuard *initOpenFileGuard ( int key, int nc )
{
  pioOpenFileGuard *ofg;
  int i;

  ofg = ( pioOpenFileGuard * ) malloc ( sizeof ( pioOpenFileGuard ));
  memset ( ofg, 0, sizeof ( pioOpenFileGuard ));
  ofg->ID = key;
  ofg->offset = 0;
  ofg->finished = false;
  ofg->nfinished = ( bool * ) malloc ( nc * sizeof ( bool ));

  for ( i = 0; i < nc; i++ )
    *( ofg->nfinished + i ) = false;

  return ofg;
}

/***************************************************************/

int destroyOpenFile ( void *v )
{
  int iret = 0;
  pioOpenFile *of = ( pioOpenFile * ) v;
  
  /* close file */
  
  if ( ddebug )
    fprintf ( stdout, "pe%d in destroyOpenFile(): name=%s, close file\n", 
	      pioinfo->rank, of->name );
       
  if (( iret = fclose ( of->fp )) == EOF )
    {
      fprintf ( stderr, "Failed to close file:\n" );
      perror ( "System error message" );
      MPI_Abort ( pioinfo->comm, 1 );
    }  

  /* file closed, cleanup */

  if ( ddebug )
    fprintf ( stdout, "pe%d in destroyOpenFile(): name=%s, file closed, cleanup ...\n", 
	      pioinfo->rank, of->name );
  
  dbuffer_cleanup ( &( of->db1 ));
  dbuffer_cleanup ( &( of->db2 ));

  free ( of->name );
  free ( of );

  return iret;
}

/***************************************************************/

int destroyOpenFileGuard ( void *v )
{
  int iret = 0;
  pioOpenFileGuard *ofg = (pioOpenFileGuard * ) v;
  
  free ( ofg->nfinished );
  free ( ofg );

  return iret;
}

/***************************************************************/

bool compareKeyOpenFile ( void *v1, void *v2 )
{
  pioOpenFile *of1, *of2;
  size_t len;
  bool ret;

  of1 = ( pioOpenFile * ) v1;
  of2 = ( pioOpenFile * ) v2;

  len = strlen ( of1->name );
  ret = ( len == strlen ( of2->name ) && 
	  memcmp ( of1->name, of2->name, len ) == 0 );

  return ret;
}

/***************************************************************/

bool compareKeyOpenFileGuard ( void *v1, void *v2 )
{
  pioOpenFileGuard *ofg1, *ofg2;
  bool ret;

  ofg1 = ( pioOpenFileGuard * ) v1;
  ofg2 = ( pioOpenFileGuard * ) v2;

  ret = ( ofg1->ID == ofg2->ID );

  return ret;
}

/***************************************************************/

void fpgPOSIXFPGUARDSENDRECV ( int ncollectors )
{
  int i, source;//j;
  rTag *rtag;
  MPI_Status status;
  pioOpenFileGuard *ofg; 
  queue *openFilesGuard;
  long amount;

  if ( ddebug )
    fprintf(stdout, "pe%d in fpGuard(), ncollectors=%d\n", 
	    pioinfo->rank, ncollectors);
 
  openFilesGuard = queueInit ( destroyOpenFileGuard, compareKeyOpenFileGuard );
  
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

	  if ( !(ofg = ( pioOpenFileGuard * ) queueIdx2val ( openFilesGuard, 
							     rtag->id )))
	    {
	      ofg = ( pioOpenFileGuard * ) initOpenFileGuard ( rtag->id, 
							       ncollectors );
	      queuePush ( openFilesGuard, ofg );
	    }

	  if ( ddebug )
	    fprintf ( stdout, "pe%d in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], ofg->offset );
	  
	  MPI_Sendrecv ( &( ofg->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  ofg->offset += amount; 

	  if ( ddebug )
	    fprintf ( stdout, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, ofg->offset );

	  break;

	case IO_Set_fp:
	  
	  ofg = ( pioOpenFileGuard * ) queueIdx2val ( openFilesGuard, rtag->id );

	  if ( ddebug )
	    fprintf ( stdout, "pe%d in fpGuard(), id=%d, command=%d ( %s ), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], ofg->offset );

	  MPI_Sendrecv ( &( ofg->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  ofg->offset += amount;

	  if ( ddebug )
	    fprintf ( stdout, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, ofg->offset );

	  break;

	case IO_Close_file:
	  
	  ofg = ( pioOpenFileGuard * ) queueIdx2val ( openFilesGuard, rtag->id );

	  if ( ddebug )
	    fprintf ( stdout, "pe%d in fpGuard(): id=%d, command=%d ( %s )), send offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command,
		      command2charP [ rtag->command ], ofg->offset );

	  MPI_Sendrecv ( &( ofg->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
			 &amount, 1, MPI_LONG, source,  status.MPI_TAG,
			 pioinfo->comm, &status );

	  ofg->offset += amount;

	  if ( ddebug )
	    fprintf ( stdout, 
		      "pe%d in fpguard(), id=%d, command=%d ( %s ), recv amount=%zu, set offset=%ld\n", 
		      pioinfo->rank, rtag->id, rtag->command, 
		      command2charP [ rtag->command ], amount, ofg->offset );

	  ofg->nfinished[source] = true;	  
	  ofg->finished          = true;
	  
	  for ( i = 0; i < ncollectors; i++ )
	    if ( !( ofg->nfinished[i] ))
	      {
		ofg->finished = false;
		break;
	      }
	  
	  if ( ofg->finished )
	    {
	      queueDelNode ( openFilesGuard, rtag->id );
	      if ( !openFilesGuard->head )
		{
		  free ( openFilesGuard );
		  return;
		}
	    }	      
	}
    }
}   

//*******************************************************

void writeDBuffer ( pioOpenFile *of, int id )
{
  size_t amount, written;
  long offset;
  int error, tag;
  MPI_Status status;
  
  /* send buffersize, recv offset */

  amount = dbuffer_data_size ( of->db );
  
  tag = setTag ( id, of->command );
  
  MPI_Sendrecv ( &amount, 1, MPI_LONG, pioinfo->specialRank, tag,
		 &offset, 1, MPI_LONG, pioinfo->specialRank, tag,
		 pioinfo->comm, &status );
  
  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in writeDBuffer (), id=%d, command=%d, send amount=%zu, recv offset=%ld\n", 
	      pioinfo->rank, id, of->command, amount, offset );      
  
  /* write buffer */
  
  if (( error = fseek ( of->fp, offset, SEEK_SET )) != 0 )
    {
      perror ( "Error seeking fp" );
      MPI_Abort ( pioinfo->comm, 1 );
    }	  
  if (( written = 
	fwrite ( of->db->buffer, sizeof ( char ), amount, of->fp )) !=
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
	      "pe%d in writeDBuffer (), written %d bytes in file %d with offset %ld\n", 
	      pioinfo->rank, written, id, offset ); 
  
  /* change outputBuffer */
  
  dbuffer_reset ( of->db );
  
  if ( of->db == of->db1 )
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in writeDBuffer (): id=%d, change to buffer 2 ...\n", 
		  pioinfo->rank, id );
      of->db =  of->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d in writeDBuffer (): id=%d, change to buffer 1 ...\n", 
		  pioinfo->rank, id );
      of->db =  of->db1;
    }
  
  of->command = IO_Set_fp;
}


/***************************************************************/

size_t fwPOSIXFPGUARDSENDRECV( int id, int tsId, const void *buffer, size_t len )
{
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  pioOpenFile *of;
  node *curr;

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, tsId );
      curr = openFiles->head;
      while ( curr ) 
	{
	  writeDBuffer (( pioOpenFile *) curr->val, curr->idx );
	  curr = curr->next;
	}
      oldTsId = tsId;
      MPI_Barrier ( pioinfo->collectorComm ); 
    }                                          

  of = ( pioOpenFile * ) queueIdx2val ( openFiles, id );
  filled = dbuffer_push ( of->db, ( unsigned char * )buffer, len );

  if ( ddebug )
    fprintf ( stdout, "pe%d in fileWriter (): id = %d, tsId = %d, pushed data on buffer, filled = %d\n", 
	      pioinfo->rank, id, tsId, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  writeDBuffer ( of, id );
     
	  error = dbuffer_push ( of->db, ( unsigned char * )buffer, len );
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
  pioOpenFile *of;
  int iret;

  if ( ddebug )
    fprintf ( stdout, "pe%d in fileClose (%id ): write buffer, close file and cleanup\n", 
	      pioinfo->rank, id );

  of = ( pioOpenFile * ) queueIdx2val ( openFiles, id );

  of->command = IO_Close_file;

  writeDBuffer ( of, id );

  /* dequeue file element */

  iret = queueDelNode ( openFiles, id );

  if ( !openFiles->head ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d in fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      free ( openFiles );
    }

  return iret;
}

/***************************************************************/

int fowPOSIXFPGUARDSENDRECV ( const char *filename )
{
  int bcastRank; 
  MPI_Comm bcastComm;
  static pioOpenFile *of;
  static bool firstOpen = true;
  static long buffer_size = 0;

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
	    buffer_size = atol ( getenv ( "BUFSIZE" ));
	  if ( buffer_size < initial_buffer_size )
	    buffer_size = initial_buffer_size;
	}
      
      MPI_Bcast ( &buffer_size, 1, MPI_LONG, 0, bcastComm );
  
        firstOpen = false;
      }

  /* init and enqueue file element */ 

  of = initOpenFile ( filename, buffer_size );
  queuePush ( openFiles, of );

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d in pioFileOpenW(): name=%s, init and enqueued pioOpenFile, return id = %d\n", 
	      pioinfo->rank, filename, openFiles->tail->idx );
  
  return openFiles->tail->idx;
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
    openFiles = queueInit ( destroyOpenFile, compareKeyOpenFile );
    

  return collectingData;
}

#endif
