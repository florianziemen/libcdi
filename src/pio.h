#ifndef _PIO_H
#define _PIO_H

#ifndef NOMPI
#include <stdlib.h>
#include "mpi.h"

typedef struct{
  int type;
  MPI_Comm comm;
  int size;
  int rank;
  int specialRank;
  MPI_Comm collectorComm;
}pioInfo; 

int pioFileOpenW ( const char* );
int pioFileClose ( int );
size_t pioFileWrite ( int, int, const void*, size_t );

#endif
#endif
