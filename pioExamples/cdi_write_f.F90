! TODO varId (:,:) RESTRICTED TO SIZESSTREAMS(1) 

      PROGRAM CDIWRITE

      IMPLICIT NONE

      INCLUDE 'cdi.inc'

#ifndef NOMPI 
      INCLUDE 'mpif.h'
#endif

      INTEGER, PARAMETER :: dp = SELECTED_REAL_KIND(12,307) 
 
      INTEGER, PARAMETER :: nPtypes = 4
      INTEGER :: PTYPE, numarg, iargc, iarg
      CHARACTER ( len = 80 ) :: arg    

      INTEGER, PARAMETER :: NLON  = 38   ! Number of longitudes 384
      INTEGER, PARAMETER :: NLAT  = 19   ! Number of latitudes  192
      INTEGER, PARAMETER :: NLEV  =  9   ! Number of levels      96
      INTEGER, PARAMETER :: NTIME =  2   ! Number of time steps 124
      INTEGER, PARAMETER :: NFILES = 10   ! Number of files        7
      INTEGER, PARAMETER :: SIZESSTREAMS ( NFILES ) = (/ 35, 35, 35, 35, 35, 35, 35, 35, 35, 35 /)
      CHARACTER*(*), PARAMETER :: filenames ( NFILES ) = (/ 'dat0.grb', 'dat1.grb', 'dat2.grb',  'dat3.grb', 'dat4.grb', 'dat5.grb', 'dat6.grb', 'dat7.grb', 'dat8.grb', 'dat9.grb' /)

      INTEGER, PARAMETER :: ddebug = 0
      INTEGER :: pioComm

      REAL(dp):: lons(NLON), lats(NLAT), levs(NLEV)

      INTEGER :: gridID, zaxisID, taxisID
      INTEGER :: vlistID ( NFILES ), streamID ( NFILES )

      INTEGER :: i, j, nmiss
      INTEGER :: varIDs ( SIZESSTREAMS ( 1 ), NFILES )
      REAL(dp):: var(NLON*NLAT*NLEV)
      INTEGER :: collectingData
      INTEGER :: ncollectors, sID, tsID, rank
      INTEGER :: lstreamfirst ( NFILES ), lstreamlast ( NFILES )

      INTEGER :: error, status

      REAL(dp) :: startTime, stopTime
      REAL(dp) :: accumOpen, accumClose, accumWrite

#ifndef NOMPI
      CALL MPI_INIT ( error )
      CALL MPI_COMM_DUP ( MPI_COMM_WORLD, pioComm, error )
      CALL MPI_COMM_RANK ( MPI_COMM_WORLD, rank, error )

      PTYPE = -1
      
      numarg = iargc ()
      
      IF ( numarg /= 1 ) THEN
        CALL MPI_ABORT ( pioComm, 1, error )
      END IF
      
      CALL getarg ( 1, arg )
      iarg = iachar ( arg )
      
      IF ( iarg < 48 .OR. iarg > 48 + nPtypes ) THEN
        CALL MPI_ABORT ( pioComm, 1, error )
      ELSE
        PTYPE = iarg - 48
      END IF
#endif

      accumOpen  = 0.0
      accumClose = 0.0
      accumWrite = 0.0

      nmiss = 0
      collectingData = 1

      CALL initCoords ()

#ifndef NOMPI
      collectingData =  pioInit( PTYPE, pioComm, ncollectors )
      IF ( collectingData /= 1 ) THEN
         GO TO 1
      END IF

      IF ( ddebug == 1 .AND. rank == 0 ) THEN
         write ( *, * ) 'NLON=',NLON,', NLAT=',NLAT,', NLEV=',NLEV,', NTIME=',NTIME,','
         write ( *, * ) ' SIZESSTREAMS(1)=',SIZESSTREAMS(1), ', total amount='
         write ( *, '( 2x, I12, 2x, a5 )' ) NLON * NLAT * NLEV * NTIME * SIZESSTREAMS(1) * 8,' bytes';
      END IF
#endif

!     Create a regular lon/lat grid
      gridID = gridCreate ( GRID_LONLAT, NLON*NLAT )
      CALL gridDefXsize ( gridID, NLON )
      CALL gridDefYsize ( gridID, NLAT )
      CALL gridDefXvals ( gridID, lons )
      CALL gridDefYvals ( gridID, lats )

