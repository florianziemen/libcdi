#include "cdi.h"

#define  NLON   12 /* Number of longitudes */
#define  NLAT    6 /* Number of latitudes  */
#define  NLEV    5 /* Number of levels     */
#define  NTIME   3 /* Number of time steps */

int main(void)
{
  int taxisID, vlistID1, vlistID2, varID1, varID2, streamID1, streamID2, tsID;
  int nmiss, vdate, vtime;
  double var1[NLON*NLAT];
  double var2[NLON*NLAT*NLEV];


  /* Open the input dataset */
  streamID1  = streamOpenRead("example.nc");

  /* Get the variable list of the dataset */
  vlistID1 = streamInqVlist(streamID1);

  /* Set the variable IDs */
  varID1 = 0;
  varID2 = 1;

  /* Get the Time axis form the variable list */
  taxisID = vlistInqTaxis(vlistID1);

  /* Open the output dataset */
  streamID2  = streamOpenWrite("example.grb", FILETYPE_GRB);

  vlistID2 = vlistDuplicate(vlistID1);

  streamDefVlist(streamID2, vlistID2);

  /* Loop over the number of time steps */
  for ( tsID = 0; tsID < NTIME; tsID++ )
    {
      /* Inquire the input time step */
      streamInqTimestep(streamID1, tsID);

      /* Get the verification date and time */
      vdate = taxisInqVdate(taxisID);
      vtime = taxisInqVtime(taxisID);

      /* Define the output time step */
      streamDefTimestep(streamID2, tsID);

      /* Read var1 and var2  */
      streamReadVar(streamID1, varID1, var1, &nmiss);
      streamReadVar(streamID1, varID2, var2, &nmiss);

      /* Write var1 and var2 */
      streamWriteVar(streamID2, varID1, var1, nmiss);
      streamWriteVar(streamID2, varID2, var2, nmiss);
    }

  /* Close the streams */
  streamClose(streamID1);
  streamClose(streamID2);

  return 0;
}
