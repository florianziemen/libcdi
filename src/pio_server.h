#ifndef PIO_SERVER_
#define PIO_SERVER_

#include <mpi.h>

extern MPI_Comm * commPio;

void IOServer ( MPI_Comm, int );

#endif
