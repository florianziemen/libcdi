#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#include "dmemory.h"
#include "cdi_int.h"
#include "cdi_uuid.h"
#include "stream_cdf.h"
#include "cdf_int.h"
#include "varscan.h"
#include "vlist.h"
#include "zaxis.h"


#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID

#define  POSITIVE_UP    1
#define  POSITIVE_DOWN  2


static const char bndsName[] = "bnds";


void cdfCopyRecord(stream_t *streamptr2, stream_t *streamptr1)
{
  int vlistID1 = streamptr1->vlistID;
  int tsID     = streamptr1->curTsID;
  int vrecID   = streamptr1->tsteps[tsID].curRecID;
  int recID    = streamptr1->tsteps[tsID].recIDs[vrecID];
  int ivarID   = streamptr1->tsteps[tsID].records[recID].varID;
  int gridID   = vlistInqVarGrid(vlistID1, ivarID);
  int datasize = gridInqSize(gridID);
  int datatype = vlistInqVarDatatype(vlistID1, ivarID);
  int memtype  = datatype != DATATYPE_FLT32 ? MEMTYPE_DOUBLE : MEMTYPE_FLOAT;

  void *data
    = Malloc((size_t)datasize
             * (memtype == MEMTYPE_DOUBLE ? sizeof(double) : sizeof(float)));

  int nmiss;
  cdf_read_record(streamptr1, memtype, data, &nmiss);
  cdf_write_record(streamptr2, memtype, data, nmiss);

  Free(data);
}

/* not used
int cdfInqRecord(stream_t *streamptr, int *varID, int *levelID)
{
  int tsID, recID;

  recID = streamptr->tsteps[0].curRecID++;
  printf("cdfInqRecord recID %d %d\n", recID, streamptr->tsteps[0].curRecID);
  printf("cdfInqRecord tsID %d\n", streamptr->curTsID);

  if ( streamptr->tsteps[0].curRecID >= streamptr->tsteps[0].nrecs )
    {
      streamptr->tsteps[0].curRecID = 0;
    }

  *varID   = streamptr->tsteps[0].records[recID].varID;
  *levelID = streamptr->tsteps[0].records[recID].levelID;

  streamptr->record->varID   = *varID;
  streamptr->record->levelID = *levelID;

  if ( CDI_Debug )
    Message("recID = %d  varID = %d  levelID = %d", recID, *varID, *levelID);

  return (recID+1);
}
*/


void cdfDefRecord(stream_t *streamptr)
{
  (void)streamptr;
}

static
void cdfDefTimeValue(stream_t *streamptr, int tsID)
{
  int fileID = streamptr->fileID;

  if ( CDI_Debug )
    Message("streamID = %d, fileID = %d", streamptr->self, fileID);

  taxis_t *taxis = &streamptr->tsteps[tsID].taxis;

  if ( streamptr->ncmode == 1 )
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  size_t index = (size_t)tsID;

  double timevalue = cdiEncodeTimeval(taxis->vdate, taxis->vtime, &streamptr->tsteps[0].taxis);
  if ( CDI_Debug ) Message("tsID = %d  timevalue = %f", tsID, timevalue);

  int ncvarid = streamptr->basetime.ncvarid;
  cdf_put_var1_double(fileID, ncvarid, &index, &timevalue);

  if ( taxis->has_bounds )
    {
      size_t start[2], count[2];

      ncvarid = streamptr->basetime.ncvarboundsid;

      timevalue = cdiEncodeTimeval(taxis->vdate_lb, taxis->vtime_lb, &streamptr->tsteps[0].taxis);
      start[0] = (size_t)tsID; count[0] = 1; start[1] = 0; count[1] = 1;
      cdf_put_vara_double(fileID, ncvarid, start, count, &timevalue);

      timevalue = cdiEncodeTimeval(taxis->vdate_ub, taxis->vtime_ub, &streamptr->tsteps[0].taxis);
      start[0] = (size_t)tsID; count[0] = 1; start[1] = 1; count[1] = 1;
      cdf_put_vara_double(fileID, ncvarid, start, count, &timevalue);
    }

  ncvarid = streamptr->basetime.leadtimeid;
  if ( taxis->type == TAXIS_FORECAST && ncvarid != UNDEFID )
    {
      timevalue = taxis->fc_period;
      cdf_put_var1_double(fileID, ncvarid, &index, &timevalue);
    }

  /*
printf("fileID = %d %d %d %f\n", fileID, time_varid, index, timevalue);
  */
}

static
int cdfDefTimeBounds(int fileID, int nctimevarid, int nctimedimid, const char *taxis_name, taxis_t* taxis)
{
  int time_bndsid = -1;
  int dims[2];

  dims[0] = nctimedimid;

  /* fprintf(stderr, "time has bounds\n"); */

  if ( nc_inq_dimid(fileID, bndsName, &dims[1]) != NC_NOERR )
    cdf_def_dim(fileID, bndsName, 2, &dims[1]);

  const char *bndsAttName, *bndsAttVal;
  size_t bndsAttValLen;
  char tmpstr[CDI_MAX_NAME];
  if ( taxis->climatology )
    {
      static const char climatology_bndsName[] = "climatology_bnds",
        climatology_bndsAttName[] = "climatology";
      bndsAttName = climatology_bndsAttName;
      bndsAttValLen = sizeof (climatology_bndsName) - 1;
      bndsAttVal = climatology_bndsName;
    }
  else
    {
      size_t taxisnameLen = strlen(taxis_name);
      memcpy(tmpstr, taxis_name, taxisnameLen);
      tmpstr[taxisnameLen] = '_';
      memcpy(tmpstr + taxisnameLen + 1, bndsName, sizeof (bndsName));
      size_t tmpstrLen = taxisnameLen + sizeof (bndsName);
      static const char generic_bndsAttName[] = "bounds";
      bndsAttName = generic_bndsAttName;
      bndsAttValLen = tmpstrLen;
      bndsAttVal = tmpstr;
    }
  cdf_def_var(fileID, bndsAttVal, NC_DOUBLE, 2, dims, &time_bndsid);
  cdf_put_att_text(fileID, nctimevarid, bndsAttName, bndsAttValLen, bndsAttVal);

  return time_bndsid;
}

static
void cdfDefTimeUnits(char *unitstr, taxis_t* taxis0, taxis_t* taxis)
{
  unitstr[0] = 0;

  if ( taxis0->type == TAXIS_ABSOLUTE )
    {
      if ( taxis0->unit == TUNIT_YEAR )
        sprintf(unitstr, "year as %s", "%Y.%f");
      else if ( taxis0->unit == TUNIT_MONTH )
        sprintf(unitstr, "month as %s", "%Y%m.%f");
      else
        sprintf(unitstr, "day as %s", "%Y%m%d.%f");
    }
  else
    {
      int timeunit = taxis->unit != -1 ? taxis->unit : TUNIT_HOUR;
      int rdate    = taxis->rdate;
      int rtime    = taxis->rtime;
      if ( rdate == -1 )
        {
          rdate  = taxis->vdate;
          rtime  = taxis->vtime;
        }

      int year, month, day, hour, minute, second;
      cdiDecodeDate(rdate, &year, &month, &day);
      cdiDecodeTime(rtime, &hour, &minute, &second);

      if ( timeunit == TUNIT_QUARTER   ) timeunit = TUNIT_MINUTE;
      if ( timeunit == TUNIT_30MINUTES ) timeunit = TUNIT_MINUTE;
      if ( timeunit == TUNIT_3HOURS  ||
	   timeunit == TUNIT_6HOURS  ||
	   timeunit == TUNIT_12HOURS ) timeunit = TUNIT_HOUR;

      sprintf(unitstr, "%s since %d-%d-%d %02d:%02d:%02d",
              tunitNamePtr(timeunit), year, month, day, hour, minute, second);
    }
}

static
void cdfDefForecastTimeUnits(char *unitstr, int timeunit)
{
  unitstr[0] = 0;

  if ( timeunit == -1 ) timeunit = TUNIT_HOUR;

  if ( timeunit == TUNIT_QUARTER   ) timeunit = TUNIT_MINUTE;
  if ( timeunit == TUNIT_30MINUTES ) timeunit = TUNIT_MINUTE;
  if ( timeunit == TUNIT_3HOURS  ||
       timeunit == TUNIT_6HOURS  ||
       timeunit == TUNIT_12HOURS ) timeunit = TUNIT_HOUR;

  strcpy(unitstr, tunitNamePtr(timeunit));
}

static
void cdfDefCalendar(int fileID, int ncvarid, int calendar)
{
  static const struct { int calCode; const char *calStr; } calTab[] = {
    { CALENDAR_STANDARD, "standard" },
    { CALENDAR_PROLEPTIC, "proleptic_gregorian" },
    { CALENDAR_NONE, "none" },
    { CALENDAR_360DAYS, "360_day" },
    { CALENDAR_365DAYS, "365_day" },
    { CALENDAR_366DAYS, "366_day" },
  };
  enum { calTabSize = sizeof calTab / sizeof calTab[0] };

  for (size_t i = 0; i < calTabSize; ++i)
    if (calTab[i].calCode == calendar)
      {
        const char *calstr = calTab[i].calStr;
        size_t len = strlen(calstr);
        cdf_put_att_text(fileID, ncvarid, "calendar", len, calstr);
        break;
      }
}


