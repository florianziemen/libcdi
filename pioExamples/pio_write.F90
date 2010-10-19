! mpxlf95_r -o cdi_write_parallel -I../src  cdi_write_f.F90  -L../src -lcdi 
!-qextname -L/sw/aix53/szip-2.1/lib -lsz -L/sw/aix61/netcdf-4.0.1-ibm/lib
! -lnetcdf  -L/sw/aix61/hdf5-1.8.4-patch1/lib -lhdf5_hl -lhdf5 -lz


! TODO 
!       varId (:,:) RESTRICTED TO STREAMSIZES(1) module loadbalancing
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
          WRITE(*,*)'MAIN: 1. loop used!'
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
    
  CONTAINS
    
    RECURSIVE SUBROUTINE iQsort(A)
      
      IMPLICIT NONE
      INTEGER, INTENT(inout) :: A(:,:)
      INTEGER:: pivot(1,2),temp(1,2)
      INTEGER :: n,pivotIndex,currIndex
      
      IF(SIZE(A,2)/=2) STOP 'MAIN: error: subroutine iQsort(A), dimension(A,2) has to be 2.'
      
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
  
  INTEGER, PARAMETER :: MAXTYPES = 4  
  INTEGER, PARAMETER :: MAXNODES = 249
  
  INTEGER, PARAMETER :: ddebug = 0
  
  INTEGER, PARAMETER :: NLON  = 384   ! Number of longitudes 384
  INTEGER, PARAMETER :: NLAT  = 192   ! Number of latitudes  192
  INTEGER, PARAMETER :: NLEV  =  96   ! Number of levels      96
  INTEGER, PARAMETER :: NTIME =  10  ! Number of time steps 124
  INTEGER, PARAMETER :: NSTREAMS = 10   ! Number of files        7
  INTEGER, PARAMETER :: STREAMSIZES ( NSTREAMS ) = (/ 35, 35, 35, 35, 35, &
    35, 35, 35, 35, 35 /)
  
  CHARACTER*(*), PARAMETER :: STREAMNAMES ( NSTREAMS ) = (/ 'dat0.grb', &
    'dat1.grb', 'dat2.grb',  'dat3.grb', 'dat4.grb', 'dat5.grb', 'dat6.grb', &
    'dat7.grb', 'dat8.grb', 'dat9.grb' /)
  
  CHARACTER*(*), PARAMETER :: PTYPENAMES ( 7 ) = (/ &
    'PIO_NONE                       ', &
    'PIO_MPI_NONB                   ', &
    'PIO_POSIX_NONB                 ', &
    'PIO_POSIX_ASYNCH               ', &
    'PIO_POSIX_FPGUARD_SENDRECV     ', &
    'PIO_POSIX_FPGUARD_THREAD_REFUSE', &
    'PIO_POSIX_FPGUARD_THREAD       '/)
  
  REAL(dp):: LONS(NLON), LATS(NLAT), LEVS(NLEV)
  REAL(dp):: var(NLON*NLAT*NLEV)
  
  INTEGER :: gridID, zaxisID, taxisID
  INTEGER, ALLOCATABLE :: vlistID ( : ), streamID ( : )
  INTEGER, ALLOCATABLE :: varIDs ( :, : )
  
  INTEGER :: sID, tsID, i, j, nmiss, status 
  
  INTEGER :: pioComm, pioCollectorComm_NODE, rank, error
  
  INTEGER :: PTYPE 
  INTEGER :: NNODES, nstreams_NODE, myNODE
  INTEGER, ALLOCATABLE :: streamsizes_NODE ( : )
  CHARACTER(len=8), ALLOCATABLE :: streamnames_NODE ( : )
  INTEGER, ALLOCATABLE :: streamfirsts_PE ( : ), streamlasts_PE ( : ) 
  INTEGER :: collectingData
  INTEGER :: ncollectors_NODE
  
  REAL(dp) :: timerSTART, timerSTOP
  REAL(dp) :: sumOPEN, sumCLOSE, sumWRITE
  
  
