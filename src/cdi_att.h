#ifndef CDI_ATT_H
#define CDI_ATT_H

#include <stdio.h>
#include "cdi_limits.h"

// CDI attribute
typedef struct
{
  void *xvalue;   // the actual data
  char *name;     // attribute name
  int xsz;        // amount of space at xvalue
  short nelems;   // number of elements
  short namesz;   // size of name
  short indtype;  // internal data type of xvalue (INT, FLT or TXT)
  short exdtype;  // external data type
                  // indtype    exdtype
                  // TXT        TXT
                  // INT        INT16, INT32
                  // FLT        FLT32, FLT64
} cdi_att_t;

typedef struct
{
  cdi_att_t value[MAX_ATTRIBUTES];
  short nalloc;  // number allocated >= nelems
  short nelems;  // length of the array
} cdi_atts_t;

int cdiDeleteAtts(int vlistID, int varID);
int cdiAttsGetSize(void *p, int varID, void *context);
void cdiAttsPack(void *p, int varID, void *buf, int size, int *position, void *context);
void cdiAttsUnpack(int cdiID, int varID, void *buf, int size, int *position, void *context);

int cdi_att_compare(cdi_atts_t *attsPa, cdi_atts_t *attsPb, int attnum);

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
