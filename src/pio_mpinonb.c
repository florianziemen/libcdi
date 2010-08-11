#ifndef NOMPI

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

typedef struct 
{
  char *name;
  size_t size;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  MPI_File fh;
  MPI_Request request;
  bool finished;
} aFiledataM;

static queue *bibAFiledataM;

/***************************************************************/

static aFiledataM *initAFiledataMPINONB ( const char *filename, size_t bs )
{
  aFiledataM *of;
  size_t len;
  int status;

  if ( ddebug )
    fprintf ( stdout, "pe%d initAFiledataMPINONB (): filename=%s, buffersize=%zu, in\n",
	pioinfo->rank, filename, bs );

  of = ( aFiledataM * ) malloc ( sizeof ( aFiledataM ));
  memset ( of, 0, sizeof ( aFiledataM ));

  len = strlen ( filename );
  of->name = ( char * ) malloc (( len ) * sizeof ( char ));
  strcpy ( of->name, filename );
  of->size = bs;

  /* init output buffer */

  if ( ddebug )
    fprintf ( stdout, "pe%d initOpenFileCollector(): name=%s, init output buffer\n", 
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

  MPI_File_open ( pioinfo->comm, of->name, ( MPI_MODE_CREATE|MPI_MODE_WRONLY ), 
		  MPI_INFO_NULL, &( of->fh ));

  of->request = MPI_REQUEST_NULL;
  of->finished = false;

  if ( ddebug )
    fprintf ( stdout, "pe%d initAFiledataMPINONB (), enqueued name=%s, opened file, return\n",
	pioinfo->rank, of->name );
  
  return of;
}

/***************************************************************/

int destroyAFiledataMPINONB ( void *v )
{
  int iret = 0;
  aFiledataM *of;
  MPI_Status status;

  of = (aFiledataM * ) v;

  if ( ddebug )
    fprintf ( stdout, "pe%d destroyAFiledataMPINONB(): name=%s, close file, in\n", 
	      pioinfo->rank, of->name );
  
  /* close file */

  startTime = MPI_Wtime ();
  MPI_Wait ( & ( of->request ), &status ); 
  accumWait += ( MPI_Wtime () - startTime );
  MPI_File_sync ( of->fh ); 
  
  iret = MPI_File_close ( & ( of->fh ));

  /* file closed, cleanup */
  
  dbuffer_cleanup ( & ( of->db1 ));
  dbuffer_cleanup ( & ( of->db2 ));

  free ( of->name );
  free ( of );

  if ( ddebug )
    fprintf ( stdout, "pe%d destroyAFiledataMPINONB(), closed file, cleaned up, return\n", 
	      pioinfo->rank );

  return iret == MPI_SUCCESS ? 0 : -1;
}

/***************************************************************/

bool compareNamesMPINONB  ( void *v1, void *v2 )
{
  aFiledataM *afm1, *afm2;
  size_t len;
  bool bret;

  afm1 = ( aFiledataM * ) v1;
  afm2 = ( aFiledataM * ) v2;

  len = strlen ( afm1->name );
  bret = ( len == strlen ( afm2->name ) && 
	  memcmp ( afm1->name, afm2->name, len ) == 0 );

  return bret;
}

/***************************************************************/

void writeMPINONB ( aFiledataM *of, int id )
{
  int amount;
  MPI_Status status;

  /* write buffer */
  
  amount = ( int ) dbuffer_data_size ( of->db );

  if ( amount == 0 ) return;

  if ( ddebug ) 
    fprintf ( stdout, "pe%d writeMPINONB(): Write buffer, size %d bytes, in\n", 
	      pioinfo->rank, amount );
  
  MPI_Wait ( & ( of->request ), &status );
  MPI_File_iwrite_shared ( of->fh, of->db->buffer, amount, MPI_CHAR, 
			   & ( of->request ));

  /* change outputBuffer */
  
  dbuffer_reset ( of->db );
  
  if ( of->db == of->db1 )
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d writeMPINONB (): id=%d, change to buffer 2 ...\n", 
		  pioinfo->rank, id );
      of->db =  of->db2;
    }
  else 
    {
      if ( ddebug ) 
	fprintf ( stdout, "pe%d writeMPINONB (): id=%d, change to buffer 1 ...\n", 
		  pioinfo->rank, id );
      of->db =  of->db1;
    }

  return;
}

