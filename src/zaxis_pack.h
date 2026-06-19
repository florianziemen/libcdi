#ifndef ZAXIS_PACK_H
#define ZAXIS_PACK_H

int zaxisGetPackSize(void *zaxisP, void *context);
void zaxisPack(void *zaxisP, void *buffer, int size, int *pos, void *context);
int zaxisTxCode(void *zaxisP);
int zaxisUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id);

#endif
