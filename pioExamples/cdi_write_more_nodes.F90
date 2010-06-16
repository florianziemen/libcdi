! mpxlf95_r -o cdi_write_parallel -I../src  cdi_write_f.F90  -L../src -lcdi -qextname -L/sw/aix53/szip-2.1/lib -lsz -L/sw/aix61/netcdf-4.0.1-ibm/lib -lnetcdf  -L/sw/aix61/hdf5-1.8.4-patch1/lib -lhdf5_hl -lhdf5 -lz


! TODO 
!       varId (:,:) RESTRICTED TO SIZESSTREAMS(1) module loadbalancing
!       catch cases with nonsensical relation nnodes/nstreams/sizesstreams

!--------------------------------------------------------------------------
!--------------------------------------------------------------------------

MODULE loadbalancing

CONTAINS 

SUBROUTINE mapProblems ( problemSizes, problemMapping, nProblems, nCalculator )

!...  data dictionary

IMPLICIT NONE

INTEGER, INTENT ( in ) :: nProblems
INTEGER, INTENT ( in ) :: problemSizes ( nProblems )
INTEGER, INTENT ( out ) :: problemMapping ( nProblems )
INTEGER, INTENT ( in ) :: nCalculator

INTEGER, PARAMETER :: INITVALUE1 = 99
INTEGER, PARAMETER :: INITVALUE2 = 99
INTEGER, PARAMETER :: TOLERANCE = 0
INTEGER, PARAMETER :: NOINDEX = -1
INTEGER, PARAMETER :: MIN = -99

INTEGER :: sortedProblems(nProblems,2)
INTEGER :: load(nCalculator)
REAL :: mean 
REAL :: capacityLeft
INTEGER :: currCapacity,nextCapacity
INTEGER :: currIndex,nextIndex
INTEGER :: i,j

!... init

problemMapping = INITVALUE1
load = INITVALUE2


DO i = 1,nProblems
  sortedProblems(i,1) = problemSizes(i)
  sortedProblems(i,2) = i
ENDDO

CALL iQsort(sortedProblems)
mean = SUM(problemSizes)/float(nCalculator)

!... loop over problems, biggest problem first

prbl: DO i = 1,nProblems

  !... 1. loop over PEs, find a capacity that fits

 fit: DO j = 1,nCalculator
    capacityLeft = ABS(mean-load(j)-sortedProblems(i,1))
    IF(capacityLeft<=TOLERANCE)THEN
      problemMapping(sortedProblems(i,2)) = j
      load(j) = load(j)+sortedProblems(i,1)
      WRITE(*,*)'1. loop used!'
      EXIT fit
    ENDIF
  ENDDO fit

  IF(problemMapping(sortedProblems(i,2))/=INITVALUE1) CYCLE prbl
  
  currCapacity = MIN
  currIndex = NOINDEX

  !... potential 2. loop over PEs, take PE with the least load

  leastload: DO j = 1,nCalculator
    nextIndex = j
    nextCapacity = mean-load(j)
    IF(nextCapacity>currCapacity)THEN
      currIndex = nextIndex
      currCapacity = nextCapacity
    ENDIF
  ENDDO leastload

  problemMapping(sortedProblems(i,2))=currIndex
  load(currIndex) = load(currIndex)+sortedProblems(i,1)
ENDDO prbl


!write(*,*)
!write(*,'(1X,F9.2)') mean
!write(*,*)
!write(*,'(1X,10I6)') load
!write(*,*)
!WRITE(*,'(1X,10I4)') problemSizes
!write(*,*)
!write(*,'(1X,10I4)') sortedProblems
!write(*,*)
!WRITE(*,'(1X,10I4)') problemMapping


CONTAINS

RECURSIVE SUBROUTINE iQsort(A)

IMPLICIT NONE
INTEGER, INTENT(inout) :: A(:,:)
INTEGER:: pivot(1,2),temp(1,2)
INTEGER :: n,pivotIndex,currIndex

IF(SIZE(A,2)/=2) STOP 'error: subroutine iQsort(A), dimension(A,2) has to be 2.'

n = SIZE(A,1)
IF(n<=1)RETURN

pivot(1,:) = A(n,:)
pivotIndex = n
currIndex = 1

