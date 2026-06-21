#ifndef CDI_KEY_H
#define CDI_KEY_H

#include <stdio.h>
#include "cdi_limits.h"

// CDI key
typedef struct
{
  short key;   // CDI key
  short type;  // KeyInt, KeyFloat, KeyBytes
  int length;  // number of bytes in v.s
  union
  {
    int i;
    double d;
    unsigned char *s;
  } v;
} cdi_key_t;

typedef struct
{
  short nalloc;  // number allocated >= nelems
  short nelems;  // length of the array
  cdi_key_t value[MAX_KEYS];
} cdi_keys_t;

enum
{
  KeyInt = 1,
  KeyFloat,
  KeyBytes
};

void cdiDefVarKeyInt(cdi_keys_t *keysP, int key, int value);
void cdiDefVarKeyFloat(cdi_keys_t *keysP, int key, double value);
void cdiDefVarKeyBytes(cdi_keys_t *keysP, int key, const unsigned char *bytes, int length);
int cdiInqVarKeyInt(const cdi_keys_t *keysP, int key);
int cdiInqVarKeyBytes(const cdi_keys_t *keysP, int key, unsigned char *bytes, int *length);

const cdi_key_t *find_key(const cdi_keys_t *keysP, int key);
const char *cdiInqVarKeyStringPtr(const cdi_keys_t *keysP, int key);

static inline const char *
cdiInqVarKeyString(const cdi_keys_t *keysP, int key)
{
  const char *string = cdiInqVarKeyStringPtr(keysP, key);
  if (string == NULL) string = "";
  return string;
}

int cdiCopyVarKey(const cdi_keys_t *keysP1, int key, cdi_keys_t *keysP2);
void cdiCopyVarKeys(const cdi_keys_t *keysP1, cdi_keys_t *keysP2);
void cdiDeleteVarKeys(cdi_keys_t *keysP);
void cdiDeleteKeys(int cdiID, int varID);
void cdiPrintKeys(int cdiID, int varID);

void cdiInitKeys(cdi_keys_t *keysP);

int cdi_key_compare(const cdi_keys_t *keysPa, const cdi_keys_t *keysPb, int keynum);

#endif

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
