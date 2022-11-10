#include <string.h>
#include <stdlib.h>
#include "cdi.h"

void
cdiQueryInit(CdiQuery *query)
{
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
  if (query->numNames && query)
    {
      for (int i = 0; i < query->numNames; ++i) free(query->names[i]);
      free(query);
    }
}

void
cdiQuerySetNames(CdiQuery *query, int numNames, char **names)
{
  if (numNames)
    {
      query->numNames = numNames;
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
cdiQueryPrint(CdiQuery *query)
{
  if (query->numNames)
    {
      printf("Names:");
      for (int i = 0; i < query->numNames; ++i) printf(" %s", query->names[i]);
      printf("\n");
    }
}
