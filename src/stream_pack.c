#include "cdi_int.h"
#include "stream_pack.h"
#include "namespace.h"
#include "resource_unpack.h"
#include "serialize.h"

int
streamTxCode(void *s)
{
  (void) s;
  return STREAM;
}

enum
{
  streamNint = 10,
};

int
streamGetPackSize(void *voidP, void *context)
{
  stream_t *streamP = (stream_t *) voidP;
  int packBufferSize = serializeGetSize(streamNint, CDI_DATATYPE_INT, context) + serializeGetSize(2, CDI_DATATYPE_UINT32, context)
                       + serializeGetSize((int) strlen(streamP->filename) + 1, CDI_DATATYPE_TXT, context)
                       + serializeGetSize(1, CDI_DATATYPE_FLT64, context);
  return packBufferSize;
}

void
streamPack(void *streamPV, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  stream_t *streamP = (stream_t *) streamPV;
  int intBuffer[streamNint];

  intBuffer[0] = streamP->self;
  intBuffer[1] = streamP->filetype;
  intBuffer[2] = (int) strlen(streamP->filename) + 1;
  intBuffer[3] = streamP->vlistID;
  intBuffer[4] = streamP->byteorder;
  intBuffer[5] = streamP->comptype;
  intBuffer[6] = streamP->complevel;
  intBuffer[7] = streamP->unreduced;
  intBuffer[8] = streamP->sortname;
  intBuffer[9] = streamP->have_missval;

  serializePack(intBuffer, streamNint, CDI_DATATYPE_INT, packBuffer, packBufferSize, packBufferPos, context);
  uint32_t d = cdiCheckSum(CDI_DATATYPE_INT, streamNint, intBuffer);
  serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);

  serializePack(&CDI_Default_Missval, 1, CDI_DATATYPE_FLT64, packBuffer, packBufferSize, packBufferPos, context);
  serializePack(streamP->filename, intBuffer[2], CDI_DATATYPE_TXT, packBuffer, packBufferSize, packBufferPos, context);
  d = cdiCheckSum(CDI_DATATYPE_TXT, intBuffer[2], streamP->filename);
  serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
}

struct streamAssoc
streamUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context)
{
  int intBuffer[streamNint];
  uint32_t d;
  char filename[CDI_MAX_NAME];

  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, intBuffer, streamNint, CDI_DATATYPE_INT, context);
  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
  xassert(cdiCheckSum(CDI_DATATYPE_INT, streamNint, intBuffer) == d);

  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &CDI_Default_Missval, 1, CDI_DATATYPE_FLT64, context);
  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &filename, intBuffer[2], CDI_DATATYPE_TXT, context);
  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);
  xassert(d == cdiCheckSum(CDI_DATATYPE_TXT, intBuffer[2], filename));
  int targetStreamID = namespaceAdaptKey(intBuffer[0], originNamespace),
      streamID = streamOpenID(filename, 'w', intBuffer[1], targetStreamID);
  xassert(streamID >= 0 && targetStreamID == streamID);
  streamDefByteorder(streamID, intBuffer[4]);
  streamDefCompType(streamID, intBuffer[5]);
  streamDefCompLevel(streamID, intBuffer[6]);
  stream_t *streamP = stream_to_pointer(streamID);
  streamP->unreduced = intBuffer[7];
  streamP->sortname = intBuffer[8];
  streamP->have_missval = intBuffer[9];
  struct streamAssoc retval = { streamID, intBuffer[3] };
  return retval;
}
