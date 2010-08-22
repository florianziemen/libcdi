#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "error.h"
#include "file.h"
#include "swap.h"
#include "binary.h"


#undef  IsBigendian
#define IsBigendian()  ( u_byteorder.c[sizeof(long) - 1] )


UINT32 get_UINT32(unsigned char *x)
{
  /* IsBigendian returns 1 for big endian byte order */
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};

  if ( IsBigendian() )
    return((UINT32)(((UINT32)x[0]<<24)+((UINT32)x[1]<<16)+((UINT32)x[2]<< 8)+ (UINT32)x[3]));
  else
    return((UINT32)(((UINT32)x[3]<<24)+((UINT32)x[2]<<16)+((UINT32)x[1]<< 8)+ (UINT32)x[0]));
}


UINT32 get_SUINT32(unsigned char *x)
{
  /* IsBigendian returns 1 for big endian byte order */
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};

  if ( IsBigendian() )
    return((UINT32)(((UINT32)x[3]<<24)+((UINT32)x[2]<<16)+((UINT32)x[1]<< 8)+ (UINT32)x[0]));
  else
    return((UINT32)(((UINT32)x[0]<<24)+((UINT32)x[1]<<16)+((UINT32)x[2]<< 8)+ (UINT32)x[3]));
}


UINT64 get_UINT64(unsigned char *x)
{
  /* IsBigendian returns 1 for big endian byte order */
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};

  if ( IsBigendian() )
    return((UINT64)(((UINT64)x[0]<<56)+((UINT64)x[1]<<48)+((UINT64)x[2]<<40)+((UINT64)x[3]<<32)+
		    ((UINT64)x[4]<<24)+((UINT64)x[5]<<16)+((UINT64)x[6]<< 8)+ (UINT64)x[7]));
  else
    return((UINT64)(((UINT64)x[7]<<56)+((UINT64)x[6]<<48)+((UINT64)x[5]<<40)+((UINT64)x[4]<<32)+
		    ((UINT64)x[3]<<24)+((UINT64)x[2]<<16)+((UINT64)x[1]<< 8)+ (UINT64)x[0]));
}


UINT64 get_SUINT64(unsigned char *x)
{
  /* IsBigendian returns 1 for big endian byte order */
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};

  if ( IsBigendian() )
    return((UINT64)(((UINT64)x[7]<<56)+((UINT64)x[6]<<48)+((UINT64)x[5]<<40)+((UINT64)x[4]<<32)+
		    ((UINT64)x[3]<<24)+((UINT64)x[2]<<16)+((UINT64)x[1]<< 8)+ (UINT64)x[0]));
  else
    return((UINT64)(((UINT64)x[0]<<56)+((UINT64)x[1]<<48)+((UINT64)x[2]<<40)+((UINT64)x[3]<<32)+
		    ((UINT64)x[4]<<24)+((UINT64)x[5]<<16)+((UINT64)x[6]<< 8)+ (UINT64)x[7]));
}


size_t binReadF77Block(int fileID, int byteswap)
{
  unsigned char f77block[4];
  size_t blocklen = 0;

  if ( fileRead(fileID, f77block, 4) == 4 )
    {
      if ( byteswap )
	blocklen = get_SUINT32(f77block);
      else
	blocklen =  get_UINT32(f77block);
    }

  return (blocklen);
}


void binWriteF77Block(int fileID, int byteswap, size_t blocksize)
{
  static const char *func = "binWriteF77Block";
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  unsigned char f77block[4];

  if ( IsBigendian() )
    {
      if ( byteswap )
	{
	  f77block[0] = (unsigned char) (blocksize);
	  f77block[1] = (unsigned char) (blocksize >>  8);
	  f77block[2] = (unsigned char) (blocksize >> 16);
	  f77block[3] = (unsigned char) (blocksize >> 24);
	}
      else
	{
	  f77block[3] = (unsigned char) (blocksize);
	  f77block[2] = (unsigned char) (blocksize >>  8);
	  f77block[1] = (unsigned char) (blocksize >> 16);
	  f77block[0] = (unsigned char) (blocksize >> 24);
	}
    }
  else
    {
      if ( byteswap )
	{
	  f77block[3] = (unsigned char) (blocksize);
	  f77block[2] = (unsigned char) (blocksize >>  8);
	  f77block[1] = (unsigned char) (blocksize >> 16);
	  f77block[0] = (unsigned char) (blocksize >> 24);
	}
      else
	{
	  f77block[0] = (unsigned char) (blocksize);
	  f77block[1] = (unsigned char) (blocksize >>  8);
	  f77block[2] = (unsigned char) (blocksize >> 16);
	  f77block[3] = (unsigned char) (blocksize >> 24);
	}
    }

  if ( fileWrite(fileID, f77block, 4) != 4 )
    Error(func, "write failed on %s", fileInqName(fileID));
}


