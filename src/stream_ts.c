#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "stream_ts.h"

void
ts_write_var_slice(stream_t *streamP, int varID, int levelID, int memtype, const void *data, size_t numMissVals)
{
  printf("%s: implementation missing\n", __func__);
  if (CDI_Debug) Message("streamID = %d  varID = %d", streamP->self, varID);

  int vlistID = streamP->vlistID;
  int fileID = streamP->fileID;
  /*
  int ncvarID = cdfDefVar(streamP, varID);

  int gridID = vlistInqVarGrid(vlistID, varID);
  int zaxisID = vlistInqVarZaxis(vlistID, varID);

  int xid, yid, zid;
  cdfGetXYZid(streamP, gridID, zaxisID, &xid, &yid, &zid);

  int dimorder[3];
  vlistInqVarDimorder(vlistID, varID, dimorder);
  bool swapxy = (dimorder[2] == 2 || dimorder[0] == 1) && xid != CDI_UNDEFID && yid != CDI_UNDEFID;

  size_t xsize, ysize;
  size_t start[5], count[5];
  cdfDefineStartAndCountSlice(streamP, varID, levelID, dimorder, xid, yid, zid, start, count, &xsize, &ysize);

  int dtype = vlistInqVarDatatype(vlistID, varID);

  if (numMissVals > 0) cdfDefVarMissval(streamP, varID, dtype, 1);

  size_t nvals = (size_t) gridInqSize(gridID);

  cdf_write_var_data(fileID, vlistID, varID, ncvarID, dtype, nvals, xsize, ysize, swapxy, start, count, memtype, data, numMissVals);
  */
}
