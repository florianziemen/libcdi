#ifndef PIO_RPC_H
#define PIO_RPC_H

#include <mpi.h>

extern MPI_Comm commGlob;
extern int sizeGlob;
extern int rankGlob;
extern int nProcsIO;
extern int nProcsCalc;
extern MPI_Comm * commsIO;
extern MPI_Comm * commCalc; 

void commsIOCreate ( MPI_Comm, int );
void commsIOCleanup ();

#endif
