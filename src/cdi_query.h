#ifndef CDI_QUERY_H
#define CDI_QUERY_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // CDI query interface

  struct CdiQuery;

  struct CdiQuery *cdiQueryCreate(void);
  struct CdiQuery *cdiQueryClone(const struct CdiQuery *query);
  void cdiQueryDelete(struct CdiQuery *query);
  void cdiQuerySetGroups(struct CdiQuery *query, int numGroups, const char **groups);
  void cdiQuerySetNames(struct CdiQuery *query, int numNames, const char **names);
  void cdiQuerySetCells(struct CdiQuery *query, int numCells, const int64_t *cells);
  void cdiQuerySetLayers(struct CdiQuery *query, int numLayers, const int64_t *layers);
  void cdiQuerySetSteps(struct CdiQuery *query, int numSteps, const int64_t *steps);
  void cdiQuerySetDates(struct CdiQuery *query, int numDates, const double *dates);
  const char *cdiQueryGetGroup(const struct CdiQuery *query, int index);
  int64_t cdiQueryGetCell(const struct CdiQuery *query, int index);
  int64_t cdiQueryGetLayer(const struct CdiQuery *query, int index);
  int64_t cdiQueryGetStep(const struct CdiQuery *query, int index);
  double cdiQueryGetDate(const struct CdiQuery *query, int index);
  int cdiQueryName(struct CdiQuery *query, const char *name);
  int cdiQueryStep(struct CdiQuery *query, int64_t step);
  int cdiQueryNumGroups(const struct CdiQuery *query);
  int cdiQueryNumNames(const struct CdiQuery *query);
  int cdiQueryNumCells(const struct CdiQuery *query);
  int cdiQueryNumLayers(const struct CdiQuery *query);
  int cdiQueryNumSteps(const struct CdiQuery *query);
  int cdiQueryNumDates(const struct CdiQuery *query);
  int cdiQueryNumEntries(const struct CdiQuery *query);
  int cdiQueryNumEntriesFound(const struct CdiQuery *query);
  void cdiQueryPrint(const struct CdiQuery *query);
  void cdiQueryPrintEntriesNotFound(const struct CdiQuery *query);

  // streamOpenReadQuery: Open a dataset for reading and apply query
  int streamOpenReadQuery(const char *path, struct CdiQuery *query);

#ifdef __cplusplus
}
#endif

#endif