DO WHILE(currIndex < pivotIndex)
  IF(A(currIndex,1)<pivot(1,1))THEN
    temp = pivot
    A(pivotIndex,:) = A(currIndex,:)
    A(currIndex,:) = A(pivotIndex-1,:)
    A(pivotIndex-1,:) = temp(1,:)
    pivotIndex = pivotIndex-1
  ELSE
    currIndex = currIndex+1
  ENDIF
ENDDO

IF(pivotIndex /= 1) CALL iQsort(A(1:pivotIndex-1,:))
IF(pivotIndex /= n) CALL iQsort(A(pivotIndex+1:n,:))

END SUBROUTINE iQsort

END SUBROUTINE mapProblems

END MODULE loadbalancing

!---------------------------------------------------------
!---------------------------------------------------------

      PROGRAM CDIWRITE

      USE loadbalancing

      IMPLICIT NONE

      INCLUDE 'cdi.inc'

#ifndef NOMPI 
      INCLUDE 'mpif.h'
#endif

      INTEGER, PARAMETER :: dp = SELECTED_REAL_KIND(12,307) 
 
      INTEGER, PARAMETER :: nPtypes = 4
      INTEGER, PARAMETER :: MAXNODES = 249

      INTEGER, PARAMETER :: ddebug = 0

      INTEGER, PARAMETER :: NLON  = 384   ! Number of longitudes 384
      INTEGER, PARAMETER :: NLAT  = 192   ! Number of latitudes  192
      INTEGER, PARAMETER :: NLEV  =  96   ! Number of levels      96
      INTEGER, PARAMETER :: NTIME =  10   ! Number of time steps 124
      INTEGER, PARAMETER :: NFILES = 10   ! Number of files        7
      INTEGER, PARAMETER :: SIZESSTREAMS ( NFILES ) = (/ 35, 35, 35, 35, 35, 35, 35, 35, 35, 35 /)

      CHARACTER*(*), PARAMETER :: filenames ( NFILES ) = (/ 'dat0.grb', 'dat1.grb', 'dat2.grb',  'dat3.grb', 'dat4.grb', 'dat5.grb', 'dat6.grb', 'dat7.grb', 'dat8.grb', 'dat9.grb' /)

      REAL(dp):: lons(NLON), lats(NLAT), levs(NLEV)
      REAL(dp):: var(NLON*NLAT*NLEV)

      INTEGER, ALLOCATABLE :: vlistID ( : ), streamID ( : )
      INTEGER, ALLOCATABLE :: lstreamfirst ( : ), lstreamlast ( : )
      INTEGER, ALLOCATABLE :: varIDs ( :, : )

      INTEGER :: pioComm, pioComm_NODE, rank, error, status
      INTEGER :: gridID, zaxisID, taxisID
      INTEGER :: collectingData, ncollectors
      INTEGER :: sID, tsID, i, j, nmiss

      INTEGER :: PTYPE, numarg, iargc, iarg, length, factor
      INTEGER :: NNODES, NFILES_NODE
      INTEGER :: mapping ( NFILES ), mycolor, mynode
      INTEGER, ALLOCATABLE ::  colors ( : )
      INTEGER, ALLOCATABLE :: SIZESSTREAMS_NODE ( : )

      CHARACTER ( len = 80 ) :: arg 
      CHARACTER(len=8), ALLOCATABLE :: FILENAMES_NODE ( : ) 
      
      REAL(dp) :: startTime, stopTime
      REAL(dp) :: accumOpen, accumClose, accumWrite


#ifndef NOMPI
      CALL MPI_INIT ( error )
      CALL MPI_COMM_DUP ( MPI_COMM_WORLD, pioComm, error )
      CALL MPI_COMM_RANK ( MPI_COMM_WORLD, rank, error )

      PTYPE = -1
      
      numarg = iargc ()
      
      IF ( numarg /= 2 ) THEN
        CALL MPI_ABORT ( pioComm, 1, error )
      END IF
      
      CALL getarg ( 1, arg )
      iarg = IACHAR ( arg ) - 48
      length = LEN_TRIM ( arg )
      
      IF ( length /= 1 .OR. iarg < 0 .OR. iarg > nPtypes ) THEN
        CALL MPI_ABORT ( pioComm, 1, error )
      ELSE
        PTYPE = iarg
      END IF
      
      CALL getarg ( 2, arg )
      length = LEN_TRIM ( arg )
      IF ( length > 3 ) CALL MPI_ABORT ( pioComm, 1, error ) 

      factor = 1
      NNODES = 0
      
      DO i = length, 1, -1
        iarg = IACHAR(arg(i:)) - 48
        IF ( iarg < 0  .OR. iarg > 9 ) THEN
          STOP
        ELSE
          NNODES = NNODES + factor * iarg
          factor = factor * 10
        END IF
      END DO
      
      IF ( NNODES < 1 .OR. NNODES > NFILES ) CALL MPI_ABORT ( pioComm, 1, error )
