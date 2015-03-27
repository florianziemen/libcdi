#include "proprietarySystemWorkarounds.h"
#include "dmemory.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

char* myStrDup(const char* string)
{
  char* result = xmalloc(strlen(string) + 1);
  if(result)
    {
      strcpy(result, string);
    }
  else
    {
      errno = ENOMEM;
    }
  return result;
}

char* myAsprintf(char* format, ...)
{
  va_list args;
  int size = 64;
  char *buffer = xmalloc(size);
  int nchars;
  //Try to print in the allocated space.
  va_start(args, format);
  nchars = vsnprintf(buffer, size, format, args);
  va_end(args);
  if (nchars >= size)
    {
      //Reallocate buffer now that we know how much space is needed.
      size = nchars + 1;
      buffer = xrealloc(buffer, size);
      va_start(args, format);
      vsnprintf(buffer, size, format, args);
      va_end(args);
    }
  return buffer;
}