#ifndef NOMPI
  CALL MPI_INIT ( error )
  CALL MPI_COMM_DUP ( MPI_COMM_WORLD, pioComm, error )
  CALL MPI_COMM_RANK ( pioComm, rank, error )
  
  CALL readArgs( PTYPE )
  
  IF ( ddebug == 1 ) THEN
    WRITE ( *, * ) 'MAIN after readArgs: pe', rank, ', PTYPE=', PTYPE
  END IF
#else
  PTYPE = PIO_NONE
  pioComm = 0
#endif
  
  collectingData =  pioInit( PTYPE, pioComm, &
    myNODE, NNODES, pioCollectorComm_NODE )
  
  IF ( ddebug == 1 ) THEN
#ifndef NOMPI
    WRITE ( *, * ) 'MAIN after pioInit: pe', rank, ', NNODES=', NNODES, &
      ', myNODE=', myNODE
#else
    WRITE ( *, * ) 'MAIN after pioInit: NNODES=', NNODES, &
      ', myNODE=', myNODE
#endif
  END IF
  
  IF ( collectingData /= 1 ) THEN
    GO TO 1
  END IF
  
  CALL mapStreamsOnNodes
  
  IF ( ddebug == 1 ) THEN
#ifndef NOMPI
    WRITE ( *, * ) 'MAIN after mapStreamsOnNodes: pe', rank, &
      ', nstreams_NODE=', nstreams_NODE, ', streamnames_NODE=', &
      streamnames_NODE, ', streamsizes_NODE=', streamsizes_NODE
#else
    WRITE ( *, * ) 'MAIN after mapStreamsOnNodes: nstreams_NODE=', nstreams_NODE, &
      ', streamnames_NODE=', streamnames_NODE, ', streamsizes_NODE=', &
      streamsizes_NODE
#endif
  END IF

  CALL mapVarsOnPes
  
  IF ( ddebug == 1 ) THEN
#ifndef NOMPI
    WRITE ( *, * ) 'MAIN after mapVarsOnPes: pe', rank, ', streamfirsts_PE=', &
      streamfirsts_PE, 'streamlasts_PE=', streamlasts_PE     
#else
    WRITE ( *, * ) 'MAIN after mapVarsOnPes: streamfirsts_PE=', &
      streamfirsts_PE, 'streamlasts_PE=', streamlasts_PE 
#endif
  END IF
  
  CALL initCoords ()    
  
  ALLOCATE ( vlistID ( nstreams_NODE ), stat = status )
  ALLOCATE ( streamID ( nstreams_NODE ), stat = status )
  ALLOCATE ( varIDs ( streamsizes_NODE ( 1 ), nstreams_NODE ), stat = status )
  
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
  DO i = 1, nstreams_NODE
    vlistID ( i )  = vlistCreate()
  END DO
  
  !     Define the variables and the variable names
  DO i = 1, nstreams_NODE
    DO j = 1, streamsizes_NODE ( 1 )
      varIDs ( j, i ) = vlistDefVar ( vlistID ( i ), gridID, zaxisID, TIME_VARIABLE )
      CALL vlistDefVarName ( vlistID ( i ), varIDs ( j, i ), "varname")
    END DO
  END DO
  
  !     Create a Time axis
  taxisID = taxisCreate(TAXIS_ABSOLUTE)
  
  !     Assign the Time axis to the variable list
  DO i = 1, nstreams_NODE
    CALL vlistDefTaxis ( vlistID ( i ), taxisID)
  END DO
  
  sumOPEN  = 0.0
  sumCLOSE = 0.0
  sumWRITE = 0.0
  
#ifndef NOMPI
  timerSTART = MPI_WTIME()
#else
  CALL CPU_TIME ( timerSTART )
#endif
  
  !     Open files and create a dataset in GRB fromat
  
  DO i = 1, nstreams_NODE
    streamID ( i ) = streamOpenWrite ( streamnames_NODE ( i ), FILETYPE_GRB )
    IF ( streamID ( i ) < 0 ) THEN
      WRITE(0,*) cdiStringError ( streamID ( i ))
