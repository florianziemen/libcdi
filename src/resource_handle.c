#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "resource_handle.h"
#include "pio_util.h"
#include "namespace.h"
#include "cdi.h"
#include "error.h"
#include "file.h"
#ifdef USE_MPI
#include "pio_comm.h"
#include "pio_rpc.h"
#endif


enum { MIN_LIST_SIZE = 128 };

static int *listSizeAllocated;

static void listInitialize(void);

static int listInit = 0;

// ATTENTION: not thread safe yet, namespaces are set in model!

#if  defined  (HAVE_LIBPTHREAD)
#  include <pthread.h>

static pthread_once_t  listInitThread = PTHREAD_ONCE_INIT;
static pthread_mutex_t listMutex;

#  define LIST_LOCK()         pthread_mutex_lock(&listMutex)
#  define LIST_UNLOCK()       pthread_mutex_unlock(&listMutex)
#  define LIST_INIT()        \
  if ( !listInit ) pthread_once(&listInitThread, listInitialize)

#else

#  define LIST_LOCK()
#  define LIST_UNLOCK()
#  define LIST_INIT()        \
  if ( !listInit ) listInitialize()

#endif


typedef struct listElem {
  cdiResH       resH;//idx
  struct listElem * next;
  resOps      * ops;
  void        * val;//ptr
  int           status;
} listElem_t;


static listElem_t **listResources;
static listElem_t **freeListHead;

/**************************************************************/

static
void listNew ( void )
{
  int nnsp, i;

  nnsp = namespaceGetNumber ();

  listSizeAllocated = xcalloc ( nnsp, sizeof( listSizeAllocated[0]));
  listResources     = xcalloc ( nnsp, sizeof( listResources[0]));
  freeListHead      = xcalloc ( nnsp, sizeof( freeListHead[0]));

  for ( i = 0; i < nnsp; i++ )
    {
      listSizeAllocated[i] = MIN_LIST_SIZE;

      xassert ( listResources[i] == NULL);

      listResources[i] = xcalloc ( listSizeAllocated[i], sizeof ( listElem_t ));
    }
}

/**************************************************************/

static
void listInitPointer ( void )
{
  int nnsp, i, j;

  nnsp = namespaceGetNumber ();

  for ( i = 0; i < nnsp; i++ )
    {
      for ( j = 0; j < listSizeAllocated[i]; j++ )
        {
          listResources[i][j].resH   = namespaceIdxEncode2 ( i, j );
          listResources[i][j].next   = listResources[i] + j + 1;
          listResources[i][j].ops    = NULL;
          listResources[i][j].val    = NULL;
          listResources[i][j].status = RESH_UNDEFID;
        }

      listResources[i][listSizeAllocated[i]-1].next = NULL;

      freeListHead[i] = listResources[i];
    }
}

/**************************************************************/

void listDestroy ( void )
{
  listElem_t * listElem;
  int i, j, nnsp;

  if ( listResources )
    {
      nnsp = namespaceGetNumber ();
      for ( i = 0; i < nnsp; i++ )
	{
	  pioNamespaceSetActive ( i );
          if ( listResources[i] )
            {
              if ( listSizeAllocated )
                for ( j = 0; j < listSizeAllocated[i]; j++ )
                  {
                    listElem = listResources[i] + j;
                    if ( listElem->val )
                      listElem->ops->valDestroy ( listElem->val );
                  }
              free ( listResources[i] );
              listResources[i] = NULL;
            }
	}
      free ( listResources );
      listResources = NULL;
    }

  if ( freeListHead )
    {
      free ( freeListHead );
      freeListHead = NULL;
    }
  if ( listSizeAllocated )
    {
      free ( listSizeAllocated );
      listSizeAllocated = NULL;
    }
}

/**************************************************************/

static
void listInitialize ( void )
{
#if  defined  (HAVE_LIBPTHREAD)
  /* initialize global API mutex lock */
  pthread_mutex_init ( &listMutex, NULL);
#endif

  listNew ();
  /* file is special and has its own table, which needs to be
   * created, before we register the listDestroy exit handler */
  {
    int null_id;
    null_id = fileOpen("/dev/null", "r");
    if (null_id /= -1)
      fileClose(null_id);
  }
  atexit ( listDestroy );

  LIST_LOCK();

  listInitPointer ();

  LIST_UNLOCK();

  listInit = 1;
}

