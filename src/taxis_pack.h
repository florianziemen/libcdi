#ifndef TAXIS_PACK_H
#define TAXIS_PACK_H

#include <stdlib.h>

char *new_refcount_string(size_t len);

int taxisGetPackSize(void *taxisP, void *context);
void taxisPack(void *taxisP, void *buf, int size, int *position, void *context);
int taxisTxCode(void *taxisP);
int taxisUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id);

#endif
