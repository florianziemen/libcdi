/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include "cdi.h"
#include "cdi_int.h"
#include "taxis.h"
#include "zaxis.h"
#include "grid.h"
#include "vlist.h"
#include "resource_unpack.h"
#include "dmemory.h"

static cdi_keys_t *
vlist_get_keysP(vlist_t *vlistP, int varID)
{
  if (varID == CDI_GLOBAL) return &vlistP->keys;
  if (varID >= 0 && varID < vlistP->nvars) return &(vlistP->vars[varID].keys);

  return NULL;
}

static cdi_keys_t *
grid_get_keysP(grid_t *gridP, int varID)
{
  if (varID == CDI_GLOBAL) return &gridP->keys;
  if (varID == CDI_XAXIS) return &gridP->x.keys;
  if (varID == CDI_YAXIS) return &gridP->y.keys;

  return NULL;
}

static cdi_keys_t *
zaxis_get_keysP(zaxis_t *zaxisP, int varID)
{
  return (varID == CDI_GLOBAL) ? &zaxisP->keys : NULL;
}

static cdi_keys_t *
taxis_get_keysP(taxis_t *taxisP, int varID)
{
  return (varID == CDI_GLOBAL) ? &taxisP->keys : NULL;
}

static cdi_key_t *
new_key(cdi_keys_t *keysP, int key)
{
  xassert(keysP != NULL);
  if (keysP->nelems == keysP->nalloc) return NULL;

  cdi_key_t *keyP = &(keysP->value[keysP->nelems]);
  keysP->nelems++;

  keyP->key = key;
  keyP->type = 0;
  keyP->length = 0;
  keyP->v.s = NULL;

  return keyP;
}

const cdi_key_t *
find_key(const cdi_keys_t *keysP, int key)
{
  xassert(keysP != NULL);
  if (keysP->nelems == 0) return NULL;

  for (int i = 0; i < keysP->nelems; i++)
  {
    const cdi_key_t *keyP = &(keysP->value[i]);
    if (keyP->key == key) return keyP;  // Normal return
  }

  return NULL;
}

static cdi_key_t *
find_key_mutable(cdi_keys_t *keysP, int key)
{
  return (cdi_key_t *) find_key(keysP, key);
}

static cdi_keys_t *
cdi_get_keysP(int objID, int varID)
{
  int reshID = reshGetTxCode(objID);
  switch (reshID)
  {
    case GRID:
    case DIST_GRID: return grid_get_keysP(grid_to_pointer(objID), varID);
    case ZAXIS: return zaxis_get_keysP(zaxis_to_pointer(objID), varID);
    case TAXIS: return taxis_get_keysP(taxis_to_pointer(objID), varID);
    case VLIST: return vlist_get_keysP(vlist_to_pointer(objID), varID);
    default: return NULL;
  }
}

int
cdi_key_compare(const cdi_keys_t *keysPa, const cdi_keys_t *keysPb, int keynum)
{
  xassert(keynum >= 0 && keynum < (int) keysPa->nelems && keynum < (int) keysPb->nelems);
  const cdi_key_t *keyPa = keysPa->value + keynum, *keyPb = keysPb->value + keynum;

  if (keyPa->key != keyPb->key) return 1;
  if (keyPa->type != keyPb->type) return 1;
  if (keyPa->length != keyPb->length) return 1;

  if (keyPa->type == KeyBytes) return (memcmp(keyPa->v.s, keyPb->v.s, (size_t) keyPa->length) != 0);
  if (keyPa->type == KeyFloat) return (IS_NOT_EQUAL(keyPa->v.d, keyPb->v.d));
  if (keyPa->type == KeyInt) return (keyPa->v.i != keyPb->v.i);

  return 0;
}

static void
cdi_delete_key(cdi_key_t *keyP)
{
  if (keyP != NULL && keyP->length)  // key in use
  {
    keyP->length = 0;
    if (keyP->type == KeyBytes) { FREE_AND_NULL(keyP->v.s); }
    else if (keyP->type == KeyFloat) { keyP->v.d = 0.0; }
    else if (keyP->type == KeyInt) { keyP->v.i = 0; }
  }
}

void
cdiDeleteVarKeys(cdi_keys_t *keysP)
{
  int nelems = keysP ? keysP->nelems : 0;
  for (int i = 0; i < nelems; i++) { cdi_delete_key(&(keysP->value[i])); }

  keysP->nelems = 0;
}

void
cdiDeleteKeys(int cdiID, int varID)
{
  cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  cdiDeleteVarKeys(keysP);
}