!     Create a pressure level Z-axis
      zaxisID = zaxisCreate ( ZAXIS_PRESSURE, NLEV )
      CALL zaxisDefLevels ( zaxisID, levs )
 
!     Create a variable list
      DO i = 1, NFILES
         vlistID ( i )  = vlistCreate()
      END DO

      DO i = 1, NFILES
         DO j = 1, SIZESSTREAMS ( 1 )
                                !     Define the variables
            varIDs ( j, i ) = vlistDefVar ( vlistID ( i ), gridID, zaxisID, TIME_VARIABLE )
            
                                !     Define the variable names
            CALL vlistDefVarName ( vlistID ( i ), varIDs ( j, i ), "varname")
         END DO
      END DO
      
!     Create a Time axis
      taxisID = taxisCreate(TAXIS_ABSOLUTE)

!     Assign the Time axis to the variable list
      DO i = 1, NFILES
         CALL vlistDefTaxis ( vlistID ( i ), taxisID)
      END DO

#ifndef NOMPI
      startTime = MPI_WTIME()
#else
      CALL CPU_TIME ( startTime )
#endif
      
!     Create a dataset in GRB fromat

      DO i = 1, NFILES
         streamID ( i ) = streamOpenWrite ( filenames ( i ), FILETYPE_GRB )
         IF ( streamID ( i ) < 0 ) THEN
            WRITE(0,*) cdiStringError ( streamID ( i ))
#ifndef NOMPI
            CALL MPI_ABORT ( pioComm, 1, error )
#else
            STOP
#endif
         END IF
      END DO

#ifndef NOMPI
      stopTime = MPI_WTIME()
#else
      CALL CPU_TIME ( stopTime )
#endif
      
      accumOpen = stopTime - startTime



!     Assign the variable list to the dataset
      DO i = 1, NFILES
#ifndef NOMPI
         IF ( PTYPE /= PIO_NONE ) THEN
            CALL initLocalstream ( ncollectors, SIZESSTREAMS ( i ), lstreamfirst ( i ), lstreamlast ( i ))
         ELSE 
            lstreamfirst ( i ) = 1
            lstreamlast ( i )  = SIZESSTREAMS ( i )
         END IF
#else
         lstreamfirst ( i ) = 1
         lstreamlast ( i )  = SIZESSTREAMS ( i )
#endif
         CALL streamDefVlist ( streamID ( i ), vlistID ( i ))
      END DO

      CALL initVar ( var, 1, 1 )

!     Loop over the number of time steps
      DO tsID = 0, NTIME-1
!        Set the verification date to 1985-01-01 + tsID
         CALL taxisDefVdate ( taxisID, 19850101+tsID )
!        Set the verification time to 12:00:00
         CALL taxisDefVtime ( taxisID, 120000 )
         DO i = 1, NFILES
!        Define the time step
            status = streamDefTimestep ( streamID ( i ), tsID )
            DO sID = lstreamfirst ( i ), lstreamlast ( i )
#ifndef NOMPI
              startTime = MPI_WTIME()
#else
              CALL CPU_TIME ( startTime )
#endif
               
!        Write var
               CALL streamWriteVar ( streamID ( i ), varIDs ( sID, i ), var, nmiss)

#ifndef NOMPI
               stopTime = MPI_WTIME()
#else
               CALL CPU_TIME ( stopTime )
#endif

               accumWrite = accumWrite + stopTime - startTime
               
             END DO
         END DO
      END DO

#ifndef NOMPI
      startTime = MPI_WTIME()
#else
      CALL CPU_TIME ( startTime )
#endif

!     Close the output stream  
      DO i = 1, NFILES
         CALL streamClose ( streamID ( i ))
      END DO

#ifndef NOMPI
      stopTime = MPI_WTIME()
#else
      CALL CPU_TIME ( stopTime )
#endif
      
      accumClose = stopTime - startTime

!     Destroy the objects
      DO i = 1, NFILES
         CALL vlistDestroy ( vlistID ( i ))
      END DO

      CALL taxisDestroy(taxisID)
      CALL zaxisDestroy(zaxisID)
      CALL gridDestroy(gridID)

