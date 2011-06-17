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
namespaceTuple_t namespaceResHDecode   ( int );
int              namespaceHasLocalFile ( int );
void             namespaceCleanup      ( void );
int              namespaceAdaptKey     ( int, int );

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
