#include <stdlib.h>

#include "cdi_pread.h"
#include <unistd.h>

ssize_t cdi_pread(int fd, void *buf, size_t count, off_t offset)
{
  return pread(fd, buf, count, offset);
}
