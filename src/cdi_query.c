#include <string.h>
#include <stdlib.h>
#include "cdi.h"

void
cdiQueryInit(CdiQuery *query)
{
  query->numEntries = 0;
  query->numNames = 0;
  query->names = NULL;
}

CdiQuery *
cdiQueryCreate()
{
  CdiQuery *query = (CdiQuery*) malloc(sizeof(CdiQuery));
  cdiQueryInit(query);
  return query;
}

void
cdiQueryDelete(CdiQuery *query)
{
  if (query && query->numNames)
    {
      for (int i = 0; i < query->numNames; ++i) free(query->names[i]);
      free(query->names);
      free(query->namesFound);
      free(query);
    }
}

int
cdiQueryNumEntries(const CdiQuery *query)
{
  return query ? query->numEntries : 0;
}

void
cdiQuerySetNames(CdiQuery *query, int numNames, char **names)
{
  if (numNames)
    {
      query->numEntries += numNames;
      query->numNames = numNames;
      query->namesFound = (bool*)calloc(numNames, sizeof(bool));
      query->names = (char**)malloc(numNames * sizeof(char*));
      for (int i = 0; i < numNames; ++i) query->names[i] = strdup(names[i]);
    }
}

CdiQuery *
cdiQueryClone(CdiQuery *query)
{
  CdiQuery *queryOut = cdiQueryCreate();

  if (query)
    {
      cdiQuerySetNames(queryOut, query->numNames, query->names);
    }

  return queryOut;
}

void
cdiQueryPrint(const CdiQuery *query)
{
  if (query && query->numNames)
    {
      printf("Names:");
      for (int i = 0; i < query->numNames; ++i) printf(" %s", query->names[i]);
      printf("\n");
    }
}

int
cdiQueryNumEntriesFound(const CdiQuery *query)
{
  int numEntriesFound = 0;

  if (query && query->numNames)
    {
      int numNamesFound = 0;
      for (int i = 0; i < query->numNames; ++i) numNamesFound += query->namesFound[i];

      numEntriesFound += numNamesFound;
    }

  return numEntriesFound;
}

void
cdiQueryPrintEntriesNotFound(const CdiQuery *query)
{
  if (query)
    {
      const int numEnties = cdiQueryNumEntries(query);
      const int numEntiesFound = cdiQueryNumEntriesFound(query);

      const int numEntriesNotFound = numEnties - numEntiesFound;
      if (numEntriesNotFound > 0 && query->numNames)
        {
          int numNamesFound = 0;
          for (int i = 0; i < query->numNames; ++i) numNamesFound += query->namesFound[i];

          const int numNamesNotFound = query->numNames - numNamesFound;
          if (numNamesNotFound > 0)
            {
              printf("Name not found:");
              for (int i = 0; i < query->numNames; ++i)
                if (!query->namesFound[i]) printf(" %s", query->names[i]);
              printf("\n");
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
