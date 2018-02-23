#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cdi.h"
#include "cdi_int.h"
#include "vlist.h"
#include "grid.h"
#include "zaxis.h"
#include "resource_unpack.h"

enum  {KEY_INT, KEY_FLOAT, KEY_BYTES};

static
cdi_keys_t *get_keysp(vlist_t *vlistptr, int varID)
{
  cdi_keys_t *keysp = NULL;

  if ( varID == CDI_GLOBAL )
    {
      keysp = &vlistptr->keys;
    }
  else
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
  if ( reshGetTxCode(objID) == VLIST )
    {
      vlist_t *vlistptr = vlist_to_pointer(objID);
      keysp = get_keysp(vlistptr, varID);
    }

  return keysp;
}


int vlist_key_compare(vlist_t *a, int varIDA, vlist_t *b, int varIDB, int keynum)
{
  cdi_keys_t *keyspa = get_keysp(a, varIDA),
             *keyspb = get_keysp(b, varIDB);
  if (keyspa == NULL && keyspb == NULL)
    return 0;
  xassert(keynum >= 0 && keynum < (int)keyspa->nelems
          && keynum < (int)keyspb->nelems);
  cdi_key_t *keypa = keyspa->value + keynum,
    *keypb = keyspb->value + keynum;

  if ( keypa->key != keypb->key )
    return 1;

  if ( keypa->v.i != keypb->v.i )
    return 1;

  return 0;
}


void cdiDeleteVarKeys(cdi_keys_t *keysp)
{
  for ( int keyid = 0; keyid < (int)keysp->nelems; keyid++ )
    {
      cdi_key_t *keyp = &(keysp->value[keyid]);
      if ( keyp->length )
        {
          free(keyp->v.s);
          keyp->v.s = NULL;
          keyp->length = 0;
        }
    }

  keysp->nelems = 0;
}


int cdiDeleteKeys(int cdiID, int varID)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdiDeleteVarKeys(keysp);

  return status;
}

//  cdiInqKeyLen: Get the length of the string representation of the key
int cdiInqKeyLen(int cdiID, int varID, int key, int *length)
{
  int status = -1;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp != NULL )
    {
      *length = keyp->length;
      if ( *length == 0 ) *length = 1;
      status = CDI_NOERR;
    }

  return status;
}


void cdiCopyVarKeys(cdi_keys_t *keysp, int cdiID2, int varID2)
{
  for ( size_t keyid = 0; keyid < keysp->nelems; keyid++ )
    {
      cdi_key_t *keyp = &(keysp->value[keyid]);
      if ( keyp->type == KEY_INT )
        cdiDefKeyInt(cdiID2, varID2, keyp->key, keyp->v.i);
      else if ( keyp->type == KEY_BYTES )
        cdiDefKeyBytes(cdiID2, varID2, keyp->key, keyp->v.s, keyp->length);
    }
}


int cdiCopyKeys(int cdiID1, int varID1, int cdiID2, int varID2)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID1, varID1);
  xassert(keysp != NULL);

  cdiCopyVarKeys(keysp, cdiID2, varID2);

  return status;
}


void cdiDefVarKeyInt(cdi_keys_t *keysp, int key, int value)
{
  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp == NULL ) keyp = new_key(keysp, key);

  if ( keyp != NULL )
    {
      if ( keyp->v.i != value )
        {
          keyp->type = KEY_INT;
          keyp->v.i = value;
        }
    }
}

//  cdiDefKeyInt: Define an integer value from a key of a CDI variable
int cdiDefKeyInt(int cdiID, int varID, int key, int value)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdiDefVarKeyInt(keysp, key, value);

  return status;
}

//  cdiInqKeyInt: Get an integer value from a key of a CDI variable
int cdiInqKeyInt(int cdiID, int varID, int key, int *value)
{
  int status = -1;

  if ( varID != CDI_GLOBAL ) status = cdiInqKeyInt(cdiID, CDI_GLOBAL, key, value);

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp != NULL ) // key in use
    {
      if ( keyp->type == KEY_INT )
	{
          *value = keyp->v.i;
          status = CDI_NOERR;
	}
    }

  return status;
}


void cdiDefVarKeyBytes(cdi_keys_t *keysp, int key, const unsigned char *bytes, int length)
{
  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp == NULL ) keyp = new_key(keysp, key);

  if ( keyp != NULL )
    {
      if ( keyp->length != 0 && keyp->length != length )
        {
          free(keyp->v.s);
          keyp->length = 0;
        }
      if ( keyp->length == 0 )
        {
          keyp->v.s = (unsigned char *) malloc(length);
          keyp->length = length;
        }

      memcpy(keyp->v.s, bytes, length);
      keyp->type = KEY_BYTES;
    }
}

//  cdiDefKeyBytes: Define a bytes array from a key of a CDI variable
int cdiDefKeyBytes(int cdiID, int varID, int key, const unsigned char *bytes, int length)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdiDefVarKeyBytes(keysp, key, bytes, length);

  return status;
}

//  cdiInqKeyBytes: Get a bytes array from a key of a CDI variable
int cdiInqKeyBytes(int cdiID, int varID, int key, unsigned char *bytes, int *length)
{
  int status = -1;
  xassert(bytes != NULL);
  xassert(length != NULL);

  if ( varID != CDI_GLOBAL ) status = cdiInqKeyBytes(cdiID, CDI_GLOBAL, key, bytes, length);

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp != NULL ) // key in use
    {
      if ( keyp->type == KEY_BYTES )
	{
          if ( keyp->length < *length ) *length = keyp->length;
          memcpy(bytes, keyp->v.s, *length);
          status = CDI_NOERR;
	}
    }

  return status;
}

//  cdiDefKeyString: Define a string value from a key of a CDI variable
int cdiDefKeyString(int cdiID, int varID, int key, const char *string)
{
  xassert(string != NULL);

  int length = strlen(string)+1;
  int status = cdiDefKeyBytes(cdiID, varID, key, (const unsigned char *) string, length);

  return status;
}

//  cdiInqKeyString: Get a string value from a key of a CDI variable
int cdiInqKeyString(int cdiID, int varID, int key, char *string, int *length)
{
  xassert(string != NULL);
  xassert(length != NULL);

  string[0] = '\0';

  int status = cdiInqKeyBytes(cdiID, varID, key, (unsigned char *) string, length);

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
