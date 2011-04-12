#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#if ! defined (_CDI_H)
#  include "cdi.h"
#endif

#if defined (HAVE_CF_INTERFACE)

#if ! defined (__CFORTRAN_LOADED)
#  include "cfortran.h"
#endif

#include <mpi.h>
#include "pio.h"

int
pioInit_f( int ptype, MPI_Fint *comm_f, int *color, int *nnodes,
           MPI_Fint *pioCollComm2F )
{
  MPI_Comm comm_c, pioCollComm;
  int retval;
#if defined(USE_MPI) && defined(HAVE_MPI_COMM_F2C)
  int flag = 0;
  if (MPI_Initialized(&flag) == MPI_SUCCESS && flag)
    comm_c = MPI_Comm_f2c(*comm_f);
#else
  comm_c = *comm_f;
#endif
  retval = pioInit(ptype, comm_c, color, nnodes, &pioCollComm);
#if defined(USE_MPI) && defined(HAVE_MPI_COMM_F2C)
  if (MPI_Initialized(&flag) == MPI_SUCCESS && flag)
    *pioCollComm2F = MPI_Comm_c2f(pioCollComm);
#else
  *pioCollComm2F = pioCollComm;
#endif
  return retval;
}

FCALLSCFUN5 (INT, pioInit_f, PIOINIT, pioinit, INT, PVOID, PINT, PINT, PVOID)
FCALLSCSUB0 (pioFinalize, PIOFINALIZE, piofinalize)

#endif
