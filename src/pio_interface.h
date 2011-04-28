#ifndef PIO_INTERFACE_
#define PIO_INTERFACE_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>

void IOMapping ( int *, int *, int *, int, int *, int );
void pioServerFinalize ();

#endif

#endif
