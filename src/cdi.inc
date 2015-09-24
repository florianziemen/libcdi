! This file was automatically generated, don't edit!
!
! Fortran interface for CDI library version 1.7.0
!
! Author:
! -------
! Uwe Schulzweida, MPI-MET, Hamburg,   September 2015
!

!                     gridDefUUID
!                                    (INTEGER         gridID,
!                                     INTEGER*1(16)   uuid)
      EXTERNAL        gridDefUUID

!                     gridInqUUID
!                                    (INTEGER         gridID,
!                                     INTEGER*1(16)   uuid)
      EXTERNAL        gridInqUUID

!
!  Lambert Conformal Conic grid (GRIB version)
!
!                     gridDefLCC
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION originLon,
!                                     DOUBLEPRECISION originLat,
!                                     DOUBLEPRECISION lonParY,
!                                     DOUBLEPRECISION lat1,
!                                     DOUBLEPRECISION lat2,
!                                     DOUBLEPRECISION xinc,
!                                     DOUBLEPRECISION yinc,
!                                     INTEGER         projflag,
!                                     INTEGER         scanflag)
      EXTERNAL        gridDefLCC

!                     gridInqLCC
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION originLon,
!                                     DOUBLEPRECISION originLat,
!                                     DOUBLEPRECISION lonParY,
!                                     DOUBLEPRECISION lat1,
!                                     DOUBLEPRECISION lat2,
!                                     DOUBLEPRECISION xinc,
!                                     DOUBLEPRECISION yinc,
!                                     INTEGER         projflag,
!                                     INTEGER         scanflag)
      EXTERNAL        gridInqLCC

!
!  Lambert Conformal Conic 2 grid (PROJ version)
!
!                     gridDefLcc2
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION earth_radius,
!                                     DOUBLEPRECISION lon_0,
!                                     DOUBLEPRECISION lat_0,
!                                     DOUBLEPRECISION lat_1,
!                                     DOUBLEPRECISION lat_2)
      EXTERNAL        gridDefLcc2

!                     gridInqLcc2
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION earth_radius,
!                                     DOUBLEPRECISION lon_0,
!                                     DOUBLEPRECISION lat_0,
!                                     DOUBLEPRECISION lat_1,
!                                     DOUBLEPRECISION lat_2)
      EXTERNAL        gridInqLcc2

!
!  Lambert Azimuthal Equal Area grid
!
!                     gridDefLaea
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION earth_radius,
!                                     DOUBLEPRECISION lon_0,
!                                     DOUBLEPRECISION lat_0)
      EXTERNAL        gridDefLaea

!                     gridInqLaea
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION earth_radius,
!                                     DOUBLEPRECISION lon_0,
!                                     DOUBLEPRECISION lat_0)
      EXTERNAL        gridInqLaea

!                     gridDefArea
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION area(*))
      EXTERNAL        gridDefArea

!                     gridInqArea
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION area(*))
      EXTERNAL        gridInqArea

      INTEGER         gridHasArea
!                                    (INTEGER         gridID)
      EXTERNAL        gridHasArea

!                     gridDefNvertex
!                                    (INTEGER         gridID,
!                                     INTEGER         nvertex)
      EXTERNAL        gridDefNvertex

      INTEGER         gridInqNvertex
!                                    (INTEGER         gridID)
      EXTERNAL        gridInqNvertex

!                     gridDefXbounds
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION xbounds(*))
      EXTERNAL        gridDefXbounds

      INTEGER         gridInqXbounds
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION xbounds(*))
      EXTERNAL        gridInqXbounds

!                     gridDefYbounds
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION ybounds(*))
      EXTERNAL        gridDefYbounds

      INTEGER         gridInqYbounds
!                                    (INTEGER         gridID,
!                                     DOUBLEPRECISION ybounds(*))
      EXTERNAL        gridInqYbounds

!                     gridDefRowlon
!                                    (INTEGER         gridID,
!                                     INTEGER         nrowlon,
!                                     INTEGER         rowlon(*))
      EXTERNAL        gridDefRowlon

!                     gridInqRowlon
!                                    (INTEGER         gridID,
!                                     INTEGER         rowlon(*))
      EXTERNAL        gridInqRowlon

!                     gridChangeType
!                                    (INTEGER         gridID,
!                                     INTEGER         gridtype)
      EXTERNAL        gridChangeType

