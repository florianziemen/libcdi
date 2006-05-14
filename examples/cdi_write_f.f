      PROGRAM CDIWRITE

      IMPLICIT NONE

      INTEGER NLON, NLAT, NLEV, NTIME
      PARAMETER (NLON  = 12)   ! Number of longitudes
      PARAMETER (NLAT  =  6)   ! Number of latitudes
      PARAMETER (NLEV  =  5)   ! Number of levels
      PARAMETER (NTIME =  3)   ! Number of time steps

      INTEGER gridID, zaxisID1, zaxisID2, taxisID
      INTEGER vlistID, varID1, varID2, streamID, tsID
      INTEGER i, nmiss, status
      REAL*8 lons(NLON), lats(NLAT), levs(NLEV)
      REAL*8 var1(NLON*NLAT), var2(NLON*NLAT*NLEV)

      DATA lons /0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330/
      DATA lats /-75, -45, -15, 15, 45, 75/
      DATA levs /101300, 92500, 85000, 50000, 20000/

      INCLUDE 'cdi.inc'

      nmiss = 0

!     Create a regular lon/lat grid
      gridID  = gridCreate(GRID_LONLAT, NLON*NLAT)
      CALL gridDefXsize(gridID, NLON)
      CALL gridDefYsize(gridID, NLAT)
      CALL gridDefXvals(gridID, lons)
      CALL gridDefYvals(gridID, lats)

!     Create a surface level Z-axis
      zaxisID1 = zaxisCreate(ZAXIS_SURFACE, 1)

!     Create a pressure level Z-axis
      zaxisID2 = zaxisCreate(ZAXIS_PRESSURE, NLEV)
      CALL zaxisDefLevels(zaxisID2, levs)
 
!     Create a variable list
      vlistID = vlistCreate()

!     Define the variables
      varID1 = vlistDefVar(vlistID, gridID, zaxisID1, TIME_VARIABLE)
      varID2 = vlistDefVar(vlistID, gridID, zaxisID2, TIME_VARIABLE)

!     Define the variable names
      CALL vlistDefVarName(vlistID, varID1, "varname1")
      CALL vlistDefVarName(vlistID, varID2, "varname2")

!     Create a Time axis
      taxisID = taxisCreate(TAXIS_ABSOLUTE)

!     Assign the Time axis to the variable list
      CALL vlistDefTaxis(vlistID, taxisID)

!     Create a dataset in netCDF format
      streamID  = streamOpenWrite("example.nc", FILETYPE_NC)

!     Assign the variable list to the dataset
      CALL streamDefVlist(streamID, vlistID)

!     Loop over the number of time steps
      DO tsID = 0, NTIME-1
!        Set the verification date to 1985-01-01 + tsID
         CALL taxisDefVdate(taxisID, 19850101+tsID)
!        Set the verification time to 12:00
         CALL taxisDefVtime(taxisID, 1200)
!        Define the time step
         status = streamDefTimestep(streamID, tsID)

!        Init var1 and var2
         DO i = 1, NLON*NLAT
            var1(i) = 1
         END DO
         DO i = 1, NLON*NLAT*NLEV
            var2(i) = 2
         END DO

!        Write var1 and var2
         CALL streamWriteVar(streamID, varID1, var1, nmiss)
         CALL streamWriteVar(streamID, varID2, var2, nmiss)
      END DO

!     Close the output stream
      CALL streamClose(streamID)

!     Destroy the objects
      CALL vlistDestroy(vlistID)
      CALL taxisDestroy(taxisID)
      CALL zaxisDestroy(zaxisID1)
      CALL zaxisDestroy(zaxisID2)
      CALL gridDestroy(gridID)

      END
