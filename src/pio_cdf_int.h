#ifndef PIO_CDF_INT_H
#define PIO_CDF_INT_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_LIBNETCDF
#include "cdf_int.h"

int
cdiPio_nc__create(const char *path, int cmode,
                  size_t initialsz, size_t *chunksizehintp,
                  int *ncidp);

void
cdiPioCdfDefVar(int ncid, const char *name, nc_type xtype, int ndims,
                const int dimids[], int *varidp);

void
cdiPioEnableNetCDFParAccess();


#endif
#endif