/***************************************************************/

size_t fwMPINONB ( int id, int tsId, const void *buffer, size_t len )
{  
  static int oldTsId = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataM *of;
  node *curr;

  flush = ( tsId != oldTsId ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d fileWriter (): tsId = %d, flush buffer\n", 
		  pioinfo->rank, tsId );
      curr = bibAFiledataM->head;
      while ( curr ) 
	{
	  writeMPINONB (( aFiledataM *) curr->val, curr->idx );
	  curr = curr->next;
	}
      oldTsId = tsId;
      MPI_Barrier ( pioinfo->comm ); 
    }                                          

  of = ( aFiledataM * ) queueIdx2val ( bibAFiledataM, id );
  filled = dbuffer_push ( of->db, ( unsigned char * ) buffer, len );

  if ( ddebug )
    fprintf ( stdout, "pe%d fileWriter (): id = %d, tsId = %d, pushed data on buffer, filled = %d\n", 
	      pioinfo->rank, id, tsId, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  writeMPINONB ( of, id );
     
	  error = dbuffer_push ( of->db, ( unsigned char * ) buffer, len );
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

int fcMPINONB ( int id )
{
  aFiledataM *of;
  int iret;
  double accumWaitMax;

  if ( ddebug )
    fprintf ( stdout, "pe%d fileClose (%d ): write buffer, close file and cleanup, in\n", 
	      pioinfo->rank, id );

  if ( ! ( of = ( aFiledataM * ) queueIdx2val ( bibAFiledataM, id )))
    {
      fprintf ( stderr, "pe%d error, fileID=%d not found\n",
		pioinfo->rank, id);
      MPI_Abort ( pioinfo->comm, 1 );
    }

  writeMPINONB ( of, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataM, id );

  if ( !bibAFiledataM->head ) 
    {
      if ( ddebug )
	fprintf ( stdout, "pe%d fileClose (): cleanup queue \n", 
		  pioinfo->rank );
      free ( bibAFiledataM );
    }

  /* timer output */

  if ( ddebug )
    {
      MPI_Reduce ( &accumWait, &accumWaitMax, 
		   1, MPI_DOUBLE, MPI_MAX, 0, pioinfo->comm  );
      fprintf ( stdout, "pe%d pioFileClose(): Wait time %15.10lf s\n",
		pioinfo->rank, accumWait );
      if ( pioinfo->rank == 0 )
	fprintf ( stdout, "pe%d pioFileClose(): Max wait time %15.10lf s\n",
		  pioinfo->rank, accumWaitMax );
    }

  return iret;
}

/***************************************************************/

int fowMPINONB ( const char *filename )
{
  static aFiledataM *of;
  static long buffersize = 0; 
  int id, bcastRank = 0; 

  /* broadcast buffersize to collectors ( just once, for all files )*/
  
  if ( ! buffersize )
    {
      if ( ddebug )
	fprintf ( stdout, 
		  "pe%d fowMPINONB(): Broadcast buffersize to collectors ...\n", 
		  pioinfo->rank );
      
      if  ( pioinfo->rank == bcastRank )
	{ 
	  if ( getenv( "BUFSIZE" ) != NULL )
	    buffersize = atol ( getenv ( "BUFSIZE" ));
	  if ( buffersize < initial_buffersize )
	    buffersize = initial_buffersize;
	}
      
      MPI_Bcast ( &buffersize, 1, MPI_LONG, bcastRank, pioinfo->comm );
    }

  of = initAFiledataMPINONB ( filename, buffersize );
  if (( id = queuePush ( bibAFiledataM, of, 0 )) < 0 )
    {   
      fprintf ( stderr, "Filename %s not unique\n", of->name );
      MPI_Abort ( pioinfo->comm, 1 );
    }

  if ( ddebug )
    fprintf ( stdout, 
	      "pe%d fowMPINONB(): name=%s, init and enqueued aFiledataM, return id = %d\n", 
	      pioinfo->rank, filename, id );
  
  return id;
}

/***************************************************************/

int initMPINONB ( int *ncollectors )
{
  int collectingData = 1;

  *ncollectors = pioinfo->size;

  bibAFiledataM = queueInit ( destroyAFiledataMPINONB, compareNamesMPINONB );
	    
  return collectingData;
}

#endif
