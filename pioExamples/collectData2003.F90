#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
PROGRAM COLLECTDATA2003
  
  USE iso_c_binding
  
  IMPLICIT NONE
  INCLUDE 'cdi.inc'
  
#ifdef USE_MPI
  INCLUDE 'mpif.h'
#endif

  ! For parallel IO:
  !  Parameter and variables needed.
  INTEGER nProcsIO, IOMode, nNamespaces
  PARAMETER ( nProcsIO = 4 )
  !PARAMETER ( IOMode   = PIO_NONE )                 
  !PARAMETER ( IOMode   = PIO_MPI_NONB )             
  PARAMETER ( IOMode   = PIO_POSIX_NONB )           
  !PARAMETER ( IOMode   = PIO_POSIX_ASYNCH )         
  !PARAMETER ( IOMode   = PIO_POSIX_FPGUARD_SENDRECV )

  PARAMETER ( nNamespaces = 1 )
  INTEGER, PARAMETER :: hasLocalFile ( nNamespaces ) = (/ 0 /) 
 
  INTEGER commGlob, commModel, error

  ! Start parallel environment
#ifdef USE_MPI
  CALL MPI_INIT ( error )
  CALL MPI_COMM_DUP ( MPI_COMM_WORLD, commGlob, error )
#endif

  ! For parallel IO:
  ! Initialize environment.
  commModel = pioInit ( commGlob, nProcsIO, IOMode, nNamespaces, hasLocalFile )

  CALL MODELRUN ( commModel )

  ! For parallel IO: 
  ! Cleanup environment.
  CALL pioFinalize ()
  
#ifdef USE_MPI
  CALL MPI_FINALIZE ( error )
#endif
  
CONTAINS
  
  SUBROUTINE MODELRUN ( commModel )
    
    IMPLICIT NONE
    
    INTEGER, INTENT ( IN ) :: commModel

    INTEGER nlon, nlat, nlev, nts, vdate, vtime, filetype
    PARAMETER ( nlon = 12 )   ! Number of longitudes
    PARAMETER ( nlat =  6 )   ! Number of latitudes
    PARAMETER ( nlev =  5 )   ! Number of levels
    PARAMETER ( nts  =  3 )   ! Number of time steps
    PARAMETER ( vdate = 19850101 )
    PARAMETER ( vtime = 120000 )
    PARAMETER ( filetype = FILETYPE_GRB )
    
    INTEGER gridID, zaxisID1, zaxisID2, taxisID
    INTEGER vlistID, varID1, varID2, streamID, tsID
    INTEGER i, nmiss, status
    DOUBLE PRECISION lons ( nlon ), lats ( nlat ), levs ( nlev )
    DOUBLE PRECISION var1 ( nlon * nlat ), var2 ( nlon * nlat * nlev )
    CHARACTER(len=256) :: varname
    INTEGER startID, stopID, chunk
    
    DATA lons /0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330/
    DATA lats /-75, -45, -15, 15, 45, 75/
    DATA levs /101300, 92500, 85000, 50000, 20000/
    
    nmiss = 0
    
    !     Create a regular lon/lat grid
    gridID = gridCreate(GRID_LONLAT, nlon*nlat)
    CALL gridDefXsize(gridID, nlon)
    CALL gridDefYsize(gridID, nlat)
    CALL gridDefXvals(gridID, lons)
    CALL gridDefYvals(gridID, lats)
    
    !     Create a surface level Z-axis
    zaxisID1 = zaxisCreate(ZAXIS_SURFACE, 1)
    
    !     Create a pressure level Z-axis
    zaxisID2 = zaxisCreate(ZAXIS_PRESSURE, nlev)
    CALL zaxisDefLevels(zaxisID2, levs)
    
    !     Create a variable list
    vlistID = vlistCreate()
    
    !     Define the variables
    varID1 = vlistDefVar(vlistID, gridID, zaxisID1, TIME_VARIABLE)
    varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARIABLE)
    
    !     Define the variable names
    varname = "varname1"
    CALL vlistDefVarName(vlistID, varID1, TRIM(varname)//C_NULL_CHAR)
    CALL vlistDefVarName(vlistID, varID2, C_CHAR_"varname2"//C_NULL_CHAR)
    
    !     Create a Time axis
    taxisID = taxisCreate(TAXIS_ABSOLUTE)
    
    !     Assign the Time axis to the variable list
    CALL vlistDefTaxis(vlistID, taxisID)
    
    streamID = streamOpenWrite(C_CHAR_"example.grb"//C_NULL_CHAR, filetype)
    IF ( streamID < 0 ) THEN
      WRITE(0,*) cdiStringError(streamID)
      STOP
    END IF
    
    !     Assign the variable list to the dataset
    CALL streamDefVlist(streamID, vlistID)

    !     For parallel IO: 
    !     End SUBROUTINE MODELRUNpart for CDI resources: 
    !          Map variables on IO server,
    !          Decompose data on model processes for IO
    !          Transmit resources to the IO server.
    CALL pioEndDef (); 
    
    !     Loop over the number of time steps
    DO tsID = 0, nts-1
      CALL taxisDefVdate ( taxisID, vdate + tsID )
      CALL taxisDefVtime ( taxisID, vtime )
      status = streamDefTimestep ( streamID, tsID )
       
      ! For parallel IO:    
      ! Inquire start index and chunk for IO transposition of var1
      startID = pioInqVarDecoOff   ( vlistID, varID1 ) + 1
      chunk   = pioInqVarDecoChunk ( vlistID, varID1 ) - 1
      stopID = startID + chunk 
      ! Init decomposed data for var1
      DO i = startID, stopID
        var1(i) = 1.1
      END DO
      ! Write var1
      CALL streamWriteVar(streamID, varID1, var1 ( startID ), nmiss)

      ! For parallel IO: 
      ! Inquire start index and chunk for IO transposition of var2
      startID = pioInqVarDecoOff ( vlistID, varID2 ) + 1
      chunk = pioInqVarDecoChunk ( vlistID, varID2 ) - 1
      stopID = startID + chunk
      ! Init decomposed data for var2
      DO i = startID, stopID
        var2(i) = 2.2
      END DO
      ! Write var2
      CALL streamWriteVar(streamID, varID2, var2 ( startID ), nmiss)

      ! For parallel IO: 
      ! Start transmission of all data for output in this timestep to IO server.
      CALL pioWriteTimestep ( tsID, vdate, vtime )
    END DO
    
    !     Close the output stream
    CALL streamClose(streamID)
    
    !     Destroy the objects
    CALL vlistDestroy(vlistID)
    CALL taxisDestroy(taxisID)
    CALL zaxisDestroy(zaxisID1)
    CALL zaxisDestroy(zaxisID2)
    CALL gridDestroy(gridID)
    
  END SUBROUTINE MODELRUN
END PROGRAM COLLECTDATA2003

/************************************************************************/
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
