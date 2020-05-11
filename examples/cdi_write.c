#include <stdio.h>
#include "cdi.h"

int main(void)
{
  enum {
    nlon = 12, // Number of longitudes
    nlat =  6, // Number of latitudes
    nlev =  5, // Number of levels
    nts  =  3, // Number of time steps
  };
  size_t nmiss = 0;
  double lons[] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[] = {-75, -45, -15, 15, 45, 75};
  double levs[] = {101300, 92500, 85000, 50000, 20000};
  double var2[nlon*nlat*nlev];


  // Create a regular lon/lat grid
  int gridID = gridCreate(GRID_LONLAT, nlon*nlat);
  gridDefXsize(gridID, nlon);
  gridDefYsize(gridID, nlat);
  gridDefXvals(gridID, lons);
  gridDefYvals(gridID, lats);

  // Create a pressure level Z-axis
  int zaxisID2 = zaxisCreate(ZAXIS_PRESSURE, nlev);
  zaxisDefLevels(zaxisID2, levs);
 
  // Create a variable list
  int vlistID = vlistCreate();

  // Define the variables
  int varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARYING);

  // Define the variable names
  vlistDefVarName(vlistID, varID2, "QC");
  vlistDefVarDatatype(vlistID, varID2, 16);

  // Create a Time axis
  int taxisID = taxisCreate(TAXIS_ABSOLUTE);

  // Assign the Time axis to the variable list
  vlistDefTaxis(vlistID, taxisID);

  // Create a dataset in netCDF format
  int streamID = streamOpenWrite("example.grb", CDI_FILETYPE_GRB2);
  if ( streamID < 0 )
    {
      fprintf(stderr, "%s\n", cdiStringError(streamID));
      return 1;
    }

  // Assign the variable list to the dataset
  streamDefVlist(streamID, vlistID);

  // Loop over the number of time steps
  for ( int tsID = 0; tsID < nts; tsID++ )
    {
      // Set the verification date to 1985-01-01 + tsID
      taxisDefVdate(taxisID, 19850101+tsID);
      // Set the verification time to 12:00:00
      taxisDefVtime(taxisID, 120000);
      // Define the time step
      streamDefTimestep(streamID, tsID);

      // Init var2
      // for ( size_t i = 0; i < nlon*nlat*2; i++ ) var2[i] = 0.;
      // for ( size_t i = nlon*nlat*2; i < nlon*nlat*nlev; i++ ) var2[i] = sin((double)i);
      for ( size_t i = 0; i < nlon*nlat*nlev; i++ ) var2[i] = sin((double)i);

      // Write var1 and var2
      streamWriteVar(streamID, varID2, var2, nmiss);
    }

  // Close the output stream
  streamClose(streamID);

  // Destroy the objects
  vlistDestroy(vlistID);
  taxisDestroy(taxisID);
  zaxisDestroy(zaxisID2);
  gridDestroy(gridID);

  return 0;
}
