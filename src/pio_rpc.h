#ifndef PIO_RPC_H
#define PIO_RPC_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>

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
  SEPARATOR = 66666666,
  DATATOKEN = 77777777,
  FUNCCALL  = 88888888,
  END       = 99999999 
};

enum
{
  winBufferOverhead = 1,
  winBufferOverheadChunk = 5,
  winBufferOverheadFuncCall = 3,
  timestepSize = 3
};

#define MAXWINBUFFERSIZE ((size_t)512 * 1024 * 1024)

enum { nFuncs = 3, STREAMOPEN = 0, STREAMDEFVLIST = 1, STREAMCLOSE = 2 };
enum { MAXDATAFILENAME = 256, MINFUNCID = STREAMOPEN, MAXFUNCID = STREAMCLOSE };
extern char * funcMap[nFuncs];

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
