#ifndef NAMESPACE_H
#define NAMESPACE_H

typedef struct {
  int idx;
  int nsp;
}namespaceTuple_t;


void             namespaceShowbits   ( int n, char *name );
int              namespaceIdxEncode  ( namespaceTuple_t tin );
int              namespaceIdxEncode2 ( int, int );
namespaceTuple_t namespaceIdxDecode  ( int code );

#endif
