#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#include "pio_c_temp.h"
#endif

#include "cdi.h"
#include "pio_util.h"

void timePassingHack ( int * vdate, int * vtime, int timePassed )
{
  int sum, days, hours, oldDays;

  sum = * vtime + timePassed;
  days = sum / 240000;
  hours = sum % 240000;
  oldDays = * vdate % 100;
  if ( oldDays + days > 28 ) xabort ( "UNEXPECTED USE" );
  * vtime = hours;
  * vdate = * vdate + days;
  xdebug ( "vdate=%d, vtime=%d", * vdate, * vtime );
}
 
void modelRun ()
{
  enum {
    filetype    = FILETYPE_GRB,
    nStreams    = 2,
    ntfiles     = 2,
    ntsteps     = 3,
    nVars       = 5,
    nlon        = 12,
    nlat        = 6,
    maxlev      = 5 };

  static int nlev[nStreams][nVars] = {{1,1,5,5,2},{3,5,2,2,1}};
  static char nameExp[7]           = "example";
  static char postStream[nStreams][2] = {"A\0","B\0" };
  char nameStream[8];

  int gridID, zaxisID[nStreams][nVars], taxisID;
  int streamID[nStreams], vlistID[nStreams], varID[nStreams][nVars], tsID, tfID = 0;
  int i, j, k, nmiss = 0, rank;
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

  for ( i = 0; i < nStreams; i++ )
    {
      for ( j = 0; j < nVars; j++ )
        {
          zaxisID[i][j] = zaxisCreate ( ZAXIS_PRESSURE, nlev[i][j] );
          zaxisDefLevels ( zaxisID[i][j], levs );
        }
      vlistID[i] = vlistCreate ();
    }

  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < nVars; j++ )
      varID[i][j] = vlistDefVar ( vlistID[i], gridID, zaxisID[i][j], TIME_VARIABLE );

  taxisID = taxisCreate ( TAXIS_ABSOLUTE );
  for ( i = 0; i < nStreams; i++ )
    vlistDefTaxis ( vlistID[i], taxisID );

  for ( i = 0; i < nStreams; i++ )
    {
      memset ( filename, 0, 1024 );
      sprintf ( &filename[0], "%s%s_%d.grb", nameExp, postStream[i], tfID );
      streamID[i] = streamOpenWrite ( filename, filetype );
      xassert ( streamID[i] >= 0 );
      streamDefVlist ( streamID[i], vlistID[i]);
    }

  pioEndDef ();

  for ( tfID = 0; tfID < ntfiles; tfID++ )
    {
      xdebug ( "tfID=%d, ntfiles=%d", tfID, ntfiles );
      if ( tfID > 0 )
	{
          for ( i = 0; i < nStreams; i++ )
            {
              streamClose ( streamID[i] );
              sprintf ( &filename[0], "%s%s_%d.grb", nameExp, postStream[i], tfID );
              streamID[i] = streamOpenWrite ( filename, filetype );
              xassert ( streamID[i] >= 0 );
              streamDefVlist ( streamID[i], vlistID[i] );
            }
	}
      xdebug ();
      for ( tsID = 0; tsID < ntsteps; tsID++ )
	{
          timePassingHack ( &vdate, &vtime, 20000 );
	  taxisDefVdate ( taxisID, vdate );
	  taxisDefVtime ( taxisID, vtime );
          for ( i = 0; i < nStreams; i++ )
            {
              streamDefTimestep ( streamID[i], tsID );
              for ( j = 0; j < nVars; j++ )
                {
                  start = pioInqVarDecoOff   ( vlistID[i], varID[i][j] );
                  chunk = pioInqVarDecoChunk ( vlistID[i], varID[i][j] );
                  stop = start + chunk;
                  for ( k = start; k < stop; k++ ) var[k] = 2.2;
                  streamWriteVar ( streamID[i], varID[i][j], &var[start], nmiss );
                  start = CDI_UNDEFID;
                  chunk = CDI_UNDEFID;
                }
            }
	  pioWriteTimestep ( tsID, vdate, vtime );
	}
      xdebug ();
    }

  pioEndTimestepping ();

  for ( i = 0; i < nStreams; i++ )
    {
      streamClose ( streamID[i] );
      vlistDestroy ( vlistID[i] );
    }
  taxisDestroy ( taxisID );
  for ( i = 0; i < nStreams; i++ )
    for ( j = 0; j < nVars; j++ )
      zaxisDestroy ( zaxisID[i][j] );
  gridDestroy ( gridID );
  xdebug ( "RETURN" );
}


int main (int argc, char *argv[])
{
  enum {
    nProcsIO    = 3,
    //IOMode      = PIO_NONE,
    //IOMode      = PIO_MPI,
    IOMode      = PIO_FPGUARD,
    //IOMode      = PIO_ASYNCH,
    //IOMode      = PIO_WRITER,
    nNamespaces = 1 };

  static int hasLocalFile[nNamespaces] = { 0 };

#ifdef USE_MPI
  MPI_Comm commGlob, commModel;
  int sizeGlob;
  int rankGlob;

  xmpi ( MPI_Init ( &argc, &argv));
  xmpi ( MPI_Comm_dup ( MPI_COMM_WORLD, &commGlob ));
  xmpi ( MPI_Comm_set_errhandler ( commGlob, MPI_ERRORS_RETURN ));
  xmpi ( MPI_Comm_size ( commGlob, &sizeGlob ));
  xmpi ( MPI_Comm_rank ( commGlob, &rankGlob ));

  commModel = pioInit_c ( commGlob, nProcsIO, IOMode, nNamespaces, hasLocalFile );
#endif

  modelRun ();

#ifdef USE_MPI
  pioFinalize ();
  MPI_Finalize ();
#endif
  return 0;
}