void
cdiPrintVarKeys(cdi_keys_t *keysP)
{
  int nelems = keysP ? (int) keysP->nelems : 0;
  for (int i = 0; i < nelems; i++)
  {
    cdi_key_t *keyP = &(keysP->value[i]);
    if (keyP->length == 0) continue;
    if (keyP->type == KeyBytes) { printf("%d key %d length %d value %s\n", i + 1, keyP->key, keyP->length, keyP->v.s); }
    else if (keyP->type == KeyFloat) { printf("%d key %d value %g\n", i + 1, keyP->key, keyP->v.d); }
    else if (keyP->type == KeyInt) { printf("%d key %d value %d\n", i + 1, keyP->key, keyP->v.i); }
  }
}

void
cdiPrintKeys(int cdiID, int varID)
{
  cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  cdiPrintVarKeys(keysP);
}

//  cdiInqKeyLen: Get the length of the string representation of the key
int
cdiInqKeyLen(int cdiID, int varID, int key, int *length)
{
  const cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  const cdi_key_t *keyP = find_key(keysP, key);
  if (keyP != NULL && keyP->length > 0)
  {
    *length = keyP->length;
    return CDI_NOERR;
  }

  return -1;
}

static void
cdi_define_key(const cdi_key_t *keyP, cdi_keys_t *keysP)
{
  switch (keyP->type)
  {
    case KeyInt: cdiDefVarKeyInt(keysP, keyP->key, keyP->v.i); break;
    case KeyFloat: cdiDefVarKeyFloat(keysP, keyP->key, keyP->v.d); break;
    case KeyBytes: cdiDefVarKeyBytes(keysP, keyP->key, keyP->v.s, keyP->length); break;
  }
}

int
cdiDeleteKey(int cdiID, int varID, int key)
{
  cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  cdi_delete_key(find_key_mutable(keysP, key));

  return CDI_NOERR;
}

void
cdiCopyVarKeys(const cdi_keys_t *keysP1, cdi_keys_t *keysP2)
{
  for (int i = 0; i < keysP1->nelems; i++)
  {
    const cdi_key_t *keyP = &(keysP1->value[i]);
    if (keyP->length > 0) cdi_define_key(keyP, keysP2);
  }
}

int
cdiCopyKeys(int cdiID1, int varID1, int cdiID2, int varID2)
{
  cdi_keys_t *keysP1 = cdi_get_keysP(cdiID1, varID1);
  xassert(keysP1 != NULL);

  cdi_keys_t *keysP2 = cdi_get_keysP(cdiID2, varID2);
  xassert(keysP2 != NULL);

  cdiCopyVarKeys(keysP1, keysP2);

  return CDI_NOERR;
}

int
cdiCopyVarKey(const cdi_keys_t *keysP1, int key, cdi_keys_t *keysP2)
{
  const cdi_key_t *keyP = find_key(keysP1, key);
  if (keyP == NULL) return -1;

  if (keyP->length > 0) cdi_define_key(keyP, keysP2);

  return CDI_NOERR;
}

int
cdiCopyKey(int cdiID1, int varID1, int key, int cdiID2)
{
  cdi_keys_t *keysP1 = cdi_get_keysP(cdiID1, varID1);
  xassert(keysP1 != NULL);

  cdi_keys_t *keysP2 = cdi_get_keysP(cdiID2, varID1);
  xassert(keysP2 != NULL);

  return cdiCopyVarKey(keysP1, key, keysP2);
}

void
cdiDefVarKeyInt(cdi_keys_t *keysP, int key, int value)
{
  cdi_key_t *keyP = find_key_mutable(keysP, key);
  if (keyP == NULL) { keyP = new_key(keysP, key); }
  if (keyP == NULL) return;

  keyP->type = KeyInt;
  keyP->v.i = value;
  keyP->length = 1;
}

/*
@Function  cdiDefKeyInt
@Title     Define an integer value from a key

@Prototype int cdiDefKeyInt(int cdiID, int varID, int key, int value)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  value    An integer where the data will be read.

@Description
The function @func{cdiDefKeyInt} defines an integer value from a key.

@Result
@func{cdiDefKeyInt} returns CDI_NOERR if OK.

@EndFunction
*/
int
cdiDefKeyInt(int cdiID, int varID, int key, int value)
{
  cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  cdiDefVarKeyInt(keysP, key, value);

  return CDI_NOERR;
}

