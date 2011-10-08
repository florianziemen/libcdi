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
#include "pio_comm.h"
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

static queue_t *bibAFiledataPF;

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

  xdebug ( " name=%s, init output buffer",  afd->name );
   
  iret = dbuffer_init ( &( afd->db1 ), afd->size );
  iret += dbuffer_init ( &( afd->db2 ), afd->size );

  if ( iret > 0 )
    xabort ( "dbuffer_init did not succeed" );

  afd->db = afd->db1;

  /* open file */ 
  xdebug ( "name=%s, open file",  afd->name );

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
  
  xdebug ( "name=%s, close file",  afd->name );
       
  if (( iret = fclose ( afd->fp )) == EOF )
    {
      sprintf ( errorString, "Failed to close %s", afd->name );
      xabort ( errorString );
    }  

  /* file closed, cleanup */

  xdebug ( "name=%s, file closed, cleanup ...",  afd->name );
  
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
  tag_t *rtag;
  MPI_Status status;
  bFiledataPF *bfd; 
  queue_t *bibBFiledataPF;
  long amount;
  char errorString[maxErrorString];
  MPI_Comm commNode = commInqCommNode ();

  xdebug ( "ncollectors=%d", ncollectors);
  
  bibBFiledataPF = queueInit ( destroyBFiledataPF, NULL );

  for ( ;; )
    {
      xmpi ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commNode, &status ));
      source = status.MPI_SOURCE;
      rtag   = getTag ( status.MPI_TAG ); 
      
      xdebug ( "receive message from source=%d, id=%d, command=%d ( %s )",
               source, rtag->id, rtag->command, 
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

          xdebug ( "id=%d, command=%d ( %s ), send offset=%ld", 
                   rtag->id, rtag->command, 
                   command2charP [ rtag->command ], bfd->offset );
	  
	  xmpi ( MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
                                &amount, 1, MPI_LONG, source,  status.MPI_TAG,
                                commNode, &status ));

	  bfd->offset += amount; 
 
          xdebug ( "id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld", 
                   rtag->id, rtag->command, 
                   command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Set_fp:
	  
	  if ( ! ( bfd = ( bFiledataPF * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		sprintf ( errorString, "fileId=%d not in queue", rtag->id );
		xabort ( errorString );
	      }

          xdebug ( "id=%d, command=%d ( %s ), send offset=%ld", 
                   rtag->id, rtag->command, 
                   command2charP [ rtag->command ], bfd->offset );

	  xmpi ( MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
                                &amount, 1, MPI_LONG, source,  status.MPI_TAG,
                                commNode, &status ));

	  bfd->offset += amount;

          xdebug ( "id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld", 
                   rtag->id, rtag->command, 
                   command2charP [ rtag->command ], amount, bfd->offset );

	  break;

	case IO_Close_file:
	  
	  if ( ! ( bfd = ( bFiledataPF * ) queueIdx2val ( bibBFiledataPF,  
							  rtag->id )))
	      {
		sprintf ( errorString, "fileId=%d not in queue", rtag->id );
		xabort ( errorString );
	      }

          xdebug ( "id=%d, command=%d ( %s )), send offset=%ld", 
                   rtag->id, rtag->command,
                   command2charP [ rtag->command ], bfd->offset );

	  xmpi ( MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
                                &amount, 1, MPI_LONG, source,  status.MPI_TAG,
                                commNode, &status ));

	  bfd->offset += amount;

          xdebug ( "id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld", 
                   rtag->id, rtag->command, 
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
  int specialRank = commInqSpecialRankNode ();
  MPI_Comm commNode = commInqCommNode ();
  
  /* send buffersize, recv offset */

  amount = dbuffer_data_size ( afd->db );
  amountL = ( long ) amount;

  tag = setTag ( id, afd->command );
  
  xmpi ( MPI_Sendrecv ( &amountL, 1, MPI_LONG, specialRank, tag,
                        &offset, 1, MPI_LONG, specialRank, tag,
                        commNode, &status ));
   
  xdebug ( "id=%d, command=%d, amount=%llu, send amountL=%ld, recv offset=%ld", 
           id, afd->command, (unsigned long long)amount,
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
 
  xdebug ( "written %llu bytes in file %d with offset %ld", 
           (unsigned long long) written,
           id, offset ); 
  
  /* change outputBuffer */
  
  dbuffer_reset ( afd->db );
  
  if ( afd->db == afd->db1 )
    {
      xdebug ( "id=%d, change to buffer 2 ...", id );
      afd->db =  afd->db2;
    }
  else 
    {
      xdebug ( "id=%d, change to buffer 1 ...", id );
      afd->db =  afd->db1;
    }
  
  afd->command = IO_Set_fp;
}


/***************************************************************/

size_t fwPOSIXFPGUARDSENDRECV( int fileID, int tsID, const void *buffer, size_t len )
{
  static int oldTsID = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataPF *afd;
  listElem_t *curr;
  char errorString[maxErrorString];

  flush = ( tsID != oldTsID ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      xdebug ( "tsID = %d, flush buffer", tsID );
      curr = bibAFiledataPF->head;
      while ( curr ) 
	{
	  writePF (( aFiledataPF *) curr->val, curr->idx );
	  curr = curr->next;
	}
      oldTsID = tsID;
      xmpi ( MPI_Barrier ( commInqCommColl ())); 
    }                                          

  afd = ( aFiledataPF * ) queueIdx2val ( bibAFiledataPF, fileID );
  filled = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );

  xdebug ( "fileID = %d, tsID = %d, pushed data on buffer, filled = %d", 
           fileID, tsID, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  writePF ( afd, fileID );
     
	  error = dbuffer_push ( afd->db, ( unsigned char * ) buffer, len );
	}
    }
  
  if ( error == 1 )
    {
      sprintf ( errorString,"did not succeed filling output buffer, fileID=%d", fileID );
      xabort ( errorString );
    }
  
  return len;
}

