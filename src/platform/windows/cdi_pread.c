#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <windows.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include "cdi_pread.h"
#include "win_err_to_cdi_err.h"

ssize_t
cdi_pread(int fd, void *buf, size_t count, size_t offset)
{
    HANDLE file = (HANDLE)_get_osfhandle(fd);
    if (file == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return -1;
    }

    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(OVERLAPPED));
    overlapped.OffsetHigh = (uint32_t)((offset & 0xFFFFFFFF00000000ULL) >> 32);
    overlapped.Offset     = (uint32_t)(offset & 0xFFFFFFFFULL);

    DWORD chunk = (count > MAXDWORD) ? MAXDWORD : (DWORD)count;
    DWORD bytes_read = 0;

    SetLastError(0);
    BOOL ok = ReadFile(file, buf, chunk, &bytes_read, &overlapped);

    /* EOF on an async-style read via OVERLAPPED reports as an error; treat it as 0 bytes read. */
    if (!ok && GetLastError() != ERROR_HANDLE_EOF)
    {
        errno = translate_windows_error_to_errno(GetLastError());
        return -1;
    }

    return (ssize_t)bytes_read;
}
