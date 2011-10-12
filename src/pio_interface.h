#ifndef PIO_INTERFACE_
#define PIO_INTERFACE_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>

void pioBufferData ( int, int, const double *, int );
void pioBufferFuncCall ( const char *, int, ... );

#endif

#endif
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
