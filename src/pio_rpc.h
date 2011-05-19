#ifndef PIO_RPC_H
#define PIO_RPC_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>

extern MPI_Comm commGlob;
extern int sizeGlob;
extern int rankGlob;
extern int nProcsIO;
extern int nProcsCalc;
extern MPI_Comm * commsIO;
extern MPI_Comm commCalc;


enum { GRID      = 1,
       ZAXIS     = 2,
       TAXIS     = 3,
       INSTITUTE = 4,
       MODEL     = 5,
       STREAM    = 6,
       VLIST     = 7,
       SEPARATOR = 77777777,
       END       = 99999999 };



void commsIOCreate ( MPI_Comm, int );
void commsIOCleanup ();

void rpcUnpackResources ( char *, int, MPI_Comm );

#endif

#endif
