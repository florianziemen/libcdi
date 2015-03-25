#ifndef _ZAXIS_H
#define _ZAXIS_H

unsigned cdiZaxisCount(void);

void cdiZaxisGetIndexList(unsigned numIDs, int IDs[numIDs]);

void
zaxisUnpack(char * unpackBuffer, int unpackBufferSize,
            int * unpackBufferPos, int originNamespace, void *context,
            int force_id);

void zaxisDefLtype2(int zaxisID, int ltype2);

#endif
