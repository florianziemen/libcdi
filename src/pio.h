#ifndef _PIO_H
#define _PIO_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include <stdlib.h>
#include <mpi.h>

void   backendInit     ( void );
void   backendFinalize ( void );
int    pioFileOpenW    ( const char* );
int    pioFileClose    ( int );
size_t pioFileWrite    ( int, int, const void*, size_t );

#else
typedef int MPI_Comm;
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
