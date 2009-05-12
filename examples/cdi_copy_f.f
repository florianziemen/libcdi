      PROGRAM CDICOPY

      IMPLICIT NONE

      INCLUDE 'cdi.inc'

      INTEGER NLON, NLAT, NLEV, NTIME
      PARAMETER (NLON  = 12)   ! Number of longitudes
      PARAMETER (NLAT  =  6)   ! Number of latitudes
      PARAMETER (NLEV  =  5)   ! Number of levels
      PARAMETER (NTIME =  3)   ! Number of time steps

      INTEGER gridID, zaxisID1, zaxisID2, taxisID, tsID
      INTEGER vlistID1, vlistID2, varID1, varID2, streamID1, streamID2
      INTEGER i, nmiss, status, vdate, vtime
      REAL*8 var1(NLON*NLAT), var2(NLON*NLAT*NLEV)

!     Open the input dataset
      streamID1 = streamOpenRead("example.nc")
      IF ( streamID1 < 0 ) THEN
         WRITE(0,*) cdiStringError(streamID1)
         STOP
      END IF

!     Get the variable list of the dataset
      vlistID1 = streamInqVlist(streamID1)

!     Set the variable IDs
      varID1 = 0
      varID2 = 1

!     Get the Time axis from the variable list
      taxisID = vlistInqTaxis(vlistID1)

!     Open the output dataset (GRIB fromat)
      streamID2 = streamOpenWrite("example.grb", FILETYPE_GRB)
      IF ( streamID2 < 0 ) THEN
         WRITE(0,*) cdiStringError(streamID2)
         STOP
      END IF

      vlistID2 = vlistDuplicate(vlistID1)

      CALL streamDefVlist(streamID2, vlistID2)

!     Loop over the number of time steps
      DO tsID = 0, NTIME-1
!        Inquire the input time step */
         status = streamInqTimestep(streamID1, tsID)

!        Get the verification date and time
         vdate = taxisInqVdate(taxisID)
         vtime = taxisInqVtime(taxisID)

!        Define the output time step
         status = streamDefTimestep(streamID2, tsID)

!        Read var1 and var2
         CALL streamReadVar(streamID1, varID1, var1, nmiss)
         CALL streamReadVar(streamID1, varID2, var2, nmiss)

!        Write var1 and var2
         CALL streamWriteVar(streamID2, varID1, var1, nmiss)
         CALL streamWriteVar(streamID2, varID2, var2, nmiss)
      END DO

!     Close the streams
      CALL streamClose(streamID1)
      CALL streamClose(streamID2)

      END
