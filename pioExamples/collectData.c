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


extern void reshArrayPrint ( char * );

static int hasLocalFile[] = { 0 };


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

void modelRun ()
{
  int gridID, zaxisID, taxisID;
  int vlistID, varID, streamID, tsID;
  int i, nmiss = 0, rank;
  double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[nlat] = {-75, -45, -15, 15, 45, 75};
  double levs[nlev] = {101300, 92500, 85000, 50000, 20000};
  double var[nlon*nlat*nlev];

  namespaceInit ( 1, hasLocalFile );

  gridID = gridCreate(GRID_LONLAT, nlon*nlat);
  gridDefXsize(gridID, nlon);
  gridDefYsize(gridID, nlat);
  gridDefXvals(gridID, lons);
  gridDefYvals(gridID, lats);

  zaxisID = zaxisCreate(ZAXIS_PRESSURE, nlev);
  zaxisDefLevels(zaxisID, levs);

  vlistID = vlistCreate();
  varID = vlistDefVar(vlistID, gridID, zaxisID, TIME_VARIABLE);
  vlistDefVarName(vlistID, varID, "myVar");
  {
    int globfac[] = { 23, 42 };
    vlistDefAttInt(vlistID, varID, "seer's globule factors", DATATYPE_INT16,
                   2, globfac); // bug vlistAttunpack: ..DefAtt.. can't be left out
  }
  taxisID = taxisCreate(TAXIS_ABSOLUTE);
  vlistDefTaxis(vlistID, taxisID);
  
  streamID = streamOpenWrite("example.grb", FILETYPE_GRB);
  if ( streamID < 0 )
    {
      fprintf(stderr, "%s\n", cdiStringError(streamID));
      return;
    }
  

  streamDefVlist(streamID, vlistID);

#ifdef USE_MPI
  pioMetadata2 ( nNodes, nodeSizes );
#endif 

  for ( tsID = 0; tsID < ntsteps; tsID++ )
    {
      taxisDefVdate(taxisID, 19850101+tsID);
      taxisDefVtime(taxisID, 120000);
      streamDefTimestep(streamID, tsID);

      for ( i = 0; i < nlon*nlat*nlev; i++ ) var[i] = 2.2;
      streamWriteVar(streamID, varID, var, nmiss);
    }

#ifdef USE_MPI
  MPI_Comm_rank ( MPI_COMM_WORLD, &rank ); 
  if ( rank == 0 ) reshArrayPrint ( "reshArrayModel" );
#endif
  

  streamClose(streamID);
  vlistDestroy(vlistID);
  taxisDestroy(taxisID);
  zaxisDestroy(zaxisID);
  gridDestroy(gridID);  
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
                                         
  if ( nProcsIO <=0 || nProcsIO >= sizeGlob )
    xabort ( "bad distribution of tasks on PEs" );

  commModel = pioInit_c ( commGlob, nProcsIO, 0 );
#endif

  modelRun ();

#ifdef USE_MPI
  pioFinalize ();
  MPI_Finalize ();                         
#endif
  return 0;
}
