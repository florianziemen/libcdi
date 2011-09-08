#ifndef PIO_UTIL_
#define PIO_UTIL_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef USE_MPI
#include "mpi.h"
#endif


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

//todo: move to pio_rpc.h
typedef enum
  {
    FINALIZE,
    METADATA,
    WINCREATE,
    WRITETS
  } command;


#ifdef USE_MPI
#define xdebug(fmt, ...)					\
  if ( ddebug ){                                                \
    int rank;                                                   \
    MPI_Comm_rank ( MPI_COMM_WORLD, &rank );                    \
    fprintf ( stderr, "pe%d in %s, %s, line %d: " fmt "\n",     \
              rank,  __func__, __FILE__,  __LINE__,             \
              ## __VA_ARGS__ );                                 \
  }

/*
#ifdef USE_MPI
char * outTextComm ( MPI_Comm * );

#define xdebugComm(comm,fmt, ...)				\
  if ( ddebug ){						\
    fprintf ( stderr, "%s%s, %s, line %d%s: " fmt "\n",		\
	      outTextRank (),  __func__, __FILE__,  __LINE__,	\
	      outTextComm ( comm ),				\
	      ## __VA_ARGS__  );				\
    }
#endif
*/

#else
#define xdebug(fmt, ...)					\
  if ( ddebug ){                                                \
    fprintf ( stderr, "%s, %s, line %d: " fmt "\n",             \
              __func__, __FILE__,  __LINE__,                    \
              ## __VA_ARGS__ );                                 \
  }
#endif

#ifdef USE_MPI
#define xwarning(fmt, ...)						\
  if ( ddebug ){							\
    int rank;								\
    MPI_Comm_rank ( MPI_COMM_WORLD, &rank );				\
    fprintf ( stderr, "WARNING: pe%d in %s, %s, line %d: " fmt "\n",	\
              rank,  __func__, __FILE__,  __LINE__,			\
              ## __VA_ARGS__ );						\
  }
#else
#define xwarning(fmt, ...)					\
  if ( ddebug ){                                                \
    fprintf ( stderr, "WARNING: %s, %s, line %d: " fmt "\n",    \
              __func__, __FILE__,  __LINE__,                    \
              ## __VA_ARGS__ );                                 \
  }
#endif

void pcdiAbort ( char *, const char *, const char *, int );
#define xabort(text) pcdiAbort ( text, __FILE__, __func__, __LINE__ );

void * pcdiXmalloc ( size_t, const char *, const char *, int );
#define xmalloc(size) pcdiXmalloc ( size, __FILE__, __func__,  __LINE__ )

void * pcdiXcalloc ( size_t, size_t, const char *, const char *, int );
#define xcalloc(nmemb,size) pcdiXcalloc(nmemb, size,            \
                                        __FILE__, __func__, __LINE__)

void * pcdiXrealloc ( void *, size_t, const char *, const char *, int );
#define xrealloc(p,size) pcdiXrealloc(p, size,            \
                                      __FILE__, __func__, __LINE__)

void pcdiXMPI ( int, const char *, int );
#define xmpi(ret) pcdiXMPI ( ret, __FILE__, __LINE__ )

#ifdef USE_MPI
void pcdiXMPIStat ( int, const char *, int, MPI_Status * );
#define xmpiStat(ret,stat) pcdiXMPIStat ( ret, __FILE__, __LINE__, stat )

void pcdiDebugComm ( const char *filename, const char *functionname, int line, \
                     MPI_Comm *comm );
#define xdebugComm(comm) pcdiDebugComm (  __FILE__, __func__, __LINE__, comm )
#endif

void pcdiDebugMsg ( const char *filename, const char *functionname, int line, \
                    int tag, int source, int nfinished );
#define xdebugMsg(tag,source,nfinished) pcdiDebugMsg ( __FILE__, __func__, \
                                                        __LINE__, tag, source,\
                                                        nfinished )

void pcdiDebugMsg2 ( const char *filename, const char *functionname, int line, \
                    int tag, int source, char * text );
#define xdebugMsg2(tag,source,text) pcdiDebugMsg ( __FILE__, __func__, \
                                                    __LINE__, tag, source, text )
int xmaxInt ( int, int );
int xminInt ( int, int );
int xsum ( int, int * );

double xchecksum ( int, int, void * );
 
void printArray ( char *, void *, int, int, const char *, const char *, int );
#define xprintArray(ps,array,n,datatype)                                \
  if ( ddebug )                                                         \
    printArray ( ps, array, n, datatype,  __func__, __FILE__, __LINE__ )

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
