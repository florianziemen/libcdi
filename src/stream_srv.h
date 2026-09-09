#ifndef _STREAM_SRV_H
#define _STREAM_SRV_H

#ifndef _SERVICE_H
#include "service.h"
#endif

int srvInqContents(stream_t *streamP);
int srvInqTimestep(stream_t *streamP, int tsID);

void srvDefField(stream_t *streamP);
void srvCopyField(stream_t *streamP2, stream_t *streamP1);
void srv_read_field(stream_t *streamP, int memtype, void *data, size_t *numMissVals);
void srv_write_field(stream_t *streamP, int memtype, const void *data);

void srvReadVarDP(stream_t *streamP, int varID, double *data, size_t *numMissVals);
void srvWriteVarDP(stream_t *streamP, int varID, const double *data);

void srvReadVarSliceDP(stream_t *streamP, int varID, int levelID, double *data, size_t *numMissVals);
void srvWriteVarSliceDP(stream_t *streamP, int varID, int levelID, const double *data);

#endif /* _STREAM_SRV_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
