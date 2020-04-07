#include "cdi.h"
#include "cdi_int.h"
#include "zaxis.h"
#include "grid.h"
#include "vlist.h"
#include "resource_unpack.h"


static
cdi_keys_t *vlist_get_keysp(vlist_t *vlistptr, int varID)
{
  if      (varID == CDI_GLOBAL) return &vlistptr->keys;
  else if (varID >= 0 && varID < vlistptr->nvars) return &(vlistptr->vars[varID].keys);

  return NULL;
}

static
cdi_keys_t *grid_get_keysp(grid_t *gridptr, int varID)
{
  if      (varID == CDI_GLOBAL) return &gridptr->keys;
  else if (varID == CDI_XAXIS)  return &gridptr->x.keys;
  else if (varID == CDI_YAXIS)  return &gridptr->y.keys;

  return NULL;
}

static
cdi_keys_t *zaxis_get_keysp(zaxis_t *zaxisptr, int varID)
{
  if (varID == CDI_GLOBAL) return &zaxisptr->keys;

  return NULL;
}

static
cdi_key_t *new_key(cdi_keys_t *keysp, int key)
{
  xassert(keysp != NULL);

  if ( keysp->nelems == keysp->nalloc ) return NULL;

  cdi_key_t *keyp = &(keysp->value[keysp->nelems]);
  keysp->nelems++;

  keyp->key = key;
  keyp->length = 0;
  keyp->type = 0;
  keyp->v.s = NULL;

  return keyp;
}


cdi_key_t *find_key(cdi_keys_t *keysp, int key)
{
  xassert(keysp != NULL);

  if ( keysp->nelems == 0 ) return NULL;

  for ( size_t keyid = 0; keyid < keysp->nelems; keyid++ )
    {
      cdi_key_t *keyp =  &(keysp->value[keyid]);
      if ( keyp->key == key ) return keyp; // Normal return
    }

  return NULL;
}

static
const cdi_key_t *find_key_const(const cdi_keys_t *keysp, int key)
{
  xassert(keysp != NULL);

  if ( keysp->nelems == 0 ) return NULL;

  for ( size_t keyid = 0; keyid < keysp->nelems; keyid++ )
    {
      const cdi_key_t *keyp =  &(keysp->value[keyid]);
      if ( keyp->key == key ) return keyp; // Normal return
    }

  return NULL;
}

static
cdi_keys_t *cdi_get_keysp(int objID, int varID)
{
  if      ( reshGetTxCode(objID) == GRID )  return grid_get_keysp(grid_to_pointer(objID), varID);
  else if ( reshGetTxCode(objID) == ZAXIS ) return zaxis_get_keysp(zaxis_to_pointer(objID), varID);
  else if ( reshGetTxCode(objID) == VLIST ) return vlist_get_keysp(vlist_to_pointer(objID), varID);

  return NULL;
}


int cdi_key_compare(cdi_keys_t *keyspa, cdi_keys_t *keyspb, int keynum)
{
  xassert(keynum >= 0 && keynum < (int)keyspa->nelems && keynum < (int)keyspb->nelems);
  cdi_key_t *keypa = keyspa->value + keynum,
            *keypb = keyspb->value + keynum;

  if (keypa->key != keypb->key) return 1;

  if (keypa->type != keypb->type) return 1;

  if (keypa->type == KEY_BYTES)
    {
      if (keypa->length != keypb->length) return 1;
      return memcmp(keypa->v.s, keypb->v.s, keypa->length);
    }
  else if (keypa->type == KEY_FLOAT)
    {
      if (IS_NOT_EQUAL(keypa->v.d, keypb->v.d)) return 1;
    }
  else if (keypa->type == KEY_INT)
    {
      if (keypa->v.i != keypb->v.i) return 1;
    }

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


void cdiDeleteKeys(int cdiID, int varID)
{
  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdiDeleteVarKeys(keysp);
}


void cdiPrintVarKeys(cdi_keys_t *keysp)
{
  for ( int keyid = 0; keyid < (int)keysp->nelems; keyid++ )
    {
      cdi_key_t *keyp = &(keysp->value[keyid]);
      if (keyp->type == KEY_BYTES)
        {
          printf("%d key %d length %d value %s\n", keyid+1, keyp->key, keyp->length,  keyp->v.s);
        }
      else if (keyp->type == KEY_FLOAT)
        {
          printf("%d key %d value %g\n", keyid+1, keyp->key, keyp->v.d);
        }
      else if (keyp->type == KEY_INT)
        {
          printf("%d key %d value %d\n", keyid+1, keyp->key, keyp->v.i);
        }
    }
}


void cdiPrintKeys(int cdiID, int varID)
{
  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdiPrintVarKeys(keysp);
}

//  cdiInqKeyLen: Get the length of the string representation of the key
int cdiInqKeyLen(int cdiID, int varID, int key, int *length)
{
  int status = -1;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  const cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp != NULL )
    {
      *length = keyp->length;
      if ( *length == 0 ) *length = 1;
      status = CDI_NOERR;
    }

  return status;
}