void cdfDefTime(stream_t* streamptr)
{
  int time_varid;
  int time_dimid;
  int time_bndsid = -1;
  static const char default_name[] = "time";

  if ( streamptr->basetime.ncvarid != UNDEFID ) return;

  int fileID = streamptr->fileID;

  if ( streamptr->ncmode == 0 ) streamptr->ncmode = 1;
  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  taxis_t *taxis = &streamptr->tsteps[0].taxis;

  const char *taxis_name = (taxis->name && taxis->name[0]) ? taxis->name : default_name ;

  cdf_def_dim(fileID, taxis_name, NC_UNLIMITED, &time_dimid);
  streamptr->basetime.ncdimid = time_dimid;

  cdf_def_var(fileID, taxis_name, NC_DOUBLE, 1, &time_dimid, &time_varid);

  streamptr->basetime.ncvarid = time_varid;

  {
    static const char timeStr[] = "time";
    cdf_put_att_text(fileID, time_varid, "standard_name", sizeof(timeStr) - 1, timeStr);
  }

  if ( taxis->longname && taxis->longname[0] )
    cdf_put_att_text(fileID, time_varid, "long_name", strlen(taxis->longname), taxis->longname);

  if ( taxis->has_bounds )
    {
      time_bndsid = cdfDefTimeBounds(fileID, time_varid, time_dimid, taxis_name, taxis);
      streamptr->basetime.ncvarboundsid = time_bndsid;
    }

  {
    char unitstr[CDI_MAX_NAME];
    cdfDefTimeUnits(unitstr, &streamptr->tsteps[0].taxis, taxis);
    size_t len = strlen(unitstr);
    if ( len )
      {
        cdf_put_att_text(fileID, time_varid, "units", len, unitstr);
        /*
          if ( taxis->has_bounds )
          cdf_put_att_text(fileID, time_bndsid, "units", len, unitstr);
        */
      }
  }

  if ( taxis->calendar != -1 )
    {
      cdfDefCalendar(fileID, time_varid, taxis->calendar);
      /*
      if ( taxis->has_bounds )
        cdfDefCalendar(fileID, time_bndsid, taxis->calendar);
      */
    }

  if ( taxis->type == TAXIS_FORECAST )
    {
      int leadtimeid;

      cdf_def_var(fileID, "leadtime", NC_DOUBLE, 1, &time_dimid, &leadtimeid);

      streamptr->basetime.leadtimeid = leadtimeid;

      {
        static const char stdname[] = "forecast_period";
        cdf_put_att_text(fileID, leadtimeid, "standard_name", sizeof(stdname) - 1, stdname);
      }

      {
        static const char lname[] = "Time elapsed since the start of the forecast";
        cdf_put_att_text(fileID, leadtimeid, "long_name", sizeof(lname) - 1, lname);
      }

      {
          char unitstr[CDI_MAX_NAME];
          cdfDefForecastTimeUnits(unitstr, taxis->fc_unit);
          size_t len = strlen(unitstr);
          if ( len )
            cdf_put_att_text(fileID, leadtimeid, "units", len, unitstr);
      }
    }

  cdf_put_att_text(fileID, time_varid, "axis", 1, "T");

  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
}


void cdfDefTimestep(stream_t *streamptr, int tsID)
{
  int vlistID = streamptr->vlistID;

  if ( vlistHasTime(vlistID) ) cdfDefTime(streamptr);

  cdfDefTimeValue(streamptr, tsID);
}

static
void cdfDefComplex(stream_t *streamptr, int gridID, int gridindex)
{
  int dimID = UNDEFID;
  int fileID  = streamptr->fileID;
  ncgrid_t *ncgrid = streamptr->ncgrid;

  for ( int index = 0; index < gridindex; ++index )
    {
      if ( ncgrid[index].xdimID != UNDEFID )
        {
          int gridID0 = ncgrid[index].gridID;
          int gridtype0 = gridInqType(gridID0);
          if ( gridtype0 == GRID_SPECTRAL || gridtype0 == GRID_FOURIER )
            {
              dimID = ncgrid[index].xdimID;
              break;
            }
        }
    }

  if ( dimID == UNDEFID )
    {
      static const char axisname[] = "nc2";
      size_t dimlen = 2;

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
      cdf_def_dim(fileID, axisname, dimlen, &dimID);
      cdf_enddef(fileID);

      streamptr->ncmode = 2;
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].xdimID = dimID;
}

static void
cdfDefSPorFC(stream_t *streamptr, int gridID, int gridindex,
             char *restrict axisname, int gridRefType)
{
  int index, iz = 0;
  int dimID = UNDEFID;
  ncgrid_t *ncgrid = streamptr->ncgrid;

  size_t dimlen = (size_t)gridInqSize(gridID)/2;

  for ( index = 0; index < gridindex; index++ )
    {
      if ( ncgrid[index].ydimID != UNDEFID )
        {
          int gridID0 = ncgrid[index].gridID;
          int gridtype0 = gridInqType(gridID0);
          if ( gridtype0 == gridRefType )
            {
              size_t dimlen0 = (size_t)gridInqSize(gridID0)/2;
              if ( dimlen == dimlen0 )
                {
                  dimID = ncgrid[index].ydimID;
                  break;
                }
              else
                iz++;
            }
        }
    }

  if ( dimID == UNDEFID )
    {
      int fileID  = streamptr->fileID;
      if ( iz == 0 ) axisname[3] = '\0';
      else           sprintf(&axisname[3], "%1d", iz+1);

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].ydimID = dimID;
}

static
void cdfDefSP(stream_t *streamptr, int gridID, int gridindex)
{
  /*
  char longname[] = "Spherical harmonic coefficient";
  */
  char axisname[5] = "nspX";
  cdfDefSPorFC(streamptr, gridID, gridindex, axisname, GRID_SPECTRAL);
}


static
void cdfDefFC(stream_t *streamptr, int gridID, int gridindex)
{
  char axisname[5] = "nfcX";
  cdfDefSPorFC(streamptr, gridID, gridindex, axisname, GRID_FOURIER);
}

static const struct cdfDefGridAxisInqs {
  int (*axisSize)(int gridID);
  void (*axisName)(int gridID, char *dimname);
  void (*axisStdname)(int gridID, char *dimstdname);
  void (*axisLongname)(int gridID, char *dimlongname);
  void (*axisUnits)(int gridID, char *dimunits);
  double (*axisVal)(int gridID, int index);
  const double *(*axisValsPtr)(int gridID);
  const double *(*axisBoundsPtr)(int gridID);
} gridInqsX = {
  .axisSize = gridInqXsize,
  .axisName = gridInqXname,
  .axisStdname = gridInqXstdname,
  .axisLongname = gridInqXlongname,
  .axisUnits = gridInqXunits,
  .axisVal = gridInqXval,
  .axisValsPtr = gridInqXvalsPtr,
  .axisBoundsPtr = gridInqXboundsPtr,
}, gridInqsY = {
  .axisSize = gridInqYsize,
  .axisName = gridInqYname,
  .axisStdname = gridInqYstdname,
  .axisLongname = gridInqYlongname,
  .axisUnits = gridInqYunits,
  .axisVal = gridInqYval,
  .axisValsPtr = gridInqYvalsPtr,
  .axisBoundsPtr = gridInqYboundsPtr,
}, gridInqsZ = {
  .axisStdname = zaxisInqStdname,
  .axisLongname = zaxisInqLongname,
  .axisUnits = zaxisInqUnits,
};

static void
cdfPutGridStdAtts(int fileID, int ncvarid,
                  int gridID, const struct cdfDefGridAxisInqs *inqs)
{
  size_t len;
  {
    char stdname[CDI_MAX_NAME];
    inqs->axisStdname(gridID, stdname);
    if ( (len = strlen(stdname)) )
      cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);
  }
  {
    char longname[CDI_MAX_NAME];
    inqs->axisLongname(gridID, longname);
    if ( (len = strlen(longname)) )
      cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
  }
  {
    char units[CDI_MAX_NAME];
    inqs->axisUnits(gridID, units);
    if ( (len = strlen(units)) )
      cdf_put_att_text(fileID, ncvarid, "units", len, units);
  }
}

