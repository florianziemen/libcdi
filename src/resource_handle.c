#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600 /* PTHREAD_MUTEX_RECURSIVE */
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

static void listInitialize(void);

// ATTENTION: not thread safe yet, namespaces are set in model!

#if  defined  (HAVE_LIBPTHREAD)
#  include <pthread.h>

static pthread_once_t  listInitThread = PTHREAD_ONCE_INIT;
static pthread_mutex_t listMutex;

#  define LIST_LOCK()         pthread_mutex_lock(&listMutex)
#  define LIST_UNLOCK()       pthread_mutex_unlock(&listMutex)
#  define LIST_INIT()         pthread_once(&listInitThread, listInitialize)

#else

static int listInit = 0;

#  define LIST_LOCK()
#  define LIST_UNLOCK()
#  define LIST_INIT()        do {                               \
  if ( !listInit )                                              \
    {                                                           \
      listInitialize();                                         \
      listInit = 1;                                             \
    }                                                           \
  } while(0)

#endif


typedef struct listElem {
  cdiResH       resH;//idx
  int next;
  resOps      * ops;
  void        * val;//ptr
  int           status;
} listElem_t;

static struct
{
  int size, freeHead;
  listElem_t *resources;
} *resHList;

/**************************************************************/

static void
listInitResources(int nsp)
{
  xassert(nsp < namespaceGetNumber() && nsp >= 0);
  listElem_t *p = resHList[nsp].resources;

  int size = resHList[nsp].size;
  for (int i = 0; i < size; i++ )
    {
      p[i].resH   = namespaceIdxEncode2(nsp, i);
      p[i].next   = i + 1;
      p[i].ops    = NULL;
      p[i].val    = NULL;
      p[i].status = RESH_UNDEFID;
    }

  p[resHList[nsp].size-1].next = -1;
  resHList[nsp].freeHead = 0;
}

static
void listNew ( void )
{
  int nnsp;

  nnsp = namespaceGetNumber ();

  resHList = xcalloc ( nnsp, sizeof (resHList[0]));

  for (int i = 0; i < nnsp; i++ )
    {
      resHList[i].size = MIN_LIST_SIZE;

      xassert ( resHList[i].resources == NULL);

      resHList[i].resources = xcalloc ( MIN_LIST_SIZE, sizeof (listElem_t));
      listInitResources(i);
    }
}

/**************************************************************/

static void listDestroy ( void )
{
  if (resHList)
    {
      int nnsp = namespaceGetNumber ();
      for (int i = 0; i < nnsp; ++i)
	{
	  pioNamespaceSetActive(i);
          if (resHList[i].resources)
            {
              for ( int j = 0; j < resHList[i].size; j++ )
                {
                  listElem_t *listElem = resHList[i].resources + j;
                  if (listElem->val)
                    listElem->ops->valDestroy(listElem->val);
                }
              free(resHList[i].resources);
              resHList[i].resources = NULL;
            }
	}
      free(resHList);
      resHList = NULL;
    }
}

/**************************************************************/

static
void listInitialize ( void )
{
#if  defined  (HAVE_LIBPTHREAD)
  pthread_mutexattr_t ma;
  pthread_mutexattr_init(&ma);
  pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
  /* initialize global API mutex lock */
  pthread_mutex_init ( &listMutex, &ma);
  pthread_mutexattr_destroy(&ma);
#endif

  LIST_LOCK();
  listNew ();
  LIST_UNLOCK();
  /* file is special and has its own table, which needs to be
   * created, before we register the listDestroy exit handler */
  {
    int null_id;
    null_id = fileOpen("/dev/null", "r");
    if (null_id != -1)
      fileClose(null_id);
  }
  atexit ( listDestroy );

}

/**************************************************************/

static
void listSizeExtend()
{
  int nsp = namespaceGetActive ();
  int oldSize = resHList[nsp].size;
  int newListSize = oldSize + MIN_LIST_SIZE;

  resHList[nsp].resources =
    xrealloc(resHList[nsp].resources,
             newListSize * sizeof (resHList[0].resources[0]));

  for (int i = oldSize; i < newListSize; ++i)
    {
      resHList[nsp].resources[i].resH = namespaceIdxEncode2 ( nsp, i );
      resHList[nsp].resources[i].next = i + 1;
    }

  resHList[nsp].resources[newListSize-1].next = resHList[nsp].freeHead;
  resHList[nsp].freeHead = oldSize;
  resHList[nsp].size = newListSize;
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

  if ( resHList[nsp].freeHead == -1) listSizeExtend();
  newListElem               = resHList[nsp].resources + resHList[nsp].freeHead;
  resHList[nsp].freeHead    = newListElem->next;
  newListElem->next         = -1;
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

  LIST_INIT();

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( nspT.nsp == nsp &&
            nspT.idx >= 0 &&
            nspT.idx < resHList[nsp].size &&
            resHList[nsp].resources[nspT.idx].ops &&
            resHList[nsp].resources[nspT.idx].ops == ops );

  resHList[nsp].resources[nspT.idx].next   = resHList[nsp].freeHead;
  resHList[nsp].resources[nspT.idx].ops    = NULL;
  resHList[nsp].resources[nspT.idx].val    = NULL;
  resHList[nsp].resources[nspT.idx].status = RESH_UNDEFID;
  resHList[nsp].freeHead                   = nspT.idx;

  LIST_UNLOCK();
}

