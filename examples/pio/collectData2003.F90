#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

PROGRAM collectdata2003

  IMPLICIT NONE

  INCLUDE 'cdi.inc'

#ifdef USE_MPI
  INCLUDE 'mpif.h'
#endif

  ! For parallel IO:
  !  Parameter and variables needed.
  INTEGER, PARAMETER :: nProcsIO     = 3
  ! INTEGER, PARAMETER :: IOMode       = PIO_NONE                  
  ! INTEGER, PARAMETER :: IOMode       = PIO_MPI              
  ! INTEGER, PARAMETER :: IOMode       = PIO_WRITER            
  ! INTEGER, PARAMETER :: IOMode       = PIO_ASYNCH          
  INTEGER, PARAMETER :: IOMode       = PIO_FPGUARD 
  INTEGER, PARAMETER :: nNamespaces  = 1 
  INTEGER, PARAMETER :: hasLocalFile(nNamespaces) = (/ 0 /)  

  INTEGER ::commGlob, commModel, error

  ! Start parallel environment
#ifdef USE_MPI
  CALL MPI_INIT ( error )
  CALL MPI_COMM_DUP ( MPI_COMM_WORLD, commGlob, error )

  ! For parallel IO:
  ! Initialize environment.
  commModel = pioInit ( commGlob, nProcsIO, IOMode, nNamespaces, hasLocalFile )
#endif

  CALL modelrun ( commModel )

#ifdef USE_MPI
  ! For parallel IO:
  ! Cleanup environment.
  CALL pioFinalize ()

  CALL MPI_FINALIZE ( error )
#endif

CONTAINS

  SUBROUTINE modelrun ( commModel )

    INTEGER, INTENT(in) :: commModel

    INTEGER, PARAMETER :: nlon     = 12   ! Number of longitudes
    INTEGER, PARAMETER :: nlat     =  6   ! Number of latitudes
    INTEGER, PARAMETER :: nlev     =  5   ! Number of levels
    INTEGER, PARAMETER :: nts      =  3   ! Number of time steps
    INTEGER, PARAMETER :: vdate    = 19850101
    INTEGER, PARAMETER :: vtime    = 120000
    INTEGER, PARAMETER :: filetype = FILETYPE_GRB

    INTEGER :: gridID, zaxisID1, zaxisID2, taxisID
    INTEGER :: vlistID, varID1, varID2, streamID, tsID
    INTEGER :: i, nmiss, status
    DOUBLE PRECISION :: lons ( nlon ), lats ( nlat ), levs ( nlev )
    DOUBLE PRECISION :: var1 ( nlon * nlat ), var2 ( nlon * nlat * nlev )
    CHARACTER(len=256) :: varname
    INTEGER :: startID, stopID, chunk

    lons = (/0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330/)
    lats = (/-75, -45, -15, 15, 45, 75/)
    levs = (/101300, 92500, 85000, 50000, 20000/)

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
    varname = 'varname1'
    CALL vlistDefVarName(vlistID, varID1, varname)
    varname = 'varname2'
    CALL vlistDefVarName(vlistID, varID2, varname)

    !     Create a Time axis
    taxisID = taxisCreate(TAXIS_ABSOLUTE)

    !     Assign the Time axis to the variable list
    CALL vlistDefTaxis(vlistID, taxisID)

    streamID = streamOpenWrite('example.grb', filetype)
    IF ( streamID < 0 ) THEN
      WRITE(0,*) cdiStringError(streamID)
      STOP
    END IF

    !     Assign the variable list to the dataset
    CALL streamDefVlist(streamID, vlistID)

#ifdef USE_MPI
    !     For parallel IO:
    !          End definition stage for CDI resources,
    !          balance load of variables among collecting IO server,
    !          Decompose data on model processes for IO
    !          Transfer resources to the collecting IO server.
    CALL pioEndDef ();
#endif
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

#ifdef USE_MPI
      ! For parallel IO:
      ! Start transmission of all data for output in this timestep to IO server.
      CALL pioWriteTimestep ( tsID, vdate, vtime )
#endif
    END DO

#ifdef USE_MPI
    ! For parallel IO:
    ! Preparation for local cleanup
    CALL pioEndTimestepping ()
#endif

    !     Close the output stream
    CALL streamClose(streamID)

    !     Destroy the objects
    CALL vlistDestroy(vlistID)
    CALL taxisDestroy(taxisID)
    CALL zaxisDestroy(zaxisID1)
    CALL zaxisDestroy(zaxisID2)
    CALL gridDestroy(gridID)

  END SUBROUTINE modelrun

END PROGRAM collectdata2003
