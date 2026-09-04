/* DO NOT REMOVE the config.h include file under any circumstances, it's very much needed on some platforms */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cdi_query.h"
#include "dmemory.h"

struct CdiQuery
{
  int numEntries;
  // Groups
  int numGroups;
  bool *groupsFound;
  const char **groups;
  // Names
  int numNames;
  bool *namesFound;
  const char **names;
  // Grid cell indices
  int numCells;
  bool cellsFound[2];
  size_t cells[2];
  // Layer indices
  int numLayers;
  bool layersFound[2];
  int layers[2];
  // Time step indices
  int numSteps;
  bool stepsFound[2];
  int steps[2];
  // Date indices
  int numDates;
  bool datesFound[2];
  double dates[2];
};

static int
sum_found(int listSize, const bool *listFound)
{
  int numFound = 0;
  for (int i = 0; i < listSize; ++i) { numFound += listFound[i]; }
  return numFound;
}

static int
sum_not_found(int listSize, const bool *listFound)
{
  return listSize - sum_found(listSize, listFound);
}

void
cdiQueryInit(struct CdiQuery *query)
{
  query->numEntries = 0;

  query->numGroups = 0;
  query->groups = NULL;
  query->groupsFound = NULL;

  query->numNames = 0;
  query->names = NULL;
  query->namesFound = NULL;

  query->numCells = 0;
  query->cells[0] = 0;
  query->cellsFound[0] = false;

  query->numLayers = 0;
  query->layers[0] = 0;
  query->layersFound[0] = false;

  query->numSteps = 0;
  query->steps[0] = 0;
  query->stepsFound[0] = false;

  query->numDates = 0;
  query->dates[0] = 0;
  query->datesFound[0] = false;
}

struct CdiQuery *
cdiQueryCreate(void)
{
  struct CdiQuery *query = (struct CdiQuery *) Malloc(sizeof(struct CdiQuery));
  if (query) { cdiQueryInit(query); }
  return query;
}

void
cdiQueryDelete(struct CdiQuery *query)
{
  if (query)
  {
    if (query->numGroups)
    {
      for (int i = 0; i < query->numGroups; ++i) Free((void *) query->groups[i]);
      Free((void *) query->groups);
      Free(query->groupsFound);
    }

    if (query->numNames)
    {
      for (int i = 0; i < query->numNames; ++i) Free((void *) query->names[i]);
      Free((void *) query->names);
      Free(query->namesFound);
    }

    cdiQueryInit(query);
    Free(query);
  }
}

int
cdiQueryNumGroups(const struct CdiQuery *query)
{
  return query ? query->numGroups : 0;
}

int
cdiQueryNumNames(const struct CdiQuery *query)
{
  return query ? query->numNames : 0;
}

int
cdiQueryNumCells(const struct CdiQuery *query)
{
  return query ? query->numCells : 0;
}

int
cdiQueryNumLayers(const struct CdiQuery *query)
{
  return query ? query->numLayers : 0;
}

int
cdiQueryNumSteps(const struct CdiQuery *query)
{
  return query ? query->numSteps : 0;
}

int
cdiQueryNumDates(const struct CdiQuery *query)
{
  return query ? query->numDates : 0;
}

int
cdiQueryNumEntries(const struct CdiQuery *query)
{
  return query ? query->numEntries : 0;
}

void
cdiQuerySetGroups(struct CdiQuery *query, int numGroups, const char **groups)
{
  if (numGroups > 0)
  {
    query->numEntries += numGroups;
    query->numGroups = numGroups;
    query->groupsFound = (bool *) Calloc((size_t) numGroups, sizeof(bool));
    query->groups = (const char **) Malloc((size_t) numGroups * sizeof(char *));
    for (int i = 0; i < numGroups; ++i) { query->groups[i] = strdup(groups[i]); }
  }
}

