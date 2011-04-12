#ifndef PIO_H
#define PIO_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include "mpi.h"

enum {
  PIO_NONE=0,
  PIO_MPI_NONB=1,
  PIO_POSIX_NONB=2,
  PIO_POSIX_ASYNCH=3,
  PIO_POSIX_FPGUARD_SENDRECV=4,
  PIO_POSIX_FPGUARD_THREAD_REFUSE=5,
  PIO_POSIX_FPGUARD_THREAD=6,
};

typedef struct{
  int type;
  MPI_Comm comm;
  int color;
  int size;
  int rank;
  int specialRank;
  MPI_Comm collectorComm;
}pioInfo;

int pioInit ( int ptype, MPI_Comm comm, int *color, int *nnodes,
              MPI_Comm *pioCollComm );
void pioFinalize ();
int pioFileOpenW ( const char* );
int pioFileClose ( int );
size_t pioFileWrite ( int, int, const void*, size_t );

#endif
