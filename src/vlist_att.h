#ifndef VLIST_ATT_H
#define VLIST_ATT_H

int
vlistAttsGetSize(vlist_t *p, int varID, MPI_Comm comm);

void
vlistAttsPack(vlist_t *p, int varID,
              void * buf, int size, int *position, MPI_Comm comm);

void
vlistAttsUnpack(int vlistID, int varID,
                void * buf, int size, int *position, MPI_Comm comm);

#endif

