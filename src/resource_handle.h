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

typedef struct {
  valCompareFunc valCompare;
  valDestroyFunc valDestroy;
}resOps; 

int
reshPut ( void *, resOps * );

void *
reshGetVal ( cdiResH h );

resOps *
reshGetOps ( cdiResH );

void
reshRemove ( cdiResH );

#endif
