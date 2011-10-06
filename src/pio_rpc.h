#ifndef PIO_RPC_H
#define PIO_RPC_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>

extern int nProcsIO;
extern int nProcsCalc;
extern MPI_Comm * commsIO;
extern MPI_Comm commCalc;

typedef enum
  {
    FINALIZE,
    RESOURCES,
    WINCREATE,
    WRITETS
  } command;

enum 
{ GRID      = 1,
  ZAXIS     = 2,
  TAXIS     = 3,
  INSTITUTE = 4,
  MODEL     = 5,
  STREAM    = 6,
  VLIST     = 7,
  START     = 55555555,
  SEPARATOR = 77777777,
  END       = 99999999 
};

enum
{
  winBufferOverhead = 1,
  winBufferOverheadChunk = 3,
  winBufferSizeMax = 1024,
  timestepSize = 3
};



void commsIOCreate ( MPI_Comm, int );
void commsIOCleanup ();

void rpcUnpackResources ( char *, int, MPI_Comm );

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
