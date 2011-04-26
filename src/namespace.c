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
  idxbits = intbits - nspbits,
  nspmask = ((1<<nspbits) - 1) << idxbits,
  idxmask = (1<<idxbits) - 1,
};

enum {
  NUM_NAMESPACES = 1 << nspbits,
  NUM_IDX = 1 << idxbits,
};


void namespaceShowbits ( int n, char *name )
{
  int i;
  unsigned mask;
  char bitvalues[intbits + 1];

  mask = 1;
  for ( i = 0; i < intbits; i++ )
    {
      bitvalues[i] = ((unsigned)n & mask) ? '1':'0';
      mask <<= 1;
    }
  bitvalues[intbits] = '\0';
  fprintf (stdout, "%s: %s\n", name, bitvalues );
}


int namespaceIdxEncode ( namespaceTuple_t tin )
{
  assert(tin.nsp < NUM_NAMESPACES && tin.idx < NUM_IDX);
  return ( tin.nsp << idxbits ) + tin.idx;
}

int namespaceIdxEncode2 ( int idx, int nsp )
{
  assert(nsp < NUM_NAMESPACES && idx < NUM_IDX);
  return ( nsp << idxbits ) + idx;
}


namespaceTuple_t namespaceIdxDecode ( int code )
{
  namespaceTuple_t tin;

  tin.idx = code & idxmask;
  tin.nsp = (int)(((unsigned)( code & nspmask )) >> idxbits);

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
