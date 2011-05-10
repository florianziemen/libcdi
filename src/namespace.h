#ifndef NAMESPACE_H
#define NAMESPACE_H

typedef struct {
  int idx;
  int nsp;
}namespaceTuple_t;


void             namespaceShowbits     ( int, char * );
int              namespaceGetNumber    ( void );
int              namespaceGetActive    ( void );
int              namespaceIdxEncode    ( namespaceTuple_t );
int              namespaceIdxEncode2   ( int, int );
namespaceTuple_t namespaceIdxDecode    ( int );
int              namespaceHasLocalFile ( int );
void             namespaceCleanup      ( void );

#endif