/**************************************************************/

void listSizeExtend()
{
  int newListSize;
  int i, nsp;

  nsp = namespaceGetActive ();

  xassert ( listInit );

  newListSize = listSizeAllocated[nsp] + MIN_LIST_SIZE;

  listResources[nsp] =
    xrealloc(listResources[nsp], newListSize * sizeof (listResources[0][0]));

  for ( i = listSizeAllocated[nsp]; i < newListSize; ++i )
    {
      listResources[nsp][i].resH = namespaceIdxEncode2 ( nsp, i );
      listResources[nsp][i].next = listResources[nsp] + i + 1;
    }

  listResources[nsp][newListSize-1].next = freeListHead[nsp];
  freeListHead[nsp] = listResources[nsp] + listSizeAllocated[nsp];
  listSizeAllocated[nsp] = newListSize;
}

/**************************************************************/

int reshPut ( void *p, resOps *ops )
{
  cdiResH resH = -1, nsp;
  listElem_t * newListElem;

  xassert ( p && ops );

  LIST_INIT();

  LIST_LOCK();

  nsp = namespaceGetActive ();

  if ( !freeListHead[nsp] ) listSizeExtend();
  newListElem               = freeListHead[nsp];
  freeListHead[nsp]         = freeListHead[nsp]->next;
  newListElem->next         = NULL;
  resH                      = newListElem->resH;
  newListElem->val          = p;
  newListElem->ops          = ops;
  newListElem->status       = ASSIGNED;

  LIST_UNLOCK();

  return resH;
}

/**************************************************************/

void reshRemove ( cdiResH resH, resOps * ops )
{
  int nsp;
  namespaceTuple_t nspT;

  xassert ( listInit && ops );

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( nspT.nsp == nsp &&
            nspT.idx >= 0 &&
            nspT.idx < listSizeAllocated[nsp] &&
            listResources[nsp][nspT.idx].ops &&
            listResources[nsp][nspT.idx].ops == ops );

  listResources[nsp][nspT.idx].next   = freeListHead[nsp];
  listResources[nsp][nspT.idx].ops    = NULL;
  listResources[nsp][nspT.idx].val    = NULL;
  listResources[nsp][nspT.idx].status = RESH_UNDEFID;
  freeListHead[nsp]                   = listResources[nsp] + nspT.idx;

  LIST_UNLOCK();
}

/**************************************************************/

void *reshGetVal ( cdiResH resH, resOps * ops  )
{
  int nsp;
  namespaceTuple_t nspT;
  listElem_t * listElem;

  xassert ( listInit && ops );

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( listInit &&
            nspT.nsp == nsp &&
            nspT.idx >= 0 &&
            nspT.idx < listSizeAllocated[nsp] );

  listElem = listResources[nsp] + nspT.idx;

  LIST_UNLOCK();

  xassert ( listElem &&
            listElem->ops == ops );

  return listElem->val;
}

/**************************************************************/

void reshGetResHListOfType ( int c, int * resHs, resOps * ops )
{
  int i, j = 0, nsp;

  xassert ( listInit && resHs && ops );

  LIST_LOCK();

  nsp = namespaceGetActive ();

  for ( i = 0; i < listSizeAllocated[nsp]; i++ )
    if ( listResources[nsp][i].val && listResources[nsp][i].ops )
      if ( listResources[nsp][i].ops == ops )
        {
          resHs[j++] = namespaceIdxEncode2 ( nsp, i );
          if ( j == c ) break;
        }

  LIST_UNLOCK();
}

/**************************************************************/

int reshCountType ( resOps * ops )
{
  int i, nsp, countType = 0;

  xassert ( ops );

  if ( !( listInit && ops )) return 0;

  LIST_LOCK();

  nsp = namespaceGetActive ();

  for ( i = 0; i < listSizeAllocated[nsp]; i++ )
    if ( listResources[nsp][i].val )
      if ( listResources[nsp][i].ops == ops )
        countType++;

  LIST_UNLOCK();

  return countType;
}