/***************************************************************/

int fcPOSIXFPGUARDSENDRECV ( int id )
{
  aFiledataPF *afd;
  int iret;

  xdebug ( "write buffer, close file and cleanup", id );

  afd = ( aFiledataPF * ) queueIdx2val ( bibAFiledataPF, id );

  afd->command = IO_Close_file;

  writePF ( afd, id );

  /* dequeue file element */

  iret = queueDelNode ( bibAFiledataPF, id );

  if ( !bibAFiledataPF->head ) 
    {
      xdebug ( "cleanup queue" );
      queueDestroy ( bibAFiledataPF );
    }

  return iret;
}

/***************************************************************/
// baustelle, in modi vereinheitlichen, kapseln in pio_queue.c
void queueCheckFP ( queue_t *q, const char *name )
{
  listElem_t *curr;
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
  int root = 0, id; 
  static aFiledataPF *afd;
  static bool firstOpen = true;
  static long buffersize = 0;
  char errorString[maxErrorString];

  /* broadcast buffersize to collectors */
  
  if ( firstOpen )
    {      
      if  ( commInqRankColl () == root )
	{ 
          xdebug (  "name=%s, broadcast buffersize to collectors ...", 
                    filename );
	  if ( getenv( "BUFSIZE" ) != NULL )
	    buffersize = atol ( getenv ( "BUFSIZE" ));
	  if ( buffersize < initial_buffersize )
	    buffersize = initial_buffersize;
	}
      
      xmpi ( MPI_Bcast ( &buffersize, 1, MPI_LONG, root, commInqCommColl ()));
      
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

  xdebug ( "name=%s, init and enqueued aFiledataPF, return id = %d", 
           filename, id );
  
  return id;
}

/***************************************************************/

void initPOSIXFPGUARDSENDRECV ( void )
{
  int ncollectors;

  if ( commInqSizeNode () < 2 ) xabort ( "usage: #pes/#nodes >= 2" );
  
  if ( commInqRankNode () == commInqSpecialRankNode ()) 
    {
      commDefCommColl ( 0 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      ncollectors =  commInqSizeNode () - commInqSizeColl ();
      xdebug ( "Call fpGuard(), ncollectors = %d ...", ncollectors );
      fpgPOSIXFPGUARDSENDRECV ( ncollectors );
    }
  else
    {
      commDefCommColl ( 1 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      bibAFiledataPF = queueInit ( destroyAFiledataPF, compareNamesAPF );
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
