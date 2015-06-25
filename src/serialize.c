#include <inttypes.h>
#include <limits.h>
#include <string.h>

#include "cdi.h"
#include "error.h"
#include "serialize.h"
#include "namespace.h"

int
serializeGetSize(int count, int datatype, void *context)
{
  int (*serialize_get_size_p)(int count, int datatype, void *context)
    = (int (*)(int, int, void *))
    namespaceSwitchGet(NSSWITCH_SERIALIZE_GET_SIZE).func;
  return serialize_get_size_p(count, datatype, context);
}

void serializePack(const void *data, int count, int datatype,
                   void *buf, int buf_size, int *position, void *context)
{
  void (*serialize_pack_p)(const void *data, int count, int datatype,
                           void *buf, int buf_size, int *position, void *context)
    = (void (*)(const void *, int, int, void *, int, int *, void *))
    namespaceSwitchGet(NSSWITCH_SERIALIZE_PACK).func;
  serialize_pack_p(data, count, datatype, buf, buf_size, position, context);
}

void serializeUnpack(const void *buf, int buf_size, int *position,
                     void *data, int count, int datatype, void *context)
{
  void (*serialize_unpack_p)(const void *buf, int buf_size, int *position,
                             void *data, int count, int datatype, void *context)
    = (void (*)(const void *, int, int *, void *, int, int, void *))
    namespaceSwitchGet(NSSWITCH_SERIALIZE_UNPACK).func;
  serialize_unpack_p(buf, buf_size, position, data, count, datatype, context);
}



int
serializeGetSizeInCore(int count, int datatype, void *context)
{
  int elemSize;
  (void)context;
  switch (datatype)
  {
  case DATATYPE_INT8:
    elemSize = sizeof (int8_t);
    break;
  case DATATYPE_INT16:
    elemSize = sizeof (int16_t);
    break;
  case DATATYPE_UINT32:
    elemSize = sizeof (uint32_t);
    break;
  case DATATYPE_INT:
    elemSize = sizeof (int);
    break;
  case DATATYPE_FLT:
  case DATATYPE_FLT64:
    elemSize = sizeof (double);
    break;
  case DATATYPE_TXT:
  case DATATYPE_UCHAR:
    elemSize = 1;
    break;
  case DATATYPE_LONG:
    elemSize = sizeof (long);
    break;
  default:
    xabort("Unexpected datatype");
  }
  return count * elemSize;
}

void serializePackInCore(const void *data, int count, int datatype,
                         void *buf, int buf_size, int *position, void *context)
{
  int size = serializeGetSize(count, datatype, context);
  int pos = *position;
  xassert(INT_MAX - pos >= size && buf_size - pos >= size);
  memcpy((unsigned char *)buf + pos, data, (size_t)size);
  pos += size;
  *position = pos;
}

void serializeUnpackInCore(const void *buf, int buf_size, int *position,
                           void *data, int count, int datatype, void *context)
{
  int size = serializeGetSize(count, datatype, context);
  int pos = *position;
  xassert(INT_MAX - pos >= size && buf_size - pos >= size);
  memcpy(data, (unsigned char *)buf + pos, (size_t)size);
  pos += size;
  *position = pos;
}

/*
 * (de-)marshalling function for common data structures
 */
inline int
serializeStrTabGetPackSize(const char **strTab, int numStr,
                           void *context)
{
  xassert(numStr >= 0);
  int packBuffSize = 0;
  for (size_t i = 0; i < (size_t)numStr; ++i)
  {
    size_t len = strlen(strTab[i]);
    packBuffSize +=
      serializeGetSize(1, DATATYPE_INT, context)
      + serializeGetSize((int)len, DATATYPE_TXT, context);
  }
  packBuffSize +=
    serializeGetSize(1, DATATYPE_UINT32, context);
  return packBuffSize;
}

inline void
serializeStrTabPack(const char **strTab, int numStr,
                    void *buf, int buf_size, int *position, void *context)
{
  uint32_t d = 0;
  xassert(numStr >= 0);
  for (size_t i = 0; i < (size_t)numStr; ++i)
  {
    int len = (int)strlen(strTab[i]);
    serializePack(&len, 1, DATATYPE_INT,
                  buf, buf_size, position, context);
    serializePack(strTab[i], len, DATATYPE_TXT,
                  buf, buf_size, position, context);
    d ^= cdiCheckSum(DATATYPE_TXT, len, strTab[i]);
  }
  serializePack(&d, 1, DATATYPE_UINT32,
                buf, buf_size, position, context);
}

inline void
serializeStrTabUnpack(const void *buf, int buf_size, int *position,
                      char **strTab, int numStr, void *context)
{
  uint32_t d, d2 = 0;
  xassert(numStr >= 0);
  for (size_t i = 0; i < (size_t)numStr; ++i)
    {
      int len;
      serializeUnpack(buf, buf_size, position,
                      &len, 1, DATATYPE_INT, context);
      serializeUnpack(buf, buf_size, position,
                      strTab[i], len, DATATYPE_TXT, context);
      strTab[i][len] = '\0';
      d2 ^= cdiCheckSum(DATATYPE_TXT, (size_t)len, strTab[i]);
    }
  serializeUnpack(buf, buf_size, position,
                  &d, 1, DATATYPE_UINT32, context);
  xassert(d == d2);
}
