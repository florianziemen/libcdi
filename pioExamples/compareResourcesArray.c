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
extern void   arrayDestroy          ( void );

#define DOUBLE_PRECISION  8


#define nlon 12     // Number of longitudes
#define nlat  6     // Number of latitudes 
#define nlev  5     // Number of levels    
#define ntsteps   3 // Number of time steps

static int hasLocalFile[] = { 0, 1 };

double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
double lats[nlat] = {-75, -45, -15, 15, 45, 75};
double levs[nlev] = {101300, 92500, 85000, 50000, 20000};

int defineGrid ()
{
  int gridID = CDI_UNDEFID;
  int mask_vec[nlon*nlat];
  const int * mp = &mask_vec[0];
  double area_vec[nlon*nlat];
  const double * ap = &area_vec[0];
  int i;
  
  gridID = gridCreate(GRID_LONLAT, nlon*nlat);
  gridDefXsize(gridID, nlon);
  gridDefYsize(gridID, nlat);
  gridDefXvals(gridID, lons);
  gridDefYvals(gridID, lats);
  gridDefNvertex ( gridID, 1 );
  gridDefXbounds ( gridID, lons );
  gridDefYbounds ( gridID, lats );
  for ( i = 0; i < nlon*nlat; i++ )    
    mask_vec[i] = i % 2 ;
  gridDefMaskGME ( gridID, mp );
  for ( i = 0; i < nlon*nlat; i++ )    
    mask_vec[i] = 1;
  gridDefMask ( gridID, mp );
  gridDefXname ( gridID, "myXname" );
  gridDefXlongname ( gridID, "myXlongname" );
  gridDefXunits ( gridID, "myXunits" );
  gridDefYname ( gridID, "myYname" );
  gridDefYlongname ( gridID, "myYlongname" );
  gridDefYunits ( gridID, "myYunits" );
  gridDefPrec ( gridID, DOUBLE_PRECISION );
  gridDefXpole ( gridID, 90.0 );
  gridDefYpole ( gridID, 180.0 );
  gridDefAngle ( gridID, 360.0 );
  gridDefTrunc ( gridID, 1 );
  gridDefGMEnd ( gridID, 2 );
  gridDefGMEni ( gridID, 3 );
  gridDefGMEni2 ( gridID, 4 );
  gridDefGMEni3 ( gridID, 5 );
  gridDefNumber ( gridID, 6 );
  gridDefPosition ( gridID, 7 );
  gridDefReference ( gridID, "myReference" );
/* gridDefLCC ( gridID, double originLon, double originLat,  */
/* 	   double lonParY, double lat1, double lat2, double xinc, double yinc, int projflag, int scanflag); */
/* gridDefLcc2 ( gridID, double earth_radius, double lon_0,  */
/* 	    double lat_0, double lat_1,double lat_2);*/
/* gridDefLaea ( gridID, double earth_radius, double lon_0, double lat_0); */
  for ( i = 0; i < nlon*nlat; i++ )    
    area_vec[i] = 0.1 * i;
  gridDefArea ( gridID, ap );
  for ( i = 0; i < nlon*nlat; i++ )    
    mask_vec[i] = i;
  gridDefRowlon ( gridID, nlon*nlat, mp );
  gridDefComplexPacking ( gridID, 1 );	       

  return gridID;
}

int defineZaxis ()
{
  int zaxisID = CDI_UNDEFID;
  double vct[3] = { 3.0, 3.3, 3.6 };

  zaxisID = zaxisCreate(ZAXIS_PRESSURE, nlev);
  zaxisDefLevels(zaxisID, levs);
  zaxisDefLevel ( zaxisID, 2, 8507.3 );
  zaxisDefName ( zaxisID, "myName" );
  zaxisDefLongname ( zaxisID, "myLongname" );
  zaxisDefUnits ( zaxisID, "myUnits" );
  zaxisDefPrec ( zaxisID, DOUBLE_PRECISION );
  zaxisDefLtype ( zaxisID, 1 );
  zaxisDefVct ( zaxisID, 3, vct );
  zaxisDefLbounds ( zaxisID, &levs[0] );
  zaxisDefUbounds ( zaxisID, &levs[0] );
  zaxisDefWeights ( zaxisID, &levs[0] );

  return zaxisID;
}

