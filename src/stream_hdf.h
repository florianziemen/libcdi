#ifndef _STREAM_HDF_H
#define _STREAM_HDF_H

void   hdfDefDimgroup(int streamID);

void   hdfDefTimestep(int streamID, int tsID);
int    hdfInqTimestep(int streamID, int tsID);
void   hdfInqContents(int streamID);
void   hdfDefHistory(int streamID, int size, char *history);
int    hdfInqHistorySize(int streamID);
void   hdfInqHistoryString(int streamID, char *history);

void   hdfEndDef(int streamID);
int    hdfDefRecord(int streamID);
int    hdfCopyRecord(int streamIDdest, int streamIDsrc);
int    hdfReadRecord(int streamID, double *data, int *nmiss);
void   hdfWriteRecord(int streamID, double *data, int nmiss);

void   hdfReadVarDP(int streamID, int varID, double *data, int *nmiss);
void   hdfWriteVarDP(int streamID, int varID, double *data, int nmiss);

int    hdfReadVarSliceDP(int streamID, int varID, int levelID, double *data, int *nmiss);
int    hdfWriteVarSliceDP(int streamID, int varID, int levelID, double *data, int nmiss);

#endif
