#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "resource_handle.h"
#include "pio_util.h"
#include "namespace.h"
#include "cdi.h"
#include "error.h"
#include "pio_rpc.h"

extern void file_list_delete ( void );

enum { MIN_ARRAY_SIZE = 128, MIN_BUFF_SIZE = 4096 };

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
  cdiResH       resH;//idx
  struct node * next;
  resOps      * ops;
  void        * val;//ptr
  int           status;
} node;


static node **arrayResources;
static node **freeListHead;


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
          arrayResources[i][j].resH    = namespaceIdxEncode2 ( i, j );
          arrayResources[i][j].next   = arrayResources[i] + j + 1;
          arrayResources[i][j].ops    = NULL;
          arrayResources[i][j].val    = NULL;
          arrayResources[i][j].status = RESH_UNDEFID;
        }

      arrayResources[i][arraySizeAllocated[i]-1].next = NULL;

      freeListHead[i] = arrayResources[i];
    }
}


/**************************************************************/


void arrayDestroy ( void )
{
  node * node;
  int i, j;
  
  if ( arrayResources )
    {
      for ( i = 0; i < namespaceGetNumber (); i++ )
	{
	  namespaceSetActive ( i );  
	  for ( j = 0; j < arraySizeAllocated[i]; j++ )
	    {
	      node = arrayResources[i] + j;
	      if ( node->val )
		node->ops->valDestroy ( node->val );
	    }
	}
      free ( arrayResources[i] );
    }
  free ( arrayResources );
  
  if ( freeListHead ) free ( freeListHead );
  if ( arraySizeAllocated ) free ( arraySizeAllocated );

  file_list_delete ();   
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


void arraySizeExtend()
{
  int newArraySize;
  int i, nsp;

  nsp = namespaceGetActive ();

  assert ( arrayInit );

  newArraySize = arraySizeAllocated[nsp] + MIN_ARRAY_SIZE;

  arrayResources[nsp] =
    xrealloc(arrayResources[nsp], newArraySize * sizeof (arrayResources[0][0]));

  for ( i = arraySizeAllocated[nsp]; i < newArraySize; ++i )
    {
      arrayResources[nsp][i].resH  = namespaceIdxEncode2 ( nsp, i );
      arrayResources[nsp][i].next = arrayResources[nsp] + i + 1;
    }

  arrayResources[nsp][newArraySize-1].next = freeListHead[nsp];
  freeListHead[nsp] = arrayResources[nsp] + arraySizeAllocated[nsp];
  arraySizeAllocated[nsp] = newArraySize;
}


/**************************************************************/


int reshPut ( void *p, resOps *ops )
{
  cdiResH resH = -1, nsp;
  node * newNode;

  assert ( p && ops );

  ARRAY_INIT();

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  if ( !freeListHead[nsp] ) arraySizeExtend();
  newNode               = freeListHead[nsp];
  freeListHead[nsp]     = freeListHead[nsp]->next;
  newNode->next         = NULL;
  resH                  = newNode->resH;
  newNode->val          = p;
  newNode->ops          = ops;
  newNode->status       = ASSIGNED;

  ARRAY_UNLOCK();

  return resH;
}


/**************************************************************/


void reshRemove ( cdiResH resH, resOps * ops )
{
  int nsp;
  namespaceTuple_t nspT;

  assert ( arrayInit && ops );

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  assert ( nspT.nsp == nsp &&
           nspT.idx >= 0 &&
           nspT.idx < arraySizeAllocated[nsp] &&
           arrayResources[nsp][nspT.idx].ops &&
           arrayResources[nsp][nspT.idx].ops == ops );

  arrayResources[nsp][nspT.idx].next   = freeListHead[nsp];
  arrayResources[nsp][nspT.idx].ops    = NULL;
  arrayResources[nsp][nspT.idx].val    = NULL;
  arrayResources[nsp][nspT.idx].status = RESH_UNDEFID;
  freeListHead  [nsp]                  = arrayResources[nsp] + nspT.idx;

  ARRAY_UNLOCK();
}


/**************************************************************/


void *reshGetVal ( cdiResH resH, resOps * ops  )
{
  int nsp;
  namespaceTuple_t nspT;
  node * node;

  assert ( arrayInit && ops );

  ARRAY_LOCK ();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  assert ( arrayInit &&
           nspT.nsp == nsp &&
           nspT.idx >= 0 &&
           nspT.idx < arraySizeAllocated[nsp] );

  node =  arrayResources[nsp] + nspT.idx;

  ARRAY_UNLOCK ();

  assert ( node &&
           node->ops == ops );

  return node->val;
}


/**************************************************************/


void reshGetResHArrayOfType ( int c, int * resHs, resOps * ops )
{
  int i, j = 0, nsp;

  assert ( arrayInit && resHs && ops );

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  for ( i = 0; i < arraySizeAllocated[nsp]; i++ )
    if ( arrayResources[nsp][i].val && arrayResources[nsp][i].ops )
      if ( arrayResources[nsp][i].ops == ops )
        {
          resHs[j++] = namespaceIdxEncode2 ( nsp, i );
          if ( j == c ) break;
        }

  ARRAY_UNLOCK();
}


/**************************************************************/


int reshCountType ( resOps * ops )
{
  int i, nsp, countType = 0;

  assert ( ops );

  if ( !( arrayInit && ops )) return 0;

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  for ( i = 0; i < arraySizeAllocated[nsp]; i++ )
    if ( arrayResources[nsp][i].val )
      if ( arrayResources[nsp][i].ops == ops )
        countType++;

  ARRAY_UNLOCK();

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
  node * curr;

  nsp = namespaceGetActive ();

  xmpi ( MPI_Pack_size ( 1, MPI_INT, comm, &intpacksize ));

  /* pack start marker, namespace and sererator marker */
  packBufferSize += 3 * intpacksize;

  /* pack resources, type marker and seperator marker */
  for ( i = 0; i < arraySizeAllocated[nsp]; i++ )
    if ( arrayResources[nsp][i].val )
      if ( arrayResources[nsp][i].status == ASSIGNED )
        {
          curr = arrayResources[nsp] + i;
          assert ( curr->ops );

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
  node * curr;

  assert ( packBuffer );

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  * packBufferSize = getPackBufferSize ( comm );
  * packBuffer = xcalloc ( 1, * packBufferSize );

  xmpi ( MPI_Pack ( &start, 1,  MPI_INT, 
		    * packBuffer, * packBufferSize, &packBufferPos, comm ));

  xmpi ( MPI_Pack ( &nsp, 1,  MPI_INT, 
		    * packBuffer, * packBufferSize, &packBufferPos, comm ));

  xmpi ( MPI_Pack ( &sep, 1,  MPI_INT, 
		    * packBuffer, * packBufferSize, &packBufferPos, comm ));

  for ( i = 0; i < arraySizeAllocated[nsp]; i++ )
    if ( arrayResources[nsp][i].val )
      if ( arrayResources[nsp][i].status == ASSIGNED )
        {
          curr = arrayResources[nsp] + i;
          assert ( curr->ops );

          type = curr->ops->valTxCode ();

          if ( ! type )
            continue;

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

  ARRAY_UNLOCK();

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
  node * node;

  assert ( arrayInit && ops );

  ARRAY_LOCK ();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  assert ( arrayInit &&
           nspT.nsp == nsp &&
           nspT.idx >= 0 &&
           nspT.idx < arraySizeAllocated[nsp] );

  node =  arrayResources[nsp] + nspT.idx;

  assert ( node &&
           node->ops == ops );

  node->status = status;

  ARRAY_UNLOCK ();
}


/**************************************************************/


int reshGetStatus ( cdiResH resH, resOps * ops )
{
  int nsp;
  namespaceTuple_t nspT;
  node * node;

  assert ( arrayInit && ops );

  ARRAY_LOCK ();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  assert ( arrayInit &&
           nspT.nsp == nsp &&
           nspT.idx >= 0 &&
           nspT.idx < arraySizeAllocated[nsp] );

  node =  arrayResources[nsp] + nspT.idx;

  ARRAY_UNLOCK ();

  assert ( node &&
           node->ops == ops );

  return node->status;
}


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


int reshArrayCompare ( int nsp0, int nsp1 )
{
  const int equal  = 0;
  const int differ = -1;
  int i, valCompare;
  node * node0, * node1;

  assert ( arrayInit && 
	   namespaceGetNumber () > xmaxInt ( nsp0, nsp1 ) &&
	   xminInt ( nsp0, nsp1 ) >= 0 );

  for ( i = 0; i < arraySizeAllocated[nsp0]; i++ )
    {
      node0 = arrayResources[nsp0] + i;
      if ( node0->val )
	{
	  if ( i >= arraySizeAllocated[nsp1] )
	    {
	      xdebug ();
	      return differ;
	    }

	  node1 = arrayResources[nsp1] + i;
	  if ( !node1->val )
	    {
	      xdebug ();
	      return differ;
	    }

	  assert ( node0->ops && node1->ops );
	  if ( node0->ops != node1->ops )
	    {
	      xdebug ();
	      return differ;
	    }

	  valCompare =  node0->ops->valCompare ( node0->val, node1->val );
	  printf ( "type %d, values resH0=%d and resH1=%d %s\n", 
		   node0->ops->valTxCode (), 
		   node0->resH, 
		   node1->resH,
		   valCompare == equal ? "are equal" : "differ" );
	  if ( valCompare != equal ) return differ;
	}
      else if ( arrayResources[nsp1][i].val )
	return differ;
    }

  for ( ; i < arraySizeAllocated[nsp1]; i++ )
    if ( arrayResources[nsp1][i].val )
      {
	xdebug ();
	return differ;
      }
	
  return equal;
}


/**************************************************************/


void reshArrayPrint ( char * filename )
{
  int i, j, temp;
  node * curr;
  FILE * fp; 

  if ( ! arrayInit ) return;

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
            "global resource array \n#\n" );

  for ( i = 0; i < namespaceGetNumber (); i++ )
    {
      namespaceSetActive ( i );

      fprintf ( fp, "\n" );
      fprintf ( fp, "##################################\n" );
      fprintf ( fp, "#\n" );
      fprintf ( fp, "# namespace=%d\n", i );
      fprintf ( fp, "#\n" );
      fprintf ( fp, "##################################\n\n" );

      fprintf ( fp, "arraySizeAllocated[%d]=%d\n", i, arraySizeAllocated[i] );

      for ( j = 0; j < arraySizeAllocated[i]; j++)
        {
          curr = arrayResources[i] + j;
          if ( curr->ops && curr->val )
            {
              curr->ops->valPrint (( void * ) curr->val, fp );
              fprintf ( fp, "\n" );
            }
        }
    }

  fprintf ( fp, "#\n#  end global resource array" \
            "\n#\n##########################################\n\n" );

  fclose ( fp );

  namespaceSetActive ( temp );
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
