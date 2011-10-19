#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "resource_handle.h"
#include "pio_comm.h"
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


typedef struct arrayElem {
  cdiResH       resH;//idx
  struct arrayElem * next;
  resOps      * ops;
  void        * val;//ptr
  int           status;
} arrayElem_t;


static arrayElem_t **arrayResources;
static arrayElem_t **freeListHead;


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

      xassert ( arrayResources[i] == NULL);

      arrayResources[i] =  xcalloc ( arraySizeAllocated[i], sizeof ( arrayElem_t ));
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
  arrayElem_t * arrayElem;
  int i, j, nnsp;

  if ( arrayResources )
    {
        nnsp = namespaceGetNumber ();
      for ( i = 0; i < nnsp; i++ )
	{
	  pioNamespaceSetActive ( i ); 
          if ( arrayResources[i] )
              {
                  if ( arraySizeAllocated )
                      for ( j = 0; j < arraySizeAllocated[i]; j++ )
                          {
                              arrayElem = arrayResources[i] + j;
                              if ( arrayElem->val )
                                      arrayElem->ops->valDestroy ( arrayElem->val );
                          }
                  free ( arrayResources[i] );
                  arrayResources[i] = NULL;
              }
	}
      free ( arrayResources );
      arrayResources = NULL;
    }
  
  if ( freeListHead ) 
      {
          free ( freeListHead );
          freeListHead = NULL;
      }
  if ( arraySizeAllocated ) 
      {
          free ( arraySizeAllocated );
          arraySizeAllocated = NULL;
      }

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

  xassert ( arrayInit );

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
  arrayElem_t * newArrayElem;

  xassert ( p && ops );

  ARRAY_INIT();

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  if ( !freeListHead[nsp] ) arraySizeExtend();
  newArrayElem               = freeListHead[nsp];
  freeListHead[nsp]     = freeListHead[nsp]->next;
  newArrayElem->next         = NULL;
  resH                  = newArrayElem->resH;
  newArrayElem->val          = p;
  newArrayElem->ops          = ops;
  newArrayElem->status       = ASSIGNED;

  ARRAY_UNLOCK();

  return resH;
}


/**************************************************************/


void reshRemove ( cdiResH resH, resOps * ops )
{
  int nsp;
  namespaceTuple_t nspT;

  xassert ( arrayInit && ops );

  ARRAY_LOCK();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( nspT.nsp == nsp &&
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
  arrayElem_t * arrayElem;

  xassert ( arrayInit && ops );

  ARRAY_LOCK ();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( arrayInit &&
           nspT.nsp == nsp &&
           nspT.idx >= 0 &&
           nspT.idx < arraySizeAllocated[nsp] );

  arrayElem =  arrayResources[nsp] + nspT.idx;

  ARRAY_UNLOCK ();

  xassert ( arrayElem &&
           arrayElem->ops == ops );

  return arrayElem->val;
}


/**************************************************************/


void reshGetResHArrayOfType ( int c, int * resHs, resOps * ops )
{
  int i, j = 0, nsp;

  xassert ( arrayInit && resHs && ops );

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

  xassert ( ops );

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
  arrayElem_t * curr;

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
  arrayElem_t * curr;

  xassert ( packBuffer );

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
          xassert ( curr->ops );

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
  arrayElem_t * arrayElem;

  xassert ( arrayInit && ops );

  ARRAY_LOCK ();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( arrayInit &&
           nspT.nsp == nsp &&
           nspT.idx >= 0 &&
           nspT.idx < arraySizeAllocated[nsp] );

  arrayElem =  arrayResources[nsp] + nspT.idx;

  xassert ( arrayElem &&
           arrayElem->ops == ops );

  arrayElem->status = status;

  ARRAY_UNLOCK ();
}


/**************************************************************/


int reshGetStatus ( cdiResH resH, resOps * ops )
{
  int nsp;
  namespaceTuple_t nspT;
  arrayElem_t * arrayElem;

  xassert ( arrayInit && ops );

  ARRAY_LOCK ();

  nsp = namespaceGetActive ();

  nspT = namespaceResHDecode ( resH );

  xassert ( arrayInit &&
           nspT.nsp == nsp &&
           nspT.idx >= 0 &&
           nspT.idx < arraySizeAllocated[nsp] );

  arrayElem =  arrayResources[nsp] + nspT.idx;

  ARRAY_UNLOCK ();

  xassert ( arrayElem &&
           arrayElem->ops == ops );

  return arrayElem->status;
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
  arrayElem_t * arrayElem0, * arrayElem1;

#ifdef USE_MPI

  xassert ( arrayInit && 
	   namespaceGetNumber () > xmaxInt ( nsp0, nsp1 ) &&
	   xminInt ( nsp0, nsp1 ) >= 0 );

  for ( i = 0; i < arraySizeAllocated[nsp0]; i++ )
    {
      arrayElem0 = arrayResources[nsp0] + i;
      if ( arrayElem0->val )
	{
	  if ( i >= arraySizeAllocated[nsp1] )
	    {
	      xdebug ();
	      return differ;
	    }

	  arrayElem1 = arrayResources[nsp1] + i;
	  if ( !arrayElem1->val )
	    {
	      xdebug ();
	      return differ;
	    }

	  xassert ( arrayElem0->ops && arrayElem1->ops );
	  if ( arrayElem0->ops != arrayElem1->ops )
	    {
	      xdebug ();
	      return differ;
	    }

	  valCompare =  arrayElem0->ops->valCompare ( arrayElem0->val, arrayElem1->val );
	  printf ( "type %d, values resH0=%d and resH1=%d %s\n", 
		   arrayElem0->ops->valTxCode (), 
		   arrayElem0->resH, 
		   arrayElem1->resH,
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

#endif
	
  return equal;
}


/**************************************************************/


void reshArrayPrint ( char * filename )
{
  int i, j, temp;
  arrayElem_t * curr;
  FILE * fp; 

  if ( ! arrayInit ) return;

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
            "global resource array \n#\n" );

  for ( i = 0; i < namespaceGetNumber (); i++ )
    {
      pioNamespaceSetActive ( i );

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
