#ifndef PIO_WRITE_H
#define PIO_WRITE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#else
typedef int MPI_Comm;
#endif

#include <stdbool.h>

struct model_config
{
  int nlon, nlat, nts, max_nlev, nvars;
  int filetype, datatype;
  bool compute_checksum, create_uuid;
  const char *suffix;
};

void
modelRun(struct model_config setup, MPI_Comm comm);

#endif
