#ifndef BINARY_H
#define BINARY_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>

#ifndef HOST_ENDIANNESS
#ifdef __cplusplus
static const uint32_t HOST_ENDIANNESS_temp[1] = { UINT32_C(0x00030201) };
#define HOST_ENDIANNESS (((const unsigned char *) HOST_ENDIANNESS_temp)[0])
#else
#define HOST_ENDIANNESS (((const unsigned char *) &(const uint32_t[1]){ UINT32_C(0x00030201) })[0])
#endif
#endif

uint32_t get_uint32(unsigned char *x);
uint64_t get_uint64(unsigned char *x);
uint32_t get_swap_uint32(unsigned char *x);
uint64_t get_swap_uint64(unsigned char *x);

size_t binReadF77Block(int fileID, int byteswap);
void binWriteF77Block(int fileID, int byteswap, size_t blocksize);

int binReadInt32(int fileID, int byteswap, size_t size, int32_t *ptr);
int binReadInt64(int fileID, int byteswap, size_t size, int64_t *ptr);

int binWriteInt32(int fileID, int byteswap, size_t size, int32_t *ptr);
int binWriteInt64(int fileID, int byteswap, size_t size, int64_t *ptr);

int binWriteFlt32(int fileID, int byteswap, size_t size, float *ptr);
int binWriteFlt64(int fileID, int byteswap, size_t size, double *ptr);

#ifdef HAVE__FLOAT16
int binWriteFlt16(int fileID, size_t size, _Float16 *ptr);
#endif

#endif /* BINARY_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