static void
cdfDefTrajLatLon(stream_t *streamptr, int gridID, int gridindex,
                 const struct cdfDefGridAxisInqs *inqs, int dimtype)
{
  nc_type xtype = (gridInqPrec(gridID) == DATATYPE_FLT32) ? NC_FLOAT : NC_DOUBLE;
  ncgrid_t *ncgrid = streamptr->ncgrid;

  int dimlen = inqs->axisSize(gridID);
  if ( dimlen != 1 )
    Error("%c size isn't 1 for %s grid!", dimtype, gridNamePtr(gridInqType(gridID)));

  int ncvarid = UNDEFID;
  if ( dimtype == 'X' )
    ncvarid = ncgrid[gridindex].xdimID;
  else
    ncvarid = ncgrid[gridindex].ydimID;

  if ( ncvarid == UNDEFID )
    {
      int dimNcID = streamptr->basetime.ncvarid;
      int fileID  = streamptr->fileID;
      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      char axisname[CDI_MAX_NAME];
      inqs->axisName(gridID, axisname);
      cdf_def_var(fileID, axisname, xtype, 1, &dimNcID, &ncvarid);
      cdfPutGridStdAtts(fileID, ncvarid, gridID, inqs);
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  ncgrid[gridindex].gridID = gridID;
  if ( dimtype == 'X' )
    ncgrid[gridindex].xdimID = ncvarid; /* var ID for trajectory !!! */
  else
    ncgrid[gridindex].ydimID = ncvarid; /* var ID for trajectory !!! */
}

static
void cdfDefTrajLon(stream_t *streamptr, int gridID, int gridindex)
{
  cdfDefTrajLatLon(streamptr, gridID, gridindex, &gridInqsX, 'X');
}


static
void cdfDefTrajLat(stream_t *streamptr, int gridID, int gridindex)
{
  cdfDefTrajLatLon(streamptr, gridID, gridindex, &gridInqsY, 'Y');
}

static
int checkDimName(int fileID, size_t dimlen, char *dimname)
{
  /* check whether the dimenion name is already defined with the same length */
  unsigned iz = 0;
  int dimid = UNDEFID;
  char name[CDI_MAX_NAME];

  size_t len = strlen(dimname);
  memcpy(name, dimname, len + 1);

  do
    {
      if ( iz ) sprintf(name + len, "_%u", iz+1);

      int dimid0, status = nc_inq_dimid(fileID, name, &dimid0);
      if ( status != NC_NOERR )
        break;
      size_t dimlen0;
      cdf_inq_dimlen(fileID, dimid0, &dimlen0);
      if ( dimlen0 == dimlen )
        {
          dimid = dimid0;
          break;
        }
      iz++;
    }
  while ( iz <= 99 );


  if ( iz ) sprintf(dimname + len, "_%u", iz+1);

  return dimid;
}

static
void checkGridName(char *axisname, int fileID)
{
  int ncdimid;
  char axisname2[CDI_MAX_NAME];

  /* check that the name is not already defined */
  unsigned iz = 0;

  size_t axisnameLen = strlen(axisname);
  memcpy(axisname2, axisname, axisnameLen + 1);
  do
    {
      if ( iz ) sprintf(axisname2 + axisnameLen, "_%u", iz+1);

      int status = nc_inq_varid(fileID, axisname2, &ncdimid);
      if ( status != NC_NOERR ) break;

      ++iz;
    }
  while ( iz <= 99 );

  if ( iz ) sprintf(axisname + axisnameLen, "_%u", iz+1);
}

static
int checkZaxisName(char *axisname, int fileID, int vlistID, int zaxisID, int nzaxis)
{
  char axisname2[CDI_MAX_NAME];

  /* check that the name is not already defined */
  unsigned iz = 0;

  size_t axisnameLen = strlen(axisname);
  memcpy(axisname2, axisname, axisnameLen + 1);
  do
    {
      if ( iz ) sprintf(axisname2 + axisnameLen, "_%u", iz+1);

      int ncdimid, status = nc_inq_varid(fileID, axisname2, &ncdimid);

      if ( status != NC_NOERR )
        {
          if ( iz )
            {
              /* check that the name does not exist for other zaxes */
              for ( int index = 0; index < nzaxis; index++ )
                {
                  int zaxisID0 = vlistZaxis(vlistID, index);
                  if ( zaxisID != zaxisID0 )
                    {
                      const char *axisname0 = zaxisInqNamePtr(zaxisID0);
                      if ( strcmp(axisname0, axisname2) == 0 ) goto nextSuffix;
                    }
                }
            }
          break;
        }
      nextSuffix:
      ++iz;
    }
  while (iz <= 99);


  if ( iz ) sprintf(axisname + axisnameLen, "_%u", iz+1);

  return (int)iz;
}

static void
cdfDefAxisCommon(stream_t *streamptr, int gridID, int gridindex, int ndims,
                 const struct cdfDefGridAxisInqs *gridAxisInq, int dimKey, char axisLetter,
                 void (*finishCyclicBounds)(double *pbounds, size_t dimlen, const double *pvals))
{
  int dimID = UNDEFID;
  int ncvarid = UNDEFID, ncbvarid = UNDEFID;
  int nvdimID = UNDEFID;
  int fileID  = streamptr->fileID;
  size_t dimlen = (size_t)gridAxisInq->axisSize(gridID);
  nc_type xtype = (gridInqPrec(gridID) == DATATYPE_FLT32) ? NC_FLOAT : NC_DOUBLE;
  ncgrid_t *ncgrid = streamptr->ncgrid;

  for ( int index = 0; index < gridindex; ++index )
    {
      int gridID0 = ncgrid[index].gridID;
      assert(gridID0 != UNDEFID);
      int gridtype0 = gridInqType(gridID0);
      if ( gridtype0 == GRID_GAUSSIAN    ||
           gridtype0 == GRID_LONLAT      ||
           gridtype0 == GRID_PROJECTION  ||
           gridtype0 == GRID_CURVILINEAR ||
           gridtype0 == GRID_GENERIC )
        {
          size_t dimlen0 = (size_t)gridAxisInq->axisSize(gridID0);
          if ( dimlen == dimlen0 )
            {
              double (*inqVal)(int gridID, int index) = gridAxisInq->axisVal;
              if ( IS_EQUAL(inqVal(gridID0, 0), inqVal(gridID, 0)) &&
                   IS_EQUAL(inqVal(gridID0, (int)dimlen-1), inqVal(gridID, (int)dimlen-1)) )
                {
                  if ( dimKey == CDI_KEY_XDIMNAME )
                    dimID = ncgrid[index].xdimID;
                  else
                    dimID = ncgrid[index].ydimID;
                  break;
                }
            }
        }
    }

  if ( dimID == UNDEFID )
    {
      const double *pvals = gridAxisInq->axisValsPtr(gridID);

      char axisname[CDI_MAX_NAME];
      gridAxisInq->axisName(gridID, axisname);
      if ( axisname[0] == 0 ) Error("axis name undefined!");
      size_t axisnameLen = strlen(axisname);

      /* enough to append _ plus up to 100 decimal and trailing \0 */
      char extendedAxisname[axisnameLen + 4 + 1];
      memcpy(extendedAxisname, axisname, axisnameLen + 1);
      checkGridName(extendedAxisname, fileID);
      size_t extendedAxisnameLen = axisnameLen + strlen(extendedAxisname + axisnameLen);

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      if ( ndims )
        {
          char dimname[CDI_MAX_NAME+3]; dimname[0] = 0;

          if ( pvals == NULL )
            cdiGridInqKeyStr(gridID, dimKey, CDI_MAX_NAME, dimname);

          if ( dimname[0] == 0 ) strcpy(dimname, extendedAxisname);
          dimID = checkDimName(fileID, dimlen, dimname);

          if ( dimID == UNDEFID ) cdf_def_dim(fileID, dimname, dimlen, &dimID);
        }

      bool gen_bounds = false;
      bool grid_is_cyclic = gridIsCircular(gridID) > 0;
      double *pbounds = NULL;
      if ( pvals )
        {
          cdf_def_var(fileID, extendedAxisname, xtype, ndims, &dimID, &ncvarid);

          cdfPutGridStdAtts(fileID, ncvarid, gridID, gridAxisInq);
          {
            char axisStr[2] = { axisLetter, '\0' };
            cdf_put_att_text(fileID, ncvarid, "axis", 1, axisStr);
          }

          pbounds = (double *)gridAxisInq->axisBoundsPtr(gridID);

          if ( CDI_cmor_mode && grid_is_cyclic && !pbounds )
            {
              gen_bounds = true;
              pbounds = (double*) Malloc(2*dimlen*sizeof(double));
              for ( size_t i = 0; i < dimlen-1; ++i )
                {
                  pbounds[i*2+1]   = (pvals[i] + pvals[i+1])/2;
                  pbounds[(i+1)*2] = (pvals[i] + pvals[i+1])/2;
                }
              finishCyclicBounds(pbounds, dimlen, pvals);
            }
          if ( pbounds )
            {
              size_t nvertex = 2;
              if ( nc_inq_dimid(fileID, bndsName, &nvdimID) != NC_NOERR )
                cdf_def_dim(fileID, bndsName, nvertex, &nvdimID);
            }
          if ( pbounds && nvdimID != UNDEFID )
            {
              char boundsname[extendedAxisnameLen + 1 + sizeof (bndsName)];
              memcpy(boundsname, axisname, extendedAxisnameLen);
              boundsname[extendedAxisnameLen] = '_';
              memcpy(boundsname + extendedAxisnameLen + 1, bndsName, sizeof bndsName);
              int dimIDs[2] = { dimID, nvdimID };
              cdf_def_var(fileID, boundsname, xtype, 2, dimIDs, &ncbvarid);
              cdf_put_att_text(fileID, ncvarid, "bounds", extendedAxisnameLen + sizeof (bndsName), boundsname);
            }
        }

      cdf_enddef(fileID);
      streamptr->ncmode = 2;

      if ( ncvarid  != UNDEFID ) cdf_put_var_double(fileID, ncvarid, pvals);
      if ( ncbvarid != UNDEFID ) cdf_put_var_double(fileID, ncbvarid, pbounds);
      if ( gen_bounds ) Free(pbounds);

      if ( ndims == 0 )
        {
          if ( dimKey == CDI_KEY_XDIMNAME )
            ncgrid[gridindex].xvarID = ncvarid;
          else
            ncgrid[gridindex].yvarID = ncvarid;
        }
    }

  ncgrid[gridindex].gridID = gridID;
  if ( dimKey == CDI_KEY_XDIMNAME )
    ncgrid[gridindex].xdimID = dimID;
  else
    ncgrid[gridindex].ydimID = dimID;
}

static
void finishCyclicXBounds(double *pbounds, size_t dimlen, const double *pvals)
{
  pbounds[0] = (pvals[0] + pvals[dimlen-1]-360)*0.5;
  pbounds[2*dimlen-1] = (pvals[dimlen-1] + pvals[0]+360)*0.5;
}

static
void cdfDefXaxis(stream_t *streamptr, int gridID, int gridindex, int ndims)
{
  cdfDefAxisCommon(streamptr, gridID, gridindex, ndims, &gridInqsX,
                   CDI_KEY_XDIMNAME, 'X', finishCyclicXBounds);
}

static
void finishCyclicYBounds(double *pbounds, size_t dimlen, const double *pvals)
{
  pbounds[0] = copysign(90.0, pvals[0]);
  pbounds[2*dimlen-1] = copysign(90.0, pvals[dimlen-1]);
}

static
void cdfDefYaxis(stream_t *streamptr, int gridID, int gridindex, int ndims)
{
  cdfDefAxisCommon(streamptr, gridID, gridindex, ndims, &gridInqsY,
                   CDI_KEY_YDIMNAME, 'Y', finishCyclicYBounds);
}

static
void cdfGridCompress(int fileID, int ncvarid, int gridsize, int filetype, int comptype)
{
#if  defined  (HAVE_NETCDF4)
  if ( gridsize > 1 && comptype == CDI_COMPRESS_ZIP && (filetype == FILETYPE_NC4 || filetype == FILETYPE_NC4C) )
    {
      nc_def_var_chunking(fileID, ncvarid, NC_CHUNKED, NULL);
      cdfDefVarDeflate(fileID, ncvarid, 1);
    }
#endif
}

static
void cdfDefCurvilinear(stream_t *streamptr, int gridID, int gridindex)
{
  int xdimID = UNDEFID;
  int ydimID = UNDEFID;
  int ncxvarid = UNDEFID, ncyvarid = UNDEFID;
  int ncbxvarid = UNDEFID, ncbyvarid = UNDEFID, ncavarid = UNDEFID;
  nc_type xtype = (gridInqPrec(gridID) == DATATYPE_FLT32) ? NC_FLOAT : NC_DOUBLE;
  ncgrid_t *ncgrid = streamptr->ncgrid;

  int fileID  = streamptr->fileID;

  size_t dimlen = (size_t)gridInqSize(gridID);
  size_t xdimlen = (size_t)gridInqXsize(gridID);
  size_t ydimlen = (size_t)gridInqYsize(gridID);

  for ( int index = 0; index < gridindex; index++ )
    {
      if ( ncgrid[index].xdimID != UNDEFID )
        {
          int gridID0 = ncgrid[index].gridID;
          int gridtype0 = gridInqType(gridID0);
          if ( gridtype0 == GRID_CURVILINEAR )
            {
              size_t dimlen0 = (size_t)gridInqSize(gridID0);
              if ( dimlen == dimlen0 )
                if ( IS_EQUAL(gridInqXval(gridID0, 0), gridInqXval(gridID, 0)) &&
                     IS_EQUAL(gridInqXval(gridID0, (int)dimlen-1), gridInqXval(gridID, (int)dimlen-1)) &&
                     IS_EQUAL(gridInqYval(gridID0, 0), gridInqYval(gridID, 0)) &&
                     IS_EQUAL(gridInqYval(gridID0, (int)dimlen-1), gridInqYval(gridID, (int)dimlen-1)) )
                  {
                    xdimID = ncgrid[index].xdimID;
                    ydimID = ncgrid[index].ydimID;
                    ncxvarid = ncgrid[index].xvarID;
                    ncyvarid = ncgrid[index].yvarID;
                    break;
                  }
            }
        }
    }

  if ( xdimID == UNDEFID || ydimID == UNDEFID )
    {
      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
      {
        char xdimname[CDI_MAX_NAME+3];
        xdimname[0] = 0;
        cdiGridInqKeyStr(gridID, CDI_KEY_XDIMNAME, CDI_MAX_NAME, xdimname);
        if ( xdimname[0] == 0 ) { xdimname[0] = 'x'; xdimname[1] = 0; }
        xdimID = checkDimName(fileID, xdimlen, xdimname);
        if ( xdimID == UNDEFID ) cdf_def_dim(fileID, xdimname, xdimlen, &xdimID);
      }
      {
        char ydimname[CDI_MAX_NAME+3];
        ydimname[0] = 0;
        cdiGridInqKeyStr(gridID, CDI_KEY_YDIMNAME, CDI_MAX_NAME, ydimname);
        if ( ydimname[0] == 0 ) { ydimname[0] = 'y'; ydimname[1] = 0; }
        ydimID = checkDimName(fileID, ydimlen, ydimname);
        if ( ydimID == UNDEFID ) cdf_def_dim(fileID, ydimname, ydimlen, &ydimID);
      }

      int nvdimID = UNDEFID;
      int dimIDs[3];
      if ( gridInqXboundsPtr(gridID) || gridInqYboundsPtr(gridID) )
        {
          char vdimname[CDI_MAX_NAME+3];
          vdimname[0] = 0;
          cdiGridInqKeyStr(gridID, CDI_KEY_VDIMNAME, CDI_MAX_NAME, vdimname);
          if ( vdimname[0] == 0 ) strcpy(vdimname, "nv4");
          size_t nvertex = 4;
          nvdimID = checkDimName(fileID, nvertex, vdimname);
          if ( nvdimID == UNDEFID ) cdf_def_dim(fileID, vdimname, nvertex, &nvdimID);
        }

      dimIDs[0] = ydimID;
      dimIDs[1] = xdimID;
      dimIDs[2] = nvdimID;

      if ( gridInqXvalsPtr(gridID) )
        {
          char xaxisname[CDI_MAX_NAME];
          gridInqXname(gridID, xaxisname);
          checkGridName(xaxisname, fileID);

          cdf_def_var(fileID, xaxisname, xtype, 2, dimIDs, &ncxvarid);
          cdfGridCompress(fileID, ncxvarid, (int)(xdimlen*ydimlen), streamptr->filetype, streamptr->comptype);

          cdfPutGridStdAtts(fileID, ncxvarid, gridID, &gridInqsX);

          /* attribute for Panoply */
          cdf_put_att_text(fileID, ncxvarid, "_CoordinateAxisType", 3, "Lon");

          if ( gridInqXboundsPtr(gridID) && nvdimID != UNDEFID )
            {
              size_t xaxisnameLen = strlen(xaxisname);
              xaxisname[xaxisnameLen] = '_';
              memcpy(xaxisname + xaxisnameLen + 1, bndsName, sizeof (bndsName));
              cdf_def_var(fileID, xaxisname, xtype, 3, dimIDs, &ncbxvarid);
              cdfGridCompress(fileID, ncbxvarid, (int)(xdimlen*ydimlen), streamptr->filetype, streamptr->comptype);

              cdf_put_att_text(fileID, ncxvarid, "bounds", xaxisnameLen + sizeof (bndsName), xaxisname);
            }
        }

      if ( gridInqYvalsPtr(gridID) )
        {
          char yaxisname[CDI_MAX_NAME];
          gridInqYname(gridID, yaxisname);
          checkGridName(yaxisname, fileID);

          cdf_def_var(fileID, yaxisname, xtype, 2, dimIDs, &ncyvarid);
          cdfGridCompress(fileID, ncyvarid, (int)(xdimlen*ydimlen), streamptr->filetype, streamptr->comptype);

          cdfPutGridStdAtts(fileID, ncyvarid, gridID, &gridInqsY);

          /* attribute for Panoply */
          cdf_put_att_text(fileID, ncyvarid, "_CoordinateAxisType", 3, "Lat");

          if ( gridInqYboundsPtr(gridID) && nvdimID != UNDEFID )
            {
              size_t yaxisnameLen = strlen(yaxisname);
              yaxisname[yaxisnameLen] = '_';
              memcpy(yaxisname + yaxisnameLen + 1, bndsName, sizeof (bndsName));
              cdf_def_var(fileID, yaxisname, xtype, 3, dimIDs, &ncbyvarid);
              cdfGridCompress(fileID, ncbyvarid, (int)(xdimlen*ydimlen), streamptr->filetype, streamptr->comptype);

              cdf_put_att_text(fileID, ncyvarid, "bounds", yaxisnameLen + sizeof (bndsName), yaxisname);
            }
        }

      if ( gridInqAreaPtr(gridID) )
        {
          static const char yaxisname_[] = "cell_area";
          static const char units[] = "m2";
          static const char longname[] = "area of grid cell";
          static const char stdname[] = "cell_area";

          cdf_def_var(fileID, yaxisname_, xtype, 2, dimIDs, &ncavarid);

          cdf_put_att_text(fileID, ncavarid, "standard_name", sizeof (stdname) - 1, stdname);
          cdf_put_att_text(fileID, ncavarid, "long_name", sizeof (longname) - 1, longname);
          cdf_put_att_text(fileID, ncavarid, "units", sizeof (units) - 1, units);
        }

      cdf_enddef(fileID);
      streamptr->ncmode = 2;

      if ( ncxvarid  != UNDEFID ) cdf_put_var_double(fileID, ncxvarid,  gridInqXvalsPtr(gridID));
      if ( ncbxvarid != UNDEFID ) cdf_put_var_double(fileID, ncbxvarid, gridInqXboundsPtr(gridID));
      if ( ncyvarid  != UNDEFID ) cdf_put_var_double(fileID, ncyvarid,  gridInqYvalsPtr(gridID));
      if ( ncbyvarid != UNDEFID ) cdf_put_var_double(fileID, ncbyvarid, gridInqYboundsPtr(gridID));
      if ( ncavarid  != UNDEFID ) cdf_put_var_double(fileID, ncavarid,  gridInqAreaPtr(gridID));
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].xdimID = xdimID;
  ncgrid[gridindex].ydimID = ydimID;
  ncgrid[gridindex].xvarID = ncxvarid;
  ncgrid[gridindex].yvarID = ncyvarid;
  ncgrid[gridindex].avarID = ncavarid;
}

static
void cdfDefRgrid(stream_t *streamptr, int gridID, int gridindex)
{
  ncgrid_t *ncgrid = streamptr->ncgrid;
  int dimID = UNDEFID;

  size_t dimlen = (size_t)gridInqSize(gridID);

  int iz = 0;
  for ( int index = 0; index < gridindex; index++ )
    {
      if ( ncgrid[index].xdimID != UNDEFID )
        {
          int gridID0 = ncgrid[index].gridID;
          int gridtype0 = gridInqType(gridID0);
          if ( gridtype0 == GRID_GAUSSIAN_REDUCED )
            {
              size_t dimlen0 = (size_t)gridInqSize(gridID0);

              if ( dimlen == dimlen0 )
                {
                  dimID = ncgrid[index].xdimID;
                  break;
                }
              iz++;
            }
        }
    }

  if ( dimID == UNDEFID )
    {
      int fileID  = streamptr->fileID;
      static bool lwarn = true;
      if ( lwarn )
        {
          Warning("Creating a NetCDF file with data on a gaussian reduced grid.");
          Warning("The further processing of the resulting file is unsupported!");
          lwarn = false;
        }

      char axisname[7] = "rgridX";
      if ( iz == 0 ) axisname[5] = '\0';
      else           sprintf(&axisname[5], "%1d", iz+1);

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].xdimID = dimID;
}

static
void cdfDefGdim(stream_t *streamptr, int gridID, int gridindex)
{
  ncgrid_t *ncgrid = streamptr->ncgrid;
  int iz = 0;
  int dimID = UNDEFID;

  size_t dimlen = (size_t)gridInqSize(gridID);

  if ( gridInqYsize(gridID) == 0 )
    for ( int index = 0; index < gridindex; index++ )
      {
        if ( ncgrid[index].xdimID != UNDEFID )
          {
            int gridID0 = ncgrid[index].gridID;
            int gridtype0 = gridInqType(gridID0);
            if ( gridtype0 == GRID_GENERIC )
              {
                size_t dimlen0 = (size_t)gridInqSize(gridID0);
                if ( dimlen == dimlen0 )
                  {
                    dimID = ncgrid[index].xdimID;
                    break;
                  }
                else
                  iz++;
              }
          }
      }

  if ( gridInqXsize(gridID) == 0 )
    for ( int index = 0; index < gridindex; index++ )
      {
        if ( ncgrid[index].ydimID != UNDEFID )
          {
            int gridID0 = ncgrid[index].gridID;
            int gridtype0 = gridInqType(gridID0);
            if ( gridtype0 == GRID_GENERIC )
              {
                size_t dimlen0 = (size_t)gridInqSize(gridID0);
                if ( dimlen == dimlen0 )
                  {
                    dimID = ncgrid[index].ydimID;
                    break;
                  }
                else
                  iz++;
              }
          }
      }

  if ( dimID == UNDEFID )
    {
      int fileID  = streamptr->fileID;
      char dimname[CDI_MAX_NAME];
      strcpy(dimname, "gsize");

      dimID = checkDimName(fileID, dimlen, dimname);

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      if ( dimID == UNDEFID ) cdf_def_dim(fileID, dimname, dimlen, &dimID);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].xdimID = dimID;
}

static
void cdfDefGridReference(stream_t *streamptr, int gridID)
{
  int fileID  = streamptr->fileID;
  int number = gridInqNumber(gridID);

  if ( number > 0 )
    {
      cdf_put_att_int(fileID, NC_GLOBAL, "number_of_grid_used", NC_INT, 1, &number);
    }

  const char *gridfile = gridInqReferencePtr(gridID);
  if ( gridfile && gridfile[0] != 0 )
    cdf_put_att_text(fileID, NC_GLOBAL, "grid_file_uri", strlen(gridfile), gridfile);
}

static
void cdfDefGridUUID(stream_t *streamptr, int gridID)
{
  unsigned char uuidOfHGrid[CDI_UUID_SIZE];

  gridInqUUID(gridID, uuidOfHGrid);
  if ( !cdiUUIDIsNull(uuidOfHGrid) )
    {
      char uuidOfHGridStr[37];
      cdiUUID2Str(uuidOfHGrid, uuidOfHGridStr);
      if ( uuidOfHGridStr[0] != 0 && strlen(uuidOfHGridStr) == 36 )
        {
          int fileID  = streamptr->fileID;
          //if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
          cdf_put_att_text(fileID, NC_GLOBAL, "uuidOfHGrid", 36, uuidOfHGridStr);
          //if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
        }
    }
}

static
void cdfDefZaxisUUID(stream_t *streamptr, int zaxisID)
{
  unsigned char uuidOfVGrid[CDI_UUID_SIZE];
  zaxisInqUUID(zaxisID, uuidOfVGrid);

  if ( uuidOfVGrid[0] != 0 )
    {
      char uuidOfVGridStr[37];
      cdiUUID2Str(uuidOfVGrid, uuidOfVGridStr);
      if ( uuidOfVGridStr[0] != 0 && strlen(uuidOfVGridStr) == 36 )
        {
          int fileID  = streamptr->fileID;
          if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
          cdf_put_att_text(fileID, NC_GLOBAL, "uuidOfVGrid", 36, uuidOfVGridStr);
          if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
        }
    }
}

static
void cdfDefUnstructured(stream_t *streamptr, int gridID, int gridindex)
{
  int dimID = UNDEFID;
  int ncxvarid = UNDEFID, ncyvarid = UNDEFID;
  int ncbxvarid = UNDEFID, ncbyvarid = UNDEFID, ncavarid = UNDEFID;
  int nvdimID = UNDEFID;
  nc_type xtype = (gridInqPrec(gridID) == DATATYPE_FLT32) ? NC_FLOAT : NC_DOUBLE;
  ncgrid_t *ncgrid = streamptr->ncgrid;

  int fileID  = streamptr->fileID;

  size_t dimlen = (size_t)gridInqSize(gridID);

  for ( int index = 0; index < gridindex; index++ )
    {
      if ( ncgrid[index].xdimID != UNDEFID )
        {
          int gridID0 = ncgrid[index].gridID;
          int gridtype0 = gridInqType(gridID0);
          if ( gridtype0 == GRID_UNSTRUCTURED )
            {
              size_t dimlen0 = (size_t)gridInqSize(gridID0);
              if ( dimlen == dimlen0 )
		if ( gridInqNvertex(gridID0) == gridInqNvertex(gridID) &&
		     IS_EQUAL(gridInqXval(gridID0, 0), gridInqXval(gridID, 0)) &&
                     IS_EQUAL(gridInqXval(gridID0, (int)dimlen-1), gridInqXval(gridID, (int)dimlen-1)) &&
		     IS_EQUAL(gridInqYval(gridID0, 0), gridInqYval(gridID, 0)) &&
                     IS_EQUAL(gridInqYval(gridID0, (int)dimlen-1), gridInqYval(gridID, (int)dimlen-1)) )
		  {
		    dimID = ncgrid[index].xdimID;
                    ncxvarid = ncgrid[index].xvarID;
                    ncyvarid = ncgrid[index].yvarID;
                    ncavarid = ncgrid[index].avarID;
		    break;
		  }
            }
        }
    }

  if ( dimID == UNDEFID )
    {
      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
      {
        char xdimname[CDI_MAX_NAME+3];
        xdimname[0] = 0;
        cdiGridInqKeyStr(gridID, CDI_KEY_XDIMNAME, CDI_MAX_NAME, xdimname);
        if ( xdimname[0] == 0 ) strcpy(xdimname, "ncells");
        dimID = checkDimName(fileID, dimlen, xdimname);
        if ( dimID == UNDEFID ) cdf_def_dim(fileID, xdimname, dimlen, &dimID);
      }

      size_t nvertex = (size_t)gridInqNvertex(gridID);
      if ( nvertex > 0 )
        {
          char vdimname[CDI_MAX_NAME+3];
          vdimname[0] = 0;
          cdiGridInqKeyStr(gridID, CDI_KEY_VDIMNAME, CDI_MAX_NAME, vdimname);
          if ( vdimname[0] == 0 ) strcpy(vdimname, "vertices");
          nvdimID = checkDimName(fileID, nvertex, vdimname);
          if ( nvdimID == UNDEFID ) cdf_def_dim(fileID, vdimname, nvertex, &nvdimID);
        }

      cdfDefGridReference(streamptr, gridID);

      cdfDefGridUUID(streamptr, gridID);

      if ( gridInqXvalsPtr(gridID) )
        {
          char xaxisname[CDI_MAX_NAME];
          gridInqXname(gridID, xaxisname);
          checkGridName(xaxisname, fileID);
          cdf_def_var(fileID, xaxisname, xtype, 1, &dimID, &ncxvarid);
          cdfGridCompress(fileID, ncxvarid, (int)dimlen, streamptr->filetype, streamptr->comptype);

          cdfPutGridStdAtts(fileID, ncxvarid, gridID, &gridInqsX);

          if ( gridInqXboundsPtr(gridID) && nvdimID != UNDEFID )
            {
              int dimIDs[2] = { dimID, nvdimID };
              size_t xaxisnameLen = strlen(xaxisname);
              xaxisname[xaxisnameLen] = '_';
              memcpy(xaxisname + xaxisnameLen + 1, bndsName, sizeof (bndsName));
              cdf_def_var(fileID, xaxisname, xtype, 2, dimIDs, &ncbxvarid);
              cdfGridCompress(fileID, ncbxvarid, (int)dimlen, streamptr->filetype, streamptr->comptype);

              cdf_put_att_text(fileID, ncxvarid, "bounds", xaxisnameLen + sizeof (bndsName), xaxisname);
            }
        }

      if ( gridInqYvalsPtr(gridID) )
        {
          char yaxisname[CDI_MAX_NAME];
          gridInqYname(gridID, yaxisname);
          checkGridName(yaxisname, fileID);
          cdf_def_var(fileID, yaxisname, xtype, 1, &dimID, &ncyvarid);
          cdfGridCompress(fileID, ncyvarid, (int)dimlen, streamptr->filetype, streamptr->comptype);

          cdfPutGridStdAtts(fileID, ncyvarid, gridID, &gridInqsY);

          if ( gridInqYboundsPtr(gridID) && nvdimID != UNDEFID )
            {
              int dimIDs[2] = { dimID, nvdimID };
              size_t yaxisnameLen = strlen(yaxisname);
              yaxisname[yaxisnameLen] = '_';
              memcpy(yaxisname + yaxisnameLen + 1, bndsName, sizeof (bndsName));
              cdf_def_var(fileID, yaxisname, xtype, 2, dimIDs, &ncbyvarid);
              cdfGridCompress(fileID, ncbyvarid, (int)dimlen, streamptr->filetype, streamptr->comptype);

              cdf_put_att_text(fileID, ncyvarid, "bounds", yaxisnameLen + sizeof (bndsName), yaxisname);
            }
        }

      if ( gridInqAreaPtr(gridID) )
        {
          static const char yaxisname_[] = "cell_area";
          static const char units[] = "m2";
          static const char longname[] = "area of grid cell";
          static const char stdname[] = "cell_area";

          cdf_def_var(fileID, yaxisname_, xtype, 1, &dimID, &ncavarid);

          cdf_put_att_text(fileID, ncavarid, "standard_name", sizeof (stdname) - 1, stdname);
          cdf_put_att_text(fileID, ncavarid, "long_name", sizeof (longname) - 1, longname);
          cdf_put_att_text(fileID, ncavarid, "units", sizeof (units) - 1, units);
        }

      cdf_enddef(fileID);
      streamptr->ncmode = 2;

      if ( ncxvarid  != UNDEFID ) cdf_put_var_double(fileID, ncxvarid,  gridInqXvalsPtr(gridID));
      if ( ncbxvarid != UNDEFID ) cdf_put_var_double(fileID, ncbxvarid, gridInqXboundsPtr(gridID));
      if ( ncyvarid  != UNDEFID ) cdf_put_var_double(fileID, ncyvarid,  gridInqYvalsPtr(gridID));
      if ( ncbyvarid != UNDEFID ) cdf_put_var_double(fileID, ncbyvarid, gridInqYboundsPtr(gridID));
      if ( ncavarid  != UNDEFID ) cdf_put_var_double(fileID, ncavarid,  gridInqAreaPtr(gridID));
    }

  ncgrid[gridindex].gridID = gridID;
  ncgrid[gridindex].xdimID = dimID;
  ncgrid[gridindex].xvarID = ncxvarid;
  ncgrid[gridindex].yvarID = ncyvarid;
  ncgrid[gridindex].avarID = ncavarid;
}

struct attTxtTab2
{
  const char *attName, *attVal;
  size_t valLen;
};

static
void cdf_def_vct_echam(stream_t *streamptr, int zaxisID)
{
  int type = zaxisInqType(zaxisID);

  if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
    {
      int ilev = zaxisInqVctSize(zaxisID)/2;
      if ( ilev == 0 ) return;

      int mlev = ilev - 1;
      size_t start;
      size_t count = 1;
      int ncdimid, ncdimid2;
      int hyaiid, hybiid, hyamid, hybmid;
      double mval;

      if ( streamptr->vct.ilev > 0 )
        {
          if ( streamptr->vct.ilev != ilev )
            Error("more than one VCT for each file unsupported!");
          return;
        }

      int fileID = streamptr->fileID;

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, "nhym", (size_t)mlev, &ncdimid);
      cdf_def_dim(fileID, "nhyi", (size_t)ilev, &ncdimid2);

      streamptr->vct.mlev   = mlev;
      streamptr->vct.ilev   = ilev;
      streamptr->vct.mlevID = ncdimid;
      streamptr->vct.ilevID = ncdimid2;

      cdf_def_var(fileID, "hyai", NC_DOUBLE, 1, &ncdimid2, &hyaiid);
      cdf_def_var(fileID, "hybi", NC_DOUBLE, 1, &ncdimid2, &hybiid);
      cdf_def_var(fileID, "hyam", NC_DOUBLE, 1, &ncdimid,  &hyamid);
      cdf_def_var(fileID, "hybm", NC_DOUBLE, 1, &ncdimid,  &hybmid);

      {
        static const char lname_n[] = "long_name",
          lname_v_ai[] = "hybrid A coefficient at layer interfaces",
          units_n[] = "units",
          units_v_ai[] = "Pa",
          lname_v_bi[] = "hybrid B coefficient at layer interfaces",
          units_v_bi[] = "1",
          lname_v_am[] = "hybrid A coefficient at layer midpoints",
          units_v_am[] = "Pa",
          lname_v_bm[] = "hybrid B coefficient at layer midpoints",
          units_v_bm[] = "1";
        static const struct attTxtTab2 tab[]
          = {
          { lname_n, lname_v_ai, sizeof (lname_v_ai) - 1 },
          { units_n, units_v_ai, sizeof (units_v_ai) - 1 },
          { lname_n, lname_v_bi, sizeof (lname_v_bi) - 1 },
          { units_n, units_v_bi, sizeof (units_v_bi) - 1 },
          { lname_n, lname_v_am, sizeof (lname_v_am) - 1 },
          { units_n, units_v_am, sizeof (units_v_am) - 1 },
          { lname_n, lname_v_bm, sizeof (lname_v_bm) - 1 },
          { units_n, units_v_bm, sizeof (units_v_bm) - 1 },
        };
        enum { tabLen = sizeof (tab) / sizeof (tab[0]) };
        int ids[tabLen] = { hyaiid, hyaiid, hybiid, hybiid,
                            hyamid, hyamid, hybmid, hybmid };
        for ( size_t i = 0; i < tabLen; ++i )
          cdf_put_att_text(fileID, ids[i], tab[i].attName, tab[i].valLen, tab[i].attVal);
      }

      cdf_enddef(fileID);
      streamptr->ncmode = 2;

      const double *vctptr = zaxisInqVctPtr(zaxisID);

      cdf_put_var_double(fileID, hyaiid, vctptr);
      cdf_put_var_double(fileID, hybiid, vctptr+ilev);

      for ( int i = 0; i < mlev; i++ )
        {
          start = (size_t)i;
          mval = (vctptr[i] + vctptr[i+1]) * 0.5;
          cdf_put_vara_double(fileID, hyamid, &start, &count, &mval);
          mval = (vctptr[ilev+i] + vctptr[ilev+i+1]) * 0.5;
          cdf_put_vara_double(fileID, hybmid, &start, &count, &mval);
        }
    }
}

