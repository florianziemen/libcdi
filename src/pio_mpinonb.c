#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mpi.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_impl.h"
#include "pio_util.h"

extern char * command2charP[6];

extern long initial_buffersize;

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
  MPI_File fh;
  MPI_Request request;
  int fileID;
  bool finished;
} aFiledataM;

static queue_t *bibAFiledataM;

static int
fileIDTest(void *a, void *fileID)
{
  return ((aFiledataM *)a)->fileID == (int)(intptr_t)fileID;
}


/***************************************************************/

static aFiledataM *initAFiledataMPINONB ( const char *filename, size_t bs )
{
  aFiledataM *of = NULL;
  size_t len;
  int iret;
  MPI_Comm commNode = commInqCommNode ();
  char * endString = "\0";

  of = xmalloc ( sizeof ( of[0] ));

  len = strlen ( filename );
  of->name = xmalloc (( len + 1 ) * sizeof ( of->name[0] ));
  memcpy ( of->name, filename, len );
  memcpy ( of->name + len, endString, 1 );
  of->size = bs;
  of->db1 = NULL;
  of->db2 = NULL;

  /* init output buffer */
   
  iret = dbuffer_init ( &( of->db1 ), of->size );
  iret += dbuffer_init ( &( of->db2 ), of->size );

  if ( iret > 0 ) xabort ( "dbuffer_init did not succeed" );

  of->db = of->db1;

  /* open file */ 
  xmpi ( MPI_File_open ( commNode, of->name, 
			 ( MPI_MODE_CREATE|MPI_MODE_WRONLY ), 
			 MPI_INFO_NULL, &( of->fh )));
  of->request = MPI_REQUEST_NULL;
  of->finished = false;
  
  return of;
}

/***************************************************************/

int destroyAFiledataMPINONB ( void *v )
{
  int iret = 0;
  aFiledataM *of;
  MPI_Status status;
  int rankNode = commInqRankNode ();
  double startTime;

  of = (aFiledataM * ) v;

  xdebug ( "IOPE%d: name=%s, close file, in", 
           rankNode, of->name );
  
  /* close file */

  startTime = MPI_Wtime ();
  xmpi ( MPI_Wait ( & ( of->request ), &status )); 
  xmpi(MPI_Barrier(commInqCommNode()));
  accumWait += ( MPI_Wtime () - startTime );
  iret = MPI_File_sync ( of->fh ); 
  iret = MPI_File_close ( & ( of->fh ));

  /* file closed, cleanup */
  
  dbuffer_cleanup ( & ( of->db1 ));
  dbuffer_cleanup ( & ( of->db2 ));

  free ( of->name );
  free ( of );

  xdebug ( "IOPE%d: closed file, cleaned up, return", 
           rankNode );

  return iret == MPI_SUCCESS ? 0 : -1;
}

/***************************************************************/

bool compareNamesMPINONB  ( void *v1, void *v2 )
{
  aFiledataM *afm1, *afm2;
  size_t len1, len2;
  bool bret;

  afm1 = ( aFiledataM * ) v1;
  afm2 = ( aFiledataM * ) v2;

  len1 = strlen ( afm1->name );
  len2 = strlen ( afm2->name );
  bret = ( len1 == len2 && 
	  memcmp ( afm1->name, afm2->name, len1 ) == 0 );

  return bret;
}

/***************************************************************/

void writeMPINONB(aFiledataM *of)
{
  int amount;
  MPI_Status status;
  int rankNode = commInqRankNode ();
  int fileID = of->fileID;

  /* write buffer */
  
  amount = ( int ) dbuffer_data_size ( of->db );

  if ( amount == 0 ) return;

  xdebug3 ( "IOPI%d: Write buffer, size %d bytes, in", 
           rankNode, amount );
  
  xmpi ( MPI_Wait ( & ( of->request ), &status ));
  xmpi ( MPI_File_iwrite_shared ( of->fh, of->db->buffer, amount, MPI_CHAR, 
                                  & ( of->request )));
  xdebug ( "%d bytes written for fileID=%d", amount, fileID );

  /* change outputBuffer */
  
  dbuffer_reset ( of->db );
  
  if ( of->db == of->db1 )
    {
        xdebug3 ( "IOPE%d: fileID=%d, change to buffer 2 ...", 
                 rankNode, fileID );
      of->db =  of->db2;
    }
  else 
    {
        xdebug3 ( "IOPE%d: fileID=%d, change to buffer 1 ...", 
		  rankNode, fileID );
      of->db =  of->db1;
    }

  return;
}

/***************************************************************/

