#ifndef _SERVICE_H
#define _SERVICE_H


typedef struct {
  int    checked;
  int    byteswap;
  int    header[8];
  int    hprec;      /* header precision */
  int    dprec;      /* data   precision */
  int    recnum;
  size_t datasize;
  size_t buffersize;
  void  *buffer;
}
SRVREC;


const char *srvLibraryVersion(void);

void srvDebug(int debug);

int  srvCheckFiletype(int fileID, int *swap);

SRVREC *srvNew(void);
void srvDelete(SRVREC *srvp);

int  srvRead(int fileID, SRVREC *srvp);
int  srvWrite(int fileID, SRVREC *srvp);

int  srvInqHeader(SRVREC *srvp, int *header);
int  srvInqDataSP(SRVREC *srvp, float *data);
int  srvInqDataDP(SRVREC *srvp, double *data);

int  srvDefHeader(SRVREC *srvp, const int *header);
int  srvDefDataSP(SRVREC *srvp, const float *data);
int  srvDefDataDP(SRVREC *srvp, const double *data);


#endif  /* _SERVICE_H */
