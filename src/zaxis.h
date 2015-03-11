#ifndef _ZAXIS_H
#define _ZAXIS_H


void zaxisGetTypeDescription(int zaxisType, int* outPositive, const char** outName, const char** outLongName, const char** outStdName, const char** outUnit);  //The returned const char* point to static storage. Don't free or modify them.

int zaxisSize(void);

void
zaxisUnpack(char * unpackBuffer, int unpackBufferSize,
            int * unpackBufferPos, int originNamespace, void *context,
            int force_id);

#endif
