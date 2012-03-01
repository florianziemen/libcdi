#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifdef USE_MPI
#include <mpi.h>
#include "pio_c_temp.h"
#endif

#include "cdi.h"
#include "pio_util.h"

void hoursPassingHack ( int * vdate, int * vtime, int hoursPassed )
{
  int sum, days, hours, oldDays;
  
  xassert ( hoursPassed % 10000 == 0 );
  sum = * vtime + hoursPassed;
  days = sum / 240000;
  hours = sum % 240000;
  oldDays = * vdate % 100;
  if ( oldDays + days > 28 ) xabort ( "UNEXPECTED USE" );
  * vtime = hours;
  * vdate = * vdate + days;
}
 
void modelRun ()
{
  enum {
    filetype    = FILETYPE_GRB,
    nStreams    = 5,
    MAXNSTREAMS = 25,
    ntfiles     = 2,
    ntsteps     = 3,
    nVars       = 5,
    nlon        = 12,
    nlat        = 6,
    MAXNLEV     = 5 };

  static int nlev[nStreams][nVars] =
    {{1,1,5,5,2},{3,5,2,2,1},{3,5,2,2,1},{5,2,2,2,1}, {3,3,3,3,3}};

  static char nameExp[7]           = "example";
  static int asciiA                = 65;
  char nameStream[8];
  char filename[1024];

  int gridID, zaxisID[nStreams][nVars], taxisID;
  int streamID[nStreams], vlistID[nStreams], varID[nStreams][nVars], tsID, tfID = 0;
  int i, j, k, nmiss = 0, rank;
  double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
  double lats[nlat] = {-75, -45, -15, 15, 45, 75};
  double levs[MAXNLEV] = {101300, 92500, 85000, 50000, 20000};
  double var[nlon*nlat*MAXNLEV];
  int vdate = 19850101, vtime = 120000, hourStep = 20000;
  int start = CDI_UNDEFID, chunk = CDI_UNDEFID, stop = CDI_UNDEFID;

  xassert ( nStreams < MAXNSTREAMS );

  gridID = gridCreate ( GRID_LONLAT, nlon*nlat );
  gridDefXsize ( gridID, nlon );
  gridDefYsize ( gridID, nlat );
  gridDefXvals ( gridID, lons );
  gridDefYvals ( gridID, lats );

  for ( i = 0; i < nStreams; i++ )
    {
      for ( j = 0; j < nVars; j++ )
        {
          xassert ( nlev[i][j] > 0 && nlev[i][j] <= MAXNLEV );
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
      sprintf ( &filename[0], "%s%c_%d.grb", nameExp, asciiA + i, tfID );
      streamID[i] = streamOpenWrite ( filename, filetype );
      xassert ( streamID[i] >= 0 );
      streamDefVlist ( streamID[i], vlistID[i]);
    }

  pioEndDef ();

  for ( tfID = 0; tfID < ntfiles; tfID++ )
    {
      if ( tfID > 0 )
	{
          for ( i = 0; i < nStreams; i++ )
            {
              streamClose ( streamID[i] );
              sprintf ( &filename[0], "%s%c_%d.grb", nameExp, asciiA + i, tfID );
              streamID[i] = streamOpenWrite ( filename, filetype );
              xassert ( streamID[i] >= 0 );
              streamDefVlist ( streamID[i], vlistID[i] );
            }
	}

      for ( tsID = 0; tsID < ntsteps; tsID++ )
	{
          hoursPassingHack ( &vdate, &vtime, hourStep );
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
    nProcsIODef    = 3,
    //IOModeDef       = PIO_NONE,
    //IOModeDef       = PIO_MPI,
    IOModeDef       = PIO_FPGUARD,
    //IOModeDef       = PIO_ASYNCH,
    //IOModeDef       = PIO_WRITER,
    nNamespaces = 1 };

  static int hasLocalFile[nNamespaces] = { 0 };

#ifdef USE_MPI
  MPI_Comm commGlob, commModel;
  int sizeGlob;
  int rankGlob; 
  int IOMode, nProcsIO, count;

  xmpi ( MPI_Init ( &argc, &argv));
  xmpi ( MPI_Comm_dup ( MPI_COMM_WORLD, &commGlob ));
  xmpi ( MPI_Comm_set_errhandler ( commGlob, MPI_ERRORS_RETURN ));
  xmpi ( MPI_Comm_size ( commGlob, &sizeGlob ));
  xmpi ( MPI_Comm_rank ( commGlob, &rankGlob ));

  if (argc > 1)
    {    
      xassert ( argc >= 3 );
      IOMode = atoi(argv[1]);
      nProcsIO = atoi(argv[2]);
      xassert ( IOMode >= PIO_MINIOMODE && IOMode <= PIO_MAXIOMODE &&
                nProcsIO >= 0 && nProcsIO <= sizeGlob );
      printf ( "IOMode=%d, nProcsIO=%d", IOMode, nProcsIO );
    }
  else
    {
      IOMode = IOModeDef;
      nProcsIO = nProcsIODef;
    }

  commModel = pioInit_c ( commGlob, nProcsIO, IOMode, nNamespaces, hasLocalFile );
#endif

  modelRun ();

#ifdef USE_MPI
  pioFinalize ();
  MPI_Finalize ();
#endif
  return 0;
}