static
void cdf_def_vct_cf(stream_t *streamptr, int zaxisID, int nclevID, int ncbndsID)
{
  int type = zaxisInqType(zaxisID);

  if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
    {
      int ilev = zaxisInqVctSize(zaxisID)/2;
      if ( ilev == 0 ) return;

      int mlev = ilev - 1;
      int hyaiid = 0, hybiid = 0, hyamid, hybmid;

      if ( streamptr->vct.ilev > 0 )
        {
          if ( streamptr->vct.ilev != ilev )
            Error("more than one VCT for each file unsupported!");
          return;
        }

      int fileID = streamptr->fileID;

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      int dimIDs[2];
      dimIDs[0] = nclevID;
      dimIDs[1] = ncbndsID;

      streamptr->vct.mlev   = mlev;
      streamptr->vct.ilev   = ilev;
      streamptr->vct.mlevID = nclevID;
      streamptr->vct.ilevID = nclevID;

      cdf_def_var(fileID, "ap", NC_DOUBLE, 1, dimIDs,  &hyamid);
      cdf_def_var(fileID, "b",  NC_DOUBLE, 1, dimIDs,  &hybmid);

      {
        static const char lname[] = "vertical coordinate formula term: ap(k)";
        cdf_put_att_text(fileID, hyamid, "long_name", sizeof (lname) - 1, lname);
      }
      {
        static const char units[] = "Pa";
        cdf_put_att_text(fileID, hyamid, "units", sizeof (units) - 1, units);
      }
      {
        static const char lname[] = "vertical coordinate formula term: b(k)";
        cdf_put_att_text(fileID, hybmid, "long_name", sizeof (lname) - 1, lname);
      }
      {
        static const char units[] = "1";
        cdf_put_att_text(fileID, hybmid, "units", sizeof (units) - 1, units);
      }

      if ( ncbndsID != -1 )
        {
          cdf_def_var(fileID, "ap_bnds", NC_DOUBLE, 2, dimIDs, &hyaiid);
          cdf_def_var(fileID, "b_bnds",  NC_DOUBLE, 2, dimIDs, &hybiid);
          {
            static const char lname[] = "vertical coordinate formula term: ap(k+1/2)";
            cdf_put_att_text(fileID, hyaiid, "long_name", sizeof (lname) - 1, lname);
          }
          {
            static const char units[] = "Pa";
            cdf_put_att_text(fileID, hyaiid, "units", sizeof (units) - 1, units);
          }
          {
            static const char lname[] = "vertical coordinate formula term: b(k+1/2)";
            cdf_put_att_text(fileID, hybiid, "long_name", sizeof (lname) - 1, lname);
          }
          {
            static const char units[] = "1";
            cdf_put_att_text(fileID, hybiid, "units", sizeof (units) - 1, units);
          }
        }

      cdf_enddef(fileID);
      streamptr->ncmode = 2;

      const double *vctptr = zaxisInqVctPtr(zaxisID);
      double tarray[ilev*2];

      if ( ncbndsID != -1 )
        {
          for ( int i = 0; i < mlev; ++i )
            {
              tarray[2*i  ] = vctptr[i];
              tarray[2*i+1] = vctptr[i+1];
            }
          cdf_put_var_double(fileID, hyaiid, tarray);

          for ( int i = 0; i < mlev; ++i )
            {
              tarray[2*i  ] = vctptr[ilev+i];
              tarray[2*i+1] = vctptr[ilev+i+1];
            }
          cdf_put_var_double(fileID, hybiid, tarray);
        }

      for ( int i = 0; i < mlev; ++i )
        tarray[i] = (vctptr[i] + vctptr[i+1]) * 0.5;
      cdf_put_var_double(fileID, hyamid, tarray);

      for ( int i = 0; i < mlev; ++i )
        tarray[i] = (vctptr[ilev+i] + vctptr[ilev+i+1]) * 0.5;
      cdf_put_var_double(fileID, hybmid, tarray);
    }
}

