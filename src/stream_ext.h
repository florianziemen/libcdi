#ifndef _STREAM_EXT_H
#define _STREAM_EXT_H

#ifndef _EXTRA_H
#include "extra.h"
#endif

int extInqContents(stream_t *streamP);
int extInqTimestep(stream_t *streamP, int tsID);

void extDefField(stream_t *streamP);
void extCopyField(stream_t *streamP2, stream_t *streamP1);
void ext_read_field(stream_t *streamP, int memtype, void *data, size_t *numMissVals);
void ext_write_field(stream_t *streamP, int memtype, const void *data);

void extReadVarDP(stream_t *streamP, int varID, double *data, size_t *numMissVals);
void extWriteVarDP(stream_t *streamP, int varID, const double *data);

void extReadVarSliceDP(stream_t *streamP, int varID, int levelID, double *data, size_t *numMissVals);
void extWriteVarSliceDP(stream_t *streamP, int varID, int levelID, const double *data);

#endif /* _STREAM_EXT_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
