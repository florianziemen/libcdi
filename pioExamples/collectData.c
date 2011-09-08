#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#endif 

#include "cdi.h"
#include "pio_c_temp.h"
#include "pio_util.h"
#include <assert.h>

extern void reshArrayPrint ( char * );

#define nProcsIO 1
#define nNodes 1
static int nodeSizes[nNodes] = {1};

static int hasLocalFile[] = { 0 };

#define nVars 5
static int nlev[nVars] = {1,1,5,5,2};

#define nlon    12 
#define nlat     6 
#define maxlev   5
#define ntsteps  3 



void modelRun ()
{
  int gridID, zaxisID[nVars], taxisID;
  int vlistID, varID[nVars], streamID, tsID;
  int i, j, nmiss = 0, rank;
  double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[nlat] = {-75, -45, -15, 15, 45, 75};
  double levs[maxlev] = {101300, 92500, 85000, 50000, 20000};
  double var[nlon*nlat*maxlev];
  int vdate = 19850101, vtime = 120000;
  int start = CDI_UNDEFID, chunk = CDI_UNDEFID, stop = CDI_UNDEFID;
  char text[1024];

  pioNamespaceInit ( 1, hasLocalFile );

  gridID = gridCreate ( GRID_LONLAT, nlon*nlat );
  gridDefXsize ( gridID, nlon );
  gridDefYsize ( gridID, nlat );
  gridDefXvals ( gridID, lons );
  gridDefYvals ( gridID, lats );

  for ( i = 0; i < nVars; i++ )
    { 
      zaxisID[i] = zaxisCreate ( ZAXIS_PRESSURE, nlev[i] );
      zaxisDefLevels ( zaxisID[i], levs );
    }
  vlistID = vlistCreate ();
    
  for ( i = 0; i < nVars; i++ )  
    varID[i] = vlistDefVar ( vlistID, gridID, zaxisID[i], TIME_VARIABLE );
    
  taxisID = taxisCreate ( TAXIS_ABSOLUTE );
  vlistDefTaxis ( vlistID, taxisID );
  
  streamID = streamOpenWrite ( "example.grb", FILETYPE_GRB );
  if ( streamID < 0 )
    {
      fprintf ( stderr, "%s\n", cdiStringError ( streamID ));
      return;
    }
  

  streamDefVlist ( streamID, vlistID);

#ifdef USE_MPI
  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );
  pioMetadata ( nNodes, nodeSizes );
#endif 

  for ( tsID = 0; tsID < ntsteps; tsID++ )
    {
      taxisDefVdate ( taxisID, vdate + tsID );
      taxisDefVtime ( taxisID, vtime );
      streamDefTimestep ( streamID, tsID );
      for ( i = 0; i < nVars; i++ )
	{
#ifdef USE_MPI
	  start = vlistInqVarDecoOff ( vlistID, varID[i], rank );
	  chunk = vlistInqVarDecoChunk ( vlistID, varID[i], rank );
#else
	  start = 0;
	  chunk = nlon * nlat * nlev[i];
#endif
	  assert ( start != CDI_UNDEFID &&
		   chunk != CDI_UNDEFID );
	  stop = start + chunk;
	  for ( j = start; j < stop; j++ ) var[j] = 2.2;
	  sprintf ( text, "var[%d], start=%d, chunk=%d", i, start, chunk );
	  //xprintArray ( text, &var[start], chunk, DATATYPE_FLT );
	  //streamWriteVar ( streamID, varID[i], &var[start], nmiss );
	  start = CDI_UNDEFID;
	  chunk = CDI_UNDEFID;
	}
    } 
  

#ifdef USE_MPI 
  if ( rank == 0 ) reshArrayPrint ( "reshArrayModel" );
#endif
  
  streamClose ( streamID );
  vlistDestroy ( vlistID );
  taxisDestroy ( taxisID );
  for ( i = 0; i < nVars; i++ )
    zaxisDestroy ( zaxisID[i] );
  gridDestroy ( gridID ); 

  pioNamespaceCleanup (); 
}


int main (int argc, char *argv[]) 
{
  int sizeGlob, rankGlob;

#ifdef USE_MPI
  MPI_Comm commGlob, commModel; 


  MPI_Init ( &argc, &argv);           
  MPI_Comm_dup ( MPI_COMM_WORLD, &commGlob );
  MPI_Comm_set_errhandler ( commGlob, MPI_ERRORS_RETURN );
  MPI_Comm_size ( commGlob, &sizeGlob );
  MPI_Comm_rank ( commGlob, &rankGlob ); 
                                         
  if ( nProcsIO <=0 || nProcsIO >= sizeGlob )
    xabort ( "bad distribution of tasks on PEs" );

  commModel = pioInit_c ( commGlob, nProcsIO, PIO_MPI_NONB );
#endif

  modelRun ();

#ifdef USE_MPI
  pioFinalize ();
  MPI_Finalize ();                         
#endif
  return 0;
}