static
void cdi_define_key(const cdi_key_t *keyp, cdi_keys_t *keysp)
{
  if      ( keyp->type == KEY_INT )
    cdiDefVarKeyInt(keysp, keyp->key, keyp->v.i);
  else if ( keyp->type == KEY_FLOAT )
    cdiDefVarKeyFloat(keysp, keyp->key, keyp->v.d);
  else if ( keyp->type == KEY_BYTES )
    cdiDefVarKeyBytes(keysp, keyp->key, keyp->v.s, keyp->length);
}


void cdiCopyVarKeys(const cdi_keys_t *keysp1, cdi_keys_t *keysp2)
{
  for ( size_t keyid = 0; keyid < keysp1->nelems; keyid++ )
    {
      const cdi_key_t *keyp = &(keysp1->value[keyid]);
      cdi_define_key(keyp, keysp2);
    }
}


int cdiCopyKeys(int cdiID1, int varID1, int cdiID2, int varID2)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp1 = cdi_get_keysp(cdiID1, varID1);
  xassert(keysp1 != NULL);

  cdi_keys_t *keysp2 = cdi_get_keysp(cdiID2, varID2);
  xassert(keysp2 != NULL);

  cdiCopyVarKeys(keysp1, keysp2);

  return status;
}


int cdiCopyVarKey(const cdi_keys_t *keysp1, int key, cdi_keys_t *keysp2)
{
  int status = CDI_NOERR;

  const cdi_key_t *keyp = find_key_const (keysp1, key);
  if (keyp == NULL) return -1;

  cdi_define_key(keyp, keysp2);

  return status;
}


int cdiCopyKey(int cdiID1, int varID1, int key, int cdiID2)
{
  cdi_keys_t *keysp1 = cdi_get_keysp(cdiID1, varID1);
  xassert(keysp1 != NULL);

  cdi_keys_t *keysp2 = cdi_get_keysp(cdiID2, varID1);
  xassert(keysp2 != NULL);

  return cdiCopyVarKey(keysp1, key, keysp2);
}


void cdiDefVarKeyInt(cdi_keys_t *keysp, int key, int value)
{
  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp == NULL ) keyp = new_key(keysp, key);

  if ( keyp != NULL )
    {
      //if ( keyp->v.i != value )
        {
          keyp->type = KEY_INT;
          keyp->v.i = value;
        }
    }
}

/*
@Function  cdiDefKeyInt
@Title     Define an integer value from a key of a CDI variable

@Prototype int cdiDefKeyInt(int cdiID, int varID, int key, int value)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched
    @Item  value    An integer where the data will be read

@Description
The function @func{cdiDefKeyInt} defines a CDI integer value from a key.

@Result
@func{cdiDefKeyInt} returns CDI_NOERR if OK.

@EndFunction
*/
int cdiDefKeyInt(int cdiID, int varID, int key, int value)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdiDefVarKeyInt(keysp, key, value);

  return status;
}

/*
@Function  cdiInqKeyInt
@Title     Get a CDI floating point value from a key

@Prototype int cdiInqKeyInt(int cdiID, int varID, int key, int *value)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{zaxisCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  value    The address of an integer where the data will be retrieved.

@Description
The function @func{cdiInqKeyInt} return a CDI integer value from a key.

@Result
@func{cdiInqKeyInt} returns CDI_NOERR if key is available.

@EndFunction
*/
int cdiInqKeyInt(int cdiID, int varID, int key, int *value)
{
  int status = -1;

  // if ( varID != CDI_GLOBAL ) status = cdiInqKeyInt(cdiID, CDI_GLOBAL, key, value);

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


int cdiInqVarKeyInt(const cdi_keys_t *keysp, int key)
{
  int value = 0;

  const cdi_key_t *keyp = find_key_const(keysp, key);
  if (keyp && keyp->type == KEY_INT ) value = keyp->v.i;

  return value;
}


void cdiDefVarKeyFloat(cdi_keys_t *keysp, int key, double value)
{
  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp == NULL ) keyp = new_key(keysp, key);

  if ( keyp != NULL )
    {
      //if ( keyp->v.i != value )
        {
          keyp->type = KEY_INT;
          keyp->v.d = value;
        }
    }
}

