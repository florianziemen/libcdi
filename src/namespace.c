#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include "cdi.h"
#include "namespace.h"

static int nNamespaces = 1;
static int activeNamespace;

enum {
  intbits = sizeof(int) * CHAR_BIT,
  nspbits = 4,
  codebits = intbits - nspbits,
  nspmask = ((1<<nspbits) - 1) << codebits,
  codemask = (1<<codebits) - 1,
};

enum {
  NUM_NAMESPACES = 1 << nspbits,
  NUM_CODES = 1 << codebits,
};


void namespaceShowbits ( int n, char *name )
{
  int i, bit = 0, andmask;

  fprintf (stdout, "%s: ", name );
  for ( i = 0; i < intbits; i++ )
    {
      andmask = 1 << i;
      bit = n & andmask;
      fprintf ( stdout, "%d", bit == 0 ? 0 : 1 );
    }
  fprintf (stdout, "\n" );
}


int namespaceIdxEncode ( namespaceTuple_t tin )
{
  assert(tin.nsp < NUM_NAMESPACES && tin.idx < NUM_CODES);
  return ( tin.nsp << codebits ) + tin.idx;
}


namespaceTuple_t namespaceIdxDecode ( int code )
{
  namespaceTuple_t tin;

  tin.idx = code & codemask;
  tin.nsp = (int)(((unsigned)( code & nspmask )) >> codebits);

  return tin;
}


void namespaceSetNumber ( int nspn )
{
  assert(nspn <= NUM_NAMESPACES && nspn >= 1 );
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