struct attTxtTab { const char *txt; size_t txtLen; };

static
void cdf_def_zaxis_hybrid_echam(stream_t *streamptr, int type, int ncvarid, int zaxisID, int zaxisindex, int xtype, size_t dimlen, int *dimID, char *axisname)
{
  int fileID  = streamptr->fileID;

  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  cdf_def_dim(fileID, axisname, dimlen, dimID);
  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, dimID,  &ncvarid);

  {
    static const char sname[] = "hybrid_sigma_pressure";
    cdf_put_att_text(fileID, ncvarid, "standard_name", sizeof (sname) - 1, sname);
  }
  {
    static const char *attName[] = {
      "long_name",
      "formula",
      "formula_terms"
    };
    enum { nAtt = sizeof (attName) / sizeof (attName[0]) };
    static const char lname_m[] = "hybrid level at layer midpoints",
      formula_m[] = "hyam hybm (mlev=hyam+hybm*aps)",
      fterms_m[] = "ap: hyam b: hybm ps: aps",
      lname_i[] = "hybrid level at layer interfaces",
      formula_i[] = "hyai hybi (ilev=hyai+hybi*aps)",
      fterms_i[] = "ap: hyai b: hybi ps: aps";
    static const struct attTxtTab tab[2][nAtt] = {
      {
        { lname_i, sizeof (lname_i) - 1 },
        { formula_i, sizeof (formula_i) - 1 },
        { fterms_i, sizeof (fterms_i) - 1 }
      },
      {
        { lname_m, sizeof (lname_m) - 1 },
        { formula_m, sizeof (formula_m) - 1 },
        { fterms_m, sizeof (fterms_m) - 1 }
      }
    };

    size_t tabSelect = type == ZAXIS_HYBRID;
    for (size_t i = 0; i < nAtt; ++i)
      cdf_put_att_text(fileID, ncvarid, attName[i],
                       tab[tabSelect][i].txtLen, tab[tabSelect][i].txt);
  }

  {
    static const char units[] = "level";
    cdf_put_att_text(fileID, ncvarid, "units", sizeof (units) - 1, units);
  }
  {
    static const char direction[] = "down";
    cdf_put_att_text(fileID, ncvarid, "positive", sizeof (direction) - 1, direction);
  }

  cdf_enddef(fileID);
  streamptr->ncmode = 2;

  cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));

  cdf_def_vct_echam(streamptr, zaxisID);

  if ( *dimID == UNDEFID )
    {
      if ( type == ZAXIS_HYBRID )
        streamptr->zaxisID[zaxisindex] = streamptr->vct.mlevID;
      else
        streamptr->zaxisID[zaxisindex] = streamptr->vct.ilevID;
    }
}

