#ifndef CDI_BORGES_H
#define CDI_BORGES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cdi_int.h"

int borges_connect(const char *path, char filemode, stream_t *streamptr);
void borges_disconnect(int sock);
int borges_write_grib_message(stream_t *streamptr, const void *gribbuffer, size_t nbytes);

#endif /* CDI_BORGES_H */