/**************************************************************/

#ifdef USE_MPI
static
int  getPackBufferSize ( MPI_Comm comm )
{
  int nsp, i;
  int packBufferSize = 0;
  int intpacksize;
  listElem_t * curr;

  nsp = namespaceGetActive ();

  xmpi ( MPI_Pack_size ( 1, MPI_INT, comm, &intpacksize ));

  /* pack start marker, namespace and sererator marker */
  packBufferSize += 3 * intpacksize;

  /* pack resources, type marker and seperator marker */
  for ( i = 0; i < listSizeAllocated[nsp]; i++ )
    if ( listResources[nsp][i].val )
      if ( listResources[nsp][i].status == ASSIGNED )
        {
          curr = listResources[nsp] + i;
          xassert ( curr->ops );

          /* message plus frame of 2 ints */
          packBufferSize += curr->ops->valGetPackSize ( curr->val, comm )
            + 2 * intpacksize;
        }

  /* end marker */
  packBufferSize += intpacksize;

  return packBufferSize;
}

/**************************************************************/

void reshPackBufferDestroy ( char ** buffer )
{
  if ( buffer ) free ( *buffer );
}

/**************************************************************/

void reshPackBufferCreate ( char ** packBuffer, int * packBufferSize, MPI_Comm comm )
{
  int i, nsp, packBufferPos = 0;
  int start = START, end = END, sep = SEPARATOR, type;
  listElem_t * curr;

  xassert ( packBuffer );

  LIST_LOCK();

  nsp = namespaceGetActive ();

  * packBufferSize = getPackBufferSize ( comm );
  * packBuffer = xcalloc ( 1, * packBufferSize );

  xmpi ( MPI_Pack ( &start, 1,  MPI_INT,
		    * packBuffer, * packBufferSize, &packBufferPos, comm ));

  xmpi ( MPI_Pack ( &nsp, 1,  MPI_INT,
		    * packBuffer, * packBufferSize, &packBufferPos, comm ));

  xmpi ( MPI_Pack ( &sep, 1,  MPI_INT,
		    * packBuffer, * packBufferSize, &packBufferPos, comm ));

  for ( i = 0; i < listSizeAllocated[nsp]; i++ )
    if ( listResources[nsp][i].val )
      if ( listResources[nsp][i].status == ASSIGNED )
        {
          curr = listResources[nsp] + i;
          xassert ( curr->ops );

          type = curr->ops->valTxCode ();

          if ( ! type ) continue;

          xmpi ( MPI_Pack ( &type, 1, MPI_INT, * packBuffer,
			    * packBufferSize, &packBufferPos, comm ));

          curr->ops->valPack ( curr->val,
                               * packBuffer ,
                               * packBufferSize,
                               &packBufferPos,
                               comm );

          xmpi ( MPI_Pack ( &sep, 1,  MPI_INT, * packBuffer,
			    * packBufferSize, &packBufferPos, comm ));

          curr->status = CLOSED;
        }

  LIST_UNLOCK();

  xmpi ( MPI_Pack ( &end, 1,  MPI_INT,
                  * packBuffer, * packBufferSize, &packBufferPos, comm ));
}

#endif

/**************************************************************/

/* for thread safety this feature would have to be integrated in reshPut */

void reshSetStatus ( cdiResH resH, resOps * ops, int status )
{
  int nsp;
  namespaceTuple_t nspT;
  listElem_t * listElem;

  xassert ( listInit && ops );

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( listInit &&
            nspT.nsp == nsp &&
            nspT.idx >= 0 &&
            nspT.idx < listSizeAllocated[nsp] );

  listElem = listResources[nsp] + nspT.idx;

  xassert ( listElem &&
            listElem->ops == ops );

  listElem->status = status;

  LIST_UNLOCK();
}

/**************************************************************/