#ifndef NOMPI
      CALL MPI_FINALIZE ( error )
      STOP
#else
      STOP
#endif
    END IF
  END DO
  
#ifndef NOMPI
  timerSTOP = MPI_WTIME()
#else
  CALL CPU_TIME ( timerSTOP )
#endif
  
  sumOPEN = timerSTOP - timerSTART        
  
  CALL initVar ( var, 1, 1 )
  
  !     Assign the variable list to the dataset
  DO i = 1, nstreams_NODE
    CALL streamDefVlist ( streamID ( i ), vlistID ( i ))
  END DO
  
  nmiss = 0
  
  !     Loop over the number of time steps
  DO tsID = 0, NTIME-1
    !        Set the verification date to 1985-01-01 + tsID
    CALL taxisDefVdate ( taxisID, 19850101+tsID )
    !        Set the verification time to 12:00:00
    CALL taxisDefVtime ( taxisID, 120000 )
    DO i = 1, nstreams_NODE
      !        Define the time step
      status = streamDefTimestep ( streamID ( i ), tsID )
      DO sID = streamfirsts_PE ( i ), streamlasts_PE ( i )
#ifndef NOMPI
        timerSTART = MPI_WTIME()
#else
        CALL CPU_TIME ( timerSTART )
#endif
        
        !        Write var
        CALL streamWriteVar ( streamID ( i ), varIDs ( sID, i ), var, nmiss)
        
#ifndef NOMPI
        timerSTOP = MPI_WTIME()
#else
        CALL CPU_TIME ( timerSTOP )
#endif
        
        sumWRITE = sumWRITE + timerSTOP - timerSTART
        
      END DO
    END DO
  END DO
  
#ifndef NOMPI
  timerSTART = MPI_WTIME()
#else
  CALL CPU_TIME ( timerSTART )
#endif
  
  !     Close the output stream  
  DO i = 1, nstreams_NODE
    CALL streamClose ( streamID ( i ))
  END DO
  
#ifndef NOMPI
  timerSTOP = MPI_WTIME()
#else
  CALL CPU_TIME ( timerSTOP )
#endif
  
  sumCLOSE = timerSTOP - timerSTART
  
  !     Destroy the objects
  DO i = 1, nstreams_NODE
    CALL vlistDestroy ( vlistID ( i ))
  END DO
  
  CALL taxisDestroy(taxisID)
  CALL zaxisDestroy(zaxisID)
  CALL gridDestroy(gridID)
  
  DEALLOCATE ( streamsizes_NODE, stat = status )
  DEALLOCATE ( streamnames_NODE, stat = status )
  DEALLOCATE ( streamfirsts_PE, stat = status )
  DEALLOCATE ( streamlasts_PE, stat = status )
  DEALLOCATE ( vlistID, stat = status )
  DEALLOCATE ( streamID, stat = status )
  DEALLOCATE ( varIDs, stat = status )
  
1 CALL pioFinalize ()
  
