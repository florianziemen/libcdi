#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NOMPI
#include "pio_impl.h"

queue *queueInit ( valDestroyFunction vD, keyCompareFunction kC )
{
  queue *myq;

  myq = ( queue * ) malloc ( sizeof ( queue ));
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
  int i;

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

void queuePush ( queue *q, void *v )
{
  node *newNode, *curr;
  size_t len;

  // ensure unique key values
 
  curr = q->head; 
      
  while ( curr )
    { 
      if (( *( q->keyCompare )) ( v, curr->val ))
	return;
      curr = curr->next;
    } 
  
  newNode = ( node * ) malloc (sizeof ( node ));
  newNode->idx = q->count;
  newNode->val = v;
  newNode->next = NULL;

  if ( q->tail != NULL)
    q->tail->next = newNode;
  else
    q->head = newNode;

  q->tail = newNode; 
  q->count  ++;
  
  return;
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
  int iret;

  if ( !q->head ) return -1;

  pred = q->head;
  curr = pred->next;
  
  if ( pred->idx == id )
    {
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
      if ( curr == q->tail ) q->tail = pred;
      pred->next = curr->next;
      iret = ( *( q->valDestroy )) ( curr->val );
      free ( curr ); 
    }
    
  return iret;
}

#endif
