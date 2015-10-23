#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include "cdipio.h"

#include "pio_util.h"

int
cdiPioCSRLastN(MPI_Comm commSuper, int IOMode, int nProcsIO)
{
  int commRank, commSize;
  xmpi(MPI_Comm_size(commSuper, &commSize));
  xmpi(MPI_Comm_rank(commSuper, &commRank));
  int role;
  switch (IOMode)
  {
  case PIO_NONE:
    xassert(commRank == 0 && commSize == 1);
    role = PIO_NONE;
    break;
  case PIO_MPI:
    role = (commRank < commSize - nProcsIO)
      ? PIO_ROLE_CLIENT : PIO_ROLE_WRITER_COLLECTOR;
    break;
  case PIO_WRITER:
  case PIO_ASYNCH:
    role = (commRank < commSize - nProcsIO)
      ? PIO_ROLE_CLIENT : ((commRank == commSize - 1)
                           ? PIO_ROLE_WRITER
                           : PIO_ROLE_COLLECTOR);
    break;
  case PIO_FPGUARD:
    role = (commRank < commSize - nProcsIO)
      ? PIO_ROLE_CLIENT : ((commRank == commSize - 1)
                           ? PIO_ROLE_FPGUARD
                           : PIO_ROLE_WRITER_COLLECTOR);
    break;
  default:
    abort();
  }
  return role;
}

