#ifndef TSTORE_H
#define TSTORE_H

// clang-format off

#ifdef __cplusplus
extern "C" {
#endif

#include "cdi_int.h"

const char *ts_lib_version(void);

int ts_open(const char *fileName, const char *mode, int *fileType);
void ts_close(int fileID);

int ts_inq_contents(stream_t *streamP);
int ts_inq_timestep(stream_t *streamP, int tsID);
void ts_read_var_slice(stream_t *streamP, int varID, int levelID, int memtype, void *data, size_t *numMissVals);
void ts_read_field(stream_t *streamP, int memtype, void *data, size_t *numMissVals);

void cdf_create_records(stream_t *streamP, size_t tsID);

#ifdef __cplusplus
}
#endif

// clang-format on

#endif
