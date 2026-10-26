#ifndef _STREAM_TS_H
#define _STREAM_TS_H

#include "cdi_int.h"

void ts_read_var_slice(stream_t *streamP, int varID, int levelID, int memType, void *data, size_t *numMissVals);
void ts_write_var_slice(stream_t *streamP, int varID, int levelID, int memType, const void *data, size_t numMissVals);

#endif
