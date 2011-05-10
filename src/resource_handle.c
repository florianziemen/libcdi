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


// ATTENTION: not thread safe yet, namespaces are set in model!

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
  int         sent;
} node;


static node **arrayResources;
static node **freeListHead;


/**************************************************************/


void reshLock ()
{
  ARRAY_LOCK();
}


/**************************************************************/


void reshUnlock ()
{
  ARRAY_UNLOCK();
}


/**************************************************************/


static
void arrayNew ( void )
{
  int nnsp, i;

  nnsp = namespaceGetNumber ();

  arraySizeAllocated = xcalloc ( nnsp, sizeof ( arraySizeAllocated[0]));
  arrayResources     = xcalloc ( nnsp, sizeof ( arrayResources[0] ));
  freeListHead       = xcalloc ( nnsp, sizeof( freeListHead[0] ));

  for ( i = 0; i < nnsp; i++ )
    {
      arraySizeAllocated[i] = MIN_ARRAY_SIZE;

      assert ( arrayResources[i] == NULL);

      arrayResources[i] =  xcalloc ( arraySizeAllocated[i], sizeof ( node ));
    }
}


/**************************************************************/


static
void arrayInitPointer ( void )
{
  int nnsp, i, j;

  nnsp = namespaceGetNumber ();

  for ( i = 0; i < nnsp; i++ )
    {
      for ( j = 0; j < arraySizeAllocated[i]; j++ )
        {
          arrayResources[i][j].idx  = namespaceIdxEncode2 ( i, j );
          arrayResources[i][j].next = arrayResources[i] + j + 1;
	  arrayResources[i][j].ops  = NULL;
	  arrayResources[i][j].ptr  = NULL;
	  arrayResources[i][j].sent = 0;
        }

      arrayResources[i][arraySizeAllocated[i]-1].next = NULL;

      freeListHead[i] = arrayResources[i];
    }
}


/**************************************************************/


static 
void arrayDestroy ( void )
{
  int i;

  if ( arrayResources )
    {
      for ( i = 0; i < namespaceGetNumber (); i++ )
        if (  arrayResources[i] ) 
	  free ( arrayResources[i] );
      free ( arrayResources );
    }

  if ( freeListHead ) free ( freeListHead );
  if ( arraySizeAllocated ) free ( arraySizeAllocated );
}


/**************************************************************/


static
void arrayInitialize ( void )
{
#if  defined  (HAVE_LIBPTHREAD)
  /* initialize global API mutex lock */
  pthread_mutex_init ( &arrayMutex, NULL);
#endif

  arrayNew ();
  atexit ( arrayDestroy );

  ARRAY_LOCK ();

  arrayInitPointer ();

  ARRAY_UNLOCK ();

  arrayInit = 1;
}


/**************************************************************/


void *reshGetVal ( cdiResH idx, resOps * ops  )
{
  int nsp;
  namespaceTuple_t nspT;
  node * node;

  ARRAY_LOCK ();

  nsp = namespaceGetActive ();

  nspT = namespaceIdxDecode ( idx );

  assert ( arrayInit && 
	   nspT.nsp == nsp &&
	   nspT.idx >= 0 && 
	   nspT.idx < arraySizeAllocated[nsp] );

  node =  arrayResources[nsp] + nspT.idx;

  ARRAY_UNLOCK ();
   
  assert ( node );
  assert ( node->ops == ops ); 

  return node->ptr;
}


/**************************************************************/


void arraySizeExtend()
{
  int newArraySize;
  int i, nsp, code;

  nsp = namespaceGetActive ();

  assert ( arrayInit );

  newArraySize = arraySizeAllocated[nsp] + MIN_ARRAY_SIZE;

  arrayResources[nsp] =
    xrealloc(arrayResources[nsp], newArraySize * sizeof (arrayResources[0][0]));

  for ( i = arraySizeAllocated[nsp]; i < newArraySize; ++i )
    { 
      arrayResources[nsp][i].idx  = namespaceIdxEncode2 ( nsp, i );
      arrayResources[nsp][i].next = arrayResources[nsp] + i + 1;
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

  assert ( p && ops );

  ARRAY_INIT();

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

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


void reshRemove ( cdiResH idx, resOps * ops )
{
  int nsp;

  assert ( arrayInit );

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  assert ( arrayResources[nsp][idx].ops == ops );

  arrayResources[nsp][idx].next = freeListHead[nsp];
  arrayResources[nsp][idx].ops  = NULL;
  arrayResources[nsp][idx].ptr  = NULL;
  arrayResources[nsp][idx].sent = 0;
  freeListHead[nsp]             = arrayResources[nsp] + idx;

  ARRAY_UNLOCK();
}


/**************************************************************/


int reshCountType ( resOps * ops )
{
  int i, nsp, countType = 0;

  if ( !( arrayInit && ops )) return 0;
  
  ARRAY_LOCK();

  nsp = namespaceGetActive ();
  
  for ( i = 0; i < arraySizeAllocated[nsp]; i++ )
    if ( arrayResources[nsp][i].ptr ) 
      if ( arrayResources[nsp][i].ops == ops )
	countType++;
  
  ARRAY_UNLOCK();
  
  return countType;
}


/**************************************************************/


void reshGetIndiceArrayOfType ( int c, int * indice, resOps * ops )
{
  int i, j = 0, nsp;

  assert ( arrayInit && indice && ops );
  
  ARRAY_LOCK();

  nsp = namespaceGetActive ();
  
  for ( i = 0; i < arraySizeAllocated[nsp]; i++ )
    if ( arrayResources[nsp][i].ptr && arrayResources[nsp][i].ops ) 
      if ( arrayResources[nsp][i].ops == ops )
	{
	  indice[j++] = i;
	  if ( j == c ) break;
	}
  
  ARRAY_UNLOCK();
}


/**************************************************************/


void reshArrayPrint ()
{
  int i, j, temp;
  node * curr;
 
  if ( ! arrayInit ) return;

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
	  curr = arrayResources[i] + j;
	  if ( curr->ops && curr->ptr )
	    {
	      curr->ops->valPrint (( void * ) curr->ptr );
	      fprintf ( stdout, "\n" ); 
	    }
	}
    }
  
  fprintf ( stdout, "#\n#  end global resource array \n#\n#################\n\n" );
  
  namespaceSetActive ( temp );
}
