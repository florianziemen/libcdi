#ifndef CDI_PREAD_H
#define CDI_PREAD_H

#include "cdi_pread.h"
cdi_ssize_t cdi_pread(int fd, void *buf, size_t count, off_t offset);
#endif
