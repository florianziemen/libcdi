#ifndef RESOURCE_HANDLE_H
#define RESOURCE_HANDLE_H

/*
 * CDI internal handling of resource handles given to user code
 */

/*
 * for reasons of compatibility with cfortran.h, the handle type is
 * int
 */

typedef int cdiResH;

/* return 0 on equality, not 0 otherwise */
typedef int ( * valCompareFunc )( void *, void * );
typedef void ( * valDestroyFunc ) ( void * );
typedef void ( * valPrintFunc ) ( void * );

typedef struct {
  valCompareFunc valCompare;
  valDestroyFunc valDestroy;
  valPrintFunc   valPrint;
}resOps; 

int    reshPut ( void *, resOps * );

void   reshRemove ( cdiResH, resOps * );

int    reshCountType ( resOps * );

void * reshGetVal ( cdiResH, resOps * );

void   reshGetIndiceArrayOfType ( int, int *, resOps * );

size_t reshGetSendSize ( void );

int    reshGetSendData ( size_t, char * );

void   reshLock   ( void );

void   reshUnlock ( void ); 

#endif
