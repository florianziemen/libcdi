#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef USE_MPI
#include "pio_impl.h"

queue *queueInit ( valDestroyFunction vD, keyCompareFunction kC )
{
  queue *myq;

  if (( myq = ( queue * ) malloc ( sizeof ( queue ))) == NULL )
    {
      perror ( "pio_queue: queueInit: Not enough memory" );
      return NULL;
    }

  myq->head = NULL;
  myq->tail = NULL;
  myq->valDestroy = vD;
  myq->keyCompare = kC;
  myq->count = 0;

  return myq;
}

void queueDestroy ( queue *q )
{
  node *curr, *succ;

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

int queuePush ( queue *q, void *v, int noCollector, ... )
{
  node *newNode, *curr;
  va_list ap;
  int id;
 
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
  
  if (( newNode = ( node * ) malloc (sizeof ( node ))) == NULL )    
    {
      perror ( "pio_queue: queuePush (): Not enough memory" );
      return 1;
    }

  newNode->idx = noCollector ? id : q->count;
  newNode->val = v;
  newNode->next = NULL;
 

  if ( q->tail != NULL)
    q->tail->next = newNode;
  else
    q->head = newNode;

  q->tail = newNode; 
  q->count  ++;
  
  return newNode->idx;
}

void *queueIdx2val ( queue *q, int id )
{
  node *curr;
  
  curr = q->head;
  
  while ( curr )
    {
      if ( curr->idx == id ) break;
      curr = curr->next;
    }

  return curr ? curr->val : NULL;
}

int queueDelNode ( queue *q, int id ) // error
{
  node *pred, *curr;

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
