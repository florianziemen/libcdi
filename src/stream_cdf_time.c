#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cdi.h"
#include "cdi_int.h"
#include "stream_cdf.h"
#include "cdf_int.h"

static int
cdfDefTimeBounds(int fileID, int nctimevarid, int nctimedimid, const char *taxis_name, taxis_t *taxis)
{
  int dims[2] = { nctimedimid, -1 };
  static const char bndsName[] = "bnds";
  if (nc_inq_dimid(fileID, bndsName, &dims[1]) != NC_NOERR) cdf_def_dim(fileID, bndsName, 2, &dims[1]);

  const char *bndsAttName, *bndsAttVal;
  size_t bndsAttValLen;
  char tmpstr[CDI_MAX_NAME];
  if (taxis->climatology)
  {
    static const char climatology_bndsName[] = "climatology_bnds", climatology_bndsAttName[] = "climatology";
    bndsAttName = climatology_bndsAttName;
    bndsAttValLen = sizeof(climatology_bndsName) - 1;
    bndsAttVal = climatology_bndsName;
  }
  else
  {
    size_t taxisnameLen = strlen(taxis_name);
    memcpy(tmpstr, taxis_name, taxisnameLen);
    tmpstr[taxisnameLen] = '_';
    memcpy(tmpstr + taxisnameLen + 1, bndsName, sizeof(bndsName));
    size_t tmpstrLen = taxisnameLen + sizeof(bndsName);
    static const char generic_bndsAttName[] = "bounds";
    bndsAttName = generic_bndsAttName;
    bndsAttValLen = tmpstrLen;
    bndsAttVal = tmpstr;
  }

  int time_bndsid = -1;
  cdf_def_var(fileID, bndsAttVal, NC_DOUBLE, 2, dims, &time_bndsid);
  cdf_put_att_text(fileID, nctimevarid, bndsAttName, bndsAttValLen, bndsAttVal);

  return time_bndsid;
}

static int
roundTimeUnits(int timeUnits)
{
  switch (timeUnits)
  {
    case -1:
    case TUNIT_3HOURS:
    case TUNIT_6HOURS:
    case TUNIT_12HOURS: return TUNIT_HOUR;
    case TUNIT_QUARTER:
    case TUNIT_30MINUTES: return TUNIT_MINUTE;
    default: return timeUnits;
  }
}

static const char *
cdfGetTimeUnits(taxis_t *taxis)
{
  const char *unitsStr;
  if (taxis->units && taxis->units[0]) { unitsStr = taxis->units; }
  else
  {
    if (taxis->type == TAXIS_ABSOLUTE)
    {
      static const char *const unitsStrFmt[3] = { "year as %Y.%f", "month as %Y%m.%f", "day as %Y%m%d.%f" };
      size_t fmtidx = ((taxis->unitsValue == TUNIT_YEAR) ? 0 : ((taxis->unitsValue == TUNIT_MONTH) ? 1 : 2));
      unitsStr = unitsStrFmt[fmtidx];
    }
    else
    {
      int year = taxis->rDateTime.date.year;
      int month = taxis->rDateTime.date.month;
      int day = taxis->rDateTime.date.day;
      int hour = taxis->rDateTime.time.hour;
      int minute = taxis->rDateTime.time.minute;
      int second = taxis->rDateTime.time.second;

      int timeUnits = roundTimeUnits(taxis->unitsValue);
      char *unitsStr2 = ptaxisAllocUnits(taxis, CDF_MAX_TIME_UNIT_STR);
      assert(month >= 1 && month <= 12 && day > 0 && day <= 31 && hour >= 0 && hour <= 24 && minute >= 0 && minute < 60
             && second >= 0 && second <= 60);
      snprintf(unitsStr2, CDF_MAX_TIME_UNIT_STR, "%s since %d-%d-%d %02d:%02d:%02d", tunitNamePtr(timeUnits), year, month & 31,
               day & 63, hour & 63, minute & 63, second & 63);
      unitsStr = unitsStr2;
    }
  }
  return unitsStr;
}

