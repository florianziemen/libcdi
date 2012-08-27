#ifndef MODEL_H
#define MODEL_H

#ifdef USE_MPI
int
modelUnpack(void *buf, int size, int *position,
            int, MPI_Comm comm);
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
