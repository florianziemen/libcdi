PROGRAM example

  INCLUDE "cdi.inc"

  INTEGER, PARAMETER :: identificationNumberOfTile = 4
  INTEGER, PARAMETER :: attribute                  = 4

  INTEGER, PARAMETER     :: wp = SELECTED_REAL_KIND(12,307)
  INTEGER                :: streamID, vlistID, varID, subtypeID, gridID, &
    &                       nvars,  tile_index, size, nmiss
  REAL(wp), ALLOCATABLE  :: data_vec(:)
  CHARACTER(LEN=128)     :: name

  WRITE (0,*) "cdi-tiles example"
  ! Register additional GRIB keys which are read when file is opened.
  CALL cdiDefAdditionalKey("tileIndex")
  CALL cdiDefAdditionalKey("tileAttribute")

  streamID = streamOpenRead("NWP_tiles_test.grb")
!  streamID = streamOpenRead("NWP_tiles_test.nc")
  vlistID  = streamInqVlist(streamID)
  nvars    = streamInqNvars(streamID)

  ! loop over variables
  DO varID=0,(nvars-1)
    CALL vlistInqVarName(vlistID, varID, name);
    WRITE (0,*) "VAR ", TRIM(name)
  END DO

  CALL streamClose(streamID)

END PROGRAM example
