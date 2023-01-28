#include <string.h>
#include <stdlib.h>
#include "cdi.h"

void
cdiQueryInit(CdiQuery *query)
{
  query->numEntries = 0;

  query->numNames = 0;
  query->names = NULL;
  query->namesFound = NULL;

  query->numCellidx = 0;
  query->cellidx = NULL;
  query->cellidxFound = NULL;

  query->numLevidx = 0;
  query->levidx = NULL;
  query->levidxFound = NULL;

  query->numStepidx = 0;
  query->stepidx = NULL;
  query->stepidxFound = NULL;
}

CdiQuery *
cdiQueryCreate()
{
  CdiQuery *query = (CdiQuery *) malloc(sizeof(CdiQuery));
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
          for (int i = 0; i < query->numNames; ++i) free(query->names[i]);
          free(query->names);
          free(query->namesFound);
        }

      if (query->numCellidx)
        {
          free(query->cellidx);
          free(query->cellidxFound);
        }

      if (query->numLevidx)
        {
          free(query->levidx);
          free(query->levidxFound);
        }

      if (query->numStepidx)
        {
          free(query->stepidx);
          free(query->stepidxFound);
        }

      cdiQueryInit(query);
      free(query);
    }
}

int
cdiQueryNumNames(const CdiQuery *query)
{
  return query ? query->numNames : 0;
}

int
cdiQueryNumStepidx(const CdiQuery *query)
{
  return query ? query->numStepidx : 0;
}

int
cdiQueryNumEntries(const CdiQuery *query)
{
  return query ? query->numEntries : 0;
}

void
cdiQuerySetNames(CdiQuery *query, int numEntries, char **names)
{
  if (numEntries)
    {
      query->numEntries += numEntries;
      query->numNames = numEntries;
      query->namesFound = (bool *) calloc(numEntries, sizeof(bool));
      query->names = (char **) malloc(numEntries * sizeof(char *));
      for (int i = 0; i < numEntries; ++i) query->names[i] = strdup(names[i]);
    }
}

void
cdiQuerySetCellidx(CdiQuery *query, int numEntries, size_t *cellidx)
{
  if (numEntries)
    {
      query->numEntries += numEntries;
      query->numCellidx = numEntries;
      query->cellidxFound = (bool *) calloc(numEntries, sizeof(bool));
      query->cellidx = (size_t *) malloc(numEntries * sizeof(size_t));
      for (int i = 0; i < numEntries; ++i) query->cellidx[i] = cellidx[i];
    }
}

void
cdiQuerySetLevidx(CdiQuery *query, int numEntries, int *levidx)
{
  if (numEntries)
    {
      query->numEntries += numEntries;
      query->numLevidx = numEntries;
      query->levidxFound = (bool *) calloc(numEntries, sizeof(bool));
      query->levidx = (int *) malloc(numEntries * sizeof(int));
      for (int i = 0; i < numEntries; ++i) query->levidx[i] = levidx[i];
    }
}

void
cdiQuerySetStepidx(CdiQuery *query, int numEntries, int *stepidx)
{
  if (numEntries)
    {
      query->numEntries += numEntries;
      query->numStepidx = numEntries;
      query->stepidxFound = (bool *) calloc(numEntries, sizeof(bool));
      query->stepidx = (int *) malloc(numEntries * sizeof(int));
      for (int i = 0; i < numEntries; ++i) query->stepidx[i] = stepidx[i];
    }
}

CdiQuery *
cdiQueryClone(const CdiQuery *query)
{
  CdiQuery *queryOut = cdiQueryCreate();

  if (query)
    {
      cdiQuerySetNames(queryOut, query->numNames, query->names);
      cdiQuerySetCellidx(queryOut, query->numCellidx, query->cellidx);
      cdiQuerySetLevidx(queryOut, query->numLevidx, query->levidx);
      cdiQuerySetStepidx(queryOut, query->numStepidx, query->stepidx);
    }

  return queryOut;
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

      if (query->numCellidx)
        {
          printf("Cellidx:");
          for (int i = 0; i < query->numCellidx; ++i) printf(" %zu", query->cellidx[i]);
          printf("\n");
        }

      if (query->numLevidx)
        {
          printf("Levidx:");
          for (int i = 0; i < query->numLevidx; ++i) printf(" %d", query->levidx[i]);
          printf("\n");
        }

      if (query->numStepidx)
        {
          printf("Stepidx:");
          for (int i = 0; i < query->numStepidx; ++i) printf(" %d", query->stepidx[i]);
          printf("\n");
        }
    }
}