/**************************************************************/

void *reshGetValue(const char * caller, cdiResH resH, resOps * ops)
{
  int nsp;
  namespaceTuple_t nspT;
  listElem_t * listElem;

  xassert ( ops );

  LIST_INIT();

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  if (nspT.nsp == nsp &&
      nspT.idx >= 0 &&
      nspT.idx < resHList[nsp].size)
    {
      listElem = resHList[nsp].resources + nspT.idx;
      LIST_UNLOCK();
    }
  else
    {
      LIST_UNLOCK();
      xabortC(caller, "Invalid namespace %d or index %d for resource handle %d!",
              nspT.nsp, nspT.idx, (int)resH);
    }

  if ( !(listElem && listElem->ops == ops) )
    xabortC(caller, "Invalid resource handle %d, list element not found!",
            (int)resH);

  return listElem->val;
}

/**************************************************************/

void reshGetResHListOfType ( int c, int * resHs, resOps * ops )
{
  int i, j = 0, nsp;

  xassert ( resHs && ops );

  LIST_INIT();

  LIST_LOCK();

  nsp = namespaceGetActive ();

  for ( i = 0; i < resHList[nsp].size; i++ )
    if ( resHList[nsp].resources[i].val && resHList[nsp].resources[i].ops )
      if ( resHList[nsp].resources[i].ops == ops )
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

  LIST_INIT();

  LIST_LOCK();

  nsp = namespaceGetActive ();

  for ( i = 0; i < resHList[nsp].size; i++ )
    if ( resHList[nsp].resources[i].val )
      if ( resHList[nsp].resources[i].ops == ops )
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
  for ( i = 0; i < resHList[nsp].size; i++ )
    if ( resHList[nsp].resources[i].val )
      if ( resHList[nsp].resources[i].status == ASSIGNED )
        {
          curr = resHList[nsp].resources + i;
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

  for ( i = 0; i < resHList[nsp].size; i++ )
    if ( resHList[nsp].resources[i].val )
      if ( resHList[nsp].resources[i].status == ASSIGNED )
        {
          curr = resHList[nsp].resources + i;
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

  xassert ( ops );

  LIST_INIT();

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( nspT.nsp == nsp &&
            nspT.idx >= 0 &&
            nspT.idx < resHList[nsp].size );

  listElem = resHList[nsp].resources + nspT.idx;

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

  xassert ( ops );

  LIST_INIT();

  LIST_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( nspT.nsp == nsp &&
            nspT.idx >= 0 &&
            nspT.idx < resHList[nsp].size );

  listElem = resHList[nsp].resources + nspT.idx;

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
#ifdef USE_MPI
  const int differ = -1;
  int i, valCompare;
  listElem_t * listElem0, * listElem1;


  LIST_INIT();

  xassert(namespaceGetNumber () > xmaxInt ( nsp0, nsp1 ) &&
          xminInt ( nsp0, nsp1 ) >= 0 );

  for ( i = 0; i < resHList[nsp0].size; i++ )
    {
      listElem0 = resHList[nsp0].resources + i;
      if ( listElem0->val )
	{
	  if ( i >= resHList[nsp1].size )
	    {
	      xdebug("%s", "");
	      return differ;
	    }

	  listElem1 = resHList[nsp1].resources + i;
	  if ( !listElem1->val )
	    {
	      xdebug("%s", "");
	      return differ;
	    }

	  xassert ( listElem0->ops && listElem1->ops );
	  if ( listElem0->ops != listElem1->ops )
	    {
	      xdebug("%s", "");
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
      else if ( resHList[nsp1].resources[i].val )
	return differ;
    }

  for ( ; i < resHList[nsp1].size; i++ )
    if ( resHList[nsp1].resources[i].val )
      {
	xdebug("%s", "");
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

  LIST_INIT();

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
	  xdebug("%s", "could not open file" );
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

      fprintf ( fp, "resHList[%d].size=%d\n", i, resHList[i].size );

      for ( j = 0; j < resHList[i].size; j++ )
        {
          curr = resHList[i].resources + j;
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


void cdiReset(void)
{
  LIST_LOCK();

  listDestroy();

  listNew ();

  LIST_UNLOCK();
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
