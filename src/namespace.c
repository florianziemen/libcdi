#include <stdlib.h>
#include <stdio.h>
#include "cdi.h"
#include "namespace.h"

enum { MAX_NAMESPACES = 16 };

static int nNamespaces = 1;
static int activeNamespace;



void namespaceShowbits ( int n, char *name )
{
  int i, bit = 0, andmask;

  fprintf (stdout, "%s: ", name );  
  for ( i = 0; i < 32; i++ )
    {
      andmask = 1 << i;
      bit = n & andmask;
      fprintf ( stdout, "%d", bit == 0 ? 0 : 1 );
    }
  fprintf (stdout, "\n" );
}


int namespaceIdxEncode ( namespaceTuple_t tin )
{
  
  return ( tin.nsp << 28 ) + tin.idx;
} 


namespaceTuple_t namespaceIdxDecode ( int code )
{
  namespaceTuple_t tin;

  tin.idx = code & ~( 15 << 28 );
  tin.nsp = ( code & ( 15 << 28 )) >> 28;

  return tin;
}


void namespaceSetNumber ( int nspn )
{
  if ( nspn > MAX_NAMESPACES || nspn <= 1 )
    abort ();

  nNamespaces = nspn;
}

int namespaceGetNumber ()
{
  return nNamespaces;
}


void namespaceSetActive ( int nId )
{
  if ( nId >= nNamespaces || nId < 0 )
    abort ();

  activeNamespace = nId;
}


int namespaceGetActive ()
{
  return activeNamespace;
}



  