#else
      NNODES = 1
#endif

#ifndef NOMPI
      IF ( rank == 0 ) THEN
        CALL mapProblems ( SIZESSTREAMS, mapping, NFILES, NNODES  )
      
        IF ( ddebug == 1 ) THEN
          WRITE (*, *) 'SIZESSTREAMS=', SIZESSTREAMS, 'mapping=', mapping, ', NNODES=',NNODES
        END IF
      END IF
      CALL MPI_BCAST ( mapping, NFILES, MPI_INTEGER, 0, pioComm, error )
#else
      CALL mapProblems ( SIZESSTREAMS, mapping, NFILES, NNODES  )
      
      IF ( ddebug == 1 ) THEN
        WRITE (*, *) 'SIZESSTREAMS=', SIZESSTREAMS, 'mapping=', mapping, ', NNODES=',NNODES
      END IF
#endif

      accumOpen  = 0.0
      accumClose = 0.0
      accumWrite = 0.0

      nmiss = 0
      collectingData = 1

      CALL initCoords ()

#ifndef NOMPI
      ALLOCATE ( colors ( NNODES ), stat=status )
      collectingData =  pioInit( PTYPE, pioComm,  mycolor, colors, NNODES, pioComm_NODE, ncollectors )

      IF ( ddebug == 1 ) THEN
        WRITE (*,*) 'MAIN: pioComm=', pioComm, 'pioComm_NODE=', pioComm_NODE, 'rank=',rank,'mycolor=', mycolor, 'colors=', colors, 'ncollectors=', ncollectors
      END IF

      IF ( collectingData /= 1 ) THEN
         GO TO 1
      END IF

      IF ( ddebug == 1 .AND. rank == 0 ) THEN
         WRITE ( *, * ) 'NLON=',NLON,', NLAT=',NLAT,', NLEV=',NLEV,', NTIME=',NTIME,','
         WRITE ( *, * ) ' SIZESSTREAMS(1)=',SIZESSTREAMS(1), ', total amount raw='
         WRITE ( *, '( 2x, F9.3, 2x, a5 )' ) NLON * NLAT * NLEV * NTIME * SIZESSTREAMS(1) /(1024*1024),' MB';
      END IF

      DO i = 1, NNODES
        IF ( colors ( i ) == mycolor ) THEN 
          mynode = i
        END IF
      END DO

      NFILES_NODE = 0

      DO i = 1, NFILES
        IF ( mapping ( i ) == mynode ) THEN
          NFILES_NODE = NFILES_NODE + 1
        END IF
      END DO
#else 
      mynode = 1
      NFILES_NODE = NFILES
#endif

      ALLOCATE ( SIZESSTREAMS_NODE ( NFILES_NODE ), stat = status )
      ALLOCATE ( FILENAMES_NODE ( NFILES_NODE ), stat = status )

      j = 1

      DO i = 1, NFILES
        IF ( mapping ( i ) == mynode ) THEN
          SIZESSTREAMS_NODE ( j ) = SIZESSTREAMS ( i )
          FILENAMES_NODE ( j ) = filenames ( i )
          j = j + 1
        END IF
      END DO
      
      IF ( ddebug == 1 ) THEN
        WRITE ( *, * ) 'mynode=', mynode, ', NFILES_NODE=', NFILES_NODE, ', SIZESSTREAMS_NODE=',SIZESSTREAMS_NODE, ', FILENAMES_NODE=', FILENAMES_NODE 
      END IF

      ALLOCATE ( vlistID ( NFILES_NODE ), stat = status )
      ALLOCATE ( streamID ( NFILES_NODE ), stat = status )
      ALLOCATE ( lstreamfirst ( NFILES_NODE ), stat = status )
      ALLOCATE ( lstreamlast ( NFILES_NODE ), stat = status )
      ALLOCATE ( varIDs ( SIZESSTREAMS_NODE ( 1 ), NFILES_NODE ), stat = status )

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
      DO i = 1, NFILES_NODE
         vlistID ( i )  = vlistCreate()
      END DO

      DO i = 1, NFILES_NODE
         DO j = 1, SIZESSTREAMS_NODE ( 1 )
                                !     Define the variables
            varIDs ( j, i ) = vlistDefVar ( vlistID ( i ), gridID, zaxisID, TIME_VARIABLE )
            
                                !     Define the variable names
            CALL vlistDefVarName ( vlistID ( i ), varIDs ( j, i ), "varname")
         END DO
      END DO
      
