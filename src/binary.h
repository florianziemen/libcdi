#ifndef _BINARY_H
#define _BINARY_H

#include <stdio.h>

#ifndef _DTYPES_H
#include "dtypes.h"
#endif


UINT32 get_UINT32(unsigned char *x);
UINT32 get_SUINT32(unsigned char *x);
UINT64 get_UINT64(unsigned char *x);
UINT64 get_SUINT64(unsigned char *x);


size_t binReadF77Block(int fileID, int byteswap);
void   binWriteF77Block(int fileID, int byteswap, size_t blocksize);

int binReadInt32(int fileID, int byteswap, size_t size, INT32 *ptr);
int binReadInt64(int fileID, int byteswap, size_t size, INT64 *ptr);

int binWriteInt32(int fileID, int byteswap, size_t size, INT32 *ptr);
int binWriteInt64(int fileID, int byteswap, size_t size, INT64 *ptr);

int binReadReal4(int fileID, int byteswap, size_t size, REAL4 *ptr);
int binReadReal8(int fileID, int byteswap, size_t size, REAL8 *ptr);

int binWriteReal4(int fileID, int byteswap, size_t size, REAL4 *ptr);
int binWriteReal8(int fileID, int byteswap, size_t size, REAL8 *ptr);

#endif  /* _BINARY_H */
