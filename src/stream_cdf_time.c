#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#  include <stdio.h>
#  include <string.h>

#  include <netcdf.h>

#  include "cdi.h"
#  include "cdi_int.h"
#  include "dmemory.h"
#  include "stream_cdf.h"
#  include "cdf_int.h"
#  include "vlist.h"

static
int cdfDefTimeBounds(int fileID, int nctimevarid, int nctimedimid, const char *taxis_name, taxis_t* taxis)
{
  int time_bndsid = -1;
  int dims[2];

  dims[0] = nctimedimid;

  /* fprintf(stderr, "time has bounds\n"); */
  static const char bndsName[] = "bnds";
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

static const char *
cdfGetTimeUnits(taxis_t *taxis)
{
  const char *unitstr;
  if ( taxis->units && taxis->units[0] )
    {
      unitstr = taxis->units;
    }
  else
    {
      if ( taxis->type == TAXIS_ABSOLUTE )
        {
          static const char *const unitstrfmt[3]
            = { "year as %Y.%f",
                "month as %Y%m.%f",
                "day as %Y%m%d.%f" };
          size_t fmtidx = (taxis->unit == TUNIT_YEAR ? 0
                           : (taxis->unit == TUNIT_MONTH ? 1
                              : 2));
          unitstr = unitstrfmt[fmtidx];
        }
      else
        {
          int timeunit = taxis->unit != -1 ? taxis->unit : TUNIT_HOUR;
          int rdate    = taxis->rdate != -1 ? taxis->rdate : taxis->vdate;
          int rtime    = taxis->rdate != -1 ? taxis->rtime : taxis->vtime;
          int year, month, day, hour, minute, second;
          cdiDecodeDate(rdate, &year, &month, &day);
          cdiDecodeTime(rtime, &hour, &minute, &second);

          if ( timeunit == TUNIT_QUARTER   ) timeunit = TUNIT_MINUTE;
          else if ( timeunit == TUNIT_30MINUTES ) timeunit = TUNIT_MINUTE;
          else if (    timeunit == TUNIT_3HOURS
                    || timeunit == TUNIT_6HOURS
                    || timeunit == TUNIT_12HOURS ) timeunit = TUNIT_HOUR;

          char *unitstr_ =  ptaxisAllocUnits(taxis, CDF_MAX_TIME_UNIT_STR);
          sprintf(unitstr_, "%s since %d-%d-%d %02d:%02d:%02d",
                  tunitNamePtr(timeunit), year, month, day, hour, minute, second);
          unitstr = unitstr_;
        }
    }
  return unitstr;
}

static const char *
cdfGetForecastTimeUnits(int timeunit)
{
  if ( timeunit == -1 ) timeunit = TUNIT_HOUR;
  else if ( timeunit == TUNIT_QUARTER   ) timeunit = TUNIT_MINUTE;
  else if ( timeunit == TUNIT_30MINUTES ) timeunit = TUNIT_MINUTE;
  else if (    timeunit == TUNIT_3HOURS
            || timeunit == TUNIT_6HOURS
            || timeunit == TUNIT_12HOURS ) timeunit = TUNIT_HOUR;

  return tunitNamePtr(timeunit);
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

  for ( size_t i = 0; i < calTabSize; ++i )
    if ( calTab[i].calCode == calendar )
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

  if ( streamptr->basetime.ncvarid != CDI_UNDEFID ) return;

  int fileID = streamptr->fileID;

  if ( streamptr->ncmode == 0 ) streamptr->ncmode = 1;
  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  taxis_t *taxis = taxisPtr(vlistInqTaxis(streamptr->vlistID));

  const char *taxis_name = (taxis->name && taxis->name[0]) ? taxis->name : default_name ;

  cdf_def_dim(fileID, taxis_name, NC_UNLIMITED, &time_dimid);
  streamptr->basetime.ncdimid = time_dimid;

  nc_type xtype = (taxis->datatype == CDI_DATATYPE_FLT32) ? NC_FLOAT : NC_DOUBLE;

  cdf_def_var(fileID, taxis_name, xtype, 1, &time_dimid, &time_varid);

  streamptr->basetime.ncvarid = time_varid;

#if  defined  (HAVE_NETCDF4)
  if ( streamptr->filetype == CDI_FILETYPE_NC4 || streamptr->filetype == CDI_FILETYPE_NC4C )
    {
      static const size_t chunk = 512;
      cdf_def_var_chunking(fileID, time_varid, NC_CHUNKED, &chunk);
    }
#endif

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
    char unitstr_[CDF_MAX_TIME_UNIT_STR];
    const char *unitstr;
    size_t unitstrLen;
    if (taxis->units && taxis->units[0])
      {
        unitstr = taxis->units;
        unitstrLen = strlen(taxis->units);
      }
    else
      {
        /* define bogus value since at this time, streamDefTimestep has
         * not been called yet
         * but since taxis->units is not set, it clearly will not
         * exceed the size of unitstr_, i.e. when defining the units
         * attribute to this value, a later redefinition will not
         * cause a recreation of on-disk data
         */
        for (size_t i = 0; i < CDF_MAX_TIME_UNIT_STR; ++i)
          unitstr_[i] = 'a';
        unitstr_[CDF_MAX_TIME_UNIT_STR-1] = '\0';
        unitstr = unitstr_;
        unitstrLen = CDF_MAX_TIME_UNIT_STR-1;
      }
    cdf_put_att_text(fileID, time_varid, "units", unitstrLen, unitstr);
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
      cdf_def_var(fileID, "leadtime", xtype, 1, &time_dimid, &leadtimeid);
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
          const char *unitstr = cdfGetForecastTimeUnits(taxis->fc_unit);
          size_t len = strlen(unitstr);
          if ( len )
            cdf_put_att_text(fileID, leadtimeid, "units", len, unitstr);
      }
    }

  cdf_put_att_text(fileID, time_varid, "axis", 1, "T");

  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
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

  double timevalue = cdiEncodeTimeval(taxis->vdate, taxis->vtime, &streamptr->tsteps[0].taxis);
  if ( CDI_Debug ) Message("tsID = %d  timevalue = %f", tsID, timevalue);

  int ncvarid = streamptr->basetime.ncvarid;
  size_t index = (size_t)tsID;
  cdf_put_var1_double(fileID, ncvarid, &index, &timevalue);

  if ( taxis->has_bounds )
    {
      size_t start[2], count[2];

      ncvarid = streamptr->basetime.ncvarboundsid;

      double timebounds[2];
      timebounds[0] = cdiEncodeTimeval(taxis->vdate_lb, taxis->vtime_lb, &streamptr->tsteps[0].taxis);
      timebounds[1] = cdiEncodeTimeval(taxis->vdate_ub, taxis->vtime_ub, &streamptr->tsteps[0].taxis);
      start[0] = (size_t)tsID; count[0] = 1; start[1] = 0; count[1] = 2;
      cdf_put_vara_double(fileID, ncvarid, start, count, timebounds);
    }

  ncvarid = streamptr->basetime.leadtimeid;
  if ( taxis->type == TAXIS_FORECAST && ncvarid != CDI_UNDEFID )
    cdf_put_var1_double(fileID, ncvarid, &index, &taxis->fc_period);
}


void cdfDefTimestep(stream_t *streamptr, int tsID)
{
  int vlistID = streamptr->vlistID;

  int time_varid = streamptr->basetime.ncvarid;
  if ( vlistHasTime(vlistID) ) cdfDefTime(streamptr);
  if ( time_varid != CDI_UNDEFID && tsID == 0 )
    {
      taxis_t *taxis = taxisPtr(vlistInqTaxis(vlistID));
      const char *unitstr = cdfGetTimeUnits(taxis);
      size_t len = strlen(unitstr);
      if ( len )
        {
          int fileID = streamptr->fileID;
          cdf_put_att_text(fileID, time_varid, "units", len, unitstr);
          /*
            if ( taxis->has_bounds )
            cdf_put_att_text(fileID, time_bndsid, "units", len, unitstr);
          */
        }
    }
  cdfDefTimeValue(streamptr, tsID);
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
