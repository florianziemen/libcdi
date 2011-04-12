#ifndef PIO_INTERFACE_
#define PIO_INTERFACE_

#include <mpi.h>

void IOMapping ( int *, int *, int *, int, int *, int );
void IOStop ();
void IOFinalize ();
MPI_Comm  IOInit ( MPI_Comm, int );

#endif