!                     gridDefComplexPacking
!                                    (INTEGER         gridID,
!                                     INTEGER         lpack)
      EXTERNAL        gridDefComplexPacking

      INTEGER         gridInqComplexPacking
!                                    (INTEGER         gridID)
      EXTERNAL        gridInqComplexPacking

!
!  ZAXIS routines
!
!                     zaxisName
!                                    (INTEGER         zaxistype,
!                                     CHARACTER*(*)   zaxisname)
      EXTERNAL        zaxisName

      INTEGER         zaxisCreate
!                                    (INTEGER         zaxistype,
!                                     INTEGER         size)
      EXTERNAL        zaxisCreate

!                     zaxisDestroy
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisDestroy

      INTEGER         zaxisInqType
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisInqType

      INTEGER         zaxisInqSize
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisInqSize

      INTEGER         zaxisDuplicate
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisDuplicate

!                     zaxisResize
!                                    (INTEGER         zaxisID,
!                                     INTEGER         size)
      EXTERNAL        zaxisResize

!                     zaxisPrint
!                                    (INTEGER         zaxisID,
!                                     INTEGER         index)
      EXTERNAL        zaxisPrint

!                     zaxisDefLevels
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION levels(*))
      EXTERNAL        zaxisDefLevels

!                     zaxisInqLevels
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION levels(*))
      EXTERNAL        zaxisInqLevels

!                     zaxisDefLevel
!                                    (INTEGER         zaxisID,
!                                     INTEGER         levelID,
!                                     DOUBLEPRECISION levels)
      EXTERNAL        zaxisDefLevel

      DOUBLEPRECISION zaxisInqLevel
!                                    (INTEGER         zaxisID,
!                                     INTEGER         levelID)
      EXTERNAL        zaxisInqLevel

!                     zaxisDefNlevRef
!                                    (INTEGER         gridID,
!                                     INTEGER         nhlev)
      EXTERNAL        zaxisDefNlevRef

      INTEGER         zaxisInqNlevRef
!                                    (INTEGER         gridID)
      EXTERNAL        zaxisInqNlevRef

!                     zaxisDefNumber
!                                    (INTEGER         gridID,
!                                     INTEGER         number)
      EXTERNAL        zaxisDefNumber

      INTEGER         zaxisInqNumber
!                                    (INTEGER         gridID)
      EXTERNAL        zaxisInqNumber

!                     zaxisDefUUID
!                                    (INTEGER         zaxisID,
!                                     INTEGER*1(16)   uuid)
      EXTERNAL        zaxisDefUUID

!                     zaxisInqUUID
!                                    (INTEGER         zaxisID,
!                                     INTEGER*1(16)   uuid)
      EXTERNAL        zaxisInqUUID

!                     zaxisDefName
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   name_optional)
      EXTERNAL        zaxisDefName

!                     zaxisInqName
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   name)
      EXTERNAL        zaxisInqName

!                     zaxisDefLongname
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   longname_optional)
      EXTERNAL        zaxisDefLongname

!                     zaxisInqLongname
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   longname)
      EXTERNAL        zaxisInqLongname

!                     zaxisDefUnits
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   units_optional)
      EXTERNAL        zaxisDefUnits

!                     zaxisInqUnits
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   units)
      EXTERNAL        zaxisInqUnits

!                     zaxisInqStdname
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   stdname)
      EXTERNAL        zaxisInqStdname

!                     zaxisDefPsName
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   psname_optional)
      EXTERNAL        zaxisDefPsName

!                     zaxisInqPsName
!                                    (INTEGER         zaxisID,
!                                     CHARACTER*(*)   psname)
      EXTERNAL        zaxisInqPsName

!                     zaxisDefPrec
!                                    (INTEGER         zaxisID,
!                                     INTEGER         prec)
      EXTERNAL        zaxisDefPrec

      INTEGER         zaxisInqPrec
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisInqPrec

!                     zaxisDefPositive
!                                    (INTEGER         zaxisID,
!                                     INTEGER         positive)
      EXTERNAL        zaxisDefPositive

      INTEGER         zaxisInqPositive
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisInqPositive

!                     zaxisDefScalar
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisDefScalar

      INTEGER         zaxisInqScalar
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisInqScalar

!                     zaxisDefLtype
!                                    (INTEGER         zaxisID,
!                                     INTEGER         ltype)
      EXTERNAL        zaxisDefLtype

      INTEGER         zaxisInqLtype
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisInqLtype

