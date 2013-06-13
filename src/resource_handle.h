#ifndef RESOURCE_HANDLE_H
#define RESOURCE_HANDLE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#include <stdlib.h>
#endif

/*
 * CDI internal handling of resource handles given to user code
 */

/*
 * for reasons of compatibility with cfortran.h, the handle type is: int
 */
typedef int cdiResH;

/* return 0 on equality, not 0 otherwise */
typedef int    ( * valCompareFunc     )( void *, void * );
typedef void   ( * valDestroyFunc     )( void * );
typedef void   ( * valPrintFunc       )( void *, FILE * );
#ifdef USE_MPI
typedef int    ( * valGetPackSizeFunc )( void *, MPI_Comm comm );
typedef void   ( * valPackFunc        )( void *, void *buf, int size, int *pos, MPI_Comm comm );
typedef int    ( * valTxCodeFunc      )( void );
#endif

typedef struct {
  valCompareFunc     valCompare;
  valDestroyFunc     valDestroy;
  valPrintFunc       valPrint;
#ifdef USE_MPI
  valGetPackSizeFunc valGetPackSize;
  valPackFunc        valPack;
  valTxCodeFunc      valTxCode;
#endif
}resOps;

enum { RESH_UNDEFID, ASSIGNED, SUSPENDED, CLOSED };

void   reshListCreate(int namespaceID);
void   reshListDestruct(int namespaceID);
int    reshPut ( void *, resOps * );
void   reshRemove ( cdiResH, resOps * );

int    reshCountType ( resOps * );

void * reshGetValue(const char *, cdiResH, resOps * );
#define reshGetVal(resH, ops)  reshGetValue(__func__, resH, ops)

void   reshGetResHListOfType ( int, int *, resOps * );

#ifdef USE_MPI
void   reshPackBufferCreate ( char **, int *, MPI_Comm );
void   reshPackBufferDestroy ( char ** );
#endif

void   reshSetStatus ( cdiResH, resOps *, int );
int    reshGetStatus ( cdiResH, resOps * );

void   reshLock   ( void );
void   reshUnlock ( void );

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