int
cdiQueryNumEntriesFound(const CdiQuery *query)
{
  int numEntriesFound = 0;

  if (query)
    {
      if (query->numNames)
        {
          int numNamesFound = 0;
          for (int i = 0; i < query->numNames; ++i) numNamesFound += query->namesFound[i];
          numEntriesFound += numNamesFound;
        }

      if (query->numCellidx)
        {
          int numCellidxFound = 0;
          for (int i = 0; i < query->numCellidx; ++i) numCellidxFound += query->cellidxFound[i];
          numEntriesFound += numCellidxFound;
        }

      if (query->numLevidx)
        {
          int numLevidxFound = 0;
          for (int i = 0; i < query->numLevidx; ++i) numLevidxFound += query->levidxFound[i];
          numEntriesFound += numLevidxFound;
        }

      if (query->numStepidx)
        {
          int numStepidxFound = 0;
          for (int i = 0; i < query->numStepidx; ++i) numStepidxFound += query->stepidxFound[i];
          numEntriesFound += numStepidxFound;
        }
    }

  return numEntriesFound;
}

void
cdiQueryPrintEntriesNotFound(const CdiQuery *query)
{
  if (query)
    {
      int numEnties = cdiQueryNumEntries(query);
      int numEntiesFound = cdiQueryNumEntriesFound(query);

      int numEntriesNotFound = numEnties - numEntiesFound;
      if (numEntriesNotFound > 0)
        {
          if (query->numNames)
            {
              int numNamesFound = 0;
              for (int i = 0; i < query->numNames; ++i) numNamesFound += query->namesFound[i];

              int numNamesNotFound = query->numNames - numNamesFound;
              if (numNamesNotFound > 0)
                {
                  printf("Name not found:");
                  for (int i = 0; i < query->numNames; ++i)
                    if (!query->namesFound[i]) printf(" %s", query->names[i]);
                  printf("\n");
                }
            }

          if (query->numCellidx)
            {
              int numCellidxFound = 0;
              for (int i = 0; i < query->numCellidx; ++i) numCellidxFound += query->cellidxFound[i];

              int numCellidxNotFound = query->numCellidx - numCellidxFound;
              if (numCellidxNotFound > 0)
                {
                  printf("Grid cell index not found:");
                  for (int i = 0; i < query->numCellidx; ++i)
                    if (!query->cellidxFound[i]) printf(" %zu", query->cellidx[i]);
                  printf("\n");
                }
            }

          if (query->numLevidx)
            {
              int numLevidxFound = 0;
              for (int i = 0; i < query->numLevidx; ++i) numLevidxFound += query->levidxFound[i];

              int numLevidxNotFound = query->numLevidx - numLevidxFound;
              if (numLevidxNotFound > 0)
                {
                  printf("Level index not found:");
                  for (int i = 0; i < query->numLevidx; ++i)
                    if (!query->levidxFound[i]) printf(" %d", query->levidx[i]);
                  printf("\n");
                }
            }

          if (query->numStepidx)
            {
              int numStepidxFound = 0;
              for (int i = 0; i < query->numStepidx; ++i) numStepidxFound += query->stepidxFound[i];

              int numStepidxNotFound = query->numStepidx - numStepidxFound;
              if (numStepidxNotFound > 0)
                {
                  printf("Step index not found:");
                  for (int i = 0; i < query->numStepidx; ++i)
                    if (!query->stepidxFound[i]) printf(" %d", query->stepidx[i]);
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
cdiQueryCellidx(CdiQuery *query, size_t cellidx)
{
  if (query && query->numCellidx)
    {
      for (int i = 0; i < query->numCellidx; ++i)
        if (query->cellidx[i] == cellidx)
          {
            query->cellidxFound[i] = true;
            return 0;
          }
    }

  return -1;
}

int
cdiQueryLevidx(CdiQuery *query, int levidx)
{
  if (query && query->numLevidx)
    {
      for (int i = 0; i < query->numLevidx; ++i)
        if (query->levidx[i] == levidx)
          {
            query->levidxFound[i] = true;
            return 0;
          }
    }

  return -1;
}

int
cdiQueryStepidx(CdiQuery *query, int stepidx)
{
  if (query && query->numStepidx)
    {
      for (int i = 0; i < query->numStepidx; ++i)
        if (query->stepidx[i] == stepidx)
          {
            query->stepidxFound[i] = true;
            return 0;
          }
    }

  return -1;
}
