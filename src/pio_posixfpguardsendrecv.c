/* 
   todo 
   build in control, for consistance of pairs filename / filenumber 
   ( pioOpenFile member name, recv in tmpbuffer, if(!uniqueName(q,v,n))abort )
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <inttypes.h>
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

extern double accumProbe;
extern double accumRecv;
extern double accumSend;
extern double accumSuspend;
extern double accumWait;
extern double accumWrite; 

typedef struct 
{
  size_t size;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  FILE *fp;
  IO_Server_command command;
  int tsID, fileID;
  char name[];
} aFiledataPF;

static int
fileIDTestA(void *a, void *fileID)
{
  return ((aFiledataPF *)a)->fileID == (int)(intptr_t)fileID;
}

typedef struct 
{
  long offset;
  bool finished;
  bool *nfinished;
  int fileID;
} bFiledataPF;

static int
fileIDTestB(void *a, void *fileID)
{
  return ((bFiledataPF *)a)->fileID == (int)(intptr_t)fileID;
}

static bool
fileIDCmpB(void *a, void *b)
{
  return ((bFiledataPF *)a)->fileID == ((bFiledataPF *)b)->fileID;
}

static queue_t *bibAFiledataPF;

/***************************************************************/
  
static aFiledataPF *initAFiledataPF ( const char *key, size_t bs)
{
  aFiledataPF *afd;
  size_t len;
  int iret;

  len = strlen(key);
  afd = xcalloc(1, sizeof (*afd) + len + 1);
  strcpy(afd->name, key);
  afd->size = bs;
  afd->tsID = 0;

  /* init output buffer */

  xdebug ( " name=%s, init output buffer",  afd->name );
   
  iret = dbuffer_init ( &( afd->db1 ), afd->size );
  iret += dbuffer_init ( &( afd->db2 ), afd->size );

  if ( iret > 0 )
    xabort("dbuffer_init did not succeed");

  afd->db = afd->db1;

  /* open file */ 
  xdebug ( "name=%s, open file",  afd->name );

  if ( ( afd->fp = fopen ( afd->name, "w" )) == NULL ) 
    xabort("Failed to open %s", afd->name);

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

  /* close file */
  
  xdebug ( "name=%s, close file",  afd->name );
       
  if (( iret = fclose ( afd->fp )) == EOF )
    xabort("Failed to close %s", afd->name);

  /* file closed, cleanup */

  xdebug ( "name=%s, file closed, cleanup ...",  afd->name );
  
  dbuffer_cleanup ( &( afd->db1 ));
  dbuffer_cleanup ( &( afd->db2 ));

  free(afd);

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

void fpgPOSIXFPGUARDSENDRECV ( void )
{
  int i, source, iret;
  struct fileOpTag rtag;
  MPI_Status status;
  bFiledataPF *bfd; 
  queue_t *bibBFiledataPF;
  long amount;
  MPI_Comm commNode = commInqCommNode ();
  int nProcsCollNode =  commInqSizeNode () - commInqSizeColl ();
  bool * sentFinalize, doFinalize = false;

  xdebug ( "ncollectors=%d on this node", nProcsCollNode );
  
  bibBFiledataPF = queueInit ( destroyBFiledataPF, fileIDCmpB);
  sentFinalize = xmalloc ( nProcsCollNode * sizeof ( sentFinalize[0] ));

  for ( ;; )
    {
      xmpi ( MPI_Probe ( MPI_ANY_SOURCE, MPI_ANY_TAG, commNode, &status ));
      source = status.MPI_SOURCE;
      rtag = decodeFileOpTag(status.MPI_TAG);
      
      xdebug("receive message from source=%d, id=%d, command=%d ( %s )",
             source, rtag.id, rtag.command, command2charP[rtag.command]);
      
      switch (rtag.command)
      	{
      	case IO_Open_file:

          if (!(bfd = queueGet(bibBFiledataPF, fileIDTestB,
                               (void *)(intptr_t)rtag.id)))
	    {
	      bfd = initBFiledataPF(rtag.id, nProcsCollNode);

	      if ((iret = queueAdd(bibBFiledataPF, bfd)) < 0)
		xabort("fileID=%d not unique", rtag.id);
	    }

	  *( bfd->nfinished + source ) = false;

          xdebug("id=%d, command=%d ( %s ), send offset=%ld", rtag.id,
                 rtag.command, command2charP[rtag.command], bfd->offset);
	  
	  xmpi ( MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
                                &amount, 1, MPI_LONG, source,  status.MPI_TAG,
                                commNode, &status ));

	  bfd->offset += amount; 
 
          xdebug("id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld",
                 rtag.id, rtag.command, command2charP[rtag.command], amount,
                 bfd->offset);

	  break;

	case IO_Set_fp:

          if (!(bfd = queueGet(bibBFiledataPF, fileIDTestB,
                               (void *)(intptr_t)rtag.id)))
            xabort("fileId=%d not in queue", rtag.id);

          xdebug("id=%d, command=%d ( %s ), send offset=%ld", rtag.id,
                 rtag.command, command2charP[rtag.command], bfd->offset);

	  xmpi ( MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
                                &amount, 1, MPI_LONG, source,  status.MPI_TAG,
                                commNode, &status ));

	  bfd->offset += amount;

          xdebug("id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld",
                 rtag.id, rtag.command, command2charP[rtag.command], amount,
                 bfd->offset);

	  break;

	case IO_Close_file:

          if (!(bfd = queueGet(bibBFiledataPF, fileIDTestB,
                               (void *)(intptr_t)rtag.id)))
            xabort("fileId=%d not in queue", rtag.id);

          xdebug("id=%d, command=%d ( %s )), send offset=%ld", rtag.id,
                 rtag.command, command2charP[rtag.command], bfd->offset);

	  xmpi ( MPI_Sendrecv ( &( bfd->offset ), 1, MPI_LONG, source,  status.MPI_TAG,
                                &amount, 1, MPI_LONG, source,  status.MPI_TAG,
                                commNode, &status ));

	  bfd->offset += amount;

          xdebug("id=%d, command=%d ( %s ), recv amount=%ld, set offset=%ld",
                 rtag.id, rtag.command, command2charP[rtag.command], amount,
                 bfd->offset);


	  bfd->nfinished[source] = true;  
	  bfd->finished          = true;
	  
	  for ( i = 0; i < nProcsCollNode; i++ )
	    if ( !( bfd->nfinished[i] ))
	      {
		bfd->finished = false;
		break;
	      }

	  if ( bfd->finished )
            queueDelNode(bibBFiledataPF, fileIDTestB, (void *)(intptr_t)rtag.id);
          break;
        case IO_Finalize:
          {  
            int buffer = CDI_UNDEFID, collID; 

            xmpi ( MPI_Recv ( &buffer, 1, MPI_INT, source, status.MPI_TAG,
                              commNode, &status ));
            sentFinalize[source] = true;
            doFinalize = true;
            for ( collID = 0; collID < nProcsCollNode; collID++ )
              if ( !sentFinalize[collID] ) 
                {
                  doFinalize = false;
                  break;
                }
            if ( doFinalize )
              {
                if (!queueIsEmpty(bibBFiledataPF))
                  xabort("queue bibBFiledataM not empty");
                else
                  {
                    xdebug("%s", "cleanup queue");
                    queueDestroy ( bibBFiledataPF );
                  }
                return;
              }
          }
          break;
        default:
          xabort ( "COMMAND NOT IMPLEMENTED" );
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
  int specialRank = commInqSpecialRankNode ();
  MPI_Comm commNode = commInqCommNode ();
  
  /* send buffersize, recv offset */

  amount = dbuffer_data_size ( afd->db );
  amountL = ( long ) amount;

  tag = encodeFileOpTag(id, afd->command);
  
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
    xabort("fileId=%d, expect to write %zu byte, written %zu byte",
           id, amount, written);
 
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

void defTimestepPF ( aFiledataPF *afd, int tsID)
{
  if ( afd == NULL || tsID < 0 || tsID != afd->tsID + 1 ) 
    xabort ( " defTimestepPF() didn't succeed." );
  afd->tsID = tsID;
}


/***************************************************************/

static void
flushOp(void *a, void *tsID)
{
  writePF((aFiledataPF *)a, ((aFiledataPF *)a)->fileID);
  defTimestepPF((aFiledataPF *)a, (int)(intptr_t)tsID);
}


size_t fwPOSIXFPGUARDSENDRECV( int fileID, int tsID, const void *buffer, size_t len )
{
  int error = 0;
  int flush = 0;
  int filled = 0;
  aFiledataPF *afd;

  afd = queueGet(bibAFiledataPF, fileIDTestA, (void *)(intptr_t)fileID);

  flush = ( tsID != afd->tsID ) ? 1 : 0;

  if ( flush == 1 ) 
    {
      xdebug ( "tsID = %d, flush buffer", tsID );
      queueForeach(bibAFiledataPF, flushOp, (void *)(intptr_t)tsID);
      xmpi ( MPI_Barrier ( commInqCommColl ())); 
    }

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
    xabort("did not succeed filling output buffer, fileID=%d", fileID);
  
  return len;
}

/***************************************************************/

int fcPOSIXFPGUARDSENDRECV ( int id )
{
  aFiledataPF *afd;
  int iret;

  xdebug("write buffer, close file %d and cleanup", id);

  afd = queueGet(bibAFiledataPF, fileIDTestA, (void *)(intptr_t)id);

  afd->command = IO_Close_file;

  writePF ( afd, id );

  /* dequeue file element */

  iret = queueDelNode(bibAFiledataPF, fileIDTestA, (void *)(intptr_t)id);

  return iret;
}

/***************************************************************/
// baustelle, in modi vereinheitlichen, kapseln in pio_queue.c
static void
elemCheck(void *q, void *nm)
{
  aFiledataPF *afd = q;
  const char *name = nm;

  if (!strcmp(name, afd->name))
    xabort("Filename %s is already enqueued\n", name);
}

int fowPOSIXFPGUARDSENDRECV ( const char *filename )
{
  int root = 0, id; 
  static aFiledataPF *afd;
  static bool firstOpen = true;
  static long buffersize = 0;

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
  
  queueForeach(bibAFiledataPF, elemCheck, (void *)filename);

  afd = initAFiledataPF ( filename, buffersize );

  if ((id = queueAdd(bibAFiledataPF, afd)) < 0)
    xabort("filename %s not unique", afd->name);

  xdebug ( "name=%s, init and enqueued aFiledataPF, return id = %d", 
           filename, id );
  
  return id;
}

/***************************************************************/

void  finalizePOSIXFPGUARDSENDRECV ( void )
{
  int buffer = 0, tag, specialRank = commInqSpecialRankNode ();
  MPI_Comm commNode = commInqCommNode ();

  tag = encodeFileOpTag(0, IO_Finalize);

  xmpi ( MPI_Send ( &buffer, 1, MPI_INT, specialRank, tag, commNode ));

  if (!queueIsEmpty(bibAFiledataPF))
    xabort("queue bibAFiledataM not empty");
  else
    {
      xdebug("%s", "cleanup queue");
      queueDestroy ( bibAFiledataPF );
    }
}

/***************************************************************/

void initPOSIXFPGUARDSENDRECV ( void )
{
  if ( commInqSizeNode () < 2 ) 
    xabort ( "USAGE: # IO PROCESSES ON A PHYSICAL NODE >= 2" );
  
  if ( commInqRankNode () == commInqSpecialRankNode ()) 
    {
      commDefCommColl ( 0 );
      commSendNodeInfo ();
      commRecvNodeMap ();
      commDefCommsIO ();
      fpgPOSIXFPGUARDSENDRECV ();
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