!                     zaxisDefVct
!                                    (INTEGER         zaxisID,
!                                     INTEGER         size,
!                                     DOUBLEPRECISION vct(*))
      EXTERNAL        zaxisDefVct

!                     zaxisInqVct
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION vct(*))
      EXTERNAL        zaxisInqVct

      INTEGER         zaxisInqVctSize
!                                    (INTEGER         zaxisID)
      EXTERNAL        zaxisInqVctSize

!                     zaxisDefLbounds
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION lbounds(*))
      EXTERNAL        zaxisDefLbounds

      INTEGER         zaxisInqLbounds
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION lbounds_optional(*))
      EXTERNAL        zaxisInqLbounds

      DOUBLEPRECISION zaxisInqLbound
!                                    (INTEGER         zaxisID,
!                                     INTEGER         index)
      EXTERNAL        zaxisInqLbound

!                     zaxisDefUbounds
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION ubounds(*))
      EXTERNAL        zaxisDefUbounds

      INTEGER         zaxisInqUbounds
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION ubounds_optional(*))
      EXTERNAL        zaxisInqUbounds

      DOUBLEPRECISION zaxisInqUbound
!                                    (INTEGER         zaxisID,
!                                     INTEGER         index)
      EXTERNAL        zaxisInqUbound

!                     zaxisDefWeights
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION weights(*))
      EXTERNAL        zaxisDefWeights

      INTEGER         zaxisInqWeights
!                                    (INTEGER         zaxisID,
!                                     DOUBLEPRECISION weights_optional(*))
      EXTERNAL        zaxisInqWeights

!                     zaxisChangeType
!                                    (INTEGER         zaxisID,
!                                     INTEGER         zaxistype)
      EXTERNAL        zaxisChangeType

!
!  TAXIS routines
!
      INTEGER         taxisCreate
!                                    (INTEGER         timetype)
      EXTERNAL        taxisCreate

!                     taxisDestroy
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisDestroy

      INTEGER         taxisDuplicate
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisDuplicate

!                     taxisCopyTimestep
!                                    (INTEGER         taxisIDdes,
!                                     INTEGER         taxisIDsrc)
      EXTERNAL        taxisCopyTimestep

!                     taxisDefType
!                                    (INTEGER         taxisID,
!                                     INTEGER         type)
      EXTERNAL        taxisDefType

!                     taxisDefVdate
!                                    (INTEGER         taxisID,
!                                     INTEGER         date)
      EXTERNAL        taxisDefVdate

!                     taxisDefVtime
!                                    (INTEGER         taxisID,
!                                     INTEGER         time)
      EXTERNAL        taxisDefVtime

      INTEGER         taxisInqVdate
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqVdate

      INTEGER         taxisInqVtime
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqVtime

!                     taxisDefRdate
!                                    (INTEGER         taxisID,
!                                     INTEGER         date)
      EXTERNAL        taxisDefRdate

!                     taxisDefRtime
!                                    (INTEGER         taxisID,
!                                     INTEGER         time)
      EXTERNAL        taxisDefRtime

      INTEGER         taxisInqRdate
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqRdate

      INTEGER         taxisInqRtime
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqRtime

!                     taxisDefFdate
!                                    (INTEGER         taxisID,
!                                     INTEGER         date)
      EXTERNAL        taxisDefFdate

!                     taxisDefFtime
!                                    (INTEGER         taxisID,
!                                     INTEGER         time)
      EXTERNAL        taxisDefFtime

      INTEGER         taxisInqFdate
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqFdate

      INTEGER         taxisInqFtime
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqFtime

      INTEGER         taxisHasBounds
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisHasBounds

!                     taxisDeleteBounds
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisDeleteBounds

!                     taxisDefVdateBounds
!                                    (INTEGER         taxisID,
!                                     INTEGER         vdate_lb,
!                                     INTEGER         vdate_ub)
      EXTERNAL        taxisDefVdateBounds

!                     taxisDefVtimeBounds
!                                    (INTEGER         taxisID,
!                                     INTEGER         vtime_lb,
!                                     INTEGER         vtime_ub)
      EXTERNAL        taxisDefVtimeBounds

!                     taxisInqVdateBounds
!                                    (INTEGER         taxisID,
!                                     INTEGER         vdate_lb,
!                                     INTEGER         vdate_ub)
      EXTERNAL        taxisInqVdateBounds

!                     taxisInqVtimeBounds
!                                    (INTEGER         taxisID,
!                                     INTEGER         vtime_lb,
!                                     INTEGER         vtime_ub)
      EXTERNAL        taxisInqVtimeBounds

