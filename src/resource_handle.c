#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "resource_handle.h"
#include "pio_util.h"
#include "namespace.h"
#include "cdi.h"
#include "error.h"

extern resOps gridOps;

enum { MIN_ARRAY_SIZE = 128 };

static int *arraySizeAllocated;

static void arrayInitialize(void);

static int arrayInit = 0;

#if  defined  (HAVE_LIBPTHREAD)
#  include <pthread.h>

static pthread_once_t  arrayInitThread = PTHREAD_ONCE_INIT;
static pthread_mutex_t arrayMutex;

#  define ARRAY_LOCK()         pthread_mutex_lock(&arrayMutex)
#  define ARRAY_UNLOCK()       pthread_mutex_unlock(&arrayMutex)
#  define ARRAY_INIT()        \
  if ( !arrayInit ) pthread_once(&arrayInitThread, arrayInitialize)

#else

#  define ARRAY_LOCK()
#  define ARRAY_UNLOCK()
#  define ARRAY_INIT()        \
  if ( !arrayInit ) arrayInitialize()

#endif


typedef struct node {
  int          idx;
  struct node *next;
  resOps      *ops;
  void        *ptr;
} node;


static node **arrayResources;
static node **freeListHead;


/**************************************************************/


static
void arrayNew ( void )
{
  int nnsp, i;

  nnsp = namespaceGetNumber ();

  arraySizeAllocated  = xcalloc ( nnsp,
                                 sizeof ( arraySizeAllocated[0]));
  arrayResources  = xcalloc ( nnsp, sizeof ( arrayResources[0] ));
  freeListHead = xcalloc ( nnsp, sizeof( freeListHead[0] ));

  for ( i = 0; i < nnsp; i++ )
    {
      arraySizeAllocated[i] = MIN_ARRAY_SIZE;
      assert ( arrayResources[i] == NULL);
      arrayResources[i] =
        xcalloc ( arraySizeAllocated[i], sizeof ( node ));
    }
}


/**************************************************************/


static
void arrayInitPointer ( void )
{
  int nnsp, i, j, code;
  namespaceTuple_t nspt;

  nnsp = namespaceGetNumber ();

  for ( i = 0; i < nnsp; i++ )
    {
      for ( j = 0; j < arraySizeAllocated[i]; j++ )
        {
          nspt.nsp = i;
          nspt.idx = j;
          code = namespaceIdxEncode ( nspt );
          arrayResources[i][j].next = arrayResources[i] + j + 1;
          arrayResources[i][j].idx  = code;
          arrayResources[i][j].ptr  = NULL;
        }

      arrayResources[i][arraySizeAllocated[i]-1].next = NULL;

      freeListHead[i] = arrayResources[i];
    }
}


/**************************************************************/


static
void arrayDelete ( void )
{
  int i, nnsp;

  nnsp = namespaceGetNumber ();

  if ( arrayResources )
    {
      for ( i = 0; i < nnsp; i++ )
        if (  arrayResources[i] ) free ( arrayResources[i] );
      free ( arrayResources );
    }

  if ( freeListHead ) free ( freeListHead );
}


/**************************************************************/


static
void arrayInitialize ( void )
{
#if  defined  (HAVE_LIBPTHREAD)
  /* initialize global API mutex lock */
  pthread_mutex_init ( &arrayMutex, NULL);
#endif

  arrayNew();
  atexit ( arrayDelete );

  ARRAY_LOCK ();

  arrayInitPointer ();

  ARRAY_UNLOCK ();

  arrayInit = 1;
}


/**************************************************************/


void *reshGetVal ( cdiResH idx )
{
  void *p = NULL;
  int nsp;
  namespaceTuple_t nspT;

  nsp = namespaceGetActive ();

  nspT = namespaceIdxDecode ( idx );

  if ( nspT.nsp != nsp )
    Error ( "idx %d from namespace %d not available in active namespace %d !\n",
          idx, nspT.nsp, nsp );

  ARRAY_INIT();

  if ( nspT.idx >= 0 && nspT.idx < arraySizeAllocated[nsp] )
    {
      ARRAY_LOCK();

      p = arrayResources[nsp][nspT.idx].ptr;

      ARRAY_UNLOCK();
    }
  else
    Error("resource handle %d invalid!", idx);

  return p;
}


/**************************************************************/


