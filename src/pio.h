#ifndef _PIO_H
#define _PIO_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <mpi.h>


void cdiPioFileWritingInit(void);
void cdiPioFileWritingFinalize(void);
size_t cdiPioFileWrite(int fileID, const void *restrict buffer, size_t len,
                       int tsID);

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