void
cdiQuerySetNames(struct CdiQuery *query, int numNames, const char **names)
{
  if (numNames > 0)
  {
    query->numEntries += numNames;
    query->numNames = numNames;
    query->namesFound = (bool *) Calloc((size_t) numNames, sizeof(bool));
    query->names = (const char **) Malloc((size_t) numNames * sizeof(char *));
    for (int i = 0; i < numNames; ++i) { query->names[i] = strdup(names[i]); }
  }
}

void
cdiQuerySetCells(struct CdiQuery *query, int numCells, const size_t *cells)
{
  if (numCells > 0 && numCells <= 2)
  {
    query->numEntries += numCells;
    query->numCells = numCells;
    for (int i = 0; i < numCells; ++i) { query->cells[i] = cells[i]; }
  }
}

void
cdiQuerySetLayers(struct CdiQuery *query, int numLayers, const int *layers)
{
  if (numLayers > 0 && numLayers <= 2)
  {
    query->numEntries += numLayers;
    query->numLayers = numLayers;
    for (int i = 0; i < numLayers; ++i) { query->layers[i] = layers[i]; }
  }
}

void
cdiQuerySetSteps(struct CdiQuery *query, int numSteps, const int *steps)
{
  if (numSteps > 0 && numSteps <= 2)
  {
    query->numEntries += numSteps;
    query->numSteps = numSteps;
    for (int i = 0; i < numSteps; ++i) { query->steps[i] = steps[i]; }
  }
}

void
cdiQuerySetDates(struct CdiQuery *query, int numDates, const double *dates)
{
  if (numDates > 0 && numDates <= 2)
  {
    query->numEntries += numDates;
    query->numDates = numDates;
    for (int i = 0; i < numDates; ++i) { query->dates[i] = dates[i]; }
  }
}

const char *
cdiQueryGetGroup(const struct CdiQuery *query, int index)
{
  return (index >= 0 && index < query->numGroups) ? query->groups[index] : NULL;
}

size_t
cdiQueryGetCell(const struct CdiQuery *query, int index)
{
  return (index >= 0 && index < query->numCells) ? query->cells[index] : (size_t) -1;
}

int
cdiQueryGetLayer(const struct CdiQuery *query, int index)
{
  return (index >= 0 && index < query->numLayers) ? query->layers[index] : -1;
}

int
cdiQueryGetStep(const struct CdiQuery *query, int index)
{
  return (index >= 0 && index < query->numSteps) ? query->steps[index] : -1;
}

double
cdiQueryGetDate(const struct CdiQuery *query, int index)
{
  return (index >= 0 && index < query->numDates) ? query->dates[index] : -1;
}

struct CdiQuery *
cdiQueryClone(const struct CdiQuery *query)
{
  struct CdiQuery *queryOut = cdiQueryCreate();

  if (query)
  {
    cdiQuerySetGroups(queryOut, query->numGroups, query->groups);
    cdiQuerySetNames(queryOut, query->numNames, query->names);
    cdiQuerySetCells(queryOut, query->numCells, query->cells);
    cdiQuerySetLayers(queryOut, query->numLayers, query->layers);
    cdiQuerySetSteps(queryOut, query->numSteps, query->steps);
    cdiQuerySetDates(queryOut, query->numDates, query->dates);
  }

  return queryOut;
}

static void
print_list_compact_int(int n, const int *list)
{
  for (int i = 0; i < n; ++i)
  {
    int value = list[i];
    printf(" %d", value);
    if ((i + 2) < n && (value + 1) == list[i + 1] && (value + 2) == list[i + 2])
    {
      printf("/to/");
      int last = list[++i];
      while ((i + 1) < n && (last + 1) == list[i + 1]) last = list[++i];
      printf("%d", last);
    }
  }
  printf("\n");
}

