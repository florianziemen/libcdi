#include <stdlib.h>
#include "cdi.h"

enum { MAX_NAMESPACES = 16 };

static int nNamespaces = 1;
static int activeNamespace;


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



  

