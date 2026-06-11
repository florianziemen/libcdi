!  gfortran cdi-2.6.1//src/mo_cdi.f90 cdi_read_extpar.f90  cdi-2.6.1/build/test/src/.libs/libcdi.a $HOME/local/netcdf-c-4.9.3/lib/libnetcdf.dylib

MODULE mo_ext_data_inquire

    USE mo_cdi,                ONLY: FILETYPE_NC, FILETYPE_NC2, FILETYPE_NC4
    USE mo_cdi,                ONLY: streamInqVlist, vlistInqVarName, CDI_MAX_NAME
    USE mo_cdi,                ONLY: CDI_GLOBAL, FILETYPE_GRB2, streamOpenRead, streamInqFileType, &
    &                              streamInqVlist, cdiStringError, vlistInqVarZaxis, zaxisInqSize, &
    &                              vlistNtsteps, vlistInqVarGrid, cdiInqAttTxt, vlistInqVarIntKey, &
    &                              gridInqUUID, vlistNvars, streamReadVar, streamReadVarSlice

  IMPLICIT NONE
  PRIVATE

  PUBLIC :: has_filetype_netcdf
  PUBLIC :: test_cdi_varID
  PUBLIC :: get_cdi_varID
  PUBLIC :: inquire_extpar_file
  PUBLIC :: print_cdi_vars
  PUBLIC :: read_vars

  CONTAINS

    FUNCTION has_filetype_netcdf(cdi_filetype)
    LOGICAL :: has_filetype_netcdf
    INTEGER, INTENT(IN) :: cdi_filetype

    has_filetype_netcdf = ((cdi_filetype == FILETYPE_NC)  .OR. &
      &                    (cdi_filetype == FILETYPE_NC2) .OR. &
      &                    (cdi_filetype == FILETYPE_NC4))
  END FUNCTION has_filetype_netcdf
  !-------------------------------------------------------------------------
  !> @return CDI variable ID if CDI stream contains a variable of the
  !> given name, -1 otherwise
  !
  !  Uses cdilib for file access.
  !  Initial revision by F. Prill, DWD (2013-02-19)
  !
  FUNCTION test_cdi_varID(streamID, name) RESULT(result_varID)
    INTEGER                                 :: result_varID
    INTEGER,           INTENT(IN)           :: streamID            !< link to file
    CHARACTER (LEN=*), INTENT(IN)           :: name                !< variable name
    ! local variables
    CHARACTER(len=CDI_MAX_NAME) :: zname
    INTEGER                         :: nvars, varID, vlistID

    zname   = ""
    vlistID = streamInqVlist(streamID)

    result_varID = -1
    ! total number of available fields:
    nvars = vlistNvars(vlistID)
    ! loop over vlist, find the corresponding varID
    LOOP : DO varID=0,(nvars-1)

      CALL vlistInqVarName(vlistID, varID, zname)
      IF (zname == name) THEN

        result_varID = varID
        EXIT LOOP
      END IF
    END DO LOOP
  END FUNCTION test_cdi_varID

  SUBROUTINE print_cdi_vars(streamID)
    INTEGER,           INTENT(IN)           :: streamID            !< link to file
    ! local variables
    CHARACTER(len=CDI_MAX_NAME) :: zname
    INTEGER                         :: nvars, varID, vlistID

    zname   = ""
    vlistID = streamInqVlist(streamID)

    ! total number of available fields:
    nvars = vlistNvars(vlistID)
    ! loop over vlist, find the corresponding varID
    LOOP : DO varID=0,(nvars-1)

      CALL vlistInqVarName(vlistID, varID, zname)
      WRITE(*,*) "var", varID+1, "  ", TRIM(zname)
    END DO LOOP
  END SUBROUTINE print_cdi_vars

  SUBROUTINE read_vars(streamID)
    INTEGER,           INTENT(IN)           :: streamID            !< link to file
    ! local variables
    CHARACTER(len=CDI_MAX_NAME) :: zname
    INTEGER                         :: nvars, varID, vlistID
    REAL*8 :: data(1884160)
    INTEGER :: numMissVals

    zname   = ""
    vlistID = streamInqVlist(streamID)

    ! total number of available fields:
    nvars = vlistNvars(vlistID)
    ! loop over vlist, find the corresponding varID
    LOOP : DO varID=0,(nvars-1)

      CALL streamReadVarSlice(streamID, varID, 0, data, numMissVals)
      CALL vlistInqVarName(vlistID, varID, zname)
      WRITE(*,*) "var", varID+1, "  ", TRIM(zname), "  ", data(1)
    END DO LOOP
  END SUBROUTINE read_vars

  !-------------------------------------------------------------------------
  !> @return vlist variable ID for a given variable name
  !
  !  Uses cdilib for file access.
  !  Initial revision by F. Prill, DWD (2013-02-19)
  !
  FUNCTION get_cdi_varID(streamID, name) RESULT(result_varID)
    INTEGER                                 :: result_varID
    INTEGER,           INTENT(IN)           :: streamID            !< link to file
    CHARACTER (LEN=*), INTENT(IN)           :: name                !< variable name
    ! local variables

    result_varID = test_cdi_varID(streamID, name)
    IF (result_varID== -1) THEN
      WRITE(*,*) "Variable "//TRIM(name)//" not found!"
    END IF
  END FUNCTION get_cdi_varID

  !-------------------------------------------------------------------------
  ! Opens ExtPar file for the atmosphere and inquires file attributes.
  ! Relevant attributes and CDI IDs are stored in an object of
  ! type t_ext_atm_attr.
  !
  !-------------------------------------------------------------------------
  SUBROUTINE inquire_extpar_file


    ! local variables
    INTEGER :: mpi_comm, vlist_id, lu_class_fraction_id, zaxis_id, var_id
    INTEGER :: horizon_id
    LOGICAL :: l_exist


    LOGICAL                 :: lmatch                         ! for comparing UUIDs
    INTEGER                 :: cdiGridID

    INTEGER :: lu_var_id, localInformationNumber
    INTEGER :: ret
    CHARACTER(len=512) :: rawdata_attr

    CHARACTER(len=512)    :: extpar_file
    INTEGER                    :: cdi_extpar_id
    INTEGER                    :: cdi_filetype
    !
    INTEGER                    :: nhori     ! number of horizon sectors
    INTEGER                    :: nclass_lu
    INTEGER                    :: nmonths_ext
    INTEGER                    :: i_lctype
    LOGICAL                    :: is_frglac_in


    !---------------------------------------------!
    ! Check validity of external parameter file   !
    !---------------------------------------------!

      ! generate file name
      extpar_file = "icon_extpar_0008_R02B05_G.nc"

      INQUIRE (FILE=extpar_file, EXIST=l_exist)
      IF (.NOT.l_exist)  WRITE(*,*) 'external data file is not found.'

      ! open file
      cdi_extpar_id = streamOpenRead(TRIM(extpar_file))
      IF (cdi_extpar_id < 0) THEN
        WRITE (*, '(133a)') "Cannot open external parameter file ", &
             cdiStringError(cdi_extpar_id)
      END IF
      cdi_filetype = streamInqFileType(cdi_extpar_id)

      vlist_id = streamInqVlist(cdi_extpar_id)

     CALL  print_cdi_vars(cdi_extpar_id)

      ! get time dimension from external data file
      nmonths_ext = vlistNtsteps(vlist_id)

        nhori = -1

      ! get the number of landuse classes
      lu_class_fraction_id = get_cdi_varID(cdi_extpar_id, "LU_CLASS_FRACTION")
      zaxis_id             = vlistInqVarZaxis(vlist_id, lu_class_fraction_id)

      ! Compare UUID of external parameter file with UUID of grid.
      !
      ! get horizontal grid UUID contained in extpar file
      ! use lu_class_fraction as sample field
      cdiGridID = vlistInqVarGrid(vlist_id, lu_class_fraction_id)
      !CALL gridInqUUID(cdiGridID, extpar_uuidOfHGrid%DATA)
      !

      ! Determine which data source has been used to generate the
      ! external perameters: For NetCDF format, we check the
      ! global attribute "rawdata". For GRIB2 format we check the
      ! key "localInformationNumber".
      IF (has_filetype_netcdf(cdi_filetype)) THEN
        ret      = cdiInqAttTxt(vlist_id, CDI_GLOBAL, 'rawdata', 512, rawdata_attr)
      ELSE IF (cdi_filetype == FILETYPE_GRB2) THEN
        lu_var_id              = get_cdi_varID(cdi_extpar_id, 'LU_CLASS_FRACTION')
        localInformationNumber = vlistInqVarIntKey(vlist_id, lu_var_id, "localInformationNumber")
      END IF

      ! Check whether external parameter file contains MODIS albedo-data
        IF ( (test_cdi_varID(cdi_extpar_id, 'ALB')   == -1) .OR.    &
          &  (test_cdi_varID(cdi_extpar_id, 'ALNID') == -1) .OR.    &
          &  (test_cdi_varID(cdi_extpar_id, 'ALUVD') == -1) ) THEN
          WRITE(*,*) 'MODIS albedo fields missing in '//TRIM(extpar_file)
        ENDIF

      ! Check whether external parameter file contains monthly longwave surface emissivity data
        IF (test_cdi_varID(cdi_extpar_id, 'EMISS')   == -1) THEN
          WRITE(*,*) 'Monthly longwave surface emissvity data missing in '//TRIM(extpar_file)
        ENDIF

      ! Check whether external parameter file contains SST climatology
        IF ( test_cdi_varID(cdi_extpar_id, 'T_SEA')  == -1 ) THEN
          WRITE(*,*) 'SST climatology missing in '//TRIM(extpar_file)
        ENDIF

        ! Check whether external parameter file contains cloud droplet number climatology
        IF ( test_cdi_varID(cdi_extpar_id, 'cdnc')  == -1 ) THEN
          WRITE(*,*) 'icpl_aero_gscp=3 but cloud droplet number climatology missing in '//TRIM(extpar_file)
        ELSE
          WRITE(*,*)'Found cloud droplet number in extpar file'
        ENDIF

      ! Search for glacier fraction in Extpar file
      !
      IF (has_filetype_netcdf(cdi_filetype)) THEN
        var_id = test_cdi_varID(cdi_extpar_id,'ICE')
      ELSE IF (cdi_filetype == FILETYPE_GRB2) THEN
        var_id = test_cdi_varID(cdi_extpar_id,'FR_ICE')
      ENDIF
      IF (var_id == -1) THEN
        is_frglac_in = .FALSE.
      ELSE
        is_frglac_in = .TRUE.
      ENDIF

      CALL read_vars(cdi_extpar_id)

  END SUBROUTINE inquire_extpar_file

  END MODULE mo_ext_data_inquire

  PROGRAM READ_EXTPAR
    USE  mo_ext_data_inquire
    
    CALL inquire_extpar_file
  END PROGRAM READ_EXTPAR