!     Create a Time axis
      taxisID = taxisCreate(TAXIS_ABSOLUTE)

!     Assign the Time axis to the variable list
      DO i = 1, NFILES_NODE
         CALL vlistDefTaxis ( vlistID ( i ), taxisID)
      END DO

#ifndef NOMPI
      startTime = MPI_WTIME()
#else
      CALL CPU_TIME ( startTime )
#endif
      
!     Create a dataset in GRB fromat

      DO i = 1, NFILES_NODE
         streamID ( i ) = streamOpenWrite ( FILENAMES_NODE ( i ), FILETYPE_GRB )
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
      DO i = 1, NFILES_NODE
#ifndef NOMPI
         IF ( PTYPE /= PIO_NONE ) THEN
            CALL initLocalstream ( ncollectors, SIZESSTREAMS_NODE ( i ), lstreamfirst ( i ), lstreamlast ( i ))
         ELSE 
            lstreamfirst ( i ) = 1
            lstreamlast ( i )  = SIZESSTREAMS_NODE ( i )
         END IF
#else
         lstreamfirst ( i ) = 1
         lstreamlast ( i )  = SIZESSTREAMS_NODE ( i )
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
         DO i = 1, NFILES_NODE
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
      DO i = 1, NFILES_NODE
         CALL streamClose ( streamID ( i ))
      END DO

#ifndef NOMPI
      stopTime = MPI_WTIME()
#else
      CALL CPU_TIME ( stopTime )
#endif
      
      accumClose = stopTime - startTime

!     Destroy the objects
      DO i = 1, NFILES_NODE
         CALL vlistDestroy ( vlistID ( i ))
      END DO

      CALL taxisDestroy(taxisID)
      CALL zaxisDestroy(zaxisID)
      CALL gridDestroy(gridID)


      DEALLOCATE ( SIZESSTREAMS_NODE, stat = status )
      DEALLOCATE ( FILENAMES_NODE, stat = status )
      DEALLOCATE ( vlistID, stat = status )
      DEALLOCATE ( streamID, stat = status )
      DEALLOCATE ( lstreamfirst, stat = status )
      DEALLOCATE ( lstreamlast, stat = status )
      DEALLOCATE ( varIDs, stat = status )

#ifndef NOMPI 
1     IF ( ddebug == 1 ) THEN
        WRITE ( *, * ) 'pe', rank,'in main() at label "finish"' 
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
        WRITE ( *, * ) 'ptype=', PTYPE
        WRITE ( *, * ) '------------------------------------------------------------------'
        WRITE ( *, * ) 'Maximum open  time:', accumOpen,  's'
        WRITE ( *, * ) 'Maximum write time:', accumWrite, 's'
        WRITE ( *, * ) 'Maximum close time:', accumClose, 's'
        WRITE ( *, * ) '------------------------------------------------------------------'
      END IF

  CALL pioFinalize ()
  CALL MPI_FINALIZE ( error ) 
  
#else
  WRITE ( *, * ) 'ptype=', PIO_NONE
  WRITE ( *, * ) '------------------------------------------------------------------'
  WRITE ( *, * ) 'Open  time:',  accumOpen, 's'
  WRITE ( *, * ) 'Write time:', accumWrite, 's'
  WRITE ( *, * ) 'Close time:', accumClose, 's'
  WRITE ( *, * ) '------------------------------------------------------------------'
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

      CALL MPI_COMM_RANK ( pioComm_NODE, rank, error )
 
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
         WRITE ( *, * ) 'pe', rank, 'has', lrecords, 'out of ', sizestream, 'records to write. ifirst=', ifirst, ', ilast=', ilast
      END IF
      
      END SUBROUTINE initLocalstream

      END