static const char *
cdfGetForecastTimeUnitsStr(int timeUnits)
{
  return tunitNamePtr(roundTimeUnits(timeUnits));
}

static void
cdfDefCalendar(int fileID, int ncvarid, int calendar)
{
  static const struct
  {
    int calCode;
    const char *calStr;
  } calTab[] = {
    { CALENDAR_STANDARD, "standard" }, { CALENDAR_GREGORIAN, "gregorian" }, { CALENDAR_PROLEPTIC, "proleptic_gregorian" },
    { CALENDAR_NONE, "none" },         { CALENDAR_360DAYS, "360_day" },     { CALENDAR_365DAYS, "365_day" },
    { CALENDAR_366DAYS, "366_day" },
  };
  enum
  {
    calTabSize = sizeof calTab / sizeof calTab[0]
  };

  for (size_t i = 0; i < calTabSize; ++i)
    if (calTab[i].calCode == calendar)
    {
      const char *calstr = calTab[i].calStr;
      size_t len = strlen(calstr);
      cdf_put_att_text(fileID, ncvarid, "calendar", len, calstr);
      break;
    }
}

void
cdfDefTime(stream_t *streamP)
{
  static const char defaultTimeAxisName[] = "time";

  if (streamP->basetime.ncvarid != CDI_UNDEFID) return;

  int fileID = streamP->fileID;

  if (streamP->ncmode == 0) streamP->ncmode = 1;
  if (streamP->ncmode == 2) cdf_redef(fileID);

  taxis_t *taxis = taxis_to_pointer(vlistInqTaxis(streamP->vlistID));

  const char *taxisName = (taxis->name && taxis->name[0]) ? taxis->name : defaultTimeAxisName;

  size_t timeDimLen = NC_UNLIMITED;
  if (streamP->filetype == CDI_FILETYPE_NCZARR)
  {
    if (streamP->maxSteps == CDI_UNDEFID)
      fprintf(stderr, "Max. number of timesteps undefined for NCZarr!\n");
    else
      timeDimLen = (size_t) streamP->maxSteps;
  }

  int timeDimId;
  cdf_def_dim(fileID, taxisName, timeDimLen, &timeDimId);
  streamP->basetime.ncdimid = timeDimId;

  int datatype = CDI_UNDEFID;
  cdiInqKeyInt(taxis->self, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);
  nc_type xtype = (datatype == CDI_DATATYPE_INT32) ? NC_INT : ((datatype == CDI_DATATYPE_FLT32) ? NC_FLOAT : NC_DOUBLE);

  int timeVarId;
  cdf_def_var(fileID, taxisName, xtype, 1, &timeDimId, &timeVarId);
  streamP->basetime.ncvarid = timeVarId;

  if (timeDimLen == NC_UNLIMITED && is_filetype_nc4(streamP->filetype))
  {
    static const size_t chunk = 512;
    cdf_def_var_chunking(fileID, timeVarId, NC_CHUNKED, &chunk);
  }

  static const char timeStr[] = "time";
  cdf_put_att_text(fileID, timeVarId, "standard_name", sizeof(timeStr) - 1, timeStr);

  if (taxis->longname && taxis->longname[0])
    cdf_put_att_text(fileID, timeVarId, "long_name", strlen(taxis->longname), taxis->longname);

  if (taxis->hasBounds) streamP->basetime.ncvarboundsid = cdfDefTimeBounds(fileID, timeVarId, timeDimId, taxisName, taxis);

  char unitsStr_[CDF_MAX_TIME_UNIT_STR];
  const char *unitsStr;
  size_t unitsStrLen;
  if (taxis->units && taxis->units[0])
  {
    unitsStr = taxis->units;
    unitsStrLen = strlen(taxis->units);
  }
  else
  {
    /* define bogus value since at this time, streamDefTimestep has not been called yet
     * but since taxis->units is not set, it clearly will not exceed the size of unitstr_,
     * i.e. when defining the units attribute to this value, a later redefinition will not
     * cause a recreation of on-disk data
     */
    memset(unitsStr_, 'a', CDF_MAX_TIME_UNIT_STR);
    unitsStr_[CDF_MAX_TIME_UNIT_STR - 1] = '\0';
    unitsStr = unitsStr_;
    unitsStrLen = CDF_MAX_TIME_UNIT_STR - 1;
  }
  cdf_put_att_text(fileID, timeVarId, "units", unitsStrLen, unitsStr);

  if (taxis->calendar != -1) cdfDefCalendar(fileID, timeVarId, taxis->calendar);

  if (taxis->type == TAXIS_FORECAST)
  {
    int leadtimeid;
    cdf_def_var(fileID, "leadtime", xtype, 1, &timeDimId, &leadtimeid);
    streamP->basetime.leadtimeid = leadtimeid;

    static const char stdName[] = "forecast_period";
    cdf_put_att_text(fileID, leadtimeid, "standard_name", sizeof(stdName) - 1, stdName);

    static const char longName[] = "Time elapsed since the start of the forecast";
    cdf_put_att_text(fileID, leadtimeid, "long_name", sizeof(longName) - 1, longName);

    unitsStr = cdfGetForecastTimeUnitsStr(taxis->fc_units);
    size_t len = strlen(unitsStr);
    if (len) cdf_put_att_text(fileID, leadtimeid, "units", len, unitsStr);
  }

  cdf_put_att_text(fileID, timeVarId, "axis", 1, "T");

  if (streamP->ncmode == 2) cdf_enddef(fileID, streamP->self);
}

