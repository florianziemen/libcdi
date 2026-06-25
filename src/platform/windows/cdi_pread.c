#include <stdlib.h>

#include <windows.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include "cdi_pread.h"

cdi_ssize_t
cdi_pread(int fd, void *buf, size_t count, size_t offset)
{
  cdi_ssize_t read_bytes = 0;

  OVERLAPPED overlapped;
  memset(&overlapped, 0, sizeof(OVERLAPPED));

  overlapped.OffsetHigh = (uint32_t) ((offset & 0xFFFFFFFF00000000LL) >> 32);
  overlapped.Offset = (uint32_t) (offset & 0xFFFFFFFFLL);

  HANDLE file = (HANDLE) _get_osfhandle(fd);
  SetLastError(0);
  int RF = ReadFile(file, buf, count, &read_bytes, &overlapped);

  /* For some reason it errors when it hits end of file so we don't want to check that */
  if ((RF == 0) && GetLastError() != ERROR_HANDLE_EOF)
  {
    DWORD dwError = GetLastError();
    errno = translate_windows_error_to_errno(dwError);
    return -1;
  }

  return read_bytes;
}
