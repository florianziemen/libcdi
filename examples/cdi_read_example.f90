PROGRAM CDIREAD

  IMPLICIT NONE

  INCLUDE 'cdi.inc'

  INTEGER :: gridsize, nlevel, nvars, code
  INTEGER :: i, varID, levelID
  REAL*8, ALLOCATABLE :: field(:,:)
  CHARACTER(len=256) :: name, longname, units

  INTEGER NLON, NLAT, NLEV, NTIME
  PARAMETER (NLON  = 12)   ! Number of longitudes
  PARAMETER (NLAT  =  6)   ! Number of latitudes
  PARAMETER (NLEV  =  5)   ! Number of levels
  PARAMETER (NTIME =  3)   ! Number of time steps

  INTEGER gridID, zaxisID1, zaxisID2, taxisID
  INTEGER vlistID, varID1, varID2, streamID, tsID
  INTEGER nmiss, status, vdate, vtime
  REAL*8 var1(NLON*NLAT), var2(NLON*NLAT*NLEV)

! Open the dataset
  streamID = streamOpenRead("example.nc")
  IF ( streamID < 0 ) THEN
    WRITE(0,*) cdiStringError(streamID)
    STOP
  END IF

! Get the variable list of the dataset
  vlistID = streamInqVlist(streamID)

  nvars = vlistNvars(vlistID)

  WRITE(*,*) 'nvars: ', nvars

  DO varID = 0, nvars-1
    code = vlistInqVarCode(vlistID, varID)
    CALL vlistInqVarName(vlistID, varID, name)
    CALL vlistInqVarLongname(vlistID, varID, longname)
    CALL vlistInqVarUnits(vlistID, varID, units)
    WRITE(*,*) varID, code, TRIM(name), TRIM(longname), TRIM(units)
  END DO


!     Get the Time axis form the variable list
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

    END PROGRAM CDIREAD

