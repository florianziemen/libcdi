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

  query->numIndices = 0;
  query->indices = NULL;
  query->indicesFound = NULL;
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

      if (query->numIndices)
        {
          free(query->indices);
          free(query->indicesFound);
        }

      cdiQueryInit(query);
      free(query);
    }
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
cdiQuerySetIndices(CdiQuery *query, int numEntries, size_t *indices)
{
  if (numEntries)
    {
      query->numEntries += numEntries;
      query->numIndices = numEntries;
      query->indicesFound = (bool *) calloc(numEntries, sizeof(bool));
      query->indices = (size_t *) malloc(numEntries * sizeof(size_t));
      for (int i = 0; i < numEntries; ++i) query->indices[i] = indices[i];
    }
}

CdiQuery *
cdiQueryClone(const CdiQuery *query)
{
  CdiQuery *queryOut = cdiQueryCreate();

  if (query)
    {
      cdiQuerySetNames(queryOut, query->numNames, query->names);
      cdiQuerySetIndices(queryOut, query->numIndices, query->indices);
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

      if (query->numIndices)
        {
          printf("Indices:");
          for (int i = 0; i < query->numIndices; ++i) printf(" %zu", query->indices[i]);
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

      if (query->numIndices)
        {
          int numIndicesFound = 0;
          for (int i = 0; i < query->numIndices; ++i) numIndicesFound += query->indicesFound[i];
          numEntriesFound += numIndicesFound;
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

          if (query->numIndices)
            {
              int numIndicesFound = 0;
              for (int i = 0; i < query->numIndices; ++i) numIndicesFound += query->indicesFound[i];

              int numIndicesNotFound = query->numIndices - numIndicesFound;
              if (numIndicesNotFound > 0)
                {
                  printf("Name not found:");
                  for (int i = 0; i < query->numIndices; ++i)
                    if (!query->indicesFound[i]) printf(" %zu", query->indices[i]);
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
cdiQueryIndex(CdiQuery *query, size_t index)
{
  if (query && query->numIndices)
    {
      for (int i = 0; i < query->numIndices; ++i)
        if (query->indices[i] == index)
          {
            query->indicesFound[i] = true;
            return 0;
          }
    }

  return -1;
}