static
void cdf_def_zaxis_hybrid_cf(stream_t *streamptr, int type, int ncvarid, int zaxisID, int zaxisindex, int xtype, size_t dimlen, int *dimID, char *axisname)
{
  char psname[CDI_MAX_NAME];
  psname[0] = 0;
  zaxisInqPsName(zaxisID, psname);
  if ( psname[0] == 0 ) strcpy(psname, "ps");

  int fileID = streamptr->fileID;
  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  strcpy(axisname, "lev");

  cdf_def_dim(fileID, axisname, dimlen, dimID);
  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, dimID,  &ncvarid);

  {
    static const char sname[] = "standard_name",
      sname_v[] = "atmosphere_hybrid_sigma_pressure_coordinate",
      lname[] = "long_name",
      lname_v[] = "hybrid sigma pressure coordinate",
      formula[] = "formula",
      formula_v[] = "p = ap + b*ps",
      fterms[] = "formula_terms",
      fterms_v[] = "ap: ap b: b ps: ",
      units[] = "units",
      units_v[] = "1",
      axis[] = "axis",
      axis_v[] = "Z",
      direction[] = "positive",
      direction_v[] = "down";
    struct attTxtTab2 tab[] = {
      { sname, sname_v, sizeof (sname_v) - 1 },
      { lname, lname_v, sizeof (lname_v) - 1 },
      { formula, formula_v, sizeof (formula_v) - 1 },
      { fterms, fterms_v, sizeof (fterms_v) - 1 },
      { units, units_v, sizeof (units_v) - 1 },
      { axis, axis_v, sizeof (axis_v) - 1 },
      { direction, direction_v, sizeof (direction_v) - 1 },
    };
    enum { nAtt = sizeof (tab) / sizeof (tab[0]) };
    for (size_t i = 0; i < nAtt; ++i)
      cdf_put_att_text(fileID, ncvarid, tab[i].attName, tab[i].valLen, tab[i].attVal);
  }

  int ncbvarid = UNDEFID;
  int nvdimID = UNDEFID;

  double lbounds[dimlen], ubounds[dimlen], levels[dimlen];

  zaxisInqLevels(zaxisID, levels);

  if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
    {
      zaxisInqLbounds(zaxisID, lbounds);
      zaxisInqUbounds(zaxisID, ubounds);
    }
  else
    {
      for ( size_t i = 0; i < dimlen; ++i ) lbounds[i] = levels[i];
      for ( size_t i = 0; i < dimlen-1; ++i ) ubounds[i] = levels[i+1];
      ubounds[dimlen-1] = levels[dimlen-1] + 1;
    }

  //if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
    {
      size_t nvertex = 2;
      if ( nc_inq_dimid(fileID, bndsName, &nvdimID) != NC_NOERR )
        cdf_def_dim(fileID, bndsName, nvertex, &nvdimID);

      if ( nvdimID != UNDEFID )
        {
          size_t axisnameLen = strlen(axisname);
          axisname[axisnameLen] = '_';
          memcpy(axisname + axisnameLen + 1, bndsName, sizeof (bndsName));
          axisnameLen += sizeof (bndsName);
          int dimIDs[2] = { *dimID, nvdimID };
          cdf_def_var(fileID, axisname, (nc_type) xtype, 2, dimIDs, &ncbvarid);
          cdf_put_att_text(fileID, ncvarid, "bounds", axisnameLen, axisname);
          {
            static const char sname[] = "standard_name",
              sname_v[] = "atmosphere_hybrid_sigma_pressure_coordinate",
              formula[] = "formula",
              formula_v[] = "p = ap + b*ps";
            struct attTxtTab2 tab[] = {
              { sname, sname_v, sizeof (sname_v) - 1 },
              { formula, formula_v, sizeof (formula_v) - 1 },
            };
            enum { nAtt = sizeof (tab) / sizeof (tab[0]) };
            for (size_t i = 0; i < nAtt; ++i)
              cdf_put_att_text(fileID, ncbvarid, tab[i].attName, tab[i].valLen, tab[i].attVal);
          }
          {
            char txt[CDI_MAX_NAME];
            size_t len = (size_t)(sprintf(txt, "%s%s", "ap: ap_bnds b: b_bnds ps: ", psname));
            cdf_put_att_text(fileID, ncbvarid, "formula_terms", len, txt);
          }
          {
            static const char units[] = "1";
            cdf_put_att_text(fileID, ncbvarid, "units", sizeof (units) - 1, units);
          }
        }
    }

  cdf_enddef(fileID);
  streamptr->ncmode = 2;

  cdf_put_var_double(fileID, ncvarid, levels);

  if ( ncbvarid != UNDEFID )
    {
      double zbounds[2*dimlen];
      for ( size_t i = 0; i < dimlen; ++i )
        {
          zbounds[2*i  ] = lbounds[i];
          zbounds[2*i+1] = ubounds[i];
        }
      cdf_put_var_double(fileID, ncbvarid, zbounds);
    }

  cdf_def_vct_cf(streamptr, zaxisID, *dimID, nvdimID);

  if ( *dimID == UNDEFID )
    {
      if ( type == ZAXIS_HYBRID )
        streamptr->zaxisID[zaxisindex] = streamptr->vct.mlevID;
      else
        streamptr->zaxisID[zaxisindex] = streamptr->vct.ilevID;
    }
}

