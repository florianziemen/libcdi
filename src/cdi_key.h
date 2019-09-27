#ifndef  CDI_KEY_H
#define  CDI_KEY_H

#include "cdi_limits.h"

// CDI key
typedef struct {
  int       key;          // CDI key
  int       type;         // KEY_INT, KEY_FLOAT, KEY_BYTES
  int       length;       // number of bytes in v.s
  union {
    int i;
    double d;
    unsigned char *s;
  } v;
} cdi_key_t;


typedef struct {
  size_t     nalloc;		// number allocated >= nelems
  size_t     nelems;		// length of the array
  cdi_key_t  value[MAX_KEYS];
} cdi_keys_t;


void cdiDefVarKeyInt(cdi_keys_t *keysp, int key, int value);
void cdiDefVarKeyBytes(cdi_keys_t *keysp, int key, const unsigned char *bytes, int length);

void cdiCopyVarKeys(cdi_keys_t *keysp, int cdiID2, int varID2);
void cdiDeleteVarKeys(cdi_keys_t *keysp);
int cdiDeleteKeys(int cdiID, int varID);

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
