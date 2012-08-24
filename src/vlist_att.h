#ifndef VLIST_ATT_H
#define VLIST_ATT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#endif

#ifdef USE_MPI

int
vlistAttsGetSize(vlist_t *p, int varID, MPI_Comm comm);

void
vlistAttsPack(vlist_t *p, int varID,
              void * buf, int size, int *position, MPI_Comm comm);

void
vlistAttsUnpack(int vlistID, int varID,
                void * buf, int size, int *position, MPI_Comm comm);

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
