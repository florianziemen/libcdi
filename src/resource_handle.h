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
 * for reasons of compatibility with cfortran.h, the handle type is
 * int
 */

typedef int cdiResH;

/* return 0 on equality, not 0 otherwise */
typedef int    ( * valCompareFunc )( void *, void * );
typedef void   ( * valDestroyFunc )( void * );
typedef void   ( * valPrintFunc   )( void * );
typedef size_t ( * valGetSizeFunc )( void * );
typedef void   ( * valPackFunc    )( void *, void *, void *, void *, void * );

typedef struct {
  valCompareFunc valCompare;
  valDestroyFunc valDestroy;
  valPrintFunc   valPrint;
  valGetSizeFunc valGetSize;
  valPackFunc    valPack;
}resOps; 
       
       
int    reshPut ( void *, resOps * );

void   reshRemove ( cdiResH, resOps * );

int    reshCountType ( resOps * );

void * reshGetVal ( cdiResH, resOps * );

void   reshGetIndiceArrayOfType ( int, int *, resOps * );

#ifdef USE_MPI
void   reshPackBufferCreate ( char **, size_t *, MPI_Comm );
void   reshPackBufferDestroy ( char ** );
#endif


void   reshLock   ( void );

void   reshUnlock ( void ); 

#endif
