#ifndef STREAM_PACK_H
#define STREAM_PACK_H

#include "cdi_int.h"

int streamGetPackSize(void *streamP, void *context);
void streamPack(void *streamP, void *buff, int size, int *position, void *context);
int streamTxCode(void *streamP);
struct streamAssoc streamUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context);

#endif