/*
@Function  cdiInqKeyInt
@Title     Get an integer value from a key

@Prototype int cdiInqKeyInt(int cdiID, int varID, int key, int *value)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched..
    @Item  value    The address of an integer where the data will be retrieved.

@Description
The function @func{cdiInqKeyInt} gets an integer value from a key.

@Result
@func{cdiInqKeyInt} returns CDI_NOERR if key is available.

@EndFunction
*/
int
cdiInqKeyInt(int cdiID, int varID, int key, int *value)
{
  int status = -1;

  // if (varID != CDI_GLOBAL) status = cdiInqKeyInt(cdiID, CDI_GLOBAL, key, value);

  const cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  const cdi_key_t *keyP = find_key(keysP, key);
  if (keyP != NULL && keyP->length == 1 && keyP->type == KeyInt)
  {
    *value = keyP->v.i;
    status = CDI_NOERR;
  }

  return status;
}

int
cdiInqVarKeyInt(const cdi_keys_t *keysP, int key)
{
  int value = 0;

  const cdi_key_t *keyP = find_key(keysP, key);
  if (keyP && keyP->type == KeyInt) value = keyP->v.i;

  return value;
}

void
cdiDefVarKeyFloat(cdi_keys_t *keysP, int key, double value)
{
  cdi_key_t *keyP = find_key_mutable(keysP, key);
  if (keyP == NULL) { keyP = new_key(keysP, key); }
  if (keyP == NULL) return;

  keyP->type = KeyFloat;
  keyP->v.d = value;
  keyP->length = 1;
}

/*
@Function  cdiDefKeyFloat
@Title     Define a floating point value from a key

@Prototype int cdiDefKeyFloat(int cdiID, int varID, int key, double value)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched
    @Item  value    A double where the data will be read

@Description
The function @func{cdiDefKeyFloat} defines a CDI floating point value from a key.

@Result
@func{cdiDefKeyFloat} returns CDI_NOERR if OK.

@EndFunction
*/
int
cdiDefKeyFloat(int cdiID, int varID, int key, double value)
{
  cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  cdiDefVarKeyFloat(keysP, key, value);

  return CDI_NOERR;
}

/*
@Function  cdiInqKeyFloat
@Title     Get a floating point value from a key

@Prototype int cdiInqKeyFloat(int cdiID, int varID, int key, double *value)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  value    The address of a double where the data will be retrieved.

@Description
The function @func{cdiInqKeyFloat} gets a floating point value from a key.

@Result
@func{cdiInqKeyFloat} returns CDI_NOERR if key is available.

@EndFunction
*/
int
cdiInqKeyFloat(int cdiID, int varID, int key, double *value)
{
  int status = -1;

  // if (varID != CDI_GLOBAL) status = cdiInqKeyFloat(cdiID, CDI_GLOBAL, key, value);

  const cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  const cdi_key_t *keyP = find_key(keysP, key);
  if (keyP != NULL && keyP->length == 1 && keyP->type == KeyFloat)
  {
    *value = keyP->v.d;
    status = CDI_NOERR;
  }

  return status;
}

void
cdiDefVarKeyBytes(cdi_keys_t *keysP, int key, const unsigned char *bytes, int length)
{
  cdi_key_t *keyP = find_key_mutable(keysP, key);
  if (keyP == NULL) keyP = new_key(keysP, key);
  if (keyP == NULL) return;

  size_t length_ = (length >= 0) ? (size_t) length : (size_t) 0;
  if (keyP->length != 0 && keyP->length != length)
  {
    Free(keyP->v.s);
    keyP->length = 0;
  }
  if (keyP->length == 0)
  {
    keyP->v.s = (length_ > 0) ? (unsigned char *) Malloc(length_) : NULL;
    keyP->length = length;
  }

  if (length_ > 0) memcpy(keyP->v.s, bytes, length_);
  keyP->type = KeyBytes;
}

/*
@Function  cdiDefKeyBytes
@Title     Define a byte array from a key

@Prototype int cdiDefKeyBytes(int cdiID, int varID, int key, const unsigned char *bytes, int length)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  bytes    The address of a byte array where the data will be read.
    @Item  length   Length of the byte array

@Description
The function @func{cdiDefKeyBytes} defines a byte array from a key.

@Result
@func{cdiDefKeyBytes} returns CDI_NOERR if OK.

@EndFunction
*/
int
cdiDefKeyBytes(int cdiID, int varID, int key, const unsigned char *bytes, int length)
{
  int status = CDI_NOERR;

  cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  cdiDefVarKeyBytes(keysP, key, bytes, length);

  return status;
}

int
cdiInqVarKeyBytes(const cdi_keys_t *keysP, int key, unsigned char *bytes, int *length)
{
  int status = -1;

  const cdi_key_t *keyP = find_key(keysP, key);
  if (keyP != NULL && keyP->length > 0 && keyP->type == KeyBytes)
  {
    int copyLen = (keyP->length < *length) ? keyP->length : *length;
    memcpy(bytes, keyP->v.s, (size_t) copyLen);
    *length = copyLen;
    status = CDI_NOERR;
  }

  return status;
}