static void
print_list_compact_float(int n, const double *list)
{
  for (int i = 0; i < n; ++i)
  {
    double value = list[i];
    printf(" %g", value);
    if ((i + 2) < n && (value + 1) == list[i + 1] && (value + 2) == list[i + 2])
    {
      printf("/to/");
      double last = list[++i];
      while ((i + 1) < n && (last + 1) == list[i + 1]) { last = list[++i]; }
      printf("%g", last);
    }
  }
  printf("\n");
}

void
cdiQueryPrint(const struct CdiQuery *query)
{
  if (query)
  {
    if (query->numGroups)
    {
      printf("Groups:");
      for (int i = 0; i < query->numGroups; ++i) printf(" %s", query->groups[i]);
      printf("\n");
    }

    if (query->numNames)
    {
      printf("Names:");
      for (int i = 0; i < query->numNames; ++i) printf(" %s", query->names[i]);
      printf("\n");
    }

    if (query->numCells)
    {
      printf("Cells:");
      for (int i = 0; i < query->numCells; ++i) printf(" %zu", query->cells[i]);
      printf("\n");
    }

    if (query->numLayers)
    {
      printf("Layers:");
      print_list_compact_int(query->numLayers, query->layers);
    }

    if (query->numSteps)
    {
      printf("Steps:");
      print_list_compact_int(query->numSteps, query->steps);
    }

    if (query->numDates)
    {
      printf("Dates:");
      print_list_compact_float(query->numDates, query->dates);
    }
  }
}

int
cdiQueryNumEntriesFound(const struct CdiQuery *query)
{
  int numEntriesFound = 0;

  if (query)
  {
    if (query->numGroups) { numEntriesFound += sum_found(query->numGroups, query->groupsFound); }
    if (query->numNames) { numEntriesFound += sum_found(query->numNames, query->namesFound); }
    if (query->numCells) { numEntriesFound += sum_found(query->numCells, query->cellsFound); }
    if (query->numLayers) { numEntriesFound += sum_found(query->numLayers, query->layersFound); }
    if (query->numSteps) { numEntriesFound += sum_found(query->numSteps, query->stepsFound); }
    // if (query->numDates) { numEntriesFound += sum_found(query->numDates, query->datesFound); }
  }

  return numEntriesFound;
}

void
cdiQueryPrintEntriesNotFound(const struct CdiQuery *query)
{
  if (query)
  {
    int numEntriesNotFound = cdiQueryNumEntries(query) - cdiQueryNumEntriesFound(query);
    if (numEntriesNotFound > 0)
    {
      if (query->numNames)
      {
        if (sum_not_found(query->numNames, query->namesFound) > 0)
        {
          printf("Name not found:");
          for (int i = 0; i < query->numNames; ++i)
            if (!query->namesFound[i]) printf(" %s", query->names[i]);
          printf("\n");
        }
      }

      if (query->numSteps)
      {
        if (sum_not_found(query->numSteps, query->stepsFound) > 0)
        {
          printf("Step not found:");
          for (int i = 0; i < query->numSteps; ++i)
            if (!query->stepsFound[i]) printf(" %d", query->steps[i]);
          printf("\n");
        }
      }
    }
  }
}

int
cdiQueryGroup(struct CdiQuery *query, const char *group)
{
  if (query && query->numGroups && group && *group)
  {
    for (int i = 0; i < query->numGroups; ++i)
      if (strcmp(group, query->groups[i]) == 0)
      {
        query->groupsFound[i] = true;
        return 0;
      }
  }

  return -1;
}

int
cdiQueryName(struct CdiQuery *query, const char *name)
{
  if (query && query->numNames && name && *name)
  {
    for (int i = 0; i < query->numNames; ++i)
      if (strcmp(name, query->names[i]) == 0)
      {
        query->namesFound[i] = true;
        return 0;
      }
  }

  return -1;
}

int
cdiQueryStep(struct CdiQuery *query, int step)
{
  if (query && query->numSteps)
  {
    for (int i = 0; i < query->numSteps; ++i)
      if (query->steps[i] == step)
      {
        query->stepsFound[i] = true;
        return 0;
      }
  }

  return -1;
}