static
void cdf_def_zaxis_hybrid(stream_t *streamptr, int type, int ncvarid, int zaxisID, int zaxisindex, int xtype, size_t dimlen, int *dimID, char *axisname)
{
  if ( (!CDI_cmor_mode && cdiConvention == CDI_CONVENTION_ECHAM) || type == ZAXIS_HYBRID_HALF )
    cdf_def_zaxis_hybrid_echam(streamptr, type, ncvarid, zaxisID, zaxisindex, xtype, dimlen, dimID, axisname);
  else
    cdf_def_zaxis_hybrid_cf(streamptr, type, ncvarid, zaxisID, zaxisindex, xtype, dimlen, dimID, axisname);
}

static
void cdfDefZaxis(stream_t *streamptr, int zaxisID)
{
  /*  char zaxisname0[CDI_MAX_NAME]; */
  char axisname[CDI_MAX_NAME];
  int dimID = UNDEFID;
  int dimIDs[2];
  int ncvarid = UNDEFID, ncbvarid = UNDEFID;
  int nvdimID = UNDEFID;
  int xtype = NC_DOUBLE;

  if ( zaxisInqPrec(zaxisID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

  int vlistID = streamptr->vlistID;
  int fileID  = streamptr->fileID;

  int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);

  int nzaxis = vlistNzaxis(vlistID);

  size_t dimlen = (size_t)zaxisInqSize(zaxisID);
  int type   = zaxisInqType(zaxisID);

  int is_scalar = FALSE;
  if ( dimlen == 1 )
    {
      is_scalar = zaxisInqScalar(zaxisID);
      if ( !is_scalar && CDI_cmor_mode )
        {
          is_scalar = TRUE;
          zaxisDefScalar(zaxisID);
        }
    }

  int ndims = 1;
  if ( is_scalar ) ndims = 0;

  if ( dimlen == 1 )
    switch (type)
      {
      case ZAXIS_SURFACE:
      case ZAXIS_CLOUD_BASE:
      case ZAXIS_CLOUD_TOP:
      case ZAXIS_ISOTHERM_ZERO:
      case ZAXIS_TOA:
      case ZAXIS_SEA_BOTTOM:
      case ZAXIS_ATMOSPHERE:
      case ZAXIS_MEANSEA:
      case ZAXIS_LAKE_BOTTOM:
      case ZAXIS_SEDIMENT_BOTTOM:
      case ZAXIS_SEDIMENT_BOTTOM_TA:
      case ZAXIS_SEDIMENT_BOTTOM_TW:
      case ZAXIS_MIX_LAYER:
        return;
      }

  zaxisInqName(zaxisID, axisname);

  if ( dimID == UNDEFID )
    {
      checkZaxisName(axisname, fileID, vlistID, zaxisID, nzaxis);

      char dimname[CDI_MAX_NAME+3];
      dimname[0] = 0;
      //cdiZaxisInqString(zaxisID, CDI_ZAXIS_DIMNAME, CDI_MAX_NAME, dimname);
      if ( dimname[0] == 0 ) strcpy(dimname, axisname);

      if ( type == ZAXIS_REFERENCE ) cdfDefZaxisUUID(streamptr, zaxisID);

      if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
        {
          cdf_def_zaxis_hybrid(streamptr, type, ncvarid, zaxisID, zaxisindex, xtype, dimlen, &dimID, axisname);
        }
      else
        {
          dimID = checkDimName(fileID, dimlen, dimname);

          if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

          if ( ndims && dimID == UNDEFID ) cdf_def_dim(fileID, dimname, dimlen, &dimID);

          cdf_def_var(fileID, axisname, (nc_type) xtype, ndims, &dimID, &ncvarid);

          cdfPutGridStdAtts(fileID, ncvarid, zaxisID, &gridInqsZ);

          {
            int positive = zaxisInqPositive(zaxisID);
            static const char positive_up[] = "up",
              positive_down[] = "down";
            static const struct attTxtTab tab[2] = {
              { positive_up, sizeof (positive_up) - 1 },
              { positive_down, sizeof (positive_down) - 1 },
            };
            if ( positive == POSITIVE_UP || positive == POSITIVE_DOWN )
              {
                size_t select = positive == POSITIVE_DOWN;
                cdf_put_att_text(fileID, ncvarid, "positive",
                                 tab[select].txtLen, tab[select].txt);
              }
          }
          cdf_put_att_text(fileID, ncvarid, "axis", 1, "Z");

	  if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
            {
              size_t nvertex = 2;
	      if ( nc_inq_dimid(fileID, bndsName, &nvdimID) != NC_NOERR )
		cdf_def_dim(fileID, bndsName, nvertex, &nvdimID);

	      if ( nvdimID != UNDEFID )
		{
                  size_t axisnameLen = strlen(axisname);
                  axisname[axisnameLen] = '_';
                  memcpy(axisname + axisnameLen + 1, bndsName, sizeof (bndsName));
		  dimIDs[0] = dimID;
		  dimIDs[ndims] = nvdimID;
		  cdf_def_var(fileID, axisname, (nc_type) xtype, ndims+1, dimIDs, &ncbvarid);
		  cdf_put_att_text(fileID, ncvarid, "bounds", strlen(axisname), axisname);
		}
	    }

          cdf_enddef(fileID);
          streamptr->ncmode = 2;

          cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));

          if ( ncbvarid != UNDEFID )
	    {
              double lbounds[dimlen], ubounds[dimlen], zbounds[2*dimlen];
	      zaxisInqLbounds(zaxisID, lbounds);
	      zaxisInqUbounds(zaxisID, ubounds);
	      for ( size_t i = 0; i < dimlen; ++i )
		{
		  zbounds[2*i  ] = lbounds[i];
		  zbounds[2*i+1] = ubounds[i];
		}

	      cdf_put_var_double(fileID, ncbvarid, zbounds);
	    }

          if ( ndims == 0 ) streamptr->nczvarID[zaxisindex] = ncvarid;
        }
    }

  if ( dimID != UNDEFID )
    streamptr->zaxisID[zaxisindex] = dimID;
}