resOps * reshGetOps ( cdiResH idx )
{
  resOps *ops = NULL;
  int nsp;
  namespaceTuple_t nspT;

  nsp = namespaceGetActive ();

  nspT = namespaceIdxDecode ( idx );

  if ( nspT.nsp != nsp )
    Error ( "idx %d from namespace %d not available in active namespace %d !\n",
          idx, nspT.nsp, nsp );


  ARRAY_INIT();

  if ( nspT.idx >= 0 && nspT.idx < arraySizeAllocated[nsp] )
    {
      ARRAY_LOCK();

      ops = arrayResources[nsp][nspT.idx].ops;

      ARRAY_UNLOCK();
    }
  else
    Error("resource handle %d invalid!", idx);

  return ops;
}


/**************************************************************/


void arraySizeExtend()
{
  int newArraySize;
  int i, nsp, code;
  namespaceTuple_t nspt;

  nsp = namespaceGetActive ();
  nspt.nsp = nsp;

  assert ( arrayResources[nsp] != NULL );

  newArraySize = arraySizeAllocated[nsp] + MIN_ARRAY_SIZE;

  arrayResources[nsp] =
    xrealloc(arrayResources[nsp], newArraySize * sizeof (arrayResources[0][0]));

  for ( i = arraySizeAllocated[nsp]; i < newArraySize; ++i )
    {
      nspt.idx = i;
      code = namespaceIdxEncode ( nspt );
      arrayResources[nsp][i].idx  = code;
      arrayResources[nsp][i].next = arrayResources[nsp] + i + 1;
      arrayResources[nsp][i].ptr  = NULL;
    }

  arrayResources[nsp][newArraySize-1].next = freeListHead[nsp];
  freeListHead[nsp] = arrayResources[nsp] + arraySizeAllocated[nsp];
  arraySizeAllocated[nsp] = newArraySize;
}


/**************************************************************/


int reshPut ( void *p, resOps *ops )
{
  int      idx = -1;
  node *newNode;
  int nsp;

  ARRAY_INIT();

  nsp = namespaceGetActive ();

  assert ( p );

  ARRAY_INIT();

  nsp = namespaceGetActive ();

  ARRAY_LOCK();

  if ( !freeListHead[nsp] ) arraySizeExtend();

  newNode             = freeListHead[nsp];
  freeListHead[nsp]   = freeListHead[nsp]->next;
  newNode->next       = NULL;
  idx                 = newNode->idx;
  newNode->ptr        = p;
  newNode->ops        = ops;

  ARRAY_UNLOCK();

  return idx;
}


/**************************************************************/


void
reshRemove ( cdiResH idx)
{
  int nsp;

  nsp = namespaceGetActive ();

  ARRAY_LOCK();

  arrayResources[nsp][idx].next = freeListHead[nsp];
  arrayResources[nsp][idx].ptr  = NULL;
  freeListHead[nsp]             = arrayResources[nsp] + idx;

  ARRAY_UNLOCK();
}


/**************************************************************/


int reshCountType ( resOps * ops )
{
  int i, nsp, countType = 0;

  nsp = namespaceGetActive ();
  
  ARRAY_INIT();
  
  ARRAY_LOCK();
  
  for ( i = 0; i < arraySizeAllocated[nsp]; i++ )
    if ( arrayResources[nsp][i].ptr ) 
      if ( arrayResources[nsp][i].ops == ops )
	countType++;
  
  ARRAY_UNLOCK();
  
  return countType;
}


/**************************************************************/


void reshArrayPrint ()
{
  int i, j, temp;
  node * curr;
 
  if ( arrayInit )
    {
      temp = namespaceGetActive ();

      fprintf ( stdout, "\n\n###############\n#\n#  print global resource array \n" );

      for ( i = 0; i < namespaceGetNumber (); i++ )
	{
	  namespaceSetActive ( i );

	  fprintf ( stdout, "\n" );
	  fprintf ( stdout, "##################################\n" );
	  fprintf ( stdout, "#\n" );
	  fprintf ( stdout, "# namespace=%d\n", i );
	  fprintf ( stdout, "#\n" );
	  fprintf ( stdout, "##################################\n\n" );

	  for ( j = 0; j < arraySizeAllocated[i]; j++)
	    {
	      curr = arrayResources[i]+j;
	      if ( curr->ptr )
		{
		  curr->ops->valPrint (( void * ) curr->ptr );
		  fprintf ( stdout, "\n" ); 
		}
	    }
	}

      fprintf ( stdout, "#\n#  end global resource array \n#\n#################\n\n" );

      namespaceSetActive ( temp );
    }
}
