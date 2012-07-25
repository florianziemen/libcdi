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

struct cons
{
  void * val;
  struct cons * next;
};

struct queue_t {
  struct cons *head, *tail;
  valDestroyFunction valDestroy;
  eqPredicate keyCompare;
  int count;
};

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
  struct cons *curr, *succ;

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
  struct cons *newCons;

  {
    struct cons *p;
    for (p = q->head; p; p = p->next)
      // ensure unique keys
      if (q->keyCompare(v, p->val))
        return -1;
  }

  if ((newCons = malloc(sizeof(struct cons))) == NULL)
    {
      perror ( "pio_queue: queueAdd (): Not enough memory" );
      /* FIXME: why not abort? */
      return 1;
    }

  newCons->val = v;
  newCons->next = NULL;


  if ( q->tail != NULL)
    q->tail->next = newCons;
  else
    q->head = newCons;

  q->tail = newCons;
  q->count  ++;

  return 0;
}

int
queueDelNode(queue_t *q, int (*predicate)(void *, void *),
             void *data)
{
  struct cons **p;

  for (p = &q->head; *p; p = &(*p)->next)
    if (predicate((*p)->val, data))
      {
        struct cons *rem = *p;
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
  struct cons *p;
  xassert(q && predicate);
  for (p = q->head; p; p = p->next)
    if (predicate(p->val, data))
      return p->val;
  return NULL;
}

bool
queueIsEmpty(queue_t *q)
{
  return q->head == NULL;
}


void
queueForeach(queue_t *q, void (*func)(void *elem, void *data), void *data)
{
  struct cons *p;
  for (p = q->head; p; p = p->next)
    func(p->val, data);
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
