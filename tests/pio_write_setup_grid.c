#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdlib.h>

#ifdef USE_MPI
#include <mpi.h>
#include "pio_util.h"
#endif
#include "dmemory.h"

#include "pio_write_setup_grid.h"
#include "cdi_uuid.h"
#include "simple_model_helper.h"

int
setupGrid(struct model_config *setup, MPI_Comm comm)
{
  int nlon = setup->nlon, nlat = setup->nlat;
  int gridID;
  if (setup->flags & PIO_WRITE_CONFIG_CREATE_CURVILINEAR_GRID_FLAG)
    gridID = createLocalCurvilinearGrid(nlon, nlat);
  else
    gridID = createGlobalLatLonGrid(nlon, nlat);
  if (setup->flags & PIO_WRITE_CONFIG_CREATE_UUID_FLAG)
  {
    int rank = 0;
#if USE_MPI
    xmpi(MPI_Comm_rank(comm, &rank));
#else
    (void)comm;
#endif
    unsigned char uuid[CDI_UUID_SIZE];
    if (rank == 0)
      cdiCreateUUID(uuid);
#if USE_MPI
    MPI_Bcast(uuid, CDI_UUID_SIZE, MPI_UNSIGNED_CHAR, 0, comm);
#endif
    gridDefUUID(gridID, uuid);
  }
  return gridID;
}

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
