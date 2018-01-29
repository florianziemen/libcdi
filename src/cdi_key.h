#ifndef CDI_KEY_H
#define CDI_KEY_H

/*
 * CDI key
 */
typedef struct {
  int       key;          // CDI key
  union {
    int i;
    double d;
    char *s;
  } v;
} cdi_key_t;


typedef struct {
  size_t     nalloc;		// number allocated >= nelems
  size_t     nelems;		// length of the array
  cdi_key_t  value[MAX_KEYS];
} cdi_keys_t;

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
