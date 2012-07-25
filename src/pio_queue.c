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

queue_t *queueInit ( valDestroyFunction vD, eqPredicate kC )
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

int
queueAdd(queue_t *q, void *v)
{
  listElem_t *newListElem_T, *curr;

  for (curr = q->head; curr; curr = curr->next)
    // ensure unique keys
    if (q->keyCompare(v, curr->val))
      return -1;

  if (( newListElem_T = ( listElem_t * ) malloc (sizeof ( listElem_t ))) == NULL )
    {
      perror ( "pio_queue: queueAdd (): Not enough memory" );
      /* FIXME: why not abort? */
      return 1;
    }

  newListElem_T->val = v;
  newListElem_T->next = NULL;


  if ( q->tail != NULL)
    q->tail->next = newListElem_T;
  else
    q->head = newListElem_T;

  q->tail = newListElem_T;
  q->count  ++;

  return 0;
}

int
queueDelNode(queue_t *q, int (*predicate)(void *, void *),
             void *data)
{
  listElem_t **p;

  for (p = &q->head; *p; p = &(*p)->next)
    if (predicate((*p)->val, data))
      {
        listElem_t *rem = *p;
        if (rem == q->tail) q->tail = NULL;
        int iret = q->valDestroy(rem->val);
        *p = rem->next;
        free(rem);
        return iret;
      }
  return -1;
}

void *
queueGet(queue_t *q, int (*predicate)(void *, void *), void *data)
{
  listElem_t *p;
  xassert(q && predicate);
  for (p = q->head; p; p = p->next)
    if (predicate(p->val, data))
      return p->val;
  return NULL;
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