#ifndef NOMPI 
  
  IF ( ddebug == 1 ) THEN
    WRITE ( *, * ) 'MAIN : pe', rank, ' at label "finish"' 
  END IF
  
  IF ( rank == 0 ) THEN
    CALL MPI_REDUCE ( MPI_IN_PLACE, sumOPEN,  1, MPI_DOUBLE_PRECISION, &
      MPI_MAX, 0, pioComm, error )
    CALL MPI_REDUCE ( MPI_IN_PLACE, sumWRITE, 1, MPI_DOUBLE_PRECISION, &
      MPI_MAX, 0, pioComm, error )
    CALL MPI_REDUCE ( MPI_IN_PLACE, sumCLOSE, 1, MPI_DOUBLE_PRECISION, &
      MPI_MAX, 0, pioComm, error )
  ELSE        
    CALL MPI_REDUCE (    sumOPEN, sumOPEN,  1, MPI_DOUBLE_PRECISION, &
      MPI_MAX, 0, pioComm, error )
    CALL MPI_REDUCE (   sumWRITE, sumWRITE, 1, MPI_DOUBLE_PRECISION, &
      MPI_MAX, 0, pioComm, error )
    CALL MPI_REDUCE (   sumCLOSE, sumCLOSE, 1, MPI_DOUBLE_PRECISION, &
      MPI_MAX, 0, pioComm, error )      
  END IF
  
  IF ( rank == 0 ) THEN
    WRITE ( *, * ) 'MAIN: NNODES = ', NNODES, &
      'ncollectors_NODE = ', ncollectors_NODE, &
      'PTYPE = ', PTYPENAMES ( PTYPE + 1 )
 
    WRITE ( *, * ) '------------------------------------------------------------------'
    WRITE ( *, * ) 'MAIN: Maximum open  time:', sumOPEN,  's'
    WRITE ( *, * ) 'MAIN: Maximum write time:', sumWRITE, 's'
    WRITE ( *, * ) 'MAIN: Maximum close time:', sumCLOSE, 's'
    WRITE ( *, * ) '------------------------------------------------------------------'
  END IF
  
  CALL MPI_COMM_FREE ( pioComm, error )
  CALL MPI_FINALIZE ( error )
  
#else
  
  WRITE ( *, * ) 'MAIN: ptype = SERIAL'
  WRITE ( *, * ) '------------------------------------------------------------------'
  WRITE ( *, * ) 'MAIN: Open  time:',  sumOPEN, 's'
  WRITE ( *, * ) 'MAIN: Write time:', sumWRITE, 's'
  WRITE ( *, * ) 'MAIN: Close time:', sumCLOSE, 's'
  WRITE ( *, * ) '------------------------------------------------------------------'
  
#endif
  
  
      
CONTAINS
  
  !********************************************
  
#ifndef NOMPI
  SUBROUTINE readArgs ( argMode )
    
    INTEGER, INTENT ( OUT ) :: argMode
    INTEGER :: numarg, inum, length
    CHARACTER :: arg
    
    IF ( rank == 0 ) THEN
      
      numarg = command_argument_count ()    
      
      IF ( numarg /= 1 ) THEN
        WRITE ( 0, * ) 'ERROR: ONE command line argument'
        CALL MPI_FINALIZE ( error )
        STOP
      END IF

      argMode = 0     
      CALL get_command_argument ( 1, value = arg )
      length = LEN_TRIM ( arg )

      IF ( length > 9 ) THEN
        WRITE ( 0, * ) 'ERROR: command line argument NOT READABLE'
        CALL MPI_FINALIZE ( error )
        STOP
      END IF
    
      DO i = 1, length
        inum = IACHAR ( arg ( i : i )) - 48  
        IF ( inum < 0 .OR. inum > 9 ) THEN
          WRITE ( 0, * ) 'ERROR: command line argument MODUS NOT VALID'
          CALL MPI_FINALIZE ( error )
          STOP
        END IF
        argMode = argMode + inum * 10 ** ( length - i )
      END DO
    END IF
    
    CALL MPI_BCAST ( argMode, 1, MPI_INTEGER, 0, pioComm, error );

  END SUBROUTINE readArgs
#endif
  
  !********************************************
  
  SUBROUTINE mapStreamsOnNodes 
    
#ifndef NOMPI
    INTEGER :: mapping ( NSTREAMS )
    INTEGER :: rank_NODE
    
    IF ( rank == 0 ) THEN
      CALL mapProblems ( STREAMSIZES, mapping, NSTREAMS, NNODES  )
    END IF
    
    CALL MPI_BCAST ( mapping, NSTREAMS, MPI_INTEGER, 0, pioComm, error )
    
    CALL MPI_COMM_RANK ( pioCollectorComm_NODE, rank_NODE, error )
    
    IF ( rank_NODE  == 0 ) THEN
      nstreams_NODE = 0
      
      DO i = 1, NSTREAMS
        IF ( mapping ( i ) == myNODE ) THEN
          nstreams_NODE = nstreams_NODE + 1
        END IF
      END DO
    END IF

    IF ( PTYPE /= PIO_NONE ) THEN
      CALL MPI_BCAST ( nstreams_NODE, 1, MPI_INTEGER, 0, pioCollectorComm_NODE, error )
    END IF