/*
@Function  cdiDefKeyFloat
@Title     Define a floating point value from a key of a CDI variable

@Prototype int cdiDefKeyFloat(int cdiID, int varID, int key, double value)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched
    @Item  value    A double where the data will be read

@Description
The function @func{cdiDefKeyFloat} defines a CDI floating point value from a key.

@Result
@func{cdiDefKeyFloat} returns CDI_NOERR if OK.

@EndFunction
*/
int cdiDefKeyFloat(int cdiID, int varID, int key, double value)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdiDefVarKeyFloat(keysp, key, value);

  return status;
}

/*
@Function  cdiInqKeyFloat
@Title     Get a CDI floating point value from a key

@Prototype int cdiInqKeyFloat(int cdiID, int varID, int key, double *value)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{zaxisCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  value    The address of a double where the data will be retrieved.

@Description
The function @func{cdiInqKeyFloat} return a CDI floating point value from a key.

@Result
@func{cdiInqKeyFloat} returns CDI_NOERR if key is available.

@EndFunction
*/
int cdiInqKeyFloat(int cdiID, int varID, int key, double *value)
{
  int status = -1;

  // if ( varID != CDI_GLOBAL ) status = cdiInqKeyFloat(cdiID, CDI_GLOBAL, key, value);

  cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp != NULL ) // key in use
    {
      if ( keyp->type == KEY_FLOAT )
	{
          *value = keyp->v.d;
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
          keyp->v.s = (unsigned char *) malloc((size_t) length);
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

int cdiInqVarKeyBytes(const cdi_keys_t *keysp, int key, unsigned char *bytes, int *length)
{
  int status = -1;

  const cdi_key_t *keyp = find_key_const(keysp, key);
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

//  cdiInqKeyBytes: Get a bytes array from a key of a CDI variable
int cdiInqKeyBytes(int cdiID, int varID, int key, unsigned char *bytes, int *length)
{
  xassert(bytes != NULL);
  xassert(length != NULL);

  // if ( varID != CDI_GLOBAL ) status = cdiInqKeyBytes(cdiID, CDI_GLOBAL, key, bytes, length);

  const cdi_keys_t *keysp = cdi_get_keysp(cdiID, varID);
  xassert(keysp != NULL);

  return cdiInqVarKeyBytes(keysp, key, bytes, length);
}

/*
@Function  cdiDefKeyString
@Title     Define a string value from a key of a CDI variable

@Prototype int cdiDefKeyString(int cdiID, int varID, int key, const char *string)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  string   The address of a string where the data will be read.

@Description
The function @func{cdiDefKeyString} defines a CDI string value from a key.

@Result
@func{cdiDefKeyString} returns CDI_NOERR if OK.

@EndFunction
*/
int cdiDefKeyString(int cdiID, int varID, int key, const char *string)
{
  xassert(string != NULL);

  int length = strlen(string) + 1;
  int status = cdiDefKeyBytes(cdiID, varID, key, (const unsigned char *) string, length);

  return status;
}

/*
@Function  cdiInqKeyString
@Title     Get a CDI string value from a key

@Prototype int cdiInqKeyString(int cdiID, int varID, int key, char *string, int *length)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  string   The address of a string where the data will be retrieved.
                    The caller must allocate space for the returned string.
    @Item  length   The allocated length of the string on input.
@Description
The function @func{cdiInqKeyString} return a CDI string value from a key.

@Result
@func{cdiInqKeyString} returns CDI_NOERR if key is available.

@EndFunction
*/
int cdiInqKeyString(int cdiID, int varID, int key, char *string, int *length)
{
  xassert(string != NULL);
  xassert(length != NULL);

  string[0] = '\0';

  int status = cdiInqKeyBytes(cdiID, varID, key, (unsigned char *) string, length);
  if (status != CDI_NOERR) *length = 0;

  return status;
}

const char *cdiInqVarKeyStringPtr(cdi_keys_t *keysp, int key)
{
  const cdi_key_t *keyp = find_key(keysp, key);
  if ( keyp != NULL ) // key in use
    {
      if ( keyp->type == KEY_BYTES ) return (const char *)keyp->v.s;
    }

  return NULL;
}

void cdiInitKeys(cdi_keys_t *keysp)
{
  keysp->nalloc = MAX_KEYS;
  keysp->nelems = 0;
  for ( int i = 0; i < MAX_KEYS; ++i )
    keysp->value[i].length = 0;
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
