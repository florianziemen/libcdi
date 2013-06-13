#ifndef NAMESPACE_H
#define NAMESPACE_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


typedef enum {
  STAGE_DEFINITION = 0,
  STAGE_TIMELOOP   = 1,
  STAGE_CLEANUP    = 2,
  STAGE_UNUSED     = 3,
} statusCode;

typedef struct {
  int idx;
  int nsp;
} namespaceTuple_t;

enum namespaceSwitch
{
  NSSWITCH_NO_SUCH_SWITCH = -1,
  NSSWITCH_ABORT,
  NSSWITCH_SERIALIZE_GET_SIZE,
  NSSWITCH_SERIALIZE_PACK,
  NSSWITCH_SERIALIZE_UNPACK,
#ifdef HAVE_LIBNETCDF
  NSSWITCH_NC__CREATE,
  NSSWITCH_CDF_DEF_VAR,
#endif
  NUM_NAMESPACE_SWITCH,
};


int              namespaceNew(int hasLocalFiles);
void             namespaceDelete(int namespaceID);
void             namespaceCleanup      ( void );
int              namespaceGetNumber    ( void );
int              namespaceGetActive    ( void );
int              namespaceIdxEncode    ( namespaceTuple_t );
int              namespaceIdxEncode2   ( int, int );
namespaceTuple_t namespaceResHDecode   ( int );
int              namespaceHasLocalFile ( int );
int              namespaceAdaptKey     ( int, int );
int              namespaceAdaptKey2    ( int );
void             namespaceDefResStatus ( statusCode );
statusCode       namespaceInqResStatus ( void );
void namespaceSwitchSet(enum namespaceSwitch sw, void *value);
void *namespaceSwitchGet(enum namespaceSwitch sw);


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
