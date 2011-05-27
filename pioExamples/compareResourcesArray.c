#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#define nProcsIO 1
#define nNodes 1

#ifdef USE_MPI
#include <mpi.h>
#endif 

#include "cdi.h"
#include "pio_c_temp.h"
#include "pio_util.h"

extern int    reshArrayCompare      ( int, int );
extern void   reshArrayPrint        ( char * );
extern void   reshPackBufferCreate  ( char **, int *, MPI_Comm );
extern void   reshPackBufferDestroy ( char ** );
extern void   rpcUnpackResources    ( char *, int, MPI_Comm );

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
static int hlf[] = { 0, 1 };

void modelRun ()
{
  int gridID, zaxisID1, zaxisID2, taxisID;
  int vlistID, varID1, varID2, streamID, tsID;
  int instID, modelID;
  double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[nlat] = {-75, -45, -15, 15, 45, 75};
  double levs[nlev] = {101300, 92500, 85000, 50000, 20000};
  double var1[nlon*nlat];
  double var2[nlon*nlat*nlev];

  char * recvBuffer, * sendBuffer;
  int bufferSize;
  MPI_Status status;

  int differ;

  namespaceInit ( 2, hlf );
  namespaceSetActive ( 0 );

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
  varID1 = vlistDefVar(vlistID, gridID, zaxisID1, TIME_VARIABLE);
  varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARIABLE);

  instID = institutDef( 0, 0,"MYINSTITUTE", "myInstitute");
  modelID = modelDef ( instID, 0, "myModel");
  
  // Define the variable names
  vlistDefVarName(vlistID, varID1, "varname1");
  {
    int globfac[] = { 23, 42 };
    vlistDefAttInt(vlistID, varID1, "seer's globule factors", DATATYPE_INT16,
                   2, globfac);
  }
  vlistDefVarName(vlistID, varID2, "varname2");
  vlistDefAttTxt(vlistID, varID2, "txt demo", 6, "banana");
  // Create a Time axis
  taxisID = taxisCreate(TAXIS_ABSOLUTE);

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

  // compare test
  reshPackBufferCreate ( &sendBuffer, &bufferSize, MPI_COMM_WORLD );
  xmpi ( MPI_Send ( sendBuffer, bufferSize, MPI_PACKED, 0, 0, MPI_COMM_WORLD ));  
  recvBuffer = xmalloc ( bufferSize * sizeof ( char ));
  xmpi ( MPI_Recv ( recvBuffer, bufferSize, MPI_PACKED, 0,
		    0, MPI_COMM_WORLD, &status )); 
  namespaceSetActive ( 1 );
  rpcUnpackResources ( recvBuffer, bufferSize, MPI_COMM_WORLD ); 
  namespaceSetActive ( 0 ); 
  free ( recvBuffer );  
  reshPackBufferDestroy ( &sendBuffer );
  xdebug ();
  differ = reshArrayCompare ( 0, 1 );
  printf ( "###### The resource arrays %s.\n", differ ? "differ" : "are equal" );
  xdebug ();
  reshArrayPrint ( "reshArrayModel" );

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
  int size;

  MPI_Init(&argc, &argv);           
  MPI_Comm_set_errhandler ( MPI_COMM_WORLD, MPI_ERRORS_RETURN );
  MPI_Comm_size ( MPI_COMM_WORLD, &size ); 

  if ( size != 1 )                                         
      xabort ( "test transition of resource array only with 1 PE." );

  modelRun ();

  MPI_Finalize ();   
  return 0;
}
