#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "cdi.h"

#ifdef USE_MPI
#include "pio_impl.h"
#include "pio_util.h"

queue_t *queueInit ( valDestroyFunction vD, keyCompareFunction kC )
{
  queue_t *myq;

  myq = xmalloc ( sizeof ( queue_t ));

  myq->head = NULL;
  myq->tail = NULL;
  myq->valDestroy = vD;
  myq->keyCompare = kC;
  myq->count = 0;

  return myq;
}

void queueDestroy ( queue_t *q )
{
  listElem_t *curr, *succ;

  if ( q->head )
    {
      curr = q->head;

      while ( curr )
        {
          succ = curr->next;
          ( *( q->valDestroy )) ( curr->val );
          free ( curr );
          curr = succ;
        }
    }

  free ( q );

  return;
}

int queuePush ( queue_t *q, void *v, int noCollector, ... )
{
  listElem_t *newListElem_T, *curr;
  va_list ap;
  int id = CDI_UNDEFID;

  va_start(ap, noCollector);

  if ( noCollector )
    id = va_arg( ap, int );

  va_end(ap);

  // ensure unique key values

  curr = q->head;

  while ( curr )
    {
      if (( !noCollector &&  ( *( q->keyCompare )) ( v, curr->val )) ||
          (  noCollector &&  ( id == curr->idx )))
        return -1;

      curr = curr->next;
    }

  if (( newListElem_T = ( listElem_t * ) malloc (sizeof ( listElem_t ))) == NULL )
    {
      perror ( "pio_queue: queuePush (): Not enough memory" );
      return 1;
    }

  newListElem_T->idx = noCollector ? id : q->count;
  newListElem_T->val = v;
  newListElem_T->next = NULL;


  if ( q->tail != NULL)
    q->tail->next = newListElem_T;
  else
    q->head = newListElem_T;

  q->tail = newListElem_T;
  q->count  ++;

  return newListElem_T->idx;
}

void *queueIdx2val ( queue_t *q, int id )
{
  listElem_t *curr;

  curr = q->head;

  while ( curr )
    {
      if ( curr->idx == id ) break;
      curr = curr->next;
    }

  return curr ? curr->val : NULL;
}

int queueDelNode ( queue_t *q, int id ) // error
{
  listElem_t *pred, *curr;

  if ( !q->head ) return -1;

  pred = q->head;
  curr = pred->next;

  if ( pred->idx == id )
    {
      int iret;
      if ( pred == q->tail ) q->tail = NULL;
      q->head = curr;
      iret = ( *( q->valDestroy )) ( pred->val );

      free ( pred );
      return iret;
    }

  while ( curr )
    {

      if ( curr->idx == id ) break;
      pred = curr;
      curr = curr->next;
    }

  if ( curr )
    {
      int iret;
      if ( curr == q->tail ) q->tail = pred;
      pred->next = curr->next;
      iret = ( *( q->valDestroy )) ( curr->val );
      free ( curr );
      return iret;
    }

  return -1;
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