!                     taxisDefCalendar
!                                    (INTEGER         taxisID,
!                                     INTEGER         calendar)
      EXTERNAL        taxisDefCalendar

      INTEGER         taxisInqCalendar
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqCalendar

!                     taxisDefTunit
!                                    (INTEGER         taxisID,
!                                     INTEGER         tunit)
      EXTERNAL        taxisDefTunit

      INTEGER         taxisInqTunit
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqTunit

!                     taxisDefForecastTunit
!                                    (INTEGER         taxisID,
!                                     INTEGER         tunit)
      EXTERNAL        taxisDefForecastTunit

      INTEGER         taxisInqForecastTunit
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqForecastTunit

!                     taxisDefForecastPeriod
!                                    (INTEGER         taxisID,
!                                     DOUBLEPRECISION fc_period)
      EXTERNAL        taxisDefForecastPeriod

      DOUBLEPRECISION taxisInqForecastPeriod
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqForecastPeriod

!                     taxisDefNumavg
!                                    (INTEGER         taxisID,
!                                     INTEGER         numavg)
      EXTERNAL        taxisDefNumavg

      INTEGER         taxisInqType
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqType

      INTEGER         taxisInqNumavg
!                                    (INTEGER         taxisID)
      EXTERNAL        taxisInqNumavg

      CHARACTER(80)   tunitNamePtr
!                                    (INTEGER         tunitID)
      EXTERNAL        tunitNamePtr

!
!  Institut routines
!
      INTEGER         institutDef
!                                    (INTEGER         center,
!                                     INTEGER         subcenter,
!                                     CHARACTER*(*)   name,
!                                     CHARACTER*(*)   longname)
      EXTERNAL        institutDef

      INTEGER         institutInq
!                                    (INTEGER         center,
!                                     INTEGER         subcenter,
!                                     CHARACTER*(*)   name,
!                                     CHARACTER*(*)   longname)
      EXTERNAL        institutInq

      INTEGER         institutInqNumber
      EXTERNAL        institutInqNumber

      INTEGER         institutInqCenter
!                                    (INTEGER         instID)
      EXTERNAL        institutInqCenter

      INTEGER         institutInqSubcenter
!                                    (INTEGER         instID)
      EXTERNAL        institutInqSubcenter

      CHARACTER(80)   institutInqNamePtr
!                                    (INTEGER         instID)
      EXTERNAL        institutInqNamePtr

      CHARACTER(80)   institutInqLongnamePtr
!                                    (INTEGER         instID)
      EXTERNAL        institutInqLongnamePtr

!
!  Model routines
!
      INTEGER         modelDef
!                                    (INTEGER         instID,
!                                     INTEGER         modelgribID,
!                                     CHARACTER*(*)   name)
      EXTERNAL        modelDef

      INTEGER         modelInq
!                                    (INTEGER         instID,
!                                     INTEGER         modelgribID,
!                                     CHARACTER*(*)   name)
      EXTERNAL        modelInq

      INTEGER         modelInqInstitut
!                                    (INTEGER         modelID)
      EXTERNAL        modelInqInstitut

      INTEGER         modelInqGribID
!                                    (INTEGER         modelID)
      EXTERNAL        modelInqGribID

      CHARACTER(80)   modelInqNamePtr
!                                    (INTEGER         modelID)
      EXTERNAL        modelInqNamePtr

!
!  Table routines
!
!                     tableWriteC
!                                    (CHARACTER*(*)   filename,
!                                     INTEGER         tableID)
      EXTERNAL        tableWriteC

!                     tableWrite
!                                    (CHARACTER*(*)   filename,
!                                     INTEGER         tableID)
      EXTERNAL        tableWrite

      INTEGER         tableRead
!                                    (CHARACTER*(*)   tablefile)
      EXTERNAL        tableRead

      INTEGER         tableDef
!                                    (INTEGER         modelID,
!                                     INTEGER         tablenum,
!                                     CHARACTER*(*)   tablename)
      EXTERNAL        tableDef

      CHARACTER(80)   tableInqNamePtr
!                                    (INTEGER         tableID)
      EXTERNAL        tableInqNamePtr

!                     tableDefEntry
!                                    (INTEGER         tableID,
!                                     INTEGER         code,
!                                     CHARACTER*(*)   name,
!                                     CHARACTER*(*)   longname,
!                                     CHARACTER*(*)   units)
      EXTERNAL        tableDefEntry

      INTEGER         tableInq
