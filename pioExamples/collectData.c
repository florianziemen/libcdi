#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#include "pio_c_temp.h"
#endif 

#include "cdi.h"
#include "pio_util.h"


void modelRun ()
{
  enum {
    filetype    = FILETYPE_GRB,
    ntfiles     = 2,
    ntsteps     = 3,
    nVars       = 5,
    nlon        = 12,
    nlat        = 6,
    maxlev      = 5 };
  
  static int nlev[nVars]    = {1,1,5,5,2};
  static char * name        = "example";

  int gridID, zaxisID[nVars], taxisID;
  int vlistID, varID[nVars], streamID, tsID, tfID = 0;
  int i, j, nmiss = 0, rank;
  double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[nlat] = {-75, -45, -15, 15, 45, 75};
  double levs[maxlev] = {101300, 92500, 85000, 50000, 20000};
  double var[nlon*nlat*maxlev];
  int vdate = 19850101, vtime = 120000;
  int start = CDI_UNDEFID, chunk = CDI_UNDEFID, stop = CDI_UNDEFID;
  char filename[1024];

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
  
  sprintf ( &filename[0], "%s_%d.grb", name, tfID );
  streamID = streamOpenWrite ( filename, filetype );
  xassert ( streamID >= 0 );
  streamDefVlist ( streamID, vlistID);

  pioEndDef ();

  for ( tfID = 0; tfID < ntfiles; tfID++ )
    {
      if ( tfID > 0 )
	{
	  streamClose ( streamID );
	  sprintf ( &filename[0], "%s_%d.grb", name, tfID );
	  streamID = streamOpenWrite ( filename, filetype );
	  xassert ( streamID >= 0 );
	  streamDefVlist ( streamID, vlistID );
	}
      for ( tsID = 0; tsID < ntsteps; tsID++ )
	{
	  taxisDefVdate ( taxisID, vdate + tsID );
	  taxisDefVtime ( taxisID, vtime );
	  streamDefTimestep ( streamID, tsID );
	  for ( i = 0; i < nVars; i++ )
	    {
	      start = pioInqVarDecoOff   ( vlistID, varID[i] );
	      chunk = pioInqVarDecoChunk ( vlistID, varID[i] );
	      stop = start + chunk;
	      for ( j = start; j < stop; j++ ) var[j] = 2.2;	      
	      streamWriteVar ( streamID, varID[i], &var[start], nmiss );
	      start = CDI_UNDEFID;
	      chunk = CDI_UNDEFID;
	    }
	  pioWriteTimestep ( tsID, vdate, vtime );
	}
    }
   
  streamClose ( streamID );
  vlistDestroy ( vlistID );
  taxisDestroy ( taxisID );
  for ( i = 0; i < nVars; i++ )
    zaxisDestroy ( zaxisID[i] );
  gridDestroy ( gridID );  
}


int main (int argc, char *argv[]) 
{
  enum { 
    nProcsIO    = 3,
    //IOMode      = PIO_NONE,
    //IOMode      = PIO_MPI, 
    //IOMode      = PIO_FPGUARD,
    IOMode      = PIO_ASYNCH,
    //IOMode      = PIO_POSIX,
    nNamespaces = 1 };
  
  static int hasLocalFile[nNamespaces] = { 0 };
  
#ifdef USE_MPI
  MPI_Comm commGlob, commModel; 
  int sizeGlob;
  int rankDebug;

  xmpi ( MPI_Init ( &argc, &argv));           
  xmpi ( MPI_Comm_dup ( MPI_COMM_WORLD, &commGlob ));
  xmpi ( MPI_Comm_set_errhandler ( commGlob, MPI_ERRORS_RETURN ));
  xmpi ( MPI_Comm_size ( commGlob, &sizeGlob )); 

  commModel = pioInit_c ( commGlob, nProcsIO, IOMode, nNamespaces, hasLocalFile );
#endif

  modelRun ();

#ifdef USE_MPI
  pioFinalize ();
  MPI_Finalize ();                         
#endif
  return 0;
}