int defineTaxis ()
{
  int taxisID = CDI_UNDEFID;

  taxisID = taxisCreate(TAXIS_ABSOLUTE);
  
  taxisDefType  ( taxisID, 0 );
  taxisDefVdate ( taxisID, 1 );
  taxisDefVtime ( taxisID, 2 );
  taxisDefRdate ( taxisID, 3 );
  taxisDefRtime ( taxisID, 4 );
  taxisDefVdateBounds ( taxisID, 5, 6 );
  taxisDefVtimeBounds ( taxisID, 7, 8 );
  taxisDefCalendar ( taxisID, 1 );
  taxisDefTunit ( taxisID, 1 );
  taxisDefNumavg ( taxisID, 1 );

  return taxisID;
}

int defineStream ()
{
  int streamID = CDI_UNDEFID;

  return streamID;
}

int defineVlist ( int gridID, int zaxisID, int taxisID )
{
  int vlistID = CDI_UNDEFID;
  int zaxisID2 = zaxisCreate(ZAXIS_SURFACE, 1);
  int varID1, varID2;

  vlistID = vlistCreate();
  varID1 = vlistDefVar(vlistID, gridID, zaxisID, TIME_VARIABLE);
  varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARIABLE);
  vlistDefVarName(vlistID, varID1, "varname1");
  {
    int globfac[] = { 23, 42 };
    vlistDefAttInt(vlistID, varID1, "seer's globule factors", DATATYPE_INT16,
                   2, globfac);
  }
  vlistDefVarName(vlistID, varID2, "varname2");
  vlistDefAttTxt(vlistID, varID2, "txt demo", 6, "banana");
  vlistDefTaxis(vlistID, taxisID);

  return vlistID;
}

int defineInstitute ()
{
  int instID = CDI_UNDEFID;

  instID = institutDef( 0, 0,"MYINSTITUTE", "myInstitute");

  return instID;
}

int defineModel ( int instID )
{
  int modelID = CDI_UNDEFID;

  modelID = modelDef ( instID, 0, "myModel");

  return modelID;
}

void modelRun ()
{
  int gridID, zaxisID, taxisID, instID, modelID, vlistID, streamID;

  char * recvBuffer, * sendBuffer;
  int bufferSize, differ;
  MPI_Status status;

  namespaceInit ( 2, hasLocalFile );

  namespaceSetActive ( 0 );

  gridID  = defineGrid      ();
  zaxisID = defineZaxis     ();
  taxisID = defineTaxis     ();
  instID  = defineInstitute ();
  modelID = defineModel     ( instID );
  vlistID = defineVlist     ( gridID, zaxisID, taxisID);
  
  streamID = streamOpenWrite("example.grb", FILETYPE_GRB);
  if ( streamID < 0 ) xabort ( "Could not open file" );

  streamDefVlist(streamID, vlistID);

  reshPackBufferCreate ( &sendBuffer, &bufferSize, MPI_COMM_WORLD );
  xmpi ( MPI_Send ( sendBuffer, bufferSize, MPI_PACKED, 0, 0, MPI_COMM_WORLD ));  
  recvBuffer = xmalloc ( bufferSize * sizeof ( char ));
  xmpi ( MPI_Recv ( recvBuffer, bufferSize, MPI_PACKED, 0,
		    0, MPI_COMM_WORLD, &status ));
 
  namespaceSetActive ( 1 );
  rpcUnpackResources ( recvBuffer, bufferSize, MPI_COMM_WORLD );  
  free ( recvBuffer );  
  reshPackBufferDestroy ( &sendBuffer );

  differ = reshArrayCompare ( 0, 1 );
  printf ( "The resource arrays %s.\n", differ ? "differ" : "are equal" );
  reshArrayPrint ( "reshArrayModel" );

  namespaceSetActive ( 0 );
  streamClose(streamID);
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
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
