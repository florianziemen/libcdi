#ifndef GRID_PACK_H
#define GRID_PACK_H

#include "grid.h"

int gridGetPackSizeBase(grid_t *gridP, void *context);
int gridGetPackSizeScalars(grid_t *gridP, void *context);
int gridGetPackSizeArrays(grid_t *gridP, void *context);

grid_t *gridUnpackScalars(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context,
                          int force_id, int *memberMaskP);
void gridUnpackArrays(grid_t *gridP, int memberMask, char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos,
                      int originNamespace, void *context);

void gridPackBase(grid_t *gridP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context);
int gridPackScalars(grid_t *gridP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context);
void gridPackArrays(grid_t *gridP, int memberMask, void *packBuffer, int packBufferSize, int *packBufferPos, void *context);

int gridGetPackSize(void *gridP, void *context);
void gridPack(void *gridP, void *buff, int size, int *position, void *context);
int gridTxCode(void *gridP);
int gridUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id);

#endif
