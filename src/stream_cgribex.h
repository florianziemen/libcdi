#ifndef _STREAM_CGRIBEX_H
#define _STREAM_CGRIBEX_H

void cgribexScanTimestep1(int streamID);
int cgribexScanTimestep2(int streamID);
int cgribexScanTimestep(int streamID);

int cgribexDecode(unsigned char *gribbuffer, int gribsize, double *data, int gridsize,
		  int unreduced, int *nmiss, int *zip, double *missval);

size_t cgribexEncode(int varID, int levelID, int vlistID, int gridID, int zaxisID, int date, int time, int numavg, 
		     long datasize, const double *data, int nmiss, unsigned char *gribbuffer, size_t gribbuffersize);

#endif  /* _STREAM_CGRIBEX_H */
