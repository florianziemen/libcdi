#ifndef NAMESPACE_H
#define NAMESPACE_H

typedef struct {
  int idx;
  int nsp;
  int resASent;
}namespaceTuple_t;

void             namespaceCleanup      ( void );
void             namespaceInit         ( int, int * );
void             namespaceShowbits     ( int, char * );
int              namespaceGetNumber    ( void );
int              namespaceGetActive    ( void );
int              namespaceIdxEncode    ( namespaceTuple_t );
int              namespaceIdxEncode2   ( int, int );
namespaceTuple_t namespaceResHDecode   ( int );
int              namespaceHasLocalFile ( int );
int              namespaceAdaptKey     ( int, int );
int              namespaceAdaptKey2    ( int );
void             namespaceDefResASent  ( void );
int              namespaceInqResASent  ( void );

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
