      PROGRAM CDIREAD

      IMPLICIT NONE

      INCLUDE 'cdi.inc'

      INTEGER NLON, NLAT, NLEV, NTIME
      PARAMETER (NLON  = 12)   ! Number of longitudes
      PARAMETER (NLAT  =  6)   ! Number of latitudes
      PARAMETER (NLEV  =  5)   ! Number of levels
      PARAMETER (NTIME =  3)   ! Number of time steps

      INTEGER gridID, zaxisID1, zaxisID2, taxisID
      INTEGER vlistID, varID1, varID2, streamID, tsID
      INTEGER nmiss, status, vdate, vtime
      REAL*8 var1(NLON*NLAT), var2(NLON*NLAT*NLEV)

!     Open the dataset
      streamID = streamOpenRead("example.nc")
      IF ( streamID < 0 ) THEN
         WRITE(0,*) cdiStringError(streamID)
         STOP
      END IF

!     Get the variable list of the dataset
      vlistID = streamInqVlist(streamID)

!     Set the variable IDs
      varID1 = 0
      varID2 = 1

!     Get the Time axis from the variable list
      taxisID = vlistInqTaxis(vlistID)

!     Loop over the number of time steps
      DO tsID = 0, NTIME-1
!        Inquire the time step
         status = streamInqTimestep(streamID, tsID)

!        Get the verification date and time
         vdate = taxisInqVdate(taxisID)
         vtime = taxisInqVtime(taxisID)

!        Read var1 and var2
         CALL streamReadVar(streamID, varID1, var1, nmiss)
         CALL streamReadVar(streamID, varID2, var2, nmiss)
      END DO

!     Close the input stream
      CALL streamClose(streamID)

      END
