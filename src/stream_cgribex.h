#ifndef _STREAM_GRB_H
#define _STREAM_GRB_H

#ifndef _CGRIBEX_H
#  include "cgribex.h"
#endif

int   grbInqContents(int streamID);
int   grbInqTimestep(int streamID, int tsID);

int   grbInqRecord(int streamID, int *varID, int *levelID);
int   grbDefRecord(int streamID);
int   grbWriteRecord(int streamID, const double *data, int nmiss);
int   grbReadRecord(int streamID, double *data, int *nmiss);
int   grbCopyRecord(int streamIDdest, int streamIDsrc);

void  grbReadVarDP(int streamID, int varID, double *data, int *nmiss);
void  grbWriteVarDP(int streamID, int varID, const double *data, int nmiss);

void  grbReadVarSliceDP(int streamID, int varID, int levelID, double *data, int *nmiss);
int   grbWriteVarSliceDP(int streamID, int varID, int levelID, const double *data, int nmiss);

#endif  /* _STREAM_GRB_H */
