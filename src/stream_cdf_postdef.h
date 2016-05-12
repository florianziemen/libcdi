#ifndef STREAM_CDF_POSTDEF_H
#define STREAM_CDF_POSTDEF_H

#include <stdlib.h>

struct cdfPostDefAction
{
  void *data;
  void (*action)(void *data);
  void (*cleanup)(void *data);
};

struct cdfPostDefActionList
{
  size_t size, len;
  struct cdfPostDefAction actions[];
};

void
cdfPostDefActionGridProp(int gridID, int fileID, int ncvarid,
                         enum gridPropInq gridProp,
                         struct cdfPostDefActionList **delayed);

struct cdfPostDefActionList *
cdfPostDefActionAdd(struct cdfPostDefActionList *list,
                    struct cdfPostDefAction addendum);

void
cdfDelayedPutVarDeepCleanup(void *data);

void
cdfPostDefActionAddPutVal(struct cdfPostDefActionList **delayed,
                          int fileID, int ncvarid, const double *values,
                          void (*cleanup)(void *));

#endif