!                                    (INTEGER         modelID,
!                                     INTEGER         tablenum,
!                                     CHARACTER*(*)   tablename)
      EXTERNAL        tableInq

      INTEGER         tableInqNumber
      EXTERNAL        tableInqNumber

      INTEGER         tableInqNum
!                                    (INTEGER         tableID)
      EXTERNAL        tableInqNum

      INTEGER         tableInqModel
!                                    (INTEGER         tableID)
      EXTERNAL        tableInqModel

!                     tableInqPar
!                                    (INTEGER         tableID,
!                                     INTEGER         code,
!                                     CHARACTER*(*)   name,
!                                     CHARACTER*(*)   longname,
!                                     CHARACTER*(*)   units)
      EXTERNAL        tableInqPar

      INTEGER         tableInqParCode
!                                    (INTEGER         tableID,
!                                     CHARACTER*(*)   name,
!                                     INTEGER         code)
      EXTERNAL        tableInqParCode

      INTEGER         tableInqParName
!                                    (INTEGER         tableID,
!                                     INTEGER         code,
!                                     CHARACTER*(*)   name)
      EXTERNAL        tableInqParName

      INTEGER         tableInqParLongname
!                                    (INTEGER         tableID,
!                                     INTEGER         code,
!                                     CHARACTER*(*)   longname)
      EXTERNAL        tableInqParLongname

      INTEGER         tableInqParUnits
!                                    (INTEGER         tableID,
!                                     INTEGER         code,
!                                     CHARACTER*(*)   units)
      EXTERNAL        tableInqParUnits

      CHARACTER(80)   tableInqParNamePtr
!                                    (INTEGER         tableID,
!                                     INTEGER         parID)
      EXTERNAL        tableInqParNamePtr

      CHARACTER(80)   tableInqParLongnamePtr
!                                    (INTEGER         tableID,
!                                     INTEGER         parID)
      EXTERNAL        tableInqParLongnamePtr

      CHARACTER(80)   tableInqParUnitsPtr
!                                    (INTEGER         tableID,
!                                     INTEGER         parID)
      EXTERNAL        tableInqParUnitsPtr

!
!  History routines
!
!                     streamDefHistory
!                                    (INTEGER         streamID,
!                                     INTEGER         size,
!                                     CHARACTER*(*)   history)
      EXTERNAL        streamDefHistory

      INTEGER         streamInqHistorySize
!                                    (INTEGER         streamID)
      EXTERNAL        streamInqHistorySize

!                     streamInqHistoryString
!                                    (INTEGER         streamID,
!                                     CHARACTER*(*)   history)
      EXTERNAL        streamInqHistoryString

!
!  Subtype routines
!
      INTEGER         subtypeCreate
!                                    (INTEGER         subtype)
      EXTERNAL        subtypeCreate

!
!  Gives a textual summary of the variable subtype
!
!                     subtypePrint
!                                    (INTEGER         subtypeID)
      EXTERNAL        subtypePrint

!
!  Compares two subtype data structures.
!
      INTEGER         subtypeCompare
!                                    (INTEGER         subtypeID1,
!                                     INTEGER         subtypeID2)
      EXTERNAL        subtypeCompare

      INTEGER         subtypeInqSize
!                                    (INTEGER         subtypeID)
      EXTERNAL        subtypeInqSize

      INTEGER         subtypeInqActiveIndex
!                                    (INTEGER         subtypeID)
      EXTERNAL        subtypeInqActiveIndex

!                     subtypeDefActiveIndex
!                                    (INTEGER         subtypeID,
!                                     INTEGER         index)
      EXTERNAL        subtypeDefActiveIndex

!
!  Generate a "query object" out of a key-value pair.
!
!
!  Generate an AND-combined "query object" out of two previous
!  query objects.
!
      INTEGER         subtypeInqTile
!                                    (INTEGER         subtypeID,
!                                     INTEGER         tileindex,
!                                     INTEGER         attribute)
      EXTERNAL        subtypeInqTile

      INTEGER         vlistInqVarSubtype
!                                    (INTEGER         vlistID,
!                                     INTEGER         varID)
      EXTERNAL        vlistInqVarSubtype

!                     gribapiLibraryVersion
!                                    (INTEGER         major_version,
!                                     INTEGER         minor_version,
!                                     INTEGER         revision_version)
      EXTERNAL        gribapiLibraryVersion

