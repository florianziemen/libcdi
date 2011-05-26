#ifndef VLIST_VAR_H
#define VLIST_VAR_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#endif

#include "vlist.h"

#ifdef USE_MPI
int vlistVarGetSize(vlist_t *p, int varID, MPI_Comm comm);
void vlistVarPack(vlist_t *p, int varID,
                  char * buffer, int bufferSize, int * pos, MPI_Comm comm);
void vlistVarUnpack(int vlistID,
                    char * buf, int size, int *position, int, MPI_Comm comm);
#endif

#endif