static
void cdfDefPole(stream_t *streamptr, int gridID)
{
  int ncvarid = UNDEFID;
  static const char varname[] = "rotated_pole";
  static const char mapname[] = "rotated_latitude_longitude";

  int fileID  = streamptr->fileID;

  double ypole = gridInqYpole(gridID);
  double xpole = gridInqXpole(gridID);
  double angle = gridInqAngle(gridID);

  cdf_redef(fileID);

  int ncerrcode = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
  if ( ncerrcode == NC_NOERR )
    {
      cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", sizeof (mapname) - 1, mapname);
      cdf_put_att_double(fileID, ncvarid, "grid_north_pole_latitude", NC_DOUBLE, 1, &ypole);
      cdf_put_att_double(fileID, ncvarid, "grid_north_pole_longitude", NC_DOUBLE, 1, &xpole);
      if ( IS_NOT_EQUAL(angle, 0) )
        cdf_put_att_double(fileID, ncvarid, "north_pole_grid_longitude", NC_DOUBLE, 1, &angle);
    }

  cdf_enddef(fileID);
}

static
void cdf_def_mapping(stream_t *streamptr, int gridID)
{
  char mapping[CDI_MAX_NAME]; mapping[0] = 0;
  cdiGridInqKeyStr(gridID, CDI_KEY_MAPPING, CDI_MAX_NAME, mapping);
  if ( mapping[0] )
    {
      char gmapvarname[CDI_MAX_NAME]; gmapvarname[0] = 0;
      cdiGridInqKeyStr(gridID, CDI_KEY_MAPNAME, CDI_MAX_NAME, gmapvarname);

      int fileID = streamptr->fileID;
      cdf_redef(fileID);

      int ncvarid;
      int ncerrcode = nc_def_var(fileID, gmapvarname, (nc_type) NC_INT, 0, NULL, &ncvarid);
      if ( ncerrcode == NC_NOERR )
        cdfDefineAttributes(gridID, CDI_GLOBAL, fileID, ncvarid);

      cdf_enddef(fileID);
    }
}

static
void cdfDefMapping(stream_t *streamptr, int gridID)
{
  int ncvarid = UNDEFID;
  int fileID  = streamptr->fileID;

  if ( gridInqType(gridID) == GRID_SINUSOIDAL )
    {
      static const char varname[] = "sinusoidal";
      static const char mapname[] = "sinusoidal";

      cdf_redef(fileID);

      int ncerrcode = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
      if ( ncerrcode == NC_NOERR )
        {
          cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", strlen(mapname), mapname);
          /*
          cdf_put_att_double(fileID, ncvarid, "grid_north_pole_latitude", NC_DOUBLE, 1, &ypole);
          cdf_put_att_double(fileID, ncvarid, "grid_north_pole_longitude", NC_DOUBLE, 1, &xpole);
          */
        }

      cdf_enddef(fileID);
    }
  else if ( gridInqType(gridID) == GRID_LAEA )
    {
      static const char varname[] = "laea";
      static const char mapname[] = "lambert_azimuthal_equal_area";

      cdf_redef(fileID);

      int ncerrcode = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
      if ( ncerrcode == NC_NOERR )
        {
          double a, lon_0, lat_0;

          gridInqLaea(gridID, &a, &lon_0, &lat_0);

          cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", strlen(mapname), mapname);
          cdf_put_att_double(fileID, ncvarid, "earth_radius", NC_DOUBLE, 1, &a);
          cdf_put_att_double(fileID, ncvarid, "longitude_of_projection_origin", NC_DOUBLE, 1, &lon_0);
          cdf_put_att_double(fileID, ncvarid, "latitude_of_projection_origin", NC_DOUBLE, 1, &lat_0);
        }

      cdf_enddef(fileID);
    }
  else if ( gridInqType(gridID) == GRID_LCC2 )
    {
      static const char varname[] = "Lambert_Conformal";
      static const char mapname[] = "lambert_conformal_conic";

      cdf_redef(fileID);

      int ncerrcode = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
      if ( ncerrcode == NC_NOERR )
        {
          double radius, lon_0, lat_0, lat_1, lat_2;

          gridInqLcc2(gridID, &radius, &lon_0, &lat_0, &lat_1, &lat_2);

          cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", strlen(mapname), mapname);
          if ( radius > 0 )
            cdf_put_att_double(fileID, ncvarid, "earth_radius", NC_DOUBLE, 1, &radius);
          cdf_put_att_double(fileID, ncvarid, "longitude_of_central_meridian", NC_DOUBLE, 1, &lon_0);
          cdf_put_att_double(fileID, ncvarid, "latitude_of_projection_origin", NC_DOUBLE, 1, &lat_0);
          if ( IS_EQUAL(lat_1, lat_2) )
            cdf_put_att_double(fileID, ncvarid, "standard_parallel", NC_DOUBLE, 1, &lat_1);
          else
            {
              double lat_1_2[2];
              lat_1_2[0] = lat_1;
              lat_1_2[1] = lat_2;
              cdf_put_att_double(fileID, ncvarid, "standard_parallel", NC_DOUBLE, 2, lat_1_2);
            }
        }

      cdf_enddef(fileID);
    }
}

static
void cdfDefGrid(stream_t *streamptr, int gridID, int gridindex)
{
  if ( streamptr->ncgrid[gridindex].xdimID != UNDEFID ) return;

  int gridtype = gridInqType(gridID);
  int size     = gridInqSize(gridID);

  if ( CDI_Debug )
    Message("gridtype = %d  size = %d", gridtype, size);

  if ( gridtype == GRID_GAUSSIAN    ||
       gridtype == GRID_LONLAT      ||
       gridtype == GRID_PROJECTION  ||
       gridtype == GRID_GENERIC )
    {
      if ( gridtype == GRID_GENERIC )
        {
          if ( size == 1 && gridInqXsize(gridID) == 0 && gridInqYsize(gridID) == 0 )
            {
              /* no grid information */
              streamptr->ncgrid[gridindex].gridID = gridID;
            }
          else
            {
              bool lx = false, ly = false;
              if ( gridInqXsize(gridID) > 0 /*&& gridInqXvals(gridID, NULL) > 0*/ )
                {
                  cdfDefXaxis(streamptr, gridID, gridindex, 1);
                  lx = true;
                }

              if ( gridInqYsize(gridID) > 0 /*&& gridInqYvals(gridID, NULL) > 0*/ )
                {
                  cdfDefYaxis(streamptr, gridID, gridindex, 1);
                  ly = true;
                }

              if ( !lx && !ly ) cdfDefGdim(streamptr, gridID, gridindex);
            }
        }
      else
        {
          int ndims = 1;
          if ( gridtype == GRID_LONLAT && size == 1 && gridInqHasDims(gridID) == FALSE )
            ndims = 0;

          if ( gridInqXsize(gridID) > 0 ) cdfDefXaxis(streamptr, gridID, gridindex, ndims);
          if ( gridInqYsize(gridID) > 0 ) cdfDefYaxis(streamptr, gridID, gridindex, ndims);

          cdf_def_mapping(streamptr, gridID);
        }

      if ( gridIsRotated(gridID) ) cdfDefPole(streamptr, gridID);
    }
  else if ( gridtype == GRID_CURVILINEAR )
    {
      cdfDefCurvilinear(streamptr, gridID, gridindex);
    }
  else if ( gridtype == GRID_UNSTRUCTURED )
    {
      cdfDefUnstructured(streamptr, gridID, gridindex);
    }
  else if ( gridtype == GRID_GAUSSIAN_REDUCED )
    {
      cdfDefRgrid(streamptr, gridID, gridindex);
    }
  else if ( gridtype == GRID_SPECTRAL )
    {
      cdfDefComplex(streamptr, gridID, gridindex);
      cdfDefSP(streamptr, gridID, gridindex);
    }
  else if ( gridtype == GRID_FOURIER )
    {
      cdfDefComplex(streamptr, gridID, gridindex);
      cdfDefFC(streamptr, gridID, gridindex);
    }
  else if ( gridtype == GRID_TRAJECTORY )
    {
      cdfDefTrajLon(streamptr, gridID, gridindex);
      cdfDefTrajLat(streamptr, gridID, gridindex);
    }
  else if ( gridtype == GRID_SINUSOIDAL || gridtype == GRID_LAEA || gridtype == GRID_LCC2 )
    {
      cdfDefXaxis(streamptr, gridID, gridindex, 1);
      cdfDefYaxis(streamptr, gridID, gridindex, 1);

      cdfDefMapping(streamptr, gridID);
    }
  /*
  else if ( gridtype == GRID_LCC )
    {
      cdfDefLcc(streamptr, gridID);
    }
  */
  else
    {
      Error("Unsupported grid type: %s", gridNamePtr(gridtype));
    }
}


void cdfDefHistory(stream_t *streamptr, int size, const char *history)
{
  int ncid = streamptr->fileID;
  cdf_put_att_text(ncid, NC_GLOBAL, "history", (size_t) size, history);
}


void cdfDefVars(stream_t *streamptr)
{
  int index = 0;
  int vlistID = streamptr->vlistID;
  if ( vlistID == UNDEFID )
    Error("Internal problem! vlist undefined for streamptr %p", streamptr);

  int ngrids = vlistNgrids(vlistID);
  streamptr->ncgrid = (ncgrid_t*) Malloc(2*ngrids*sizeof(ncgrid_t));
  for ( index = 0; index < 2*ngrids; ++index )
    {
      streamptr->ncgrid[index].gridID = UNDEFID;
      streamptr->ncgrid[index].xdimID = UNDEFID;
      streamptr->ncgrid[index].ydimID = UNDEFID;
      streamptr->ncgrid[index].xvarID = UNDEFID;
      streamptr->ncgrid[index].yvarID = UNDEFID;
      streamptr->ncgrid[index].avarID = UNDEFID;
    }

  for ( index = 0; index < ngrids; ++index )
    {
      int gridID = vlistGrid(vlistID, index);
      cdfDefGrid(streamptr, gridID, index);
    }
  index = ngrids-1;
  for ( int i = 0; i < ngrids; ++i )
    {
      int gridID = vlistGrid(vlistID, i);
      int projID = gridInqProj(gridID);
      if ( projID != CDI_UNDEFID ) cdfDefGrid(streamptr, projID, ++index);
    }

  int nzaxis = vlistNzaxis(vlistID);
  for ( int index = 0; index < nzaxis; ++index )
    {
      int zaxisID = vlistZaxis(vlistID, index);
      if ( streamptr->zaxisID[index] == UNDEFID ) cdfDefZaxis(streamptr, zaxisID);
    }
}

#endif

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
