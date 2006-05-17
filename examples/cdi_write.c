#include <stdio.h>
#include "cdi.h"

#define  NLON   12 /* Number of longitudes */
#define  NLAT    6 /* Number of latitudes  */
#define  NLEV    5 /* Number of levels     */
#define  NTIME   3 /* Number of time steps */

int main(void)
{
  int gridID, zaxisID1, zaxisID2, taxisID;
  int vlistID, varID1, varID2, streamID, tsID;
  int i, nmiss = 0;
  double lons[NLON] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[NLAT] = {-75, -45, -15, 15, 45, 75};
  double levs[NLEV] = {101300, 92500, 85000, 50000, 20000};
  double var1[NLON*NLAT];
  double var2[NLON*NLAT*NLEV];


  /* Create a regular lon/lat grid */
  gridID = gridCreate(GRID_LONLAT, NLON*NLAT);
  gridDefXsize(gridID, NLON);
  gridDefYsize(gridID, NLAT);
  gridDefXvals(gridID, lons);
  gridDefYvals(gridID, lats);

  /* Create a surface level Z-axis */
  zaxisID1 = zaxisCreate(ZAXIS_SURFACE, 1);

  /* Create a pressure level Z-axis */
  zaxisID2 = zaxisCreate(ZAXIS_PRESSURE, NLEV);
  zaxisDefLevels(zaxisID2, levs);
 
  /* Create a variable list */
  vlistID = vlistCreate();

  /* Define the variables */
  varID1 = vlistDefVar(vlistID, gridID, zaxisID1, TIME_VARIABLE);
  varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARIABLE);

  /* Define the variable names */
  vlistDefVarName(vlistID, varID1, "varname1");
  vlistDefVarName(vlistID, varID2, "varname2");

  /* Create a Time axis */
  taxisID = taxisCreate(TAXIS_ABSOLUTE);

  /* Assign the Time axis to the variable list */
  vlistDefTaxis(vlistID, taxisID);

  /* Create a dataset in netCDF format */
  streamID = streamOpenWrite("example.nc", FILETYPE_NC);
  if ( streamID < 0 )
    {
      fprintf(stderr, "%s\n", cdiStringError(streamID));
      return(1);
    }

  /* Assign the variable list to the dataset */
  streamDefVlist(streamID, vlistID);

  /* Loop over the number of time steps */
  for ( tsID = 0; tsID < NTIME; tsID++ )
    {
      /* Set the verification date to 1985-01-01 + tsID */
      taxisDefVdate(taxisID, 19850101+tsID);
      /* Set the verification time to 12:00 */
      taxisDefVtime(taxisID, 1200);
      /* Define the time step */
      streamDefTimestep(streamID, tsID);

      /* Init var1 and var2 */
      for ( i = 0; i < NLON*NLAT;      i++ ) var1[i] = 1;
      for ( i = 0; i < NLON*NLAT*NLEV; i++ ) var2[i] = 2;

      /* Write var1 and var2 */
      streamWriteVar(streamID, varID1, var1, nmiss);
      streamWriteVar(streamID, varID2, var2, nmiss);
    }

  /* Close the output stream */
  streamClose(streamID);

  /* Destroy the objects */
  vlistDestroy(vlistID);
  taxisDestroy(taxisID);
  zaxisDestroy(zaxisID1);
  zaxisDestroy(zaxisID2);
  gridDestroy(gridID);

  return 0;
}
