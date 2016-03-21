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

int
setupGrid(struct model_config *setup, MPI_Comm comm)
{
  int nlon = setup->nlon, nlat = setup->nlat;
  int gridID = gridCreate ( GRID_LONLAT, nlon*nlat );
  gridDefXsize ( gridID, nlon );
  gridDefYsize ( gridID, nlat );
  {
    double *lons = (double *)Malloc((size_t)nlon * sizeof (lons[0]));
    for (size_t i = 0; i < (size_t)nlon; ++i)
      lons[i] = ((double)(i * 360))/nlon;
    gridDefXvals ( gridID, lons );
    Free(lons);
  }
  {
    double *lats = (double *)Malloc((size_t)nlat * sizeof (lats[0]));
    for (size_t i = 0; i < (size_t)nlat; ++i)
      lats[i] = ((double)(i * 180))/nlat - 90.0;
    gridDefYvals ( gridID, lats );
    Free(lats);
  }
  if (setup->create_uuid)
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