size_t fwMPINONB ( int fileID, int tsID, const void *buffer, size_t len )
{  
  static int oldTsID = 0;
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataM *of;
  MPI_Comm commNode = commInqCommNode ();
  int rankNode = commInqRankNode ();

  flush = ( tsID != oldTsID ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      xdebug3 ( "IOPE%d: tsID = %d, flush buffer", 
		  rankNode, tsID );
      queueForeach(bibAFiledataM, (elemOp)writeMPINONB, NULL);
      oldTsID = tsID;
      xmpi ( MPI_Barrier ( commNode )); 
    }                                          

  of = queueGet(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID);
  filled = dbuffer_push ( of->db, ( unsigned char * ) buffer, len );

  xdebug3 ( "IOPE%d: fileID = %d, tsID = %d,"
           " pushed data on buffer, filled = %d", 
           rankNode, fileID, tsID, filled ); 

  if ( filled == 1 ) 
    {
      if ( flush )
	error = filled;
      else
	{
	  writeMPINONB(of);
     
	  error = dbuffer_push ( of->db, ( unsigned char * ) buffer, len );
	}
    }
  
  if ( error == 1 )
    xabort("did not succeed filling output buffer, fileID=%d", fileID);

  return len;
}

/***************************************************************/

int fcMPINONB ( int fileID )
{
  aFiledataM *of;
  int iret;
  double accumWaitMax;
  MPI_Comm commNode = commInqCommNode ();
  int rankNode = commInqRankNode ();

  xdebug("IOPE%d: write buffer, close file and cleanup, in %d",
         rankNode, fileID );

  if (!(of = queueGet(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID)))
    xabort("pio_queue, fileID=%d not found", fileID);

  writeMPINONB(of);

  /* dequeue file element */

  iret = queueDelNode(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID);

  /* timer output */

  if ( ddebug == MAXDEBUG )
    {
      xmpi ( MPI_Reduce ( &accumWait, &accumWaitMax, 
                          1, MPI_DOUBLE, MPI_MAX, 0, commNode  ));
      xdebug ( "IOPE%d: Wait time %15.10lf s",
               rankNode, accumWait );
      if ( rankNode == 0 )
          xdebug ( "IOPE%d: Max wait time %15.10lf s",
                   rankNode, accumWaitMax );
    }

  return iret;
}

/***************************************************************/
// baustelle, in modi vereinheitlichen, kapseln in pio_queue.c
static void
elemCheck(void *q, void *nm)
{
  aFiledataM *afm = q;
  const char *name = nm;

  if (!strcmp(name, afm->name))
    xabort("Filename %s is already enqueued\n", name);
}


int fowMPINONB ( const char *filename )
{
  static aFiledataM *of;
  static long buffersize = 0; 
  int id, bcastRank = 0; 
  MPI_Comm commNode = commInqCommNode ();
  int rankNode = commInqRankNode ();

  /* broadcast buffersize to collectors ( just once, for all files )*/
  
  if ( ! buffersize )
    {
        xdebug ( "IOPE%d: Broadcast buffersize to collectors ...", 
		  rankNode );
      
      if  ( rankNode == bcastRank )
	{ 
	  if ( getenv( "BUFSIZE" ) != NULL )
	    buffersize = atol ( getenv ( "BUFSIZE" ));
	  if ( buffersize < initial_buffersize )
	    buffersize = initial_buffersize;
	}
      
      xmpi ( MPI_Bcast ( &buffersize, 1, MPI_LONG, bcastRank, commNode ));
    }

  xdebug("buffersize=%ld", buffersize);

  queueForeach(bibAFiledataM, elemCheck, (void *)filename);
  of = initAFiledataMPINONB ( filename, buffersize );

  if ((id = queueAdd(bibAFiledataM, of)) < 0 )
    xabort("filename %s not unique", of->name);

  xdebug ( "IOPE%d: name=%s, init and enqueued aFiledataM, return id = %d", 
           rankNode, filename, id );
  
  return id;
}

/***************************************************************/

void finalizeMPINONB(void)
{
  if (!queueIsEmpty(bibAFiledataM))
    xabort("queue bibAFiledataM not empty");
  else
    {
      xdebug("%s", "cleanup queue");
      queueDestroy(bibAFiledataM);
    }
}

/***************************************************************/

void initMPINONB ( void )
{
  commDefCommColl ( 1 );
  commSendNodeInfo ();
  commRecvNodeMap ();
  commDefCommsIO ();

  bibAFiledataM = queueInit ( destroyAFiledataMPINONB, compareNamesMPINONB );
  
  if ( bibAFiledataM == NULL )
    xabort ( "queueInit did not succeed" );   
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