int binReadInt32(int fileID, int byteswap, size_t size, INT32 *ptr)
{
  static const char *func = "binReadInt32";

  if ( sizeof(INT32) == 4 )
    {
      fileRead(fileID, (void *) ptr, 4*size);
      if ( byteswap ) swap4byte(ptr, size);
    }
  else
    {
      Error(func, "not implemented for %d byte integer", sizeof(INT32));
    }

  return (0);
}


int binReadInt64(int fileID, int byteswap, size_t size, INT64 *ptr)
{
  static const char *func = "binReadInt64";

  if ( sizeof(INT64) == 8 )
    {
      fileRead(fileID, (void *) ptr, 8*size);
      if ( byteswap ) swap8byte(ptr, size);
    }
  else
    {
      Error(func, "not implemented for %d byte integer", sizeof(INT64));
    }

  return (0);
}


int binReadFlt32(int fileID, int byteswap, size_t size, FLT32 *ptr)
{
  static const char *func = "binReadFlt32";

  if ( sizeof(FLT32) == 4 )
    {
      fileRead(fileID, (void *) ptr, 4*size);
      if ( byteswap ) swap4byte(ptr, size);
    }
  else
    {
      Error(func, "not implemented for %d byte float", sizeof(FLT32));
    }

  return (0);
}


int binReadFlt64(int fileID, int byteswap, size_t size, FLT64 *ptr)
{
  static const char *func = "binReadFlt64";

  if ( sizeof(FLT64) == 8 )
    {
      fileRead(fileID, (void *) ptr, 8*size);
      if ( byteswap ) swap8byte(ptr, size);
    }
  else
    {
      Error(func, "not implemented for %d byte float", sizeof(FLT64));
    }

  return (0);
}


int binWriteInt32(int fileID, int byteswap, size_t size, INT32 *ptr)
{
  static const char *func = "binWriteInt32";

  if ( sizeof(INT32) == 4 )
    {
      if ( byteswap ) swap4byte(ptr, size);
      fileWrite(fileID, (void *) ptr, 4*size);
    }
  else
    {
      Error(func, "not implemented for %d byte integer", sizeof(INT32));
    }

  return (0);
}


int binWriteInt64(int fileID, int byteswap, size_t size, INT64 *ptr)
{
  static const char *func = "binWriteInt64";

  if ( sizeof(INT64) == 8 )
    {
      if ( byteswap ) swap8byte(ptr, size);
      fileWrite(fileID, (void *) ptr, 8*size);
    }
  else
    {
      Error(func, "not implemented for %d byte integer", sizeof(INT64));
    }

  return (0);
}


int binWriteFlt32(int fileID, int byteswap, size_t size, FLT32 *ptr)
{
  static const char *func = "binWriteFlt32";

  if ( sizeof(FLT32) == 4 )
    {
      if ( byteswap ) swap4byte(ptr, size);
      fileWrite(fileID, (void *) ptr, 4*size);
    }
  else
    {
      Error(func, "not implemented for %d byte float", sizeof(FLT32));
    }

  return (0);
}


int binWriteFlt64(int fileID, int byteswap, size_t size, FLT64 *ptr)
{
  static const char *func = "binWriteFlt64";

  if ( sizeof(FLT64) == 8 )
    {
      if ( byteswap ) swap8byte(ptr, size);
      fileWrite(fileID, (void *) ptr, 8*size);
    }
  else
    {
      Error(func, "not implemented for %d byte float", sizeof(FLT64));
    }

  return (0);
}
