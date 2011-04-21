#ifndef PIO_UTIL_
#define PIO_UTIL_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mpi.h"


#define MAXSTRLNNETCDF    32
#define MAXBUFFERSIZE     32
#define MAXVALUE          10
#define MAXDEBUG           3
#define MAXLEVELINFOS     10
#define MAXVARS           10

#define MAXLEVEL          10
#define MAXLEVELIDX       10
#define MAXRECORDS        10
#define MAXRECIDS         10 
#define MAXVARS           10
#define MAXTSTEPS         10
#define MAXFNAMES         10
#define MAXGHBUFFERSIZE_0 10
#define MAXGHBUFFERSIZE_1 10

#define MAXSTRING        256


#define ddebug MAXDEBUG


typedef enum 
  {
    FINALIZE,
    METADATA
  } command;


void pcdiAbort ( char *, const char *, int, int );
void * pcdiXmalloc ( size_t, const char *, int );
#define xmalloc(size) pcdiXmalloc ( size, __FILE__, __LINE__ )
void * pcdiXcalloc ( size_t, size_t, const char *, int );
#define xcalloc(nmemb,size) pcdiXcalloc(nmemb, size,            \
                                        __FILE__, __LINE__)
void * pcdiXrealloc ( void *, size_t, const char *, int );
#define xrealloc(p,size) pcdiXrealloc(p, size,            \
                                      __FILE__, __LINE__)
void pcdiXMPI ( int, const char *, int );
#define xmpi(ret) pcdiXMPI ( ret, __FILE__, __LINE__ )
void pcdiXMPIStat ( int, const char *, int, MPI_Status * );
#define xmpiStat(ret,stat) pcdiXMPIStat ( ret, __FILE__, __LINE__, stat )
void myDebug ( const char *, const char *, int );
#define xdebug(fun) myDebug (  __FILE__, fun, __LINE__ )
void myDebugComm ( const char *filename, const char *functionname, int line, MPI_Comm *comm );

#endif
