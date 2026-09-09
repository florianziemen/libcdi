#ifndef STREAM_SCAN_H
#define STREAM_SCAN_H

#include "cdi_int.h"

void streamScanResizeRecords1(stream_t *streamP);
int streamScanInitRecords2(stream_t *streamP);
int streamScanInitRecords(stream_t *streamP, int tsID);
void streamScanTimeConstAdjust(stream_t *streamP, const taxis_t *taxis);
void streamScanTsFixNtsteps(stream_t *streamP, off_t recpos);

#endif /* STREAM_SCAN_H */
