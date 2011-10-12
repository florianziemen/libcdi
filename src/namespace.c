#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include "cdi.h"
#include "namespace.h"
#include "pio_util.h"


static int nNamespaces = 1;
static int activeNamespace = 0;
static int HLF = 1;
static int * hasLocalFile = &HLF;
static int RAS = 0;
static int * resASent = &RAS;

enum {
  intbits = sizeof(int) * CHAR_BIT,
  nspbits = 4,
  idxbits = intbits - nspbits,
  nspmask = (( 1 << nspbits ) - 1) << idxbits,
  idxmask = ( 1 << idxbits ) - 1,
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
  assert ( tin.nsp < NUM_NAMESPACES && tin.idx < NUM_IDX);
  return ( tin.nsp << idxbits ) + tin.idx;
}

int namespaceIdxEncode2 ( int nsp, int idx )
{
  assert(nsp < NUM_NAMESPACES && idx < NUM_IDX);
  return ( nsp << idxbits ) + idx;
}


namespaceTuple_t namespaceResHDecode ( int resH )
{
  namespaceTuple_t tin;

  tin.idx = resH & idxmask;
  tin.nsp = (int)(((unsigned)( resH & nspmask )) >> idxbits);

  return tin;
}


void pioNamespaceInit ( int nspn, int * argHasLocalFile )
{
#ifdef USE_MPI
  int i;
  assert(nspn <= NUM_NAMESPACES && nspn >= 1 );
  nNamespaces = nspn;

  if ( nspn >= 1 )
    {
      hasLocalFile = xmalloc ( nspn * sizeof ( hasLocalFile[0] ));
      for ( i = 0; i < nspn; i++ )  
	hasLocalFile[i] = argHasLocalFile[i];
      resASent = xmalloc ( nspn * sizeof ( resASent[0] ));
    }
#endif
}


void pioNamespaceCleanup ( void )
{
  if ( nNamespaces > 1 ) 
      {
          free ( hasLocalFile );
          hasLocalFile = NULL;
      }
}


int namespaceGetNumber ()
{
  return nNamespaces;
}


void pioNamespaceSetActive ( int nId )
{
  if ( nId >= nNamespaces || nId < 0 )
    abort ();

  activeNamespace = nId;
}


int namespaceGetActive ()
{
  return activeNamespace;
}


int namespaceHasLocalFile ( int nId )
{
  if ( nId >= nNamespaces || nId < 0 )
    abort ();
  
  return hasLocalFile ? hasLocalFile[nId] : 0;
}


int namespaceAdaptKey ( int key, int nspTarget )
{
  namespaceTuple_t tin;
  int nsp;

  if ( key == CDI_UNDEFID ) return CDI_UNDEFID;

  tin.idx = key & idxmask;
  tin.nsp = (int)(((unsigned)( key & nspmask )) >> idxbits);

  assert ( tin.nsp == nspTarget );

  nsp = namespaceGetActive ();
 
  return namespaceIdxEncode2 ( nsp, tin.idx );
}


int namespaceAdaptKey2 ( int key )
{
  namespaceTuple_t tin;
  int nsp;

  if ( key == CDI_UNDEFID ) return CDI_UNDEFID;

  tin.idx = key & idxmask;
  tin.nsp = (int)(((unsigned)( key & nspmask )) >> idxbits);

  nsp = namespaceGetActive ();
 
  return namespaceIdxEncode2 ( nsp, tin.idx );
}


void namespaceDefResASent ( void )
{
  int nsp = namespaceGetActive ();
  resASent[nsp] = 1;
}


int namespaceInqResASent ( void )
{
  int nsp = namespaceGetActive ();
  return resASent[nsp];
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
