#ifndef PIO_C_TEMP
#define PIO_C_TEMP

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifdef USE_MPI
#include "mpi.h"

MPI_Comm pioInit_c ( MPI_Comm, int, int );
#endif

#endif