#ifndef NOMPI 
1     IF ( ddebug == 1 ) THEN
        write ( *, * ) 'pe', rank,'in main() at label "finish"' 
      END IF

      IF ( rank == 0 ) THEN
        CALL MPI_REDUCE ( MPI_IN_PLACE, accumOpen,  1, MPI_DOUBLE, MPI_MAX, 0, pioComm, error )
        CALL MPI_REDUCE ( MPI_IN_PLACE, accumWrite, 1, MPI_DOUBLE, MPI_MAX, 0, pioComm, error )
        CALL MPI_REDUCE ( MPI_IN_PLACE, accumClose, 1, MPI_DOUBLE, MPI_MAX, 0, pioComm, error )
      ELSE        
        CALL MPI_REDUCE (    accumOpen, accumOpen,  1, MPI_DOUBLE, MPI_MAX, 0, pioComm, error )
        CALL MPI_REDUCE (   accumWrite, accumWrite, 1, MPI_DOUBLE, MPI_MAX, 0, pioComm, error )
        CALL MPI_REDUCE (   accumClose, accumClose, 1, MPI_DOUBLE, MPI_MAX, 0, pioComm, error )      
      END IF
  
      IF ( rank == 0 ) THEN
        write ( *, * ) 'ptype=', PTYPE
        write ( *, * ) '------------------------------------------------------------------'
        write ( *, * ) 'Maximum open  time:', accumOpen,  's'
        write ( *, * ) 'Maximum write time:', accumWrite, 's'
        write ( *, * ) 'Maximum close time:', accumClose, 's'
        write ( *, * ) '------------------------------------------------------------------'
      END IF

  CALL pioFinalize ()
  CALL MPI_FINALIZE ( error ) 
  
#else
  write ( *, * ) 'ptype=', PIO_NONE
  write ( *, * ) '------------------------------------------------------------------'
  write ( *, * ) 'Open  time:',  accumOpen, 's'
  write ( *, * ) 'Write time:', accumWrite, 's'
  write ( *, * ) 'Close time:', accumClose, 's'
  write ( *, * ) '------------------------------------------------------------------'
#endif

CONTAINS

      !********************************************
      
      SUBROUTINE initVar ( argVar, argSID, argTSID )

      REAL(dp), INTENT ( INOUT ) :: argVar(NLON*NLAT*NLEV)
      INTEGER , INTENT ( IN ) :: argSID, argTSID
      INTEGER :: j

      DO j = 1, NLON*NLAT*NLEV
        argVar ( j ) = 100.0 * ( argSID + 1 ) + argTSID + 1.0
      END DO
        
      END SUBROUTINE initVar

      !********************************************
      
      SUBROUTINE initCoords

      INTEGER      :: i
      REAL ( dp )  :: dist

      DO i = 1, NLON
         lons ( i ) = 360.0 / NLON * (  i - 1 )
      END DO

      dist = 180.0 / NLAT

      lats ( 1 ) = -90 + dist / 2.0;

      DO i = 2, NLAT
         lats ( i ) = lats ( i - 1 ) + dist
      END DO

      DO i = 1, NLEV
         levs ( i ) = 101300.0 / NLEV * i
      END DO

      END SUBROUTINE initCoords

      !********************************************

      SUBROUTINE initLocalstream ( npes, sizestream, ifirst, ilast )

      INTEGER, INTENT  ( IN ) :: npes, sizestream
      INTEGER, INTENT ( OUT ) :: ifirst, ilast

      INTEGER :: rank, error, rest, lrecords

      CALL MPI_COMM_RANK ( pioComm, rank, error )
 
      lrecords = sizestream / npes
      rest     = MOD ( sizestream, npes )
      ifirst   = rank * lrecords + 1
      
      IF ( rest /= 0 ) THEN
         IF ( rank < rest ) THEN
            lrecords = lrecords + 1
            ifirst = ifirst + rank
         ELSE
            ifirst = ifirst + rest
         END IF    
      END IF

      ilast = ifirst + lrecords - 1

      IF ( ddebug == 1 ) THEN
         write ( *, * ) 'pe', rank, 'has', lrecords, 'out of ', sizestream, 'records to write. ifirst=', ifirst, ', ilast=', ilast
      END IF
      
      END SUBROUTINE initLocalstream

      END
