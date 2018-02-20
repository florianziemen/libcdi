#include <stdio.h>
#include "cdi.h"

#define  nlon   12 // Number of longitudes
#define  nlat    6 // Number of latitudes 
#define  nlev    5 // Number of levels    
#define  nts     3 // Number of time steps

int main(void)
{
  int gridID, zaxisID1, zaxisID2, taxisID;
  int vlistID, varID1, varID2, streamID;
  size_t nmiss = 0;
  double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[nlat] = {-75, -45, -15, 15, 45, 75};
  double levs[nlev] = {101300, 92500, 85000, 50000, 20000};
  double var1[nlon*nlat];
  double var2[nlon*nlat*nlev];


  // Create a regular lon/lat grid
  gridID = gridCreate(GRID_LONLAT, nlon*nlat);
  gridDefXsize(gridID, nlon);
  gridDefYsize(gridID, nlat);
  gridDefXvals(gridID, lons);
  gridDefYvals(gridID, lats);

  // Create a surface level Z-axis
  zaxisID1 = zaxisCreate(ZAXIS_SURFACE, 1);

  // Create a pressure level Z-axis
  zaxisID2 = zaxisCreate(ZAXIS_PRESSURE, nlev);
  zaxisDefLevels(zaxisID2, levs);
 
  // Create a variable list
  vlistID = vlistCreate();

  // Define the variables
  varID1 = vlistDefVar(vlistID, gridID, zaxisID1, TIME_VARYING);
  varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARYING);

  // Define the variable names
  vlistDefVarName(vlistID, varID1, "T");
  vlistDefVarName(vlistID, varID2, "P");

  cdiDefKeyInt(vlistID, CDI_GLOBAL, CDI_KEY_CENTRE, 252);

  // Create a Time axis
  taxisID = taxisCreate(TAXIS_ABSOLUTE);

  // Assign the Time axis to the variable list
  vlistDefTaxis(vlistID, taxisID);

  // Create a dataset in netCDF format
  streamID = streamOpenWrite("example_loc.grb", CDI_FILETYPE_GRB2);
  if ( streamID < 0 )
    {
      fprintf(stderr, "%s\n", cdiStringError(streamID));
      return(1);
    }

  // Assign the variable list to the dataset
  streamDefVlist(streamID, vlistID);

  int vlistID0 = streamInqVlist(streamID);

  for ( int varID = 0; varID < 2; ++varID )
    {
      // vlistDefVarIntKey(vlistID0, varID, "centre", 252); 
      vlistDefVarIntKey(vlistID0, varID, "grib2LocalSectionPresent", 1); 
      vlistDefVarIntKey(vlistID0, varID, "grib2LocalSectionNumber", 1); 
      vlistDefVarIntKey(vlistID0, varID, "mpimType", 1); 
      vlistDefVarIntKey(vlistID0, varID, "mpimClass", 2); 
      vlistDefVarIntKey(vlistID0, varID, "mpimUser", 1403); 
      //  vlistDefVarIntKey(vlistID0, varID, "shortendRevNo", 99); 
    }

  // Loop over the number of time steps
  for ( int tsID = 0; tsID < nts; tsID++ )
    {
      // Set the verification date to 1985-01-01 + tsID
      taxisDefVdate(taxisID, 19850101+tsID);
      // Set the verification time to 12:00:00
      taxisDefVtime(taxisID, 120000);
      // Define the time step
      streamDefTimestep(streamID, tsID);

      vlistDefVarIntKey(vlistID0, varID1, "dataTime", 1200+(tsID+1)*10); 

      // Init var1 and var2
      for ( size_t i = 0; i < nlon*nlat;      i++ ) var1[i] = 1.1;
      for ( size_t i = 0; i < nlon*nlat*nlev; i++ ) var2[i] = 2.2;

      // Write var1 and var2
      streamWriteVar(streamID, varID1, var1, nmiss);
      streamWriteVar(streamID, varID2, var2, nmiss);
    }

  // Close the output stream
  streamClose(streamID);

  // Destroy the objects
  vlistDestroy(vlistID);
  taxisDestroy(taxisID);
  zaxisDestroy(zaxisID1);
  zaxisDestroy(zaxisID2);
  gridDestroy(gridID);

  return 0;
}
