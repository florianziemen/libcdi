#ifndef _PIO_H
#define _PIO_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include <stdlib.h>
#include <mpi.h>

#include "cdi_int.h"

void   backendCleanup  ( void );
void   backendInit     ( void );
void   backendFinalize ( void );
int pioFileOpen(const char *filename, const char *mode);
int    pioFileClose    ( int );
size_t pioFileWrite    ( int, int, const void*, size_t );
int cdiPioStreamOpen(const char *filename, const char *filemode,
                     int filetype, Record **record);
void cdiPioStreamDefVlist_(int streamID, int vlistID);
void cdiPioStreamWriteVar_(int streamID, int varID, int memtype,
                           const void *data, int nmiss);
void cdiPioStreamWriteVarChunk_(int streamID, int varID, int memtype,
                                const int rect[][2], const void *data,
                                int nmiss);
void cdiPioStreamClose(stream_t *streamptr);

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