int reshGetStatus ( cdiResH resH, resOps * ops )
{
  int nsp;
  namespaceTuple_t nspT;
  listElem_t * listElem;

  xassert ( listInit && ops );

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( listInit &&
            nspT.nsp == nsp &&
            nspT.idx >= 0 &&
            nspT.idx < listSizeAllocated[nsp] );

  listElem = listResources[nsp] + nspT.idx;

  LIST_UNLOCK();

  xassert ( listElem &&
            listElem->ops == ops );

  return listElem->status;
}

/**************************************************************/

void reshLock ()
{
  LIST_LOCK();
}

/**************************************************************/

void reshUnlock ()
{
  LIST_UNLOCK();
}

/**************************************************************/

int reshListCompare ( int nsp0, int nsp1 )
{
  const int equal  = 0;
  const int differ = -1;
  int i, valCompare;
  listElem_t * listElem0, * listElem1;

#ifdef USE_MPI

  xassert ( listInit && 
	   namespaceGetNumber () > xmaxInt ( nsp0, nsp1 ) &&
	   xminInt ( nsp0, nsp1 ) >= 0 );

  for ( i = 0; i < listSizeAllocated[nsp0]; i++ )
    {
      listElem0 = listResources[nsp0] + i;
      if ( listElem0->val )
	{
	  if ( i >= listSizeAllocated[nsp1] )
	    {
	      xdebug ();
	      return differ;
	    }

	  listElem1 = listResources[nsp1] + i;
	  if ( !listElem1->val )
	    {
	      xdebug ();
	      return differ;
	    }

	  xassert ( listElem0->ops && listElem1->ops );
	  if ( listElem0->ops != listElem1->ops )
	    {
	      xdebug ();
	      return differ;
	    }

	  valCompare =  listElem0->ops->valCompare ( listElem0->val, listElem1->val );
	  printf ( "type %d, values resH0=%d and resH1=%d %s\n", 
		   listElem0->ops->valTxCode (), 
		   listElem0->resH, 
		   listElem1->resH,
		   valCompare == equal ? "are equal" : "differ" );
	  if ( valCompare != equal ) return differ;
	}
      else if ( listResources[nsp1][i].val )
	return differ;
    }

  for ( ; i < listSizeAllocated[nsp1]; i++ )
    if ( listResources[nsp1][i].val )
      {
	xdebug ();
	return differ;
      }

#endif

  return equal;
}

/**************************************************************/

void reshListPrint ( char * filename )
{
  int i, j, temp;
  listElem_t * curr;
  FILE * fp;

  if ( ! listInit ) return;

#ifdef USE_MPI
  {
    int root = 0;
    if ( commInqIsProcIO () == 0 )
      {
        if (  commInqRankModel () != root )  return;
      }
    else if ( commInqRankPio () != root ) return;
  }
#endif

  if ( filename )
    {
      fp = fopen ( filename, "w" );
      if ( ! fp )
	{
	  xdebug ( "could not open file" );
	  fp = stdout;
	}
    }
  else
    fp = stdout;

  temp = namespaceGetActive ();

  fprintf ( fp, "\n\n##########################################\n#\n#  print " \
            "global resource list \n#\n" );

  for ( i = 0; i < namespaceGetNumber (); i++ )
    {
      pioNamespaceSetActive ( i );

      fprintf ( fp, "\n" );
      fprintf ( fp, "##################################\n" );
      fprintf ( fp, "#\n" );
      fprintf ( fp, "# namespace=%d\n", i );
      fprintf ( fp, "#\n" );
      fprintf ( fp, "##################################\n\n" );

      fprintf ( fp, "listSizeAllocated[%d]=%d\n", i, listSizeAllocated[i] );

      for ( j = 0; j < listSizeAllocated[i]; j++ )
        {
          curr = listResources[i] + j;
          if ( curr->ops && curr->val )
            {
              curr->ops->valPrint (( void * ) curr->val, fp );
              fprintf ( fp, "\n" );
            }
        }
    }
  fprintf ( fp, "#\n#  end global resource list" \
            "\n#\n##########################################\n\n" );

  fclose ( fp );;
  pioNamespaceSetActive ( temp );
}

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
