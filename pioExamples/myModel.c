#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#define nProcsIO 1
#define nNodes 1

#ifdef USE_MPI
#include <mpi.h>
#endif 

#include "cdi.h"
#include "pio_c_temp.h"
#include "pio_util.h"


#define nlon 12 // Number of longitudes
#define nlat  6 // Number of latitudes 
#define nlev  5 // Number of levels    
#define ntsteps   3 // Number of time steps


/*
int nodeSizes[nNodes] = {1,2};
  int nStreams = 6;
  int streamSizes[6] = {5,127,19,18,9,52};
  int vlistSizes[230] = {
  1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,
  47,47,47,47,47,47,47,47,47,47,
  1,1,1,
  11,11,11,11,11,11,11,11,
  11,11,11,11,11,11,11,11,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,5,
  1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,
  11,11,11,11,11,11,11,11,
  11,11,11,11,11,11,11,11,
  11,11,11,11,11,11,11,11,
  11,11,11,11,11,11,11,11,
  11,11,11,11,11 };
*/

int nodeSizes[nNodes] = {1};
int nStreams = 4;
int streamSizes[4] = {2,5,1,3};
int vlistSizes[11] = {5,1,7,3,2,47,11,11,5,47,1 };
int hlf[] = { 0, 0 };

void modelRun ()
{
  int gridID, zaxisID1, zaxisID2, taxisID;
  int vlistID, varID1, varID2, streamID, tsID;
  int i, nmiss = 0;
  double lons[nlon] = {0, 30, 60, 91830, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[nlat] = {-75, -45, -15, 15, 45, 75};
  double levs[nlev] = {101300, 92500, 85000, 50000, 20000};
  double var1[nlon*nlat];
  double var2[nlon*nlat*nlev];

  int gridID2, gridID3;
  int zaxisID3;
  int taxisID2, taxisID3, taxisID4, taxisID5;
  int instID, modelID;

  if  ( ddebug >= 3 ) xdebug ();

#ifdef USE_MPI
  namespaceInit ( 2, hlf );
  namespaceSetActive ( 0 );
#endif

  // Create a regular lon/lat grid
  gridID = gridCreate(GRID_LONLAT, nlon*nlat);
  gridDefXsize(gridID, nlon);
  gridDefYsize(gridID, nlat);
  gridDefXvals(gridID, lons);
  gridDefYvals(gridID, lats);

  // Test namespaces: Create a 2. grid list

#ifdef USE_MPI
  namespaceSetActive ( 1 );
#endif

  gridID2 = gridCreate(GRID_LONLAT, (nlon-1)*(nlat-1));
  gridDefXsize(gridID2, nlon-1);
  gridDefYsize(gridID2, nlat-1);
  gridDefXvals(gridID2, lons);
  gridDefYvals(gridID2, lats);

  gridID3 = gridCreate(GRID_LONLAT, (nlon-2)*(nlat-2));
  gridDefXsize(gridID3, nlon-2);
  gridDefYsize(gridID3, nlat-2);
  gridDefXvals(gridID3, lons);
  gridDefYvals(gridID3, lats);

#ifdef USE_MPI
  namespaceSetActive ( 0 );
#endif


  // Create a surface level Z-axis
  zaxisID1 = zaxisCreate(ZAXIS_SURFACE, 1);

  // Create a pressure level Z-axis
  zaxisID2 = zaxisCreate(ZAXIS_PRESSURE, nlev);
  zaxisDefLevels(zaxisID2, levs);

  // Test namespaces: Create a 2. zaxis list

#ifdef USE_MPI
  namespaceSetActive ( 1 );
#endif
  zaxisID3 = zaxisCreate(ZAXIS_PRESSURE, 3);
  zaxisDefLevels(zaxisID3, levs);

#ifdef USE_MPI
  namespaceSetActive ( 0 );
#endif

  // Create a variable list
  vlistID = vlistCreate();

  // Define the variables
  varID1 = vlistDefVar(vlistID, gridID, zaxisID1, TIME_VARIABLE);
  varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARIABLE);

  instID = institutDef( 0, 0,"MYINSTITUTE", "myInstitute");
  modelID = modelDef ( instID, 0, "myModel");
  
  // Define the variable names
  vlistDefVarName(vlistID, varID1, "varname1");
  vlistDefVarName(vlistID, varID2, "varname2");

  // Create a Time axis
  taxisID = taxisCreate(TAXIS_ABSOLUTE);
  taxisID2 = taxisCreate(TAXIS_ABSOLUTE);

#ifdef USE_MPI
  namespaceSetActive ( 1 );
#endif
  taxisID3 = taxisCreate(TAXIS_ABSOLUTE);
  taxisID4 = taxisCreate(TAXIS_ABSOLUTE);
  taxisID5 = taxisCreate(TAXIS_ABSOLUTE);

#ifdef USE_MPI
  namespaceSetActive ( 0 );
#endif

  // Assign the Time axis to the variable list
  vlistDefTaxis(vlistID, taxisID);

  // Create a dataset in netCDF fromat
  
  streamID = streamOpenWrite("example.grb", FILETYPE_GRB);
  if ( streamID < 0 )
    {
      fprintf(stderr, "%s\n", cdiStringError(streamID));
      return;
    }

  // Assign the variable list to the dataset
  streamDefVlist(streamID, vlistID);

#ifdef USE_MPI
  pioMetadata ();
#endif

  // Loop over the number of time steps
  for ( tsID = 0; tsID < ntsteps; tsID++ )
    {
      // Set the verification date to 1985-01-01 + tsID
      taxisDefVdate(taxisID, 19850101+tsID);
      // Set the verification time to 12:00:00
      taxisDefVtime(taxisID, 120000);
      // Define the time step
      streamDefTimestep(streamID, tsID);

      // Init var1 and var2
      for ( i = 0; i < nlon*nlat;      i++ ) var1[i] = 1.1;
      for ( i = 0; i < nlon*nlat*nlev; i++ ) var2[i] = 2.2;


      // Write var1 and var2
      streamWriteVar(streamID, varID1, var1, nmiss);
      streamWriteVar(streamID, varID2, var2, nmiss);
    }
  
  //reshArrayPrint ();

  // Close the output stream
  streamClose(streamID);

  // Destroy the objects
  vlistDestroy(vlistID);
  taxisDestroy(taxisID);
  zaxisDestroy(zaxisID1);
  zaxisDestroy(zaxisID2);
  gridDestroy(gridID);
  return;
}


int main (int argc, char *argv[]) 
{
  int sizeGlob, rankGlob;

#ifdef USE_MPI
  MPI_Comm commGlob, commModel; 


  MPI_Init(&argc, &argv);           
  MPI_Comm_dup ( MPI_COMM_WORLD, &commGlob );
  MPI_Comm_set_errhandler ( commGlob, MPI_ERRORS_RETURN );
  MPI_Comm_size ( commGlob, &sizeGlob );
  MPI_Comm_rank ( commGlob, &rankGlob ); 

  if ( ! TEST_LOCAL )                                           
    if ( nProcsIO <=0 || nProcsIO >= ( float ) sizeGlob / 2.0 )
      pcdiAbort ( "bad distribution of tasks on PEs", __FILE__, __LINE__, rankGlob );
#endif  

  if  ( ddebug >= 3 ) xdebug ();

#ifdef USE_MPI
  commModel = pioInit_c ( commGlob, nProcsIO, 0 );
#endif

  modelRun ();

#ifdef USE_MPI
  pioFinalize ();
  MPI_Finalize ();                         
#endif
  return 0;
}
