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
int vlistVarGetSize(var_t *var, MPI_Comm comm);
void vlistVarPack(var_t *var, char * buffer, int bufferSize, int * pos,
                  MPI_Comm comm);
void vlistVarUnpack(int vlistID,
                    char * buf, int size, int *position, MPI_Comm comm);
#endif

#endif
