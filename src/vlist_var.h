#ifndef VLIST_VAR_H
#define VLIST_VAR_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#endif

#include "vlist.h"

int  vlistInqVarDecoChunk ( int, int, int );
int  vlistInqVarDecoOff   ( int, int, int );

#ifdef USE_MPI
int  vlistVarGetSize(vlist_t *p, int varID, MPI_Comm comm);
void vlistVarPack(vlist_t *p, int varID,
                  char * buffer, int bufferSize, int * pos, MPI_Comm comm);
void vlistVarUnpack(int vlistID,
                    char * buf, int size, int *position, int, MPI_Comm comm);
void vlistDefVarIOrank    ( int, int, int );
int  vlistInqVarIOrank    ( int, int );
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
