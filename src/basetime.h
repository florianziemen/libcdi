#ifndef _BASETIME_H
#define _BASETIME_H

#include <stdbool.h>

//#define USE_TIMECACHE 1
#ifdef USE_TIMECACHE
#define MAX_TIMECACHE_SIZE 1024 * 1024
#endif

typedef struct
{
  size_t size;
  size_t startid;
  size_t maxvals;
#ifdef USE_TIMECACHE
  double cache[MAX_TIMECACHE_SIZE];
#endif
} timecache_t;

typedef struct
{
  int ncvarid;
  int ncdimid;
  int ncvarboundsid;
  int leadtimeid;
  bool lunits;
  bool lwrf;  // true for time axis in WRF format
  timecache_t *timevar_cache;
} basetime_t;

void basetimeInit(basetime_t *basetime);

#endif /* _BASETIME_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
