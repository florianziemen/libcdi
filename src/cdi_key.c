#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cdi.h"
#include "cdi_int.h"
#include "vlist.h"
#include "grid.h"
#include "zaxis.h"


static
cdi_keys_t *get_keysp(vlist_t *vlistptr, int varID)
{
  cdi_keys_t *keysp = NULL;
  /*
  if ( varID == CDI_GLOBAL )
    {
      keysp = &vlistptr->keys;
    }
  else
  */
    {
      if ( varID >= 0 && varID < vlistptr->nvars )
	keysp = &(vlistptr->vars[varID].keys);
    }

  return keysp;
}

static
cdi_key_t *new_key(cdi_keys_t *keysp, int key)
{
  xassert(keysp != NULL);

  if ( keysp->nelems == keysp->nalloc ) return NULL;

  cdi_key_t *keyp = &(keysp->value[keysp->nelems]);
  keysp->nelems++;

  keyp->key = key;
  // keyp->v   = 0;

  return keyp;
}

static
cdi_key_t *find_key(cdi_keys_t *keysp, int key)
{
  xassert(keysp != NULL);

  if ( keysp->nelems == 0 ) return NULL;

  cdi_key_t *keys = keysp->value;
  for ( size_t keyid = 0; keyid < keysp->nelems; keyid++ )
    {
      cdi_key_t *keyp = keys + keyid;
      if ( keyp->key == key )
        return keyp; // Normal return
    }

  return NULL;
}


static
cdi_keys_t *cdi_get_keysp(int objID, int varID)
{
  cdi_keys_t *keysp = NULL;

  /*
  if ( varID == CDI_GLOBAL && reshGetTxCode(objID) == GRID )
    {
      grid_t *gridptr = grid_to_pointer(objID);
      keysp = &gridptr->keys;
    }
  else if ( varID == CDI_GLOBAL && reshGetTxCode(objID) == ZAXIS )
    {
      zaxis_t *zaxisptr = zaxis_to_pointer(objID);
      keysp = &zaxisptr->keys;
    }
  else
  */
    {
      vlist_t *vlistptr = vlist_to_pointer(objID);
      keysp = get_keysp(vlistptr, varID);
    }

  return keysp;
}

//      cqdiDefKeyInt: Define a CDI varianble integer value from a key
int     cdiDefKeyInt(int cdiID, int varID, int key, int value)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp == NULL ) keyp = new_key(keysp, key);

  if ( keyp != NULL )
    {
      if ( keyp->v.i != value )
        {
          keyp->v.i = value;
          //reshSetStatus(cdiID, &vlistOps, RESH_DESYNC_IN_USE);
        }
    }

  return status;
}

//      cdiInqKeyInt: Get a CDI variable integer value from a key
int     cdiInqKeyInt(int cdiID, int varID, int key, int *value)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp != NULL ) // key in use
    {
      // if ( keyp->indtype == indtype )
	{
         *value = keyp->v.i;
	}
        /*
      else
	{
	  Warning("Key %d has wrong data type!", key);
          status = -2;
	}
        */
    }
  else
    {
      //Warning("Internal problem, key %d not found!", key);
      status = -1;
    }

  return status;
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