//  cdiInqKeyBytes: Get a byte array from a key
/*
@Function  cdiInqKeyBytes
@Title     Get a byte array from a key

@Prototype int cdiInqKeyBytes(int cdiID, int varID, int key, unsigned char *bytes, int *length)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  bytes    The address of a byte array where the data will be retrieved.
                    The caller must allocate space for the returned byte array.
    @Item  length   The allocated length of the byte array on input.
@Description
The function @func{cdiInqKeyBytes} gets a byte array from a key.

@Result
@func{cdiInqKeyBytes} returns CDI_NOERR if key is available.

@EndFunction
*/
int
cdiInqKeyBytes(int cdiID, int varID, int key, unsigned char *bytes, int *length)
{
  xassert(bytes != NULL);
  xassert(length != NULL);

  // if (varID != CDI_GLOBAL) status = cdiInqKeyBytes(cdiID, CDI_GLOBAL, key, bytes, length);

  const cdi_keys_t *keysP = cdi_get_keysP(cdiID, varID);
  xassert(keysP != NULL);

  return cdiInqVarKeyBytes(keysP, key, bytes, length);
}

/*
@Function  cdiDefKeyString
@Title     Define a string from a key

@Prototype int cdiDefKeyString(int cdiID, int varID, int key, const char *string)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  string   The address of a string where the data will be read.

@Description
The function @func{cdiDefKeyString} defines a text string from a key.

@Result
@func{cdiDefKeyString} returns CDI_NOERR if OK.

@Example
Here is an example using @func{cdiDefKeyString} to define the name of a variable:

@Source
#include "cdi.h"
   ...
int vlistID, varID, status;
   ...
vlistID = vlistCreate();
varID = vlistDefVar(vlistID, gridID, zaxisID, TIME_VARYING);
   ...
status = cdiDefKeyString(vlistID, varID, CDI_KEY_NAME, "temperature");
   ...
@EndSource
@EndFunction
*/
int
cdiDefKeyString(int cdiID, int varID, int key, const char *string)
{
  xassert(string != NULL);

  size_t length = strlen(string) + 1;
  int status = cdiDefKeyBytes(cdiID, varID, key, (const unsigned char *) string, (int) length);

  return status;
}

/*
@Function  cdiInqKeyString
@Title     Get a string from a key

@Prototype int cdiInqKeyString(int cdiID, int varID, int key, char *string, int *length)
@Parameter
    @Item  cdiID    CDI object ID (vlistID, gridID, zaxisID).
    @Item  varID    Variable identifier or CDI_GLOBAL.
    @Item  key      The key to be searched.
    @Item  string   The address of a string where the data will be retrieved.
                    The caller must allocate space for the returned string.
    @Item  length   The allocated length of the string on input.
@Description
The function @func{cdiInqKeyString} gets a text string from a key.

@Result
@func{cdiInqKeyString} returns CDI_NOERR if key is available.

@Example
Here is an example using @func{cdiInqKeyString} to get the name of the first variable:

@Source
#include "cdi.h"
   ...
#define STRLEN 256
   ...
int streamID, vlistID, varID, status;
int length = STRLEN;
char name[STRLEN];
   ...
streamID = streamOpenRead(...);
vlistID = streamInqVlist(streamID);
   ...
varID = 0;
status = cdiInqKeyString(vlistID, varID, CDI_KEY_NAME, name, &length);
   ...
@EndSource
@EndFunction
*/
int
cdiInqKeyString(int cdiID, int varID, int key, char *string, int *length)
{
  xassert(string != NULL);
  xassert(length != NULL);

  int maxlength = *length;
  if (maxlength > 0) { string[0] = '\0'; }

  int status = cdiInqKeyBytes(cdiID, varID, key, (unsigned char *) string, length);
  if (CDI_NOERR == status)
    string[maxlength - 1] = '\0';
  else
    *length = 0;

  return status;
}

const char *
cdiInqVarKeyStringPtr(const cdi_keys_t *keysP, int key)
{
  const cdi_key_t *keyP = find_key(keysP, key);
  if (keyP != NULL && keyP->type == KeyBytes) return (const char *) keyP->v.s;
  return NULL;
}

void
cdiInitKeys(cdi_keys_t *keysP)
{
  memset(keysP, 0, sizeof(*keysP));
  keysP->nalloc = MAX_KEYS;
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
