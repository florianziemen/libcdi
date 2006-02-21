#ifndef _HDF_H
#define _HDF_H

void hdfDebug(int debug);

const char *hdfLibraryVersion(void);

int  hdfOpen(const char *filename, const char *mode);
void hdfClose(int fileID);

#endif  /* _HDF_H */