void
cdfDefTimestep(stream_t *streamP, int tsID, size_t valCount)
{
  int time_varid = streamP->basetime.ncvarid;
  if (time_varid != CDI_UNDEFID && tsID == 0)
  {
    taxis_t *taxis = taxis_to_pointer(vlistInqTaxis(streamP->vlistID));
    int fileID = streamP->fileID;
    const char *unitstr = cdfGetTimeUnits(taxis);
    size_t len = strlen(unitstr);
    if (len) cdf_put_att_text(fileID, time_varid, "units", len, unitstr);
  }

  int fileID = streamP->fileID;

  if (CDI_Debug) Message("streamID = %d, fileID = %d, tsID = %d", streamP->self, fileID, tsID);

  taxis_t *taxis = &streamP->tsteps[tsID].taxis;

  if (streamP->ncmode == 1)
  {
    cdf_enddef(fileID, streamP->self);
    streamP->ncmode = 2;
  }

  if (streamP->accessmode == 0) cdfEndDef(streamP);

  const size_t start[2] = { [0] = (size_t) tsID, [1] = 0 }, count[2] = { [0] = valCount, [1] = 2 * valCount };

  double timeValue[2] = { cdi_encode_timeval(taxis->vDateTime, &streamP->tsteps[0].taxis) };
  if (CDI_Debug) Message("tsID = %d  timeValue = %f", tsID, timeValue[0]);

  int ncvarid = streamP->basetime.ncvarid;
  cdf_put_vara_double(fileID, ncvarid, start, count, timeValue);

  if (taxis->hasBounds)
  {
    ncvarid = streamP->basetime.ncvarboundsid;
    if (ncvarid == CDI_UNDEFID) Error("Call to taxisWithBounds() missing!");

    timeValue[0] = cdi_encode_timeval(taxis->vDateTime_lb, &streamP->tsteps[0].taxis);
    timeValue[1] = cdi_encode_timeval(taxis->vDateTime_ub, &streamP->tsteps[0].taxis);
    cdf_put_vara_double(fileID, ncvarid, start, count, timeValue);
  }

  ncvarid = streamP->basetime.leadtimeid;
  if (taxis->type == TAXIS_FORECAST && ncvarid != CDI_UNDEFID)
    cdf_put_vara_double(fileID, ncvarid, start, count, &taxis->fc_period);
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
