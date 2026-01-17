/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#if defined(HAVE_CONFIG_H)
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
#include "cdi.h"
#include "dmemory.h"

static int
sum_found(int listSize, bool *listFound)
{
  int numFound = 0;
  for (int i = 0; i < listSize; ++i) numFound += listFound[i];
  return numFound;
}

static int
sum_not_found(int listSize, bool *listFound)
{
  return listSize - sum_found(listSize, listFound);
}

void
cdiQueryInit(CdiQuery *query)
{
  query->numEntries = 0;

  query->numNames = 0;
  query->names = NULL;
  query->namesFound = NULL;

  query->numCells = 0;
  query->cells = NULL;
  query->cellsFound = NULL;

  query->numLayers = 0;
  query->layers = NULL;
  query->layersFound = NULL;

  query->numSteps = 0;
  query->steps = NULL;
  query->stepsFound = NULL;
}

CdiQuery *
cdiQueryCreate(void)
{
  CdiQuery *query = (CdiQuery *) Malloc(sizeof(CdiQuery));
  cdiQueryInit(query);
  return query;
}

void
cdiQueryDelete(CdiQuery *query)
{
  if (query)
  {
    if (query->numNames)
    {
      for (int i = 0; i < query->numNames; ++i) Free(query->names[i]);
      Free(query->names);
      Free(query->namesFound);
    }

    if (query->numCells)
    {
      Free(query->cells);
      Free(query->cellsFound);
    }

    if (query->numLayers)
    {
      Free(query->layers);
      Free(query->layersFound);
    }

    if (query->numSteps)
    {
      Free(query->steps);
      Free(query->stepsFound);
    }

    cdiQueryInit(query);
    Free(query);
  }
}

int
cdiQueryNumNames(const CdiQuery *query)
{
  return query ? query->numNames : 0;
}

int
cdiQueryNumCells(const CdiQuery *query)
{
  return query ? query->numCells : 0;
}

int
cdiQueryNumSteps(const CdiQuery *query)
{
  return query ? query->numSteps : 0;
}

int
cdiQueryNumEntries(const CdiQuery *query)
{
  return query ? query->numEntries : 0;
}

void
cdiQuerySetNames(CdiQuery *query, int numEntries, char **names)
{
  if (numEntries > 0)
  {
    query->numEntries += numEntries;
    query->numNames = numEntries;
    query->namesFound = (bool *) Calloc((size_t) numEntries, sizeof(bool));
    query->names = (char **) Malloc((size_t) numEntries * sizeof(char *));
    for (int i = 0; i < numEntries; ++i) query->names[i] = strdup(names[i]);
  }
}

void
cdiQuerySetCells(CdiQuery *query, int numEntries, size_t *cellidx)
{
  if (numEntries > 0)
  {
    query->numEntries += numEntries;
    query->numCells = numEntries;
    query->cellsFound = (bool *) Calloc((size_t) numEntries, sizeof(bool));
    query->cells = (size_t *) Malloc((size_t) numEntries * sizeof(size_t));
    for (int i = 0; i < numEntries; ++i) query->cells[i] = cellidx[i];
  }
}

void
cdiQuerySetLayers(CdiQuery *query, int numEntries, int *layers)
{
  if (numEntries > 0)
  {
    query->numEntries += numEntries;
    query->numLayers = numEntries;
    query->layersFound = (bool *) Calloc((size_t) numEntries, sizeof(bool));
    query->layers = (int *) Malloc((size_t) numEntries * sizeof(int));
    for (int i = 0; i < numEntries; ++i) query->layers[i] = layers[i];
  }
}

void
cdiQuerySetSteps(CdiQuery *query, int numEntries, int *steps)
{
  if (numEntries > 0)
  {
    query->numEntries += numEntries;
    query->numSteps = numEntries;
    query->stepsFound = (bool *) Calloc((size_t) numEntries, sizeof(bool));
    query->steps = (int *) Malloc((size_t) numEntries * sizeof(int));
    for (int i = 0; i < numEntries; ++i) query->steps[i] = steps[i];
  }
}

size_t
cdiQueryGetCell(const CdiQuery *query, int index)
{
  return (index >= 0 && index < query->numCells) ? query->cells[index] : (size_t) -1;
}

CdiQuery *
cdiQueryClone(const CdiQuery *query)
{
  CdiQuery *queryOut = cdiQueryCreate();

  if (query)
  {
    cdiQuerySetNames(queryOut, query->numNames, query->names);
    cdiQuerySetCells(queryOut, query->numCells, query->cells);
    cdiQuerySetLayers(queryOut, query->numLayers, query->layers);
    cdiQuerySetSteps(queryOut, query->numSteps, query->steps);
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

void
cdiQueryPrint(const CdiQuery *query)
{
  if (query)
  {
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
  }
}

int
cdiQueryNumEntriesFound(const CdiQuery *query)
{
  int numEntriesFound = 0;

  if (query)
  {
    if (query->numNames) numEntriesFound += sum_found(query->numNames, query->namesFound);
    if (query->numCells) numEntriesFound += sum_found(query->numCells, query->cellsFound);
    if (query->numLayers) numEntriesFound += sum_found(query->numLayers, query->layersFound);
    if (query->numSteps) numEntriesFound += sum_found(query->numSteps, query->stepsFound);
  }

  return numEntriesFound;
}

void
cdiQueryPrintEntriesNotFound(const CdiQuery *query)
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

      if (query->numCells)
      {
        if (sum_not_found(query->numCells, query->cellsFound) > 0)
        {
          printf("Grid cell index not found:");
          for (int i = 0; i < query->numCells; ++i)
            if (!query->cellsFound[i]) printf(" %zu", query->cells[i]);
          printf("\n");
        }
      }

      if (query->numLayers)
      {
        if (sum_not_found(query->numLayers, query->layersFound) > 0)
        {
          printf("Layer not found:");
          for (int i = 0; i < query->numLayers; ++i)
            if (!query->layersFound[i]) printf(" %d", query->layers[i]);
          printf("\n");
        }
      }

      if (query->numSteps)
      {
        if (sum_not_found(query->numSteps, query->stepsFound) > 0)
        {
          printf("Step index not found:");
          for (int i = 0; i < query->numSteps; ++i)
            if (!query->stepsFound[i]) printf(" %d", query->steps[i]);
          printf("\n");
        }
      }
    }
  }
}

int
cdiQueryName(CdiQuery *query, const char *name)
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
cdiQueryCell(CdiQuery *query, size_t cellidx)
{
  if (query && query->numCells)
  {
    for (int i = 0; i < query->numCells; ++i)
      if (query->cells[i] == cellidx)
      {
        query->cellsFound[i] = true;
        return 0;
      }
  }

  return -1;
}

int
cdiQueryLayer(CdiQuery *query, int layer)
{
  if (query && query->numLayers)
  {
    for (int i = 0; i < query->numLayers; ++i)
      if (query->layers[i] == layer)
      {
        query->layersFound[i] = true;
        return 0;
      }
  }

  return -1;
}

int
cdiQueryStep(CdiQuery *query, int step)
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
