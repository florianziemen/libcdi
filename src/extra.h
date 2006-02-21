#ifndef _EXTRA_H
#define _EXTRA_H


typedef struct {
  int    checked;
  int    byteswap;
  int    header[4];
  int    hprec;      /* header precision */
  int    dprec;      /* data   precision */
  size_t datasize;
  size_t buffersize;
  void  *buffer;
}
EXTREC;


const char *extLibraryVersion(void);

void extDebug(int debug);

int  extCheckFiletype(int fileID, int *swap);

EXTREC *extNew(void);
void extDelete(EXTREC *extp);

int  extRead(int fileID, EXTREC *extp);
int  extWrite(int fileID, EXTREC *extp);

int  extInqHeader(EXTREC *extp, int *header);
int  extInqDataSP(EXTREC *extp, float *data);
int  extInqDataDP(EXTREC *extp, double *data);

int  extDefHeader(EXTREC *extp, const int *header);
int  extDefDataSP(EXTREC *extp, const float *data);
int  extDefDataDP(EXTREC *extp, const double *data);

#endif  /* _EXTRA_H */