#else
    
    nstreams_NODE = NSTREAMS
    
#endif
    
    ALLOCATE ( streamsizes_NODE ( nstreams_NODE ), stat = status )
    ALLOCATE ( streamnames_NODE ( nstreams_NODE ), stat = status )
    
#ifndef NOMPI
    
    IF ( rank_NODE == 0 ) THEN
      j = 1
      
      DO i = 1, NSTREAMS
        IF ( mapping ( i ) == myNODE ) THEN
          streamsizes_NODE ( j ) = STREAMSIZES ( i )
          streamnames_NODE ( j ) = STREAMNAMES ( i )
          j = j + 1
        END IF
      END DO
    END IF

    IF ( PTYPE /= PIO_NONE ) THEN
      CALL MPI_BCAST ( streamsizes_NODE, nstreams_Node, MPI_INTEGER, &
        0, pioCollectorComm_NODE, error )
      
      DO i = 1, nstreams_NODE
        CALL MPI_BCAST ( streamnames_NODE ( i ), 8, MPI_CHARACTER, &
          0, pioCollectorComm_NODE, error )
      END DO
    END IF
#else
    streamsizes_NODE = STREAMSIZES
    streamnames_NODE = STREAMNAMES
#endif
    
  END SUBROUTINE mapStreamsOnNodes
  
  !********************************************
  
  SUBROUTINE mapVarsOnPes
    
    ALLOCATE ( streamfirsts_PE ( nstreams_NODE ), stat = status )
    ALLOCATE ( streamlasts_PE ( nstreams_NODE ), stat = status )
    
#ifndef NOMPI
    IF ( PTYPE /= PIO_NONE ) THEN
      CALL MPI_COMM_SIZE ( pioCollectorComm_NODE, ncollectors_NODE, error )
      
      IF ( PTYPE /= PIO_NONE ) THEN
        DO i = 1, nstreams_NODE
          CALL initLocalstream ( ncollectors_NODE, streamsizes_NODE ( i ), &
            streamfirsts_PE ( i ), streamlasts_PE ( i ))
        END DO
      ELSE
        streamfirsts_PE = 1
        streamlasts_PE = streamsizes_NODE
      END IF
    ELSE
      streamfirsts_PE = 1
      streamlasts_PE = streamsizes_NODE
    END IF
#else
    streamfirsts_PE = 1
    streamlasts_PE = streamsizes_NODE
#endif
    
  END SUBROUTINE mapVarsOnPes
  
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
      LONS ( i ) = 360.0 / NLON * (  i - 1 )
    END DO
    
    dist = 180.0 / NLAT
    
    LATS ( 1 ) = -90 + dist / 2.0;
    
    DO i = 2, NLAT
      LATS ( i ) = LATS ( i - 1 ) + dist
    END DO
    
    DO i = 1, NLEV
      LEVS ( i ) = 101300.0 / NLEV * i
    END DO
    
  END SUBROUTINE initCoords
  
  !********************************************
#ifndef NOMPI
  SUBROUTINE initLocalstream ( npes, sizestream, ifirst, ilast )
    
    INTEGER, INTENT  ( IN ) :: npes, sizestream
    INTEGER, INTENT ( OUT ) :: ifirst, ilast
    
    INTEGER :: rank, error, rest, lrecords
    
    CALL MPI_COMM_RANK ( pioCollectorComm_NODE, rank, error )
    
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
      WRITE ( *, * ) 'MAIN: pe', rank, 'has', lrecords, 'out of ', &
        sizestream, 'records to write. ifirst=', ifirst, ', ilast=', ilast
    END IF
    
  END SUBROUTINE initLocalstream
#endif
END PROGRAM CDIWRITE
