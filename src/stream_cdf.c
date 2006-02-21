#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>

#include "dmemory.h"

#include "cdi.h"
#include "basetime.h"
#include "util.h"  /* gaussaw */
#include "stream_int.h"
#include "stream_cdf.h"
#include "cdf_int.h"
#include "varscan.h"
#include "vlist.h"


#if  defined  (HAVE_LIBNETCDF)
#  include "netcdf.h"
#endif

#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID


void cdfDefTime(int streamID);
void cdfDefGlobalAtts(int streamID);
void cdfDefLocalAtts(int streamID);

#define  MAXNAMELEN  256

#define  X_AXIS  1
#define  Y_AXIS  2
#define  Z_AXIS  3
#define  T_AXIS  4

typedef struct {
  int     ncvarid;
  int     dimtype;
  size_t  len;
  char    name[MAXNAMELEN];
}
NCDIM;

typedef struct {
  int      vartype;
  int      isvar;
  int      islon;
  int      islat;
  int      islev;
  int      warn;
  int      timeID;
  int      code;
  int      bounds;
  int      gridID;
  int      zaxisID;
  int      gridtype;
  int      zaxistype;
  int      xdim;
  int      ydim;
  int      zdim;
  int      xvarid;
  int      yvarid;
  int      zvarid;
  int      tvarid;
  int      varids[4];
  int      cellarea;
  int      calendar;
  int      tableID;
  int      truncation;
  int      defmiss;
  int      xtype;
  int      ndims;
  int      gmapid;
  int      positive;
  int      dimids[8];
  int      dimtype[8];
  size_t   vlen;
  double  *vdata;
  double   missval;
  double   addoffset;
  double   scalefactor;
  char     name[MAXNAMELEN];
  char     longname[MAXNAMELEN];
  char     stdname[MAXNAMELEN];
  char     units[MAXNAMELEN];
}
NCVAR;


static int splitBasetime(char *timeunits, TAXIS *taxis)
{
  static char func[] = "splitBasetime";
  int len;
  char *ptu;
  int year, month, day;
  int hour = 0, minute = 0, second = 0;
  int timetype = TAXIS_ABSOLUTE;
  int rdate = -1, rtime = -1;
  int timeunit = -1;

  ptu = timeunits;
  len = strlen(timeunits);

  (*taxis).type = timetype;

  if      ( strncmp(ptu, "sec",    3) == 0 ) timeunit = TUNIT_SECOND;
  else if ( strncmp(ptu, "minute", 6) == 0 ) timeunit = TUNIT_MINUTE;
  else if ( strncmp(ptu, "hour",   4) == 0 ) timeunit = TUNIT_HOUR;
  else if ( strncmp(ptu, "day",    3) == 0 ) timeunit = TUNIT_DAY;
  else if ( strncmp(ptu, "month",  5) == 0 ) timeunit = TUNIT_MONTH;
  else if ( strncmp(ptu, "calendar_month", 14) == 0 ) timeunit = TUNIT_MONTH;
  else if ( strncmp(ptu, "year",   4) == 0 ) timeunit = TUNIT_YEAR;
  else
    {
      Message(func, "Unsupported TIMEUNIT %s!", ptu);
      return (1);
    }

  while ( ! isspace(*ptu) && *ptu != 0 ) ptu++;
  if ( *ptu )
    {
      while (   isspace(*ptu) ) ptu++;

      if ( strncmp(ptu, "as", 2) == 0 )
	timetype = TAXIS_ABSOLUTE;
      else if ( strncmp(ptu, "since", 5) == 0 )
	timetype = TAXIS_RELATIVE;

      while ( ! isspace(*ptu) && *ptu != 0 ) ptu++;
      if ( *ptu )
	{
	  while (   isspace(*ptu) ) ptu++;

	  if ( timetype == TAXIS_ABSOLUTE )
	    {
	      if ( strncmp(ptu, "%Y%m%d.%f", 9) != 0 && timeunit == TUNIT_DAY )
		{
		  Message(func, "Unsupported format %s for TIMEUNIT day!", ptu);
		  timeunit = -1;
		}
	      else if ( strncmp(ptu, "%Y%m.%f", 7) != 0 && timeunit == TUNIT_MONTH )
		{
		  Message(func, "Unsupported format %s for TIMEUNIT month!", ptu);
		  timeunit = -1;
		}
	    }
	  else if ( timetype == TAXIS_RELATIVE )
	    {
	      year = atoi(ptu);
	      while ( isdigit((int) *ptu) ) ptu++;
	      month = atoi(++ptu);
	      while ( isdigit((int) *ptu) ) ptu++;
	      day = atoi(++ptu);
	      while ( isdigit((int) *ptu) ) ptu++;

	      while ( isspace((int) *ptu) ) ptu++;

	      if ( *ptu )
		{
		  while ( ! isdigit((int) *ptu) ) ptu++;

		  hour = atoi(ptu);
		  while ( isdigit((int) *ptu) ) ptu++;
		  if ( *ptu == ':' )
		    {
		      ptu++;
		      minute = atoi(ptu);
		      while ( isdigit((int) *ptu) ) ptu++;
		      if ( *ptu == ':' )
			{
			  ptu++;
			  second = atoi(ptu);
			  if ( second != 0 )
			    Message(func, "Seconds not supported in time units!");
			}
		    }
		}

	      rdate = year*10000 + month*100 + day;
	      rtime = hour*100 + minute;
	      (*taxis).rdate = rdate;
	      (*taxis).rtime = rtime;
	    }
	}
    }

  (*taxis).type = timetype;
  (*taxis).unit = timeunit;

  if ( CDI_Debug )
    Message(func, "timetype = %d  unit = %d", timetype, timeunit);

  return (0);
}


#if  defined  (HAVE_LIBNETCDF)
static void cdfGetAttInt(int fileID, int ncvarid, char *attname, int *attint)
{
  static char func[] = "cdfGetAttInt";
  size_t attlen;
  int intatt;
  int *pintatt;

  cdf_inq_attlen(fileID, ncvarid, attname, &attlen);

  if ( attlen > 1 )
    pintatt = (int *) malloc(attlen*sizeof(int));
  else
    pintatt = &intatt;

  cdf_get_att_int(fileID, ncvarid, attname, pintatt);

  *attint = *pintatt;

  if ( attlen > 1 )
    free(pintatt);
}
#endif

#if  defined  (HAVE_LIBNETCDF)
static void cdfGetAttDouble(int fileID, int ncvarid, char *attname, double *attdouble)
{
  static char func[] = "cdfGetAttInt";
  size_t attlen;
  double doubleatt;
  double *pdoubleatt;

  cdf_inq_attlen(fileID, ncvarid, attname, &attlen);

  if ( attlen > 1 )
    pdoubleatt = (double *) malloc(attlen*sizeof(double));
  else
    pdoubleatt = &doubleatt;

  cdf_get_att_double(fileID, ncvarid, attname, pdoubleatt);

  *attdouble = *pdoubleatt;

  if ( attlen > 1 )
    free(pdoubleatt);
}
#endif

#if  defined  (HAVE_LIBNETCDF)
static void cdfGetAttText(int fileID, int ncvarid, char *attname, char *atttext, int maxlen)
{
  size_t attlen;
  char attbuf[65636];

  cdf_inq_attlen(fileID, ncvarid, attname, &attlen);

  if ( (int) attlen < 65636 )
    {
      cdf_get_att_text(fileID, ncvarid, attname, attbuf);

      attbuf[attlen++] = 0;

      if ( (int) attlen > maxlen ) attlen = maxlen;
      memcpy(atttext, attbuf, attlen);
    }
}
#endif

#if  defined  (HAVE_LIBNETCDF)
int cdfInqDatatype(int xtype)
{
  int datatype;

  if      ( xtype == NC_BYTE  )  datatype = DATATYPE_INT1;
  else if ( xtype == NC_SHORT )  datatype = DATATYPE_INT2;
  else if ( xtype == NC_INT   )  datatype = DATATYPE_INT4;
  else if ( xtype == NC_FLOAT )  datatype = DATATYPE_REAL4;
  else                           datatype = DATATYPE_REAL8;

  return (datatype);
}
#endif

#if  defined  (HAVE_LIBNETCDF)
int cdfDefDatatype(int datatype)
{
  int xtype;

  if      ( datatype == DATATYPE_INT1  )  xtype = NC_BYTE;
  else if ( datatype == DATATYPE_INT2  )  xtype = NC_SHORT;
  else if ( datatype == DATATYPE_INT4  )  xtype = NC_INT;
  else if ( datatype == DATATYPE_REAL8 )  xtype = NC_DOUBLE;
  else                                    xtype = NC_FLOAT;

  return (xtype);
}
#endif


int cdfCopyRecord(int ostreamID, int istreamID)
{
  static char func[] = "cdfCopyRecord";
  double *data;
  size_t datasize;
  int itsID, otsID, irecID, orecID;
  int ivarID, gridID;
  int nmiss;
  int ierr = 0;
  int ivlistID, ovlistID;

  streamCheckID(func, istreamID);
  streamCheckID(func, ostreamID);

  ivlistID = streams[istreamID].vlistID;
  ovlistID = streams[ostreamID].vlistID;

  itsID = streams[istreamID].curTsID;
  otsID = streams[ostreamID].curTsID;

  irecID = streams[istreamID].tsteps[itsID].curRecID;
  orecID = streams[ostreamID].tsteps[otsID].curRecID;

  ivarID = streams[istreamID].tsteps[itsID].records[irecID].varID;

  gridID = vlistInqVarGrid(ivlistID, ivarID);

  datasize = gridInqSize(gridID);      
  /* bug fix for constant netCDF fields */
  if ( datasize < 1048576 ) datasize = 1048576;

  data = (double *) malloc(datasize*sizeof(double));

  streamReadRecord(istreamID, data, &nmiss);
  streamWriteRecord(ostreamID, data, nmiss);

  free(data);

  return (ierr);
}
/* not used
int cdfInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "cdfInqRecord";
  int tsID, recID;

  streamCheckID(func, streamID);

  recID = streams[streamID].tsteps[0].curRecID++;
  printf("cdfInqRecord recID %d %d\n", recID, streams[streamID].tsteps[0].curRecID);
  printf("cdfInqRecord tsID %d\n", streams[streamID].curTsID);

  if ( streams[streamID].tsteps[0].curRecID >= 
       streams[streamID].tsteps[0].nrecs )
    {
      streams[streamID].tsteps[0].curRecID = 0;
    }

  *varID   = streams[streamID].tsteps[0].records[recID].varID;
  *levelID = streams[streamID].tsteps[0].records[recID].levelID;

  streams[streamID].record->varID   = *varID;
  streams[streamID].record->levelID = *levelID;

  if ( CDI_Debug )
    Message(func, "recID = %d  varID = %d  levelID = %d", recID, *varID, *levelID);
  
  return (recID+1);
}
*/
int cdfDefRecord(int streamID)
{
  static char func[] = "cdfDefRecord";
  int ierr = 0;

  if ( CDI_Debug )
    Message(func, "streamID = %d", streamID);

  streamCheckID(func, streamID);

  return (ierr);
}

void cdfWriteGridTraj(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  int tsID, fileID;
  int lonID, latID, gridindex;
  size_t index;
  double xlon, xlat;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  gridindex = vlistGridIndex(vlistID, gridID);
  lonID = streams[streamID].xdimID[gridindex];
  latID = streams[streamID].ydimID[gridindex];

  xlon = gridInqXval(gridID, 0);
  xlat = gridInqYval(gridID, 0);
  tsID = streams[streamID].curTsID;
  index = tsID;

  cdf_put_var1_double(fileID, lonID, &index, &xlon);
  cdf_put_var1_double(fileID, latID, &index, &xlat);
#endif
}


void cdfReadGridTraj(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  int tsID, fileID;
  int lonID, latID, gridindex;
  size_t index;
  double xlon, xlat;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  gridindex = vlistGridIndex(vlistID, gridID);
  lonID = streams[streamID].xdimID[gridindex];
  latID = streams[streamID].ydimID[gridindex];

  tsID = streams[streamID].curTsID;
  index = tsID;

  cdf_get_var1_double(fileID, lonID, &index, &xlon);
  cdf_get_var1_double(fileID, latID, &index, &xlat);

  gridDefXvals(gridID, &xlon);
  gridDefYvals(gridID, &xlat);
#endif
}


void cdfDefMissval(int streamID, int varID, int dtype)
{
#if  defined  (HAVE_LIBNETCDF)
  if ( streams[streamID].vars[varID].defmiss == FALSE )
    {
      int fileID;
      int ncvarid;
      double missval;
      int vlistID;
      int xtype;

      vlistID = streamInqVlist(streamID);
      fileID  = streamInqFileID(streamID);
      ncvarid = streams[streamID].vars[varID].ncvarid;
      missval = vlistInqVarMissval(vlistID, varID);
      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      xtype = cdfDefDatatype(dtype);

      cdf_put_att_double(fileID, ncvarid, "_FillValue", (nc_type) xtype, 1L, &missval);

      if ( streams[streamID].ncmode == 2 ) cdf_enddef(fileID);

      streams[streamID].vars[varID].defmiss = TRUE;
    }
#endif
}

void cdfWriteRecord(int streamID, double *data, int nmiss)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfWriteRecord";
  int varID;
  int levelID;

  varID   = streams[streamID].record->varID;
  levelID = streams[streamID].record->levelID;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  cdfWriteVarSliceDP(streamID, varID, levelID, data, nmiss);
#endif
}

int cdfReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "cdfReadRecord";
  int ierr = 0;
  int levelID, varID, tsID, recID, vrecID;

  if ( CDI_Debug ) Message(func, "streamID = %d", streamID);

  tsID    = streams[streamID].curTsID;
  vrecID  = streams[streamID].tsteps[tsID].curRecID;
  recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
  varID   = streams[streamID].tsteps[tsID].records[recID].varID;
  levelID = streams[streamID].tsteps[tsID].records[recID].levelID;

  cdfReadVarSliceDP(streamID, varID, levelID, data, nmiss);

  return (ierr);
}


void cdfDefTimeValue(int streamID, int tsID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefTimeValue";
  int fileID;
  double timevalue;
  int ncvarid;
  size_t index;
  TAXIS *taxis;

  fileID = streamInqFileID(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d, fileID = %d", streamID, fileID);

  taxis = &streams[streamID].tsteps[tsID].taxis;

  if ( streams[streamID].ncmode == 1 )
    {
      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  index = tsID;

  timevalue = encode_timeval(taxis->vdate, taxis->vtime, &streams[streamID].tsteps[0].taxis);
  if ( CDI_Debug ) Message(func, "tsID = %d  timevalue = %f", tsID, timevalue);

  ncvarid = streams[streamID].basetime.ncvarid;
  cdf_put_var1_double(fileID, ncvarid, &index, &timevalue);

  if ( taxis->has_bounds )
    {
      size_t start[2], count[2];

      ncvarid = streams[streamID].basetime.ncvarboundsid;

      timevalue = encode_timeval(taxis->vdate_lb, taxis->vtime_lb, &streams[streamID].tsteps[0].taxis);
      start[0] = tsID; count[0] = 1; start[1] = 0; count[1] = 1;
      cdf_put_vara_double(fileID, ncvarid, start, count, &timevalue);

      timevalue = encode_timeval(taxis->vdate_ub, taxis->vtime_ub, &streams[streamID].tsteps[0].taxis);
      start[0] = tsID; count[0] = 1; start[1] = 1; count[1] = 1;
      cdf_put_vara_double(fileID, ncvarid, start, count, &timevalue);
    }
  /*
printf("fileID = %d %d %d %f\n", fileID, time_varid, index, timevalue);
  */
#endif
}


void cdfDefTimestep(int streamID, int tsID)
{
  int vlistID;

  vlistID = streamInqVlist(streamID);

  if ( vlistHasTime(vlistID) ) cdfDefTime(streamID);

  cdfDefTimeValue(streamID, tsID);
}


void cdfDefTime(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID;
  int time_varid;
  int time_bndsid;
  int dims[2];
  int year, month, day, hour, minute;
  char unitstr[80];
  char calstr[80];
  size_t len;
  TAXIS *taxis;

  if ( streams[streamID].basetime.ncvarid != UNDEFID ) return;

  fileID = streamInqFileID(streamID);

  if ( streams[streamID].ncmode == 0 ) streams[streamID].ncmode = 1;

  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

  cdf_def_dim(fileID, "time", NC_UNLIMITED, &streams[streamID].basetime.ncdimid);

  dims[0] = streams[streamID].basetime.ncdimid;
  cdf_def_var(fileID, "time", NC_DOUBLE, 1, dims, &time_varid);

  streams[streamID].basetime.ncvarid = time_varid;

  taxis = &streams[streamID].tsteps[0].taxis;

  if ( taxis->has_bounds )
    {
      /* fprintf(stderr, "time has bounds\n"); */

      cdf_def_dim(fileID, "tbnds", 2, &dims[1]);
      cdf_def_var(fileID, "time_bnds", NC_DOUBLE, 2, dims, &time_bndsid);

      streams[streamID].basetime.ncvarboundsid = time_bndsid;

      cdf_put_att_text(fileID, time_varid, "bounds", 9, "time_bnds");
    }

  unitstr[0] = 0;
  if ( streams[streamID].tsteps[0].taxis.type == TAXIS_ABSOLUTE )
    {
      if ( streams[streamID].tsteps[0].taxis.unit == TUNIT_MONTH )
	sprintf(unitstr, "month as %s", "%Y%m.%f");
      else
	sprintf(unitstr, "day as %s", "%Y%m%d.%f");
    }
  else
    {
      int rdate, rtime;
      int timeunit;

      timeunit = taxis->unit;
      rdate    = taxis->rdate;
      rtime    = taxis->rtime;
      if ( rdate == -1 )
	{
	  rdate  = taxis->vdate;
	  rtime  = taxis->vtime;
	}
      year   =  rdate / 10000;
      month  = (rdate - year*10000) / 100;
      day    =  rdate - year*10000 - month*100;
      hour   =  rtime / 100;
      minute =  rtime - hour*100;
      sprintf(unitstr, "%s since %d-%02d-%02d %02d:%02d",
	      tunitNamePtr(timeunit), year, month, day, hour, minute);
    }

  len = strlen(unitstr);
  if ( len )
    cdf_put_att_text(fileID, time_varid, "units", len, unitstr);

  if ( taxis->has_bounds )
    if ( len )
      cdf_put_att_text(fileID, time_bndsid, "units", len, unitstr);

  if ( taxis->calendar != -1 )
    {
      calstr[0] = 0;

      if      ( taxis->calendar == CALENDAR_NONE )      strcpy(calstr, "none");
      else if ( taxis->calendar == CALENDAR_360DAYS )   strcpy(calstr, "360_day");
      else if ( taxis->calendar == CALENDAR_365DAYS )   strcpy(calstr, "365_day");
      else if ( taxis->calendar == CALENDAR_366DAYS )   strcpy(calstr, "366_day");
      else if ( taxis->calendar == CALENDAR_PROLEPTIC ) strcpy(calstr, "proleptic_gregorian");

      len = strlen(calstr);
      if ( len )
	cdf_put_att_text(fileID, time_varid, "calendar", len, calstr);

      if ( taxis->has_bounds )
	if ( len )
	  cdf_put_att_text(fileID, time_bndsid, "calendar", len, calstr);
    }

  if ( streams[streamID].ncmode == 2 ) cdf_enddef(fileID);
#endif
}


void cdfDefComplex(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  char axisname[] = "complex";
  int index;
  int dimID = UNDEFID;
  int gridID0, gridtype0, gridindex;
  int ngrids;
  int fileID;
  int dimlen;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_SPECTRAL )
	    {
	      dimID = streams[streamID].xdimID[index];
	      break;
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      dimlen = 2;

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = dimID;

#endif
}

void cdfDefSpc(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  /*
  char longname[] = "Spherical harmonic coefficient";
  */
  char axisname[5] = "spcX";
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID)/2;

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].ydimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_SPECTRAL )
	    {
	      dimlen0 = gridInqSize(gridID0)/2;
	      if ( dimlen == dimlen0 )
		{
		  dimID = streams[streamID].ydimID[index];
		  break;
		}
	      else
		iz++;	
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( iz == 0 ) axisname[3] = '\0';
      else           sprintf(&axisname[3], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].ydimID[gridindex] = dimID;

#endif
}

void cdfDefTrajLon(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefTrajLon";
  char units[256];
  char longname[256];
  char stdname[256];
  char axisname[256];
  int gridtype, gridindex;
  int dimID = UNDEFID;
  int fileID;
  int dimlen;
  size_t len;
  int ncvarid;
  int vlistID;
  int xtype = NC_DOUBLE;

  if ( gridInqPrec(gridID) == DATATYPE_REAL4 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  gridtype = gridInqType(gridID);
  dimlen = gridInqXsize(gridID);
  if ( dimlen != 1 ) Error(func, "Xsize isn't 1 for %s grid!", gridNamePtr(gridtype));

  gridindex = vlistGridIndex(vlistID, gridID);
  ncvarid = streams[streamID].xdimID[gridindex];

  gridInqXname(gridID, axisname);
  gridInqXlongname(gridID, longname);
  gridInqXstdname(gridID, stdname);
  gridInqXunits(gridID, units);

  if ( ncvarid == UNDEFID )
    {
      dimID = streams[streamID].basetime.ncvarid;

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
      if ( (len = strlen(longname)) )
	cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
      if ( (len = strlen(units)) )
	cdf_put_att_text(fileID, ncvarid, "units", len, units);
      if ( (len = strlen(stdname)) )
	cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  streams[streamID].xdimID[gridindex] = ncvarid; /* var ID for trajectory !!! */

#endif
}

void cdfDefTrajLat(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefTrajLat";
  char units[] = "degrees_north";
  char longname[] = "latitude";
  char stdname[] = "latitude";
  char axisname[] = "tlat";
  int gridtype, gridindex;
  int dimID = UNDEFID;
  int fileID;
  int dimlen;
  size_t len;
  int ncvarid;
  int vlistID;
  int xtype = NC_DOUBLE;

  if ( gridInqPrec(gridID) == DATATYPE_REAL4 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID = streamInqFileID(streamID);

  gridtype = gridInqType(gridID);
  dimlen = gridInqYsize(gridID);
  if ( dimlen != 1 ) Error(func, "Ysize isn't 1 for %s grid!", gridNamePtr(gridtype));

  gridindex = vlistGridIndex(vlistID, gridID);
  ncvarid = streams[streamID].ydimID[gridindex];

  gridInqYname(gridID, axisname);
  gridInqYlongname(gridID, longname);
  gridInqYstdname(gridID, stdname);
  gridInqYunits(gridID, units);

  if ( ncvarid == UNDEFID )
    {
      dimID = streams[streamID].basetime.ncvarid;

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
      if ( (len = strlen(longname)) )
	cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
      if ( (len = strlen(units)) )
	cdf_put_att_text(fileID, ncvarid, "units", len, units);
      if ( (len = strlen(stdname)) )
	cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  streams[streamID].ydimID[gridindex] = ncvarid; /* var ID for trajectory !!! */

#endif
}


void cdfDefXaxis(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefXaxis";
  char units[256];
  char longname[256];
  char stdname[256];
  char axisname[256];
  int index, index2, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int dimIDs[2];
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  size_t len;
  int ncvarid = UNDEFID, ncbvarid = UNDEFID;
  int nvertex = 2, nvdimID = -1;
  int vlistID;
  int xtype = NC_DOUBLE;

  if ( gridInqPrec(gridID) == DATATYPE_REAL4 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqXsize(gridID);
  gridindex = vlistGridIndex(vlistID, gridID);

  gridInqXname(gridID, axisname);
  gridInqXlongname(gridID, longname);
  gridInqXstdname(gridID, stdname);
  gridInqXunits(gridID, units);

  if ( axisname[0] == 0 ) Error(func, "axis name undefined!\n");

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN    ||
	       gridtype0 == GRID_LONLAT      ||
	       gridtype0 == GRID_CURVILINEAR ||
	       gridtype0 == GRID_GENERIC )
	    {
	      dimlen0 = gridInqXsize(gridID0);
	      if ( dimlen == dimlen0 )
		if ( gridInqXval(gridID0, 0) == gridInqXval(gridID, 0) &&
		     gridInqXval(gridID0, dimlen-1) == gridInqXval(gridID, dimlen-1) )
		  {
		    dimID = streams[streamID].xdimID[index];
		    break;
		  }
	      for ( index2 = 0; index2 < index; index2++ )
		if ( streams[streamID].xdimID[index] == streams[streamID].xdimID[index2] )
		  break;
	      if ( index2 == index ) iz++;
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( iz ) sprintf(&axisname[strlen(axisname)], "_%d", iz+1);

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      if ( gridInqXboundsPtr(gridID) || gridInqYboundsPtr(gridID) )
	cdf_def_dim(fileID, "nv", nvertex, &nvdimID);

      if ( gridInqXvalsPtr(gridID) )
	{
	  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
	  if ( (len = strlen(longname)) )
	    cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
	  if ( (len = strlen(units)) )
	    cdf_put_att_text(fileID, ncvarid, "units", len, units);
	  if ( (len = strlen(stdname)) )
	    cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);

	  if ( gridInqXboundsPtr(gridID) )
	    {
	      strcat(axisname, "_bounds");
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      cdf_def_var(fileID, axisname, (nc_type) xtype, 2, dimIDs, &ncbvarid);
	      cdf_put_att_text(fileID, ncvarid, "bounds", strlen(axisname), axisname);
	    }

	  /*
	  if ( gridIsRotated(gridID) )
	    {
	      double north_pole = gridInqXpole(gridID);
	      cdf_put_att_double(fileID, ncvarid, "north_pole", NC_DOUBLE, 1L, &north_pole);
	    }
	  */
	}

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      if ( ncvarid  != UNDEFID ) cdf_put_var_double(fileID, ncvarid, gridInqXvalsPtr(gridID));
      if ( ncbvarid != UNDEFID ) cdf_put_var_double(fileID, ncbvarid, gridInqXboundsPtr(gridID));
    }

  streams[streamID].xdimID[gridindex] = dimID;

#endif
}

void cdfDefYaxis(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefYaxis";
  char units[256];
  char longname[256];
  char stdname[256];
  char axisname[256];
  int index, index2, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int dimIDs[2];
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  size_t len;
  int ncvarid = UNDEFID, ncbvarid = UNDEFID;
  int nvdimID = -1;
  int vlistID;
  int xtype = NC_DOUBLE;

  if ( gridInqPrec(gridID) == DATATYPE_REAL4 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqYsize(gridID);
  gridindex = vlistGridIndex(vlistID, gridID);

  gridInqYname(gridID, axisname);
  gridInqYlongname(gridID, longname);
  gridInqYstdname(gridID, stdname);
  gridInqYunits(gridID, units);

  if ( axisname[0] == 0 ) Error(func, "axis name undefined!\n");

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].ydimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN    ||
	       gridtype0 == GRID_LONLAT      ||
	       gridtype0 == GRID_CURVILINEAR ||
	       gridtype0 == GRID_GENERIC )
	    {
	      dimlen0 = gridInqYsize(gridID0);
	      if ( dimlen == dimlen0 )
		if ( gridInqYval(gridID0, 0) == gridInqYval(gridID, 0) &&
		     gridInqYval(gridID0, dimlen-1) == gridInqYval(gridID, dimlen-1) )
		  {
		    dimID = streams[streamID].ydimID[index];
		    break;
		  }
	      for ( index2 = 0; index2 < index; index2++ )
		if ( streams[streamID].ydimID[index] == streams[streamID].ydimID[index2] )
		  break;
	      if ( index2 == index ) iz++;
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( iz ) sprintf(&axisname[strlen(axisname)], "_%d", iz+1);

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      if ( gridInqXboundsPtr(gridID) || gridInqYboundsPtr(gridID) )
	cdf_inq_dimid(fileID, "nv", &nvdimID);

      if ( gridInqYvalsPtr(gridID) )
	{
	  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
	  if ( (len = strlen(longname)) )
	    cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
	  if ( (len = strlen(units)) )
	    cdf_put_att_text(fileID, ncvarid, "units", len, units);
	  if ( (len = strlen(stdname)) )
	    cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);

	  if ( gridInqYboundsPtr(gridID) )
	    {
	      strcat(axisname, "_bounds");
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      cdf_def_var(fileID, axisname, (nc_type) xtype, 2, dimIDs, &ncbvarid);
	      cdf_put_att_text(fileID, ncvarid, "bounds", strlen(axisname), axisname);
	    }
	  /*
	  if ( gridIsRotated(gridID) )
	    {
	      double north_pole = gridInqYpole(gridID);
	      cdf_put_att_double(fileID, ncvarid, "north_pole", NC_DOUBLE, 1L, &north_pole);
	    }
	  */
	}

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      if ( ncvarid  != UNDEFID ) cdf_put_var_double(fileID, ncvarid, gridInqYvalsPtr(gridID));
      if ( ncbvarid != UNDEFID ) cdf_put_var_double(fileID, ncbvarid, gridInqYboundsPtr(gridID));
    }

  streams[streamID].ydimID[gridindex] = dimID;

#endif
}


void cdfDefLonLat2D(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  char xunits[256];
  char xlongname[256];
  char xstdname[256];
  char yunits[256];
  char ylongname[256];
  char ystdname[256];
  char xaxisname[256];
  char yaxisname[256];
  char xdimname[4] = "x";
  char ydimname[4] = "y";
  int index, index2, iz = 0;
  int gridID0, gridtype0, gridindex;
  int xdimID = UNDEFID;
  int ydimID = UNDEFID;
  int dimIDs[3];
  int ngrids;
  int fileID;
  int xdimlen, ydimlen, dimlen0;
  size_t len;
  int ncxvarid = UNDEFID, ncyvarid = UNDEFID;
  int ncbxvarid = UNDEFID, ncbyvarid = UNDEFID, ncavarid = UNDEFID;
  int nvertex = 4, nvdimID = -1;
  int vlistID;
  int xtype = NC_DOUBLE;

  if ( gridInqPrec(gridID) == DATATYPE_REAL4 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  xdimlen = gridInqXsize(gridID);
  ydimlen = gridInqYsize(gridID);
  gridindex = vlistGridIndex(vlistID, gridID);

  gridInqXname(gridID, xaxisname);
  gridInqXlongname(gridID, xlongname);
  gridInqXstdname(gridID, xstdname);
  gridInqXunits(gridID, xunits);
  gridInqYname(gridID, yaxisname);
  gridInqYlongname(gridID, ylongname);
  gridInqYstdname(gridID, ystdname);
  gridInqYunits(gridID, yunits);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN    ||
	       gridtype0 == GRID_LONLAT      ||
	       gridtype0 == GRID_CURVILINEAR ||
	       gridtype0 == GRID_GENERIC )
	    {
	      dimlen0 = gridInqXsize(gridID0);
	      if ( xdimlen == dimlen0 )
		if ( gridInqXval(gridID0, 0) == gridInqXval(gridID, 0) &&
		     gridInqXval(gridID0, xdimlen-1) == gridInqXval(gridID, xdimlen-1) )
		  {
		    xdimID = streams[streamID].xdimID[index];
		    break;
		  }
	      for ( index2 = 0; index2 < index; index2++ )
		if ( streams[streamID].xdimID[index] == streams[streamID].xdimID[index2] )
		  break;
	      if ( index2 == index ) iz++;
	    }
	}
    }

  if ( xdimID == UNDEFID )
    {
      if ( iz )
	{
	  sprintf(&xaxisname[strlen(xaxisname)], "_%d", iz+1);
	  sprintf(&yaxisname[strlen(yaxisname)], "_%d", iz+1);
	  sprintf(&xdimname[1], "_%d", iz+1);
	  sprintf(&ydimname[1], "_%d", iz+1);
	}

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, xdimname, xdimlen, &xdimID);
      cdf_def_dim(fileID, ydimname, ydimlen, &ydimID);

      if ( gridInqXboundsPtr(gridID) || gridInqYboundsPtr(gridID) )
	cdf_def_dim(fileID, "nv", nvertex, &nvdimID);

      dimIDs[0] = ydimID;
      dimIDs[1] = xdimID;

      if ( gridInqXvalsPtr(gridID) )
	{
	  cdf_def_var(fileID, xaxisname, (nc_type) xtype, 2, dimIDs, &ncxvarid);
	  if ( (len = strlen(xlongname)) )
	    cdf_put_att_text(fileID, ncxvarid, "long_name", len, xlongname);
	  if ( (len = strlen(xunits)) )
	    cdf_put_att_text(fileID, ncxvarid, "units", len, xunits);
	  if ( (len = strlen(xstdname)) )
	    cdf_put_att_text(fileID, ncxvarid, "standard_name", len, xstdname);

	  streams[streamID].ncxvarID[gridindex] = ncxvarid;

	  if ( gridInqXboundsPtr(gridID) )
	    {
	      strcat(xaxisname, "_bounds");
	      dimIDs[0] = ydimID;
	      dimIDs[1] = xdimID;
	      dimIDs[2] = nvdimID;
	      cdf_def_var(fileID, xaxisname, (nc_type) xtype, 3, dimIDs, &ncbxvarid);
	      cdf_put_att_text(fileID, ncxvarid, "bounds", strlen(xaxisname), xaxisname);
	    }
	}

      if ( gridInqYvalsPtr(gridID) )
	{
	  cdf_def_var(fileID, yaxisname, (nc_type) xtype, 2, dimIDs, &ncyvarid);
	  if ( (len = strlen(ylongname)) )
	    cdf_put_att_text(fileID, ncyvarid, "long_name", len, ylongname);
	  if ( (len = strlen(yunits)) )
	    cdf_put_att_text(fileID, ncyvarid, "units", len, yunits);
	  if ( (len = strlen(ystdname)) )
	    cdf_put_att_text(fileID, ncyvarid, "standard_name", len, ystdname);

	  streams[streamID].ncyvarID[gridindex] = ncyvarid;

	  if ( gridInqYboundsPtr(gridID) )
	    {
	      strcat(yaxisname, "_bounds");
	      dimIDs[0] = ydimID;
	      dimIDs[1] = xdimID;
	      dimIDs[2] = nvdimID;
	      cdf_def_var(fileID, yaxisname, (nc_type) xtype, 3, dimIDs, &ncbyvarid);
	      cdf_put_att_text(fileID, ncyvarid, "bounds", strlen(yaxisname), yaxisname);
	    }
	}

      if ( gridInqAreaPtr(gridID) )
	{
	  char yaxisname[] = "cell_area";
	  char units[] = "m2";
	  char longname[] = "area of grid cell";
	  char stdname[] = "area";

	  cdf_def_var(fileID, yaxisname, (nc_type) xtype, 2, dimIDs, &ncavarid);
	  cdf_put_att_text(fileID, ncavarid, "long_name", strlen(longname), longname);
	  cdf_put_att_text(fileID, ncavarid, "standard_name", strlen(stdname), stdname);
	  cdf_put_att_text(fileID, ncavarid, "units", strlen(units), units);

	  streams[streamID].ncavarID[gridindex] = ncavarid;
	}

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      if ( ncxvarid  != UNDEFID ) cdf_put_var_double(fileID, ncxvarid,  gridInqXvalsPtr(gridID));
      if ( ncbxvarid != UNDEFID ) cdf_put_var_double(fileID, ncbxvarid, gridInqXboundsPtr(gridID));
      if ( ncyvarid  != UNDEFID ) cdf_put_var_double(fileID, ncyvarid,  gridInqYvalsPtr(gridID));
      if ( ncbyvarid != UNDEFID ) cdf_put_var_double(fileID, ncbyvarid, gridInqYboundsPtr(gridID));
      if ( ncavarid  != UNDEFID ) cdf_put_var_double(fileID, ncavarid,  gridInqAreaPtr(gridID));
    }

  streams[streamID].xdimID[gridindex] = xdimID;
  streams[streamID].ydimID[gridindex] = ydimID;

#endif
}


void cdfDefRgrid(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefRgrid";
  char axisname[7] = "rgridX";
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int vlistID;
  int lwarn = TRUE;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN_REDUCED )
	    {
	      dimlen0 = gridInqSize(gridID0);

	      if ( dimlen == dimlen0 )
		{
		  dimID = streams[streamID].xdimID[index];
		  break;
		}
	      else
		iz++;	
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( lwarn )
	{
	  Warning(func, "Writing a netCDF file with data on a gaussian reduced grid.");
	  Warning(func, "The reading of this grid is unsupported!");
	  lwarn = FALSE;
	}

      if ( iz == 0 ) axisname[5] = '\0';
      else           sprintf(&axisname[5], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = dimID;

#endif
}


void cdfDefGdim(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  char axisname[7] = "gsizeX";
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID);

  if ( gridInqYsize(gridID) == 0 )
    for ( index = 0; index < ngrids; index++ )
      {
	if ( streams[streamID].xdimID[index] != UNDEFID )
	  {
	    gridID0 = vlistGrid(vlistID, index);
	    gridtype0 = gridInqType(gridID0);
	    if ( gridtype0 == GRID_GENERIC )
	      {
		dimlen0 = gridInqSize(gridID0);
		if ( dimlen == dimlen0 )
		  {
		    dimID = streams[streamID].xdimID[index];
		    break;
		  }
		else
		  iz++;	
	      }
	  }
      }

  if ( gridInqXsize(gridID) == 0 )
    for ( index = 0; index < ngrids; index++ )
      {
	if ( streams[streamID].ydimID[index] != UNDEFID )
	  {
	    gridID0 = vlistGrid(vlistID, index);
	    gridtype0 = gridInqType(gridID0);
	    if ( gridtype0 == GRID_GENERIC )
	      {
		dimlen0 = gridInqSize(gridID0);
		if ( dimlen == dimlen0 )
		  {
		    dimID = streams[streamID].ydimID[index];
		    break;
		  }
		else
		  iz++;	
	      }
	  }
      }

  if ( dimID == UNDEFID )
    {
      if ( iz == 0 ) axisname[5] = '\0';
      else           sprintf(&axisname[5], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = dimID;

#endif
}


void cdfDefCell(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  char axisname[6] = "cellX";
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int ncxvarid = UNDEFID, ncyvarid = UNDEFID;
  int ncbxvarid = UNDEFID, ncbyvarid = UNDEFID, ncavarid = UNDEFID;
  int nvertex, nvdimID = -1;
  int vlistID;
  int xtype = NC_DOUBLE;

  if ( gridInqPrec(gridID) == DATATYPE_REAL4 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID);
  gridindex = vlistGridIndex(vlistID, gridID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_CELL )
	    {
	      dimlen0 = gridInqSize(gridID0);
	      if ( dimlen == dimlen0 )
		{
		  dimID = streams[streamID].xdimID[index];
		  break;
		}
	      else
		iz++;	
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( iz == 0 ) axisname[4] = '\0';
      else           sprintf(&axisname[4], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      nvertex = gridInqNvertex(gridID);
      if ( nvertex > 0 ) cdf_def_dim(fileID, "nv", nvertex, &nvdimID);
	
      if ( gridInqXvalsPtr(gridID) )
	{
	  char xaxisname[] = "lon";
	  char units[] = "degrees_east";
	  char longname[] = "longitude";

	  cdf_def_var(fileID, xaxisname, (nc_type) xtype, 1, &dimID, &ncxvarid);
	  cdf_put_att_text(fileID, ncxvarid, "long_name", strlen(longname), longname);
	  cdf_put_att_text(fileID, ncxvarid, "units", strlen(units), units);

	  streams[streamID].ncxvarID[gridindex] = ncxvarid;

	  if ( nvdimID != UNDEFID && gridInqXboundsPtr(gridID) )
	    {
	      char bxaxisname[] = "lon_vertices";
	      int dimIDs[2];
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      cdf_def_var(fileID, bxaxisname, (nc_type) xtype, 2, dimIDs, &ncbxvarid);
	      cdf_put_att_text(fileID, ncxvarid, "bounds", strlen(bxaxisname), bxaxisname);
	    }
	}

      if ( gridInqYvalsPtr(gridID) )
	{
	  char yaxisname[] = "lat";
	  char units[] = "degrees_north";
	  char longname[] = "latitude";

	  cdf_def_var(fileID, yaxisname, (nc_type) xtype, 1, &dimID, &ncyvarid);
	  cdf_put_att_text(fileID, ncyvarid, "long_name", strlen(longname), longname);
	  cdf_put_att_text(fileID, ncyvarid, "units", strlen(units), units);

	  streams[streamID].ncyvarID[gridindex] = ncyvarid;

	  if ( nvdimID != UNDEFID && gridInqYboundsPtr(gridID) )
	    {
	      char byaxisname[] = "lat_vertices";
	      int dimIDs[2];
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      cdf_def_var(fileID, byaxisname, (nc_type) xtype, 2, dimIDs, &ncbyvarid);
	      cdf_put_att_text(fileID, ncyvarid, "bounds", strlen(byaxisname), byaxisname);
	    }
	}

      if ( gridInqAreaPtr(gridID) )
	{
	  char yaxisname[] = "cell_area";
	  char units[] = "m2";
	  char longname[] = "area of grid cell";
	  char stdname[] = "area";

	  cdf_def_var(fileID, yaxisname, (nc_type) xtype, 1, &dimID, &ncavarid);
	  cdf_put_att_text(fileID, ncavarid, "long_name", strlen(longname), longname);
	  cdf_put_att_text(fileID, ncavarid, "standard_name", strlen(stdname), stdname);
	  cdf_put_att_text(fileID, ncavarid, "units", strlen(units), units);

	  streams[streamID].ncavarID[gridindex] = ncavarid;
	}

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      if ( ncxvarid  != UNDEFID ) cdf_put_var_double(fileID, ncxvarid,  gridInqXvalsPtr(gridID));
      if ( ncbxvarid != UNDEFID ) cdf_put_var_double(fileID, ncbxvarid, gridInqXboundsPtr(gridID));
      if ( ncyvarid  != UNDEFID ) cdf_put_var_double(fileID, ncyvarid,  gridInqYvalsPtr(gridID));
      if ( ncbyvarid != UNDEFID ) cdf_put_var_double(fileID, ncbyvarid, gridInqYboundsPtr(gridID));
      if ( ncavarid  != UNDEFID ) cdf_put_var_double(fileID, ncavarid,  gridInqAreaPtr(gridID));
    }

  streams[streamID].xdimID[gridindex] = dimID;

#endif
}


void cdfDefVCT(int streamID, int zaxisID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefVCT";
  int type;

  type = zaxisInqType(zaxisID);

  if ( type == ZAXIS_HYBRID )
    {
      int i;
      int fileID;
      int ilev = zaxisInqVctSize(zaxisID)/2;
      int mlev = ilev - 1;
      size_t start;
      size_t count = 1;      
      int ncdimid, ncdimid2;
      int ncvarid, ncvarid2;
      int hyaiid, hybiid, hyamid, hybmid;
      int *ilevels = NULL;
      const double *vctptr = zaxisInqVctPtr(zaxisID);
      double mval;
      char tmpname[256];

      if ( streams[streamID].vct.ilev > 0 )
	{
	  if ( streams[streamID].vct.ilev != ilev )
	    Error(func, "more than one VCT for each file unsupported!");
	  return;
	}

      if ( ilev == 0 )
	{
	  Warning(func, "VCT missing");
	  return;
	}

      fileID = streamInqFileID(streamID);

      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, "mlev", mlev, &ncdimid);
      cdf_def_dim(fileID, "ilev", ilev, &ncdimid2);

      streams[streamID].vct.mlev   = mlev;
      streams[streamID].vct.ilev   = ilev;
      streams[streamID].vct.mlevID = ncdimid;
      streams[streamID].vct.ilevID = ncdimid2;

      cdf_def_var(fileID, "mlev", NC_INT,    1, &ncdimid,  &ncvarid);
      cdf_def_var(fileID, "ilev", NC_INT,    1, &ncdimid2, &ncvarid2);
      cdf_def_var(fileID, "hyai", NC_DOUBLE, 1, &ncdimid2, &hyaiid);
      cdf_def_var(fileID, "hybi", NC_DOUBLE, 1, &ncdimid2, &hybiid);
      cdf_def_var(fileID, "hyam", NC_DOUBLE, 1, &ncdimid,  &hyamid);
      cdf_def_var(fileID, "hybm", NC_DOUBLE, 1, &ncdimid,  &hybmid);

      strcpy(tmpname, "hybrid level at layer midpoints");
      cdf_put_att_text(fileID, ncvarid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid_sigma_pressure");
      cdf_put_att_text(fileID, ncvarid, "standard_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "level");
      cdf_put_att_text(fileID, ncvarid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "down");
      cdf_put_att_text(fileID, ncvarid, "positive", strlen(tmpname), tmpname);
      strcpy(tmpname, "hyam hybm (mlev=hyam+hybm*aps)");
      cdf_put_att_text(fileID, ncvarid, "formula", strlen(tmpname), tmpname);
      strcpy(tmpname, "ap: hyam b: hybm ps: aps");
      cdf_put_att_text(fileID, ncvarid, "formula_terms", strlen(tmpname), tmpname);
      strcpy(tmpname, "ilev");
      cdf_put_att_text(fileID, ncvarid, "borders", strlen(tmpname), tmpname);

      strcpy(tmpname, "hybrid level at layer interfaces");
      cdf_put_att_text(fileID, ncvarid2, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid_sigma_pressure");
      cdf_put_att_text(fileID, ncvarid2, "standard_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "level");
      cdf_put_att_text(fileID, ncvarid2, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "down");
      cdf_put_att_text(fileID, ncvarid2, "positive", strlen(tmpname), tmpname);
      strcpy(tmpname, "hyai hybi (ilev=hyai+hybi*aps)");
      cdf_put_att_text(fileID, ncvarid2, "formula", strlen(tmpname), tmpname);
      strcpy(tmpname, "ap: hyai b: hybi ps: aps");
      cdf_put_att_text(fileID, ncvarid2, "formula_terms", strlen(tmpname), tmpname);

      strcpy(tmpname, "hybrid A coefficient at layer interfaces");
      cdf_put_att_text(fileID, hyaiid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "Pa");
      cdf_put_att_text(fileID, hyaiid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid B coefficient at layer interfaces");
      cdf_put_att_text(fileID, hybiid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "1");
      cdf_put_att_text(fileID, hybiid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid A coefficient at layer midpoints");
      cdf_put_att_text(fileID, hyamid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "Pa");
      cdf_put_att_text(fileID, hyamid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid B coefficient at layer midpoints");
      cdf_put_att_text(fileID, hybmid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "1");
      cdf_put_att_text(fileID, hybmid, "units", strlen(tmpname), tmpname);

      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      ilevels = (int *) malloc(ilev*sizeof(int));

      for ( i = 0; i < ilev; i++ ) ilevels[i] = i+1;
      cdf_put_var_int(fileID, ncvarid,  ilevels);
      cdf_put_var_int(fileID, ncvarid2, ilevels);

      free(ilevels);

      cdf_put_var_double(fileID, hyaiid, vctptr);
      cdf_put_var_double(fileID, hybiid, vctptr+ilev);

      for ( i = 0; i < mlev; i++ )
	{
	  start = i;
	  mval = (vctptr[i] + vctptr[i+1]) * 0.5;
	  cdf_put_vara_double(fileID, hyamid, &start, &count, &mval);
	  mval = (vctptr[ilev+i] + vctptr[ilev+i+1]) * 0.5;
	  cdf_put_vara_double(fileID, hybmid, &start, &count, &mval);	  
	}
    }

#endif
}


void cdfDefZaxis(int streamID, int zaxisID)
{
#if  defined  (HAVE_LIBNETCDF)
  char zaxisname0[256];
  char axisname[256];
  char longname[256];
  char units[256];
  char tmpname[256];
  int index;
  int zaxisID0;
  int dimID = UNDEFID;
  int fileID;
  int dimlen;
  size_t len;
  int ncvarid;
  int type;
  int nzaxis;
  int ilevel = 0;
  int vlistID;
  int zaxisindex;
  int xtype = NC_DOUBLE;

  if ( zaxisInqPrec(zaxisID) == DATATYPE_REAL4 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);

  nzaxis = vlistNzaxis(vlistID);

  dimlen = zaxisInqSize(zaxisID);
  type   = zaxisInqType(zaxisID);

  if ( dimlen == 1 && type == ZAXIS_SURFACE ) return;

  zaxisInqName(zaxisID, axisname);

  for ( index = 0; index < zaxisindex; index++ )
    {
      if ( streams[streamID].zaxisID[index] != UNDEFID )
	{
	  zaxisID0 = vlistZaxis(vlistID, index);
	  zaxisInqName(zaxisID0, zaxisname0);
	  if ( strcmp(zaxisname0, axisname) == 0 ) ilevel++;
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( ilevel ) sprintf(&axisname[strlen(axisname)], "_%1d", ilevel+1);

      if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
	{
	  size_t ilev = zaxisInqVctSize(zaxisID)/2;
	  size_t mlev = ilev - 1;

	  if ( type == ZAXIS_HYBRID )
	    {
	      if ( (int) mlev != dimlen )
		{
		  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

		  cdf_def_dim(fileID, axisname, dimlen, &dimID);
		  cdf_def_var(fileID, axisname, NC_INT, 1, &dimID,  &ncvarid);

		  strcpy(tmpname, "hybrid level at layer midpoints");
		  cdf_put_att_text(fileID, ncvarid, "long_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hybrid_sigma_pressure");
		  cdf_put_att_text(fileID, ncvarid, "standard_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "level");
		  cdf_put_att_text(fileID, ncvarid, "units", strlen(tmpname), tmpname);
		  strcpy(tmpname, "down");
		  cdf_put_att_text(fileID, ncvarid, "positive", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hyam hybm (mlev=hyam+hybm*aps)");
		  cdf_put_att_text(fileID, ncvarid, "formula", strlen(tmpname), tmpname);
		  strcpy(tmpname, "ap: hyam b: hybm ps: aps");
		  cdf_put_att_text(fileID, ncvarid, "formula_terms", strlen(tmpname), tmpname);
		  strcpy(tmpname, "ilev");
		  cdf_put_att_text(fileID, ncvarid, "borders", strlen(tmpname), tmpname);
  
		  cdf_enddef(fileID);
		  streams[streamID].ncmode = 2;

		  cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
		}
	    }

	  if ( type == ZAXIS_HYBRID_HALF )
	    {
	      if ( (int) ilev != dimlen )
		{
		  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

		  cdf_def_dim(fileID, axisname, dimlen, &dimID);
		  cdf_def_var(fileID, axisname, NC_INT, 1, &dimID,  &ncvarid);

		  strcpy(tmpname, "hybrid level at layer interfaces");
		  cdf_put_att_text(fileID, ncvarid, "long_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hybrid_sigma_pressure");
		  cdf_put_att_text(fileID, ncvarid, "standard_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "level");
		  cdf_put_att_text(fileID, ncvarid, "units", strlen(tmpname), tmpname);
		  strcpy(tmpname, "down");
		  cdf_put_att_text(fileID, ncvarid, "positive", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hyai hybi (ilev=hyai+hybi*aps)");
		  cdf_put_att_text(fileID, ncvarid, "formula", strlen(tmpname), tmpname);
		  strcpy(tmpname, "ap: hyai b: hybi ps: aps");
		  cdf_put_att_text(fileID, ncvarid, "formula_terms", strlen(tmpname), tmpname);
	      
		  cdf_enddef(fileID);
		  streams[streamID].ncmode = 2;

		  cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
		}
	    }

	  cdfDefVCT(streamID, zaxisID);

	  if ( dimID == UNDEFID )
	    {
	      if ( type == ZAXIS_HYBRID )
		streams[streamID].zaxisID[zaxisindex] = streams[streamID].vct.mlevID;
	      else
		streams[streamID].zaxisID[zaxisindex] = streams[streamID].vct.ilevID;
	    }
	}
      else
	{
	  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

	  cdf_def_dim(fileID, axisname, dimlen, &dimID);

	  zaxisInqLongname(zaxisID, longname);
	  zaxisInqUnits(zaxisID, units);

	  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
	  if ( (len = strlen(longname)) )
	    cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
	  if ( (len = strlen(units)) )
	    cdf_put_att_text(fileID, ncvarid, "units", len, units);
  
	  cdf_enddef(fileID);
	  streams[streamID].ncmode = 2;

	  cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
	}
    }

  if ( dimID != UNDEFID )
    streams[streamID].zaxisID[zaxisindex] = dimID;

#endif
}


void cdfDefPole(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID;
  int ncvarid = UNDEFID;
  int ncerr;
  double xpole, ypole;
  char varname[] = "rotated_pole";
  char mapname[] = "rotated_latitude_longitude";

  fileID  = streamInqFileID(streamID);

  ypole = gridInqYpole(gridID);
  xpole = gridInqXpole(gridID);

  cdf_redef(fileID);

  ncerr = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
  if ( ncerr == NC_NOERR )
    {
      cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", strlen(mapname), mapname);
      cdf_put_att_double(fileID, ncvarid, "grid_north_pole_latitude", NC_DOUBLE, 1L, &ypole);
      cdf_put_att_double(fileID, ncvarid, "grid_north_pole_longitude", NC_DOUBLE, 1L, &xpole);
    }

  cdf_enddef(fileID);
#endif
}


void cdfDefGrid(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefGrid";
  int gridtype, size;
  int gridindex;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  gridindex = vlistGridIndex(vlistID, gridID);
  if ( streams[streamID].xdimID[gridindex] != UNDEFID ) return;

  gridtype = gridInqType(gridID);
  size     = gridInqSize(gridID);

  if ( CDI_Debug )
    Message(func, "gridtype = %d  size = %d", gridtype, size);

  if ( gridtype == GRID_GAUSSIAN ||
       gridtype == GRID_LONLAT   ||
       gridtype == GRID_GENERIC )
    {
      if ( gridInqXsize(gridID) == 0 || gridInqYsize(gridID) == 0 )
	{
	  cdfDefGdim(streamID, gridID);
	}
      else
	{
	  cdfDefXaxis(streamID, gridID);
	  cdfDefYaxis(streamID, gridID);
	}

      if ( gridIsRotated(gridID) ) cdfDefPole(streamID, gridID);
    }
  else if ( gridtype == GRID_CURVILINEAR )
    {   
      cdfDefLonLat2D(streamID, gridID);
    }
  else if ( gridtype == GRID_GAUSSIAN_REDUCED )
    {
      cdfDefRgrid(streamID, gridID);
    }
  else if ( gridtype == GRID_SPECTRAL )
    {
      cdfDefComplex(streamID, gridID);
      cdfDefSpc(streamID, gridID);
    }
  else if ( gridtype == GRID_TRAJECTORY )
    {
      cdfDefTrajLon(streamID, gridID);
      cdfDefTrajLat(streamID, gridID);
    }
  else if ( gridtype == GRID_CELL )
    {
      cdfDefCell(streamID, gridID);
    }
  else
    {
      Error(func, "%s grid unsupported!", gridNamePtr(gridtype));
    }
#endif
}


int cdfDefVar(int streamID, int varID)
{
  static char func[] = "cdfDefVar";
  int ncvarid = -1;
#if  defined  (HAVE_LIBNETCDF)
  int fileID;
  int xid = UNDEFID, yid = UNDEFID, zid = UNDEFID, tid = UNDEFID;
  int code, gridID, zaxisID;
  char varname[9];
  char *name = NULL;
  char *longname = NULL;
  char *stdname = NULL;
  char *units = NULL;
  int dims[4];
  int tableID;
  int ndims = 0;
  int len;
  int timeID;
  int xtype;
  int gridindex, zaxisindex;
  int tablenum;
  int vlistID;

  fileID  = streamInqFileID(streamID);

  if ( streams[streamID].vars[varID].ncvarid != UNDEFID )
    return (streams[streamID].vars[varID].ncvarid);

  vlistID = streamInqVlist(streamID);
  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);
  code    = vlistInqVarCode(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) != GRID_TRAJECTORY )
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];

  tid = streams[streamID].basetime.ncdimid;

  if ( timeID == TIME_VARIABLE )
    {
      if ( tid == UNDEFID ) Error(func, "Internal problem! Time undefined.");
      dims[ndims++] = tid;
    }
  if ( zid != UNDEFID ) dims[ndims++] = zid;
  if ( yid != UNDEFID ) dims[ndims++] = yid;
  if ( xid != UNDEFID ) dims[ndims++] = xid;

  tableID  = vlistInqVarTable(vlistID, varID);

  name     = vlistInqVarNamePtr(vlistID, varID);
  longname = vlistInqVarLongnamePtr(vlistID, varID);
  stdname  = vlistInqVarStdnamePtr(vlistID, varID);
  units    = vlistInqVarUnitsPtr(vlistID, varID);

  if ( name     == NULL )     name = tableInqParNamePtr(tableID, code);
  if ( longname == NULL ) longname = tableInqParLongnamePtr(tableID, code);
  if ( units    == NULL )    units = tableInqParUnitsPtr(tableID, code);
  if ( name == NULL )
    {
      if ( code < 0 ) code = -code;
      sprintf(varname, "var%d", code);
      name = varname;
      code = 0;
    }

  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

  xtype = cdfDefDatatype(vlistInqVarDatatype(vlistID, varID));

  cdf_def_var(fileID, name, (nc_type) xtype, ndims, dims, &ncvarid);

  if ( longname )
    cdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);

  if ( stdname )
    cdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);

  if ( units )
    cdf_put_att_text(fileID, ncvarid, "units", strlen(units), units);

  if ( code > 0 )
    cdf_put_att_int(fileID, ncvarid, "code", NC_INT, 1L, &code);

  if ( tableID != UNDEFID )
    {
      tablenum = tableInqNum(tableID);
      if ( tablenum > 0 )
	cdf_put_att_int(fileID, ncvarid, "table", NC_INT, 1L, &tablenum);
    }

  if ( gridInqType(gridID) != GRID_GENERIC && 
       gridInqType(gridID) != GRID_LONLAT  && 
       gridInqType(gridID) != GRID_CURVILINEAR )
    {
      len = strlen(gridNamePtr(gridInqType(gridID)));
      if ( len > 0 )
	cdf_put_att_text(fileID, ncvarid, "grid_type", len,
			 gridNamePtr(gridInqType(gridID)));
    }

  if ( gridIsRotated(gridID) )
    {
      char mapping[] = "rotated_pole";
      cdf_put_att_text(fileID, ncvarid, "grid_mapping", strlen(mapping), mapping);
    }

  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      cdf_put_att_text(fileID, ncvarid, "coordinates", 9, "tlon tlat" );
    }

  if ( gridInqType(gridID) == GRID_CELL || gridInqType(gridID) == GRID_CURVILINEAR )
    {
      char coordinates[256] = "";
      char cellarea[256] = "area: ";
      int ncxvarID, ncyvarID, ncavarID;
      int gridindex;
      size_t len;

      gridindex = vlistGridIndex(vlistID, gridID);
      ncxvarID = streams[streamID].ncxvarID[gridindex];
      ncyvarID = streams[streamID].ncyvarID[gridindex];
      ncavarID = streams[streamID].ncavarID[gridindex];
      if ( ncxvarID != CDI_UNDEFID )
	cdf_inq_varname(fileID, ncxvarID, coordinates);
      len = strlen(coordinates);
      if ( ncyvarID != CDI_UNDEFID )
	{
	  if ( len ) coordinates[len++] = ' ';
	  cdf_inq_varname(fileID, ncyvarID, coordinates+len);
	}
      len = strlen(coordinates);
      if ( len )
	cdf_put_att_text(fileID, ncvarid, "coordinates", len, coordinates);

      if ( ncavarID != CDI_UNDEFID )
	{
	  len = strlen(cellarea);
	  cdf_inq_varname(fileID, ncavarID, cellarea+len);
	  len = strlen(cellarea);
	  cdf_put_att_text(fileID, ncvarid, "cell_measures", len, cellarea);
	}
    }

  if ( gridInqType(gridID) == GRID_SPECTRAL )
    {
      int gridTruncation = gridInqTrunc(gridID);

      cdf_put_att_int(fileID, ncvarid, "truncation", NC_INT, 1L, &gridTruncation);
    }

  /*  if ( xtype == NC_BYTE || xtype == NC_SHORT || xtype == NC_INT ) */
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;

      addoffset    = vlistInqVarAddoffset(vlistID, varID);
      scalefactor  = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = addoffset   != 0.0;
      lscalefactor = scalefactor != 1.0;

      if ( laddoffset )
	cdf_put_att_double(fileID, ncvarid, "add_offset", NC_FLOAT, 1L, &addoffset);

      if ( lscalefactor )
	cdf_put_att_double(fileID, ncvarid, "scale_factor", NC_FLOAT, 1L, &scalefactor);
    }

  streams[streamID].vars[varID].ncvarid = ncvarid;

  if ( streams[streamID].ncmode == 2 ) cdf_enddef(fileID);

#endif
  return (ncvarid);
}


void cdfReadVarDP(int streamID, int varID, double *data, int *nmiss)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfReadVarDP";
  int fileID;
  int gridID;
  int zaxisID;
  int xid = UNDEFID, yid = UNDEFID, zid = UNDEFID;
  int ncvarid;
  int tsID;
  size_t size;
  size_t start[4];
  size_t count[4];
  int ndims = 0;
  int idim;
  int timeID;
  int gridindex, zaxisindex;
  int vlistID;
  int i;
  double missval;
  int laddoffset, lscalefactor;
  double addoffset, scalefactor;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  tsID = streams[streamID].curTsID;

  if ( CDI_Debug ) Message(func, "tsID = %d", tsID);

  ncvarid = streams[streamID].vars[varID].ncvarid;

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      cdfReadGridTraj(streamID, gridID);
    }
  else
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];

  if ( timeID == TIME_VARIABLE )
    {
      start[ndims] = tsID;
      count[ndims] = 1;
      ndims++;
    }
  if ( zid != UNDEFID )
    {
      start[ndims] = 0;
      count[ndims] = zaxisInqSize(zaxisID);
      ndims++;
    }
  if ( yid != UNDEFID )
    {
      start[ndims] = 0;
      count[ndims] = gridInqYsize(gridID);
      ndims++;
    }
  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      count[ndims] = gridInqXsize(gridID);
      ndims++;
    }

  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  cdf_get_vara_double(fileID, ncvarid, start, count, data);

  *nmiss = 0;
  if ( vlistInqVarMissvalUsed(vlistID, varID) == TRUE  )
    {
      size    = gridInqSize(gridID)*zaxisInqSize(zaxisID);
      missval = vlistInqVarMissval(vlistID, varID);

      for ( i = 0; i < (int) size; i++ )
	if ( DBL_IS_EQUAL(data[i], missval) ) *nmiss += 1;
    }

  addoffset    = vlistInqVarAddoffset(vlistID, varID);
  scalefactor  = vlistInqVarScalefactor(vlistID, varID);
  laddoffset   = addoffset   != 0.0;
  lscalefactor = scalefactor != 1.0;

  if ( laddoffset || lscalefactor )
    {
      size    = gridInqSize(gridID)*zaxisInqSize(zaxisID);
      missval = vlistInqVarMissval(vlistID, varID);

      if ( *nmiss > 0 )
	{
	  for ( i = 0; i < (int) size; i++ )
	    {
	      if ( !DBL_IS_EQUAL(data[i], missval) )
		{
		  if ( lscalefactor ) data[i] = data[i] * scalefactor;
		  if ( laddoffset )   data[i] = data[i] + addoffset;
		}
	    }
	}
      else
	{
	  for ( i = 0; i < (int) size; i++ )
	    {
	      if ( lscalefactor ) data[i] = data[i] * scalefactor;
	      if ( laddoffset )   data[i] = data[i] + addoffset;
	    }
	}
    }
#endif
}

void cdfWriteVarDP(int streamID, int varID, double *data, int nmiss)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfWriteVarDP";
  int fileID;
  int gridID;
  int zaxisID;
  int xid = UNDEFID, yid = UNDEFID, zid = UNDEFID;
  int ncvarid;
  int ntsteps;
  size_t size;
  size_t start[4];
  size_t count[4];
  int ndims = 0;
  int idim;
  int timeID;
  int gridindex, zaxisindex;
  int i;
  int dtype;
  int vlistID;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ntsteps = streams[streamID].ntsteps;
  if ( CDI_Debug )
    Message(func, "ntsteps = %d", ntsteps); 

  if ( vlistHasTime(vlistID) ) cdfDefTime(streamID);

  ncvarid = cdfDefVar(streamID, varID);

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      cdfWriteGridTraj(streamID, gridID);
    }
  else
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];

  if ( timeID == TIME_VARIABLE )
    {
      start[ndims] = ntsteps - 1;
      count[ndims] = 1;
      ndims++;
    }
  if ( zid != UNDEFID )
    {
      start[ndims] = 0;
      count[ndims] = zaxisInqSize(zaxisID);
      ndims++;
    }
  if ( yid != UNDEFID )
    {
      start[ndims] = 0;
      cdf_inq_dimlen(fileID, yid, &size);
      /*      count[ndims] = gridInqYsize(gridID); */
      count[ndims] = size;
      ndims++;
    }
  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      cdf_inq_dimlen(fileID, xid, &size);
      /*      count[ndims] = gridInqXsize(gridID); */
      count[ndims] = size;
      ndims++;
    }

  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  if ( streams[streamID].ncmode == 1 )
    {
      cdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  dtype = vlistInqVarDatatype(vlistID, varID);

  if ( nmiss > 0 ) cdfDefMissval(streamID, varID, dtype);

  /*  if ( dtype == DATATYPE_INT1 || dtype == DATATYPE_INT2 || dtype == DATATYPE_INT4 ) */
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;
      double missval;

      addoffset    = vlistInqVarAddoffset(vlistID, varID);
      scalefactor  = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = addoffset   != 0.0;
      lscalefactor = scalefactor != 1.0;

      if ( laddoffset || lscalefactor )
	{
	  gridID  = vlistInqVarGrid(vlistID, varID);
	  size    = gridInqSize(gridID)*zaxisInqSize(zaxisID);
	  missval = vlistInqVarMissval(vlistID, varID);

	  if ( nmiss > 0 )
	    {
	      for ( i = 0; i < (int) size; i++ )
		{
		  if ( !DBL_IS_EQUAL(data[i], missval) )
		    {
		      if ( laddoffset )   data[i] = data[i] - addoffset;
		      if ( lscalefactor ) data[i] = data[i] / scalefactor;
		    }
		}
	    }
	  else
	    {
	      for ( i = 0; i < (int) size; i++ )
		{
		  if ( laddoffset )   data[i] = data[i] - addoffset;
		  if ( lscalefactor ) data[i] = data[i] / scalefactor;
		}
	    }
	}
    }

  cdf_put_vara_double(fileID, ncvarid, start, count, data);
#endif
}


int cdfReadVarSliceDP(int streamID, int varID, int levelID, double *data, int *nmiss)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfReadVarSliceDP";
  int fileID;
  int gridID;
  int zaxisID;
  int xid = UNDEFID, yid = UNDEFID, zid = UNDEFID;
  int ncvarid;
  int tsID;
  size_t size;
  size_t start[4];
  size_t count[4];
  int ndims = 0;
  int idim;
  int timeID;
  int gridindex;
  int zaxisindex;
  int vlistID;
  int i;
  int ndimsp;
  int dimids[9];
  int swapyz = FALSE;
  double missval;
  int laddoffset, lscalefactor;
  double addoffset, scalefactor;


  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d  levelID = %d", streamID, varID, levelID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  tsID = streams[streamID].curTsID;
  if ( CDI_Debug )
    Message(func, "tsID = %d", tsID);

  ncvarid = streams[streamID].vars[varID].ncvarid;

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      cdfReadGridTraj(streamID, gridID);
    }
  else
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];
  /*
  printf("2 %d %d %d %s\n", streamID, zaxisindex, streams[streamID].zaxisID[zaxisindex], vlistInqVarNamePtr(vlistID, varID));
  */

  if ( timeID == TIME_VARIABLE )
    {
      start[ndims] = tsID;
      count[ndims] = 1;
      ndims++;
    }

  cdf_inq_varndims(fileID, ncvarid, &ndimsp);
  cdf_inq_vardimid(fileID, ncvarid, dimids);

  if ( zid != UNDEFID && yid != UNDEFID )
    {
      if ( zid == dimids[ndims] && yid == dimids[ndims+1] )
	swapyz = FALSE;
      else if ( yid == dimids[ndims] && zid == dimids[ndims+1] )
	swapyz = TRUE;
      else
	{
	  char name[256];
	  vlistInqVarName(vlistID, varID, name);
	  Error(func, "Unsupported array structure for %s\n", name);
	}
    }

  if ( swapyz )
    {
      if ( yid != UNDEFID )
	{
	  start[ndims] = 0;
	  /* count[ndims] = gridInqYsize(gridID); */
	  cdf_inq_dimlen(fileID, yid, &size);
	  count[ndims] = size;
	  ndims++;
	}
      if ( zid != UNDEFID )
	{
	  start[ndims] = levelID;
	  count[ndims] = 1;
	  ndims++;
	}
    }
  else
    {
      if ( zid != UNDEFID )
	{
	  start[ndims] = levelID;
	  count[ndims] = 1;
	  ndims++;
	}
      if ( yid != UNDEFID )
	{
	  start[ndims] = 0;
	  /* count[ndims] = gridInqYsize(gridID); */
	  cdf_inq_dimlen(fileID, yid, &size);
	  count[ndims] = size;
	  ndims++;
	}
    }

  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      /* count[ndims] = gridInqXsize(gridID); */
      cdf_inq_dimlen(fileID, xid, &size);
      count[ndims] = size;
      ndims++;
    }

  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  cdf_get_vara_double(fileID, ncvarid, start, count, data);

  *nmiss = 0;
  if ( vlistInqVarMissvalUsed(vlistID, varID) == TRUE  )
    {
      size    = gridInqSize(gridID);
      missval = vlistInqVarMissval(vlistID, varID);

      for ( i = 0; i < (int) size; i++ )
	if ( DBL_IS_EQUAL(data[i], missval) ) *nmiss += 1;
    }

  addoffset    = vlistInqVarAddoffset(vlistID, varID);
  scalefactor  = vlistInqVarScalefactor(vlistID, varID);
  laddoffset   = addoffset   != 0.0;
  lscalefactor = scalefactor != 1.0;

  if ( laddoffset || lscalefactor )
    {
      size    = gridInqSize(gridID);
      missval = vlistInqVarMissval(vlistID, varID);

      if ( *nmiss > 0 )
	{
	  for ( i = 0; i < (int) size; i++ )
	    {
	      if ( !DBL_IS_EQUAL(data[i], missval) )
		{
		  if ( lscalefactor ) data[i] = data[i] * scalefactor;
		  if ( laddoffset )   data[i] = data[i] + addoffset;
		}
	    }
	}
      else
	{
	  for ( i = 0; i < (int) size; i++ )
	    {
	      if ( lscalefactor ) data[i] = data[i] * scalefactor;
	      if ( laddoffset )   data[i] = data[i] + addoffset;
	    }
	}
    }

#endif
  return (0);
}

int cdfWriteVarSliceDP(int streamID, int varID, int levelID, double *data, int nmiss)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfWriteVarSliceDP";
  int fileID;
  int gridID;
  int zaxisID;
  int xid = UNDEFID, yid = UNDEFID, zid = UNDEFID;
  int ncvarid;
  int ntsteps;
  size_t size;
  size_t start[4];
  size_t count[4];
  int ndims = 0;
  int idim;
  int timeID;
  int gridindex, zaxisindex;
  int i;
  int dtype;
  int vlistID;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ntsteps = streams[streamID].ntsteps;
  if ( CDI_Debug ) Message(func, "ntsteps = %d", ntsteps);

  if ( vlistHasTime(vlistID) ) cdfDefTime(streamID);

  ncvarid = cdfDefVar(streamID, varID);

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      cdfWriteGridTraj(streamID, gridID);
    }
  else
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];

  if ( timeID == TIME_VARIABLE )
    {
      start[ndims] = ntsteps - 1;
      count[ndims] = 1;
      ndims++;
    }
  if ( zid != UNDEFID )
    {
      start[ndims] = levelID;
      count[ndims] = 1;
      ndims++;
    }
  if ( yid != UNDEFID )
    {
      start[ndims] = 0;
      cdf_inq_dimlen(fileID, yid, &size);
      /*      count[ndims] = gridInqYsize(gridID); */
      count[ndims] = size;
      ndims++;
    }
  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      cdf_inq_dimlen(fileID, xid, &size);
      /*      count[ndims] = gridInqXsize(gridID); */
      count[ndims] = size;
      ndims++;
    }

  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  dtype = vlistInqVarDatatype(vlistID, varID);

  if ( nmiss > 0 ) cdfDefMissval(streamID, varID, dtype);

  /*  if ( dtype == DATATYPE_INT1 || dtype == DATATYPE_INT2 || dtype == DATATYPE_INT4 ) */
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;
      double missval;

      addoffset    = vlistInqVarAddoffset(vlistID, varID);
      scalefactor  = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = addoffset   != 0.0;
      lscalefactor = scalefactor != 1.0;

      if ( laddoffset || lscalefactor )
	{
	  gridID  = vlistInqVarGrid(vlistID, varID);
	  size    = gridInqSize(gridID);
	  missval = vlistInqVarMissval(vlistID, varID);

	  if ( nmiss > 0 )
	    {
	      for ( i = 0; i < (int) size; i++ )
		{
		  if ( !DBL_IS_EQUAL(data[i], missval) )
		    {
		      if ( laddoffset )   data[i] = data[i] - addoffset;
		      if ( lscalefactor ) data[i] = data[i] / scalefactor;
		    }
		}
	    }
	  else
	    {
	      for ( i = 0; i < (int) size; i++ )
		{
		  if ( laddoffset )   data[i] = data[i] - addoffset;
		  if ( lscalefactor ) data[i] = data[i] / scalefactor;
		}
	    }
	}
    }

  cdf_put_vara_double(fileID, ncvarid, start, count, data);

#endif
  return (0);
}

void cdfCreateRecords(int streamID, int tsID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfCreateRecords";
  int varID, levelID, recID, vrecID, zaxisID;
  int nvars, nlev, nrecs, nvrecs;
  RECORD *records = NULL;
  int *recIDs = NULL;
  int vlistID;

  vlistID  = streamInqVlist(streamID);

  if ( tsID < 0 || (tsID >= streams[streamID].ntsteps && tsID > 0) ) return;

  if ( streams[streamID].tsteps[tsID].nallrecs > 0 ) return;

  if ( tsID == 0 )
    {
      nvars = vlistNvars(vlistID);
      nrecs = vlistNrecs(vlistID);

      streams[streamID].nrecs += nrecs;

      if ( nrecs > 0 ) records = (RECORD *) malloc(nrecs*sizeof(RECORD));
      streams[streamID].tsteps[tsID].records    = records;
      streams[streamID].tsteps[tsID].nrecs      = nrecs;
      streams[streamID].tsteps[tsID].nallrecs   = nrecs;
      streams[streamID].tsteps[tsID].recordSize = nrecs;
      streams[streamID].tsteps[tsID].curRecID   = UNDEFID;

      nvrecs = nrecs; /* use all records at first timestep */
      if ( nvrecs > 0 ) recIDs = (int *) malloc(nvrecs*sizeof(int));
      streams[streamID].tsteps[tsID].recIDs     = recIDs;
      for ( recID = 0; recID < nvrecs; recID++ )
	recIDs[recID] = recID;

      recID = 0;
      for ( varID = 0; varID < nvars; varID++ )
	{
	  zaxisID = vlistInqVarZaxis(vlistID, varID);
	  nlev    = zaxisInqSize(zaxisID);
	  for ( levelID = 0; levelID < nlev; levelID++ )
	    {
	      recordInitEntry(&records[recID]);
	      records[recID].varID   = varID;
	      records[recID].levelID = levelID;
	      recID++;
	    }
	}
    }
  else if ( tsID == 1 )
    {
      nvars = vlistNvars(vlistID);
      nrecs = vlistNrecs(vlistID);

      nvrecs = 0;
      for ( varID = 0; varID < nvars; varID++ )
	{
	  if ( vlistInqVarTime(vlistID, varID) == TIME_VARIABLE )
	    {
	      zaxisID = vlistInqVarZaxis(vlistID, varID);
	      nvrecs += zaxisInqSize(zaxisID);
	    }
	}

      streams[streamID].nrecs += nvrecs;

      records = (RECORD *) malloc(nrecs*sizeof(RECORD));
      streams[streamID].tsteps[tsID].records    = records;
      streams[streamID].tsteps[tsID].nrecs      = nvrecs;
      streams[streamID].tsteps[tsID].nallrecs   = nrecs;
      streams[streamID].tsteps[tsID].recordSize = nrecs;
      streams[streamID].tsteps[tsID].curRecID   = UNDEFID;

      memcpy(streams[streamID].tsteps[tsID].records,
	     streams[streamID].tsteps[0].records,
	     nrecs*sizeof(RECORD));

      recIDs = (int *) malloc(nvrecs*sizeof(int));
      streams[streamID].tsteps[tsID].recIDs     = recIDs;
      vrecID = 0;
      for ( recID = 0; recID < nrecs; recID++ )
	{
	  varID = records[recID].varID;
	  if ( vlistInqVarTime(vlistID, varID) == TIME_VARIABLE )
	    {
	      recIDs[vrecID++] = recID;
	    }
	}
    }
  else
    {
      nvars = vlistNvars(vlistID);
      nrecs = vlistNrecs(vlistID);

      nvrecs = streams[streamID].tsteps[1].nrecs;

      streams[streamID].nrecs += nvrecs;

      records = (RECORD *) malloc(nrecs*sizeof(RECORD));
      streams[streamID].tsteps[tsID].records    = records;
      streams[streamID].tsteps[tsID].nrecs      = nvrecs;
      streams[streamID].tsteps[tsID].nallrecs   = nrecs;
      streams[streamID].tsteps[tsID].recordSize = nrecs;
      streams[streamID].tsteps[tsID].curRecID   = UNDEFID;

      memcpy(streams[streamID].tsteps[tsID].records,
	     streams[streamID].tsteps[0].records,
	     nrecs*sizeof(RECORD));

      recIDs = (int *) malloc(nvrecs*sizeof(int));
      streams[streamID].tsteps[tsID].recIDs     = recIDs;

      memcpy(streams[streamID].tsteps[tsID].recIDs,
	     streams[streamID].tsteps[1].recIDs,
	     nvrecs*sizeof(int));
    }
#endif
}

int cdfTimeDimID(int fileID, int ndims, int nvars)
{
  int dimid = 0;
#if  defined  (HAVE_LIBNETCDF)
  char dimname[80];
  char timeunits[256];
  char attname[256];
  char name[256];
  nc_type xtype;
  int nvdims, nvatts;
  int dimids[9];
  int varid, iatt;

  for ( dimid = 0; dimid < ndims; dimid++ )
    {
      cdf_inq_dimname(fileID, dimid, dimname);
      if ( strncmp(dimname, "time", 4) == 0 ) break;
    }

  if ( dimid == ndims ) dimid = UNDEFID;

  for ( varid = 0; varid < nvars; varid++ )
    {
      if ( dimid != UNDEFID ) break;

      cdf_inq_var(fileID, varid, name, &xtype, &nvdims, dimids, &nvatts);
      if ( nvdims == 1 )
	{
	  for ( iatt = 0; iatt < nvatts; iatt++ )
	    {
	      cdf_inq_attname(fileID, varid, iatt, attname);
	      if ( strncmp(attname, "units", 5) == 0 )
		{
		  cdfGetAttText(fileID, varid, "units", timeunits, 256);
		  if ( strncmp(timeunits, "sec",    3) == 0 ||
		       strncmp(timeunits, "minute", 6) == 0 ||
		       strncmp(timeunits, "hour",   4) == 0 ||
		       strncmp(timeunits, "day",    3) == 0 ||
		       strncmp(timeunits, "month",  5) == 0 )
		    {
		      dimid = dimids[0];
		      break;
		    }
		}
	    }
	}
    }

#endif
  return (dimid);
}


void cdfSetVar(NCVAR *ncvars, int ncvarid, int isvar)
{
  static char func[] = "cdfSetVar";

  if ( isvar != TRUE && isvar != FALSE )
    Error(func, "Internal problem! var %s undefined", ncvars[ncvarid].name);

  if ( ncvars[ncvarid].isvar != UNDEFID &&
       ncvars[ncvarid].isvar != isvar   &&
       ncvars[ncvarid].warn  == FALSE )
    {
      Warning(func, "Inconsistent variable definition for %s!", ncvars[ncvarid].name);

      ncvars[ncvarid].warn = TRUE;
      isvar = FALSE;
    }

  ncvars[ncvarid].isvar = isvar;
}


void cdfSetDim(NCVAR *ncvars, int ncvarid, int dimid, int dimtype)
{
  static char func[] = "cdfSetDim";

  if ( ncvars[ncvarid].dimtype[dimid] != UNDEFID &&
       ncvars[ncvarid].dimtype[dimid] != dimtype )
    {
      Warning(func, "Inconsistent dimension definition for %s! dimid %d  oldtype %d  newtype %d",
	      ncvars[ncvarid].name, dimid, ncvars[ncvarid].dimtype[dimid], dimtype);
    }

  ncvars[ncvarid].dimtype[dimid] = dimtype;
}


void printNCvars(NCVAR *ncvars, int nvars)
{
  char axis[6];
  int ncvarid, i;
  int ndim;
  int iaxis[] = {'t', 'z', 'y', 'x'};

  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      ndim = 0;
      if ( ncvars[ncvarid].isvar )
	{
	  axis[ndim++] = 'v';
	  for ( i = 0; i < ncvars[ncvarid].ndims; i++ )
	    {
	      if ( ncvars[ncvarid].tvarid != -1 )
		axis[ndim++] = iaxis[0];
	      else if ( ncvars[ncvarid].zvarid != -1 )
		axis[ndim++] = iaxis[1];
	      else if ( ncvars[ncvarid].yvarid != -1 )
		axis[ndim++] = iaxis[2];
	      else if ( ncvars[ncvarid].xvarid != -1 )
		axis[ndim++] = iaxis[3];
	      else
		axis[ndim++] = '?';
	    }
	}
      else
	{
	  axis[ndim++] = 'c';
	  if ( ncvars[ncvarid].islev )
	    axis[ndim++] = iaxis[1];
	  else if ( ncvars[ncvarid].islat )
	    axis[ndim++] = iaxis[2];
	  else if ( ncvars[ncvarid].islon )
	    axis[ndim++] = iaxis[3];
	  else
	    axis[ndim++] = '?';
	}

      axis[ndim++] = 0;

      printf("%3d %3d %3d  %s %s\n", ncvarid, ncvars[ncvarid].isvar, ndim-2, axis, ncvars[ncvarid].name);
    }
}


int cdfInqContents(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfInqContents";
  int ndims, nvars, ngatts, unlimdimid;
  int varID, ncvarid, ncvarid2;
  int ncdimid;
  int varID1;
  int fileID;
  int nvdims, nvatts;
  int *dimidsp;
  nc_type xtype;
  size_t size = 0, xsize, ysize;
  size_t ntsteps;
  int timedimid = -1;
  int *varids;
  int nvarids;
  size_t attlen;
  int  ilev;
  char name[256];
  char attname[256];
  const int attstringlen = 1024; char attstring[1024];
  char *pname, *plongname, *punits;
  int iatt, timehasunit = 0;
  int time_has_bounds = FALSE;
  size_t len;
  int nc_nvars;
  int nvcth_id = UNDEFID, vcta_id = UNDEFID, vctb_id = UNDEFID;
  GRID grid;
  int gridindex;
  int zaxisindex;
  size_t vctsize = 0;
  int tablenum, tableID;
  double *vct = NULL;
  int instID  = UNDEFID;
  int modelID = UNDEFID;
  int code;
  int taxisID;
  int i;
  int nbdims;
  size_t nvertex;
  int zprec;
  int calendar = UNDEFID;
  NCDIM *ncdims;
  NCVAR *ncvars;
  int vlistID;
  
  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d, fileID = %d", streamID, fileID);

  cdf_inq(fileID, &ndims , &nvars, &ngatts, &unlimdimid);

  /* alloc ncdims */
  if ( ndims > 0 )
    ncdims = (NCDIM *) malloc(ndims*sizeof(NCDIM));
  else
    {
      Warning(func, "ndims = %d", ndims);
      return (CDI_EUFSTRUCT);
    }
  
  /* alloc ncvars */
  if ( nvars > 0 )
    ncvars = (NCVAR *) malloc(nvars*sizeof(NCVAR));
  else
    {
      Warning(func, "nvars = %d", nvars);
      return (CDI_EUFSTRUCT);
    }

  /* init ncdims */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      ncdims[ncdimid].ncvarid      = UNDEFID;
      ncdims[ncdimid].dimtype      = UNDEFID;
      ncdims[ncdimid].len          = 0;
      ncdims[ncdimid].name[0]      = 0;
    }

  /* init ncvars */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {      
      ncvars[ncvarid].vartype      = UNDEFID;
      ncvars[ncvarid].isvar        = UNDEFID;
      ncvars[ncvarid].islon        = FALSE;
      ncvars[ncvarid].islat        = FALSE;
      ncvars[ncvarid].islev        = FALSE;
      ncvars[ncvarid].warn         = FALSE;
      ncvars[ncvarid].timeID       = TIME_CONSTANT;
      ncvars[ncvarid].code         = UNDEFID;
      ncvars[ncvarid].calendar     = FALSE;
      ncvars[ncvarid].bounds       = UNDEFID;
      ncvars[ncvarid].gridID       = UNDEFID;
      ncvars[ncvarid].zaxisID      = UNDEFID;
      ncvars[ncvarid].gridtype     = UNDEFID;
      ncvars[ncvarid].zaxistype    = UNDEFID;
      ncvars[ncvarid].xdim         = UNDEFID;
      ncvars[ncvarid].ydim         = UNDEFID;
      ncvars[ncvarid].zdim         = UNDEFID;
      ncvars[ncvarid].xvarid       = UNDEFID;
      ncvars[ncvarid].yvarid       = UNDEFID;
      ncvars[ncvarid].zvarid       = UNDEFID;
      ncvars[ncvarid].tvarid       = UNDEFID;
      ncvars[ncvarid].varids[0]    = UNDEFID;
      ncvars[ncvarid].varids[1]    = UNDEFID;
      ncvars[ncvarid].varids[2]    = UNDEFID;
      ncvars[ncvarid].varids[3]    = UNDEFID;
      ncvars[ncvarid].cellarea     = UNDEFID;
      ncvars[ncvarid].tableID      = UNDEFID;
      ncvars[ncvarid].gmapid       = UNDEFID;
      ncvars[ncvarid].vlen         = 0;
      ncvars[ncvarid].vdata        = NULL;
      ncvars[ncvarid].truncation   = 0;
      ncvars[ncvarid].positive     = 0;
      ncvars[ncvarid].defmiss      = 0;
      ncvars[ncvarid].missval      = 0;
      ncvars[ncvarid].addoffset    = 0;
      ncvars[ncvarid].scalefactor  = 1;
      ncvars[ncvarid].name[0]      = 0;
      ncvars[ncvarid].longname[0]  = 0;
      ncvars[ncvarid].stdname[0]   = 0;
      ncvars[ncvarid].units[0]     = 0;
    }

  /* read global attributtes*/
  for ( iatt = 0; iatt < ngatts; iatt++ )
    {
      cdf_inq_attname(fileID, NC_GLOBAL, iatt, attname);
      cdf_inq_atttype(fileID, NC_GLOBAL, attname, &xtype);

      if ( xtype != NC_CHAR ) continue;

      if ( strcmp(attname, "history") == 0 )
	{
	  streams[streamID].historyID = iatt;
	}
      else if ( strcmp(attname, "institution") == 0 )
	{
	  cdfGetAttText(fileID, NC_GLOBAL, attname, attstring, attstringlen);
	  instID = institutInq(0, 0, NULL, attstring);
	  if ( instID == UNDEFID )
	    instID = institutDef(0, 0, NULL, attstring);
	}
      else if ( strcmp(attname, "source") == 0 )
	{
	  cdfGetAttText(fileID, NC_GLOBAL, attname, attstring, attstringlen);
	  modelID = modelInq(-1, 0, attstring);
	}
      else if ( strcmp(attname, "Conventions") == 0 )
	{
	}
      else if ( strcmp(attname, "CDI") == 0 )
	{
	}
      else if ( strcmp(attname, "CDO") == 0 )
	{
	}
      else
	{
	  cdfGetAttText(fileID, NC_GLOBAL, attname, attstring, attstringlen);
	  vlistDefAttribute(vlistID, attname, attstring);
	}
    }

  /* find time dim */
  if ( unlimdimid >= 0 )
    timedimid = unlimdimid;
  else
    timedimid = cdfTimeDimID(fileID, ndims, nvars);

  streams[streamID].basetime.ncdimid = timedimid;

  if ( timedimid != UNDEFID )
    cdf_inq_dimlen(fileID, timedimid, &ntsteps);
  else
    ntsteps = 0;

  streams[streamID].ntsteps = ntsteps;

  if ( CDI_Debug )
    Message(func, "time dimid = %d", streams[streamID].basetime.ncdimid);

  /* read ncdims */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      cdf_inq_dimlen(fileID, ncdimid, &ncdims[ncdimid].len);
      cdf_inq_dimname(fileID, ncdimid, ncdims[ncdimid].name);
      if ( timedimid == ncdimid )
	ncdims[ncdimid].dimtype = T_AXIS;
    }

  /* read ncvars */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      dimidsp = ncvars[ncvarid].dimids;

      cdf_inq_var(fileID, ncvarid, name, &xtype, &nvdims, dimidsp, &nvatts);
      strcpy(ncvars[ncvarid].name, name);      

      for ( ncdimid = 0; ncdimid < nvdims; ncdimid++ )
	ncvars[ncvarid].dimtype[ncdimid] = -1;

      ncvars[ncvarid].xtype = xtype;
      ncvars[ncvarid].ndims = nvdims;

      if ( nvdims > 0 )
	if ( timedimid == dimidsp[0] )
	  {
	    ncvars[ncvarid].timeID = TIME_VARIABLE;
	    cdfSetDim(ncvars, ncvarid, 0, T_AXIS);
	  }

      for ( iatt = 0; iatt < nvatts; iatt++ )
	{
	  cdf_inq_attname(fileID, ncvarid, iatt, attname);
	  
	  if ( strcmp(attname, "long_name") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, ncvars[ncvarid].longname, MAXNAMELEN);
      	    }
	  if ( strcmp(attname, "standard_name") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, ncvars[ncvarid].stdname, MAXNAMELEN);
      	    }
	  else if ( strcmp(attname, "units") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, ncvars[ncvarid].units, MAXNAMELEN);
      	    }
	  else if ( strcmp(attname, "calendar") == 0 )
	    {
	      ncvars[ncvarid].calendar = TRUE;
      	    }
	  else if ( strcmp(attname, "code") == 0 )
	    {
	      cdfGetAttInt(fileID, ncvarid, attname, &ncvars[ncvarid].code);
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "table") == 0 )
	    {
	      cdfGetAttInt(fileID, ncvarid, attname, &tablenum);
	      if ( tablenum > 0 )
		{
		  ncvars[ncvarid].tableID = tableInq(modelID, tablenum, NULL);
		  if ( ncvars[ncvarid].tableID == CDI_UNDEFID )
		    ncvars[ncvarid].tableID = tableDef(modelID, tablenum, NULL);
		}
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "trunc_type") == 0 )
	    {
	      cdf_inq_attlen(fileID, ncvarid, attname, &attlen);
	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);
	      if ( strncmp(attstring, "Triangular", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_SPECTRAL;
	    }
	  else if ( strcmp(attname, "grid_type") == 0 )
	    {
	      cdf_inq_attlen(fileID, ncvarid, attname, &attlen);
	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);

	      if      ( strncmp(attstring, "gaussian", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_GAUSSIAN;
	      else if ( strncmp(attstring, "gaussian reduced", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_GAUSSIAN_REDUCED;
	      else if ( strncmp(attstring, "spectral", 8) == 0 )
		ncvars[ncvarid].gridtype = GRID_SPECTRAL;
	      else if ( strncmp(attstring, "trajectory", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_TRAJECTORY;
	      else if ( strncmp(attstring, "generic", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_GENERIC;
	      else if ( strncmp(attstring, "cell", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_CELL;
	      else if ( strncmp(attstring, "curvilinear", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_CURVILINEAR;
	      else
		{
		  static int warn = TRUE;
		  if ( warn )
		    {
		      warn = FALSE;
		      Warning(func, "Gridtype %s unsupported!", attstring);
		    }
		}

	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "trunc_count") == 0 )
	    {
	      cdfGetAttInt(fileID, ncvarid, attname, &ncvars[ncvarid].truncation);
	    }
	  else if ( strcmp(attname, "truncation") == 0 )
	    {
	      cdfGetAttInt(fileID, ncvarid, attname, &ncvars[ncvarid].truncation);
	    }
	  else if ( strcmp(attname, "add_offset") == 0 )
	    {
	      cdfGetAttDouble(fileID, ncvarid, attname, &ncvars[ncvarid].addoffset);
	      /*
	      if ( xtype != NC_BYTE && xtype != NC_SHORT && xtype != NC_INT )
		if ( ncvars[ncvarid].addoffset != 0 )
		  Warning(func, "attribute add_offset not supported for xtype %d\n", xtype);
	      */
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "scale_factor") == 0 )
	    {
	      cdfGetAttDouble(fileID, ncvarid, attname, &ncvars[ncvarid].scalefactor);
	      /*
	      if ( xtype != NC_BYTE && xtype != NC_SHORT && xtype != NC_INT )
		if ( ncvars[ncvarid].scalefactor != 1 )
		  Warning(func, "attribute scale_factor not supported for xtype %d\n", xtype);
	      */
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "bounds") == 0 )
	    {
	      int status, ncboundsid;

	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);

	      status = nc_inq_varid(fileID, attstring, &ncboundsid);

	      if ( status == NC_NOERR )
		{
		  ncvars[ncvarid].bounds = ncboundsid;
		  cdfSetVar(ncvars, ncvars[ncvarid].bounds, FALSE);
		  cdfSetVar(ncvars, ncvarid, FALSE);
		}
	      else
		Warning(func, "%s %s", nc_strerror(status), attstring);
	    }
	  else if ( strcmp(attname, "cell_measures") == 0 )
	    {
	      char *pstring, *cell_measures = NULL, *cell_var = NULL;

	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);
	      pstring = attstring;

	      while ( isspace((int) *pstring) ) pstring++;
	      cell_measures = pstring;
	      while ( isalnum((int) *pstring) ) pstring++;
	      *pstring++ = 0;
	      while ( isspace((int) *pstring) ) pstring++;
	      cell_var = pstring;
	      while ( ! isspace((int) *pstring) && *pstring != 0 ) pstring++;
	      *pstring++ = 0;
	      /*
	      printf("cell_measures >%s<\n", cell_measures);
	      printf("cell_var >%s<\n", cell_var);
	      */
	      if ( strcmp(cell_measures, "area") == 0 )
		{
		  cdf_inq_varid(fileID, cell_var, &ncvars[ncvarid].cellarea);
		  cdfSetVar(ncvars, ncvars[ncvarid].cellarea, FALSE);
		}
	      else
		{
		  Warning(func, "%s has unexpected contents: %s", attname, cell_measures);
		}
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  /*
	  else if ( strcmp(attname, "coordinates") == 0 )
	    {
	      char *pstring, *xvarname = NULL, *yvarname = NULL;
	      
	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);
	      pstring = attstring;

	      while ( isspace((int) *pstring) ) pstring++;
	      xvarname = pstring;
	      while ( isgraph((int) *pstring) ) pstring++;
	      *pstring++ = 0;
	      while ( isspace((int) *pstring) ) pstring++;
	      yvarname = pstring;
	      while ( isgraph((int) *pstring) ) pstring++;
	      *pstring++ = 0;

	      cdf_inq_varid(fileID, xvarname, &ncvars[ncvarid].xvarid);
	      cdf_inq_varid(fileID, yvarname, &ncvars[ncvarid].yvarid);

	      cdfSetVar(ncvars, ncvars[ncvarid].xvarid, FALSE);
	      cdfSetVar(ncvars, ncvars[ncvarid].yvarid, FALSE);
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  */
	  else if ( strcmp(attname, "associate") == 0 || 
		    strcmp(attname, "coordinates") == 0 )
	    {
	      int status;
	      char *pstring, *varname = NULL;
	      int lstop = FALSE;
	      
	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);
	      pstring = attstring;

	      for ( i = 0; i < 4; i++ )
		{
		  while ( isspace((int) *pstring) ) pstring++;
		  if ( *pstring == 0 ) break;
		  varname = pstring;
		  while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
		  if ( *pstring == 0 ) lstop = TRUE;
		  *pstring++ = 0;

		  status = nc_inq_varid(fileID, varname, &ncvars[ncvarid].varids[i]);
		  if ( status == NC_NOERR )
		    cdfSetVar(ncvars, ncvars[ncvarid].varids[i], FALSE);
		  else
		    Warning(func, "%s %s", nc_strerror(status), varname);

		  if ( lstop ) break;
		}
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "grid_mapping") == 0 )
	    {
	      int status;
	      
	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);

	      status = nc_inq_varid(fileID, attstring, &ncvars[ncvarid].gmapid);
	      if ( status == NC_NOERR )
		cdfSetVar(ncvars, ncvars[ncvarid].gmapid, FALSE);
	      else
		Warning(func, "%s %s", nc_strerror(status), attstring);

	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "positive") == 0 )
	    {	      
	      cdf_inq_attlen(fileID, ncvarid, attname, &attlen);
	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);

	      if    ( strncmp(attstring, "down", attlen) == 0 ) ncvars[ncvarid].positive = -1;
	      else if ( strncmp(attstring, "up", attlen) == 0 ) ncvars[ncvarid].positive = 1;

	      if ( ncvars[ncvarid].ndims == 1 )
		{
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, Z_AXIS);
		  ncdims[ncvars[ncvarid].dimids[0]].dimtype = Z_AXIS;
		}
	    }
	  else if ( strcmp(attname, "_FillValue") == 0 || 
		    strcmp(attname, "missing_value") == 0 )
	    {
	      cdfGetAttDouble(fileID, ncvarid, attname, &ncvars[ncvarid].missval);
	      ncvars[ncvarid].defmiss = TRUE;
	      /* cdfSetVar(ncvars, ncvarid, TRUE); */
	    }
	  else if ( strcmp(attname, "axis") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, attstring, attstringlen);
	      cdf_inq_attlen(fileID, ncvarid, attname, &attlen);
	      /* attlen = strlen(attstring); */
	      /* check attlen */
	      if ( (int) attlen != nvdims )
		{
		  if ( nvdims > 0 )
		    Warning(func, "axis attribute length %d does not match nvdim %d for %s",
			    attlen, nvdims, name);
		}
	      else
		{
		  while ( attlen-- )
		    {
		      if ( tolower((int) attstring[attlen]) == 't' )
			{
			  if ( attlen != 0 )
			    Warning(func, "axis attribute 't' not on first position");
			  cdfSetDim(ncvars, ncvarid, attlen, T_AXIS);
			}
		      else if ( tolower((int) attstring[attlen]) == 'z' )
			{
			  ncvars[ncvarid].zdim = dimidsp[attlen];
			  cdfSetDim(ncvars, ncvarid, attlen, Z_AXIS);
			}
		      else if ( tolower((int) attstring[attlen]) == 'y' )
			{
			  ncvars[ncvarid].ydim = dimidsp[attlen];
			  cdfSetDim(ncvars, ncvarid, attlen, Y_AXIS);
			}
		      else if ( tolower((int) attstring[attlen]) == 'x' )
			{
			  ncvars[ncvarid].xdim = dimidsp[attlen];
			  cdfSetDim(ncvars, ncvarid, attlen, X_AXIS);
			}
		    }
		}
	    }
	}
    }

  /* printNCvars(ncvars, nvars); */

  /* find coordinate vars */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
	{
	  if ( ncvars[ncvarid].ndims == 1 )
	    {
	      if ( ncvars[ncvarid].isvar != TRUE )
		cdfSetVar(ncvars, ncvarid, FALSE);
	      if ( ncdimid == ncvars[ncvarid].dimids[0] && ncdims[ncdimid].ncvarid == UNDEFID )
		if ( strcmp(ncvars[ncvarid].name, ncdims[ncdimid].name) == 0 )
		  {
		    ncdims[ncdimid].ncvarid = ncvarid;
		    ncvars[ncvarid].isvar = FALSE;
		  }
	    }
	}
    }

  /* find time var */
  if ( timedimid != UNDEFID )
    {
      if ( ncdims[timedimid].ncvarid != UNDEFID )
	streams[streamID].basetime.ncvarid = ncdims[timedimid].ncvarid;
      else
	{
	  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
	    if ( ncvars[ncvarid].ndims == 1 && timedimid == ncvars[ncvarid].dimids[0] )
	      {
		streams[streamID].basetime.ncvarid = ncvarid;
		break;
	      }
	}

      /* time varID */
      ncvarid = streams[streamID].basetime.ncvarid;

      if ( ncvarid == UNDEFID )
	Warning(func, "time variable not found!");
      else
	{
	  if ( ncvars[ncvarid].units[0] != 0 )
	    timehasunit = TRUE;

	  if ( ncvars[ncvarid].bounds != UNDEFID )
	    {
	      nbdims = ncvars[ncvars[ncvarid].bounds].ndims;
	      if ( nbdims == 2 )
		{
		  cdf_inq_dimlen(fileID, ncvars[ncvars[ncvarid].bounds].dimids[nbdims-1], &len);
		  if ( (int)len == 2 && timedimid == ncvars[ncvars[ncvarid].bounds].dimids[0] )
		    {
		      time_has_bounds = TRUE;
		      streams[streamID].basetime.ncvarboundsid = ncvars[ncvarid].bounds;
		    }
		}
	    }
	}
    }

  /* check ncvars */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( timedimid != UNDEFID )
	if ( ncvars[ncvarid].isvar == -1 &&
	     ncvars[ncvarid].ndims > 1   &&
	     timedimid == ncvars[ncvarid].dimids[0] )
	  cdfSetVar(ncvars, ncvarid, TRUE);

      if ( ncvars[ncvarid].isvar == -1 && ncvars[ncvarid].ndims == 0 )
	cdfSetVar(ncvars, ncvarid, FALSE);

      if ( ncvars[ncvarid].isvar == -1 && ncvars[ncvarid].ndims > 1 )
	cdfSetVar(ncvars, ncvarid, TRUE);
	  
      if ( ncvars[ncvarid].isvar == -1 )
	{
	  ncvars[ncvarid].isvar = 0;
	  Warning(func, "Variable %s has unknown type, skipped!", ncvars[ncvarid].name);
	}

      if ( ncvars[ncvarid].ndims > 4 )
	{
	  ncvars[ncvarid].isvar = 0;
	  Warning(func, "%d dimension variables unsupported. Skip variable %s",
		ncvars[ncvarid].ndims, ncvars[ncvarid].name);
	}

      if ( ncvars[ncvarid].xtype == NC_CHAR )
	ncvars[ncvarid].isvar = 0;
    }

  /* verify coordinate vars (dimname == varname) */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      ncvarid = ncdims[ncdimid].ncvarid;
      if ( ncvarid != -1 )
	{
	  if ( ncvars[ncvarid].dimids[0] == timedimid )
	    {
	      ncdims[ncdimid].dimtype = T_AXIS;
	      continue;
	    }

	  if ( ncvars[ncvarid].longname[0] != 0 )
	    {
	      if ( strncmp(ncvars[ncvarid].longname, "Longitude", 9) == 0 ||
		   strncmp(ncvars[ncvarid].longname, "longitude", 9) == 0 )
		{
		  ncvars[ncvarid].islon = TRUE;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, X_AXIS);
		  ncdims[ncdimid].dimtype = X_AXIS;
		  continue;
		}
	      else if ( strncmp(ncvars[ncvarid].longname, "Latitude", 8) == 0 || 
			strncmp(ncvars[ncvarid].longname, "latitude", 8) == 0 )
		{
		  ncvars[ncvarid].islat = TRUE;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, Y_AXIS);
		  ncdims[ncdimid].dimtype = Y_AXIS;
		  continue;
		}
	    }

	  if ( ncvars[ncvarid].units[0] != 0 )
	    {
	      if ( strcmp(ncvars[ncvarid].units, "degrees_east") == 0 ||
		   strcmp(ncvars[ncvarid].units, "degree_east")  == 0 ||
		   strcmp(ncvars[ncvarid].units, "degree_E")     == 0 ||
		   strcmp(ncvars[ncvarid].units, "degrees_E")    == 0 ||
		   strcmp(ncvars[ncvarid].units, "degreeE")      == 0 ||
		   strcmp(ncvars[ncvarid].units, "degreesE")     == 0 ||
		  (strcmp(ncvars[ncvarid].units, "degrees")          == 0 &&
                   strcmp(ncvars[ncvarid].stdname, "grid_longitude") == 0) )
		{
		  ncvars[ncvarid].islon = TRUE;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, X_AXIS);
		  ncdims[ncdimid].dimtype = X_AXIS;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "degrees_north") == 0 ||
			strcmp(ncvars[ncvarid].units, "degree_north")  == 0 ||
			strcmp(ncvars[ncvarid].units, "degree_N")      == 0 ||
			strcmp(ncvars[ncvarid].units, "degrees_N")     == 0 ||
			strcmp(ncvars[ncvarid].units, "degreeN")       == 0 ||
			strcmp(ncvars[ncvarid].units, "degreesN")      == 0 ||
		       (strcmp(ncvars[ncvarid].units, "degrees")         == 0 &&
                        strcmp(ncvars[ncvarid].stdname, "grid_latitude") == 0) )
		{
		  ncvars[ncvarid].islat = TRUE;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, Y_AXIS);
		  ncdims[ncdimid].dimtype = Y_AXIS;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "millibar") == 0 ||
			strncmp(ncvars[ncvarid].units, "mb", 2)   == 0 ||
			strcmp(ncvars[ncvarid].units, "hPa")      == 0 ||
			strcmp(ncvars[ncvarid].units, "Pa")       == 0 )
		{
		  ncvars[ncvarid].zaxistype = ZAXIS_PRESSURE;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "level") == 0 )
		{
		  if      ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer midpoints") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer interfaces") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else
		    ncvars[ncvarid].zaxistype = ZAXIS_GENERIC;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "cm")  == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth below land")         == 0 ||
		       strcmp(ncvars[ncvarid].longname, "levels below the surface") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_LAND;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "m")   == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth below sea") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_SEA;
		  else if ( strcmp(ncvars[ncvarid].longname, "height") == 0 ||
			    strcmp(ncvars[ncvarid].longname, "height above the surface") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HEIGHT;
		}
	    }

	  if ( ncvars[ncvarid].zaxistype != UNDEFID )
	    {
	      cdfSetVar(ncvars, ncvarid, FALSE);
	      cdfSetDim(ncvars, ncvarid, 0, Z_AXIS);
	      ncdims[ncdimid].dimtype = Z_AXIS;
	    }
	}
    }

  /* verify coordinate vars */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == 0 )
	{
	  /* not needed anymore for rotated grids */
	  if ( ncvars[ncvarid].longname[0] != 0 )
	    {
	      if ( strncmp(ncvars[ncvarid].longname, "Longitude", 9) == 0 ||
		   strncmp(ncvars[ncvarid].longname, "longitude", 9) == 0 )
		{
		  ncvars[ncvarid].islon = TRUE;
		  continue;
		}
	      else if ( strncmp(ncvars[ncvarid].longname, "Latitude", 8) == 0 || 
			strncmp(ncvars[ncvarid].longname, "latitude", 8) == 0 )
		{
		  ncvars[ncvarid].islat = TRUE;
		  continue;
		}
	    }

	  if ( ncvars[ncvarid].units[0] != 0 )
	    {
	      if ( strcmp(ncvars[ncvarid].units, "degrees_east") == 0 ||
		   strcmp(ncvars[ncvarid].units, "degree_east")  == 0 ||
		   strcmp(ncvars[ncvarid].units, "degree_E")     == 0 ||
		   strcmp(ncvars[ncvarid].units, "degrees_E")    == 0 ||
		   strcmp(ncvars[ncvarid].units, "degreeE")      == 0 ||
		   strcmp(ncvars[ncvarid].units, "degreesE")     == 0 ||
		  (strcmp(ncvars[ncvarid].units, "degrees")          == 0 &&
                   strcmp(ncvars[ncvarid].stdname, "grid_longitude") == 0) )
		{
		  ncvars[ncvarid].islon = TRUE;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "degrees_north") == 0 ||
			strcmp(ncvars[ncvarid].units, "degree_north")  == 0 ||
			strcmp(ncvars[ncvarid].units, "degree_N")      == 0 ||
			strcmp(ncvars[ncvarid].units, "degrees_N")     == 0 ||
			strcmp(ncvars[ncvarid].units, "degreeN")       == 0 ||
			strcmp(ncvars[ncvarid].units, "degreesN")      == 0 ||
		       (strcmp(ncvars[ncvarid].units, "degrees")         == 0 &&
                        strcmp(ncvars[ncvarid].stdname, "grid_latitude") == 0) )
		{
		  ncvars[ncvarid].islat = TRUE;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "millibar") == 0 ||
			strncmp(ncvars[ncvarid].units, "mb", 2)   == 0 ||
			strcmp(ncvars[ncvarid].units, "hPa")      == 0 ||
			strcmp(ncvars[ncvarid].units, "Pa")       == 0 )
		{
		  ncvars[ncvarid].zaxistype = ZAXIS_PRESSURE;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "level") == 0 )
		{
		  if      ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer midpoints") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer interfaces") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else
		    ncvars[ncvarid].zaxistype = ZAXIS_GENERIC;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "cm")  == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth below land")         == 0 ||
		       strcmp(ncvars[ncvarid].longname, "levels below the surface") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_LAND;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "m")   == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth below sea") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_SEA;
		  else if ( strcmp(ncvars[ncvarid].longname, "height") == 0 ||
			    strcmp(ncvars[ncvarid].longname, "height above the surface") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HEIGHT;
		  continue;
		}
	    }
	}
    }

  /* printNCvars(ncvars, nvars); */

  /*
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      ncvarid = ncdims[ncdimid].ncvarid;
      if ( ncvarid != -1 )
	{
	  printf("coord var %d %s %s\n", ncvarid, ncvars[ncvarid].name, ncvars[ncvarid].units);
	  if ( ncdims[ncdimid].dimtype == X_AXIS )
	    printf("coord var %d %s is x dim\n", ncvarid, ncvars[ncvarid].name);
	  if ( ncdims[ncdimid].dimtype == Y_AXIS )
	    printf("coord var %d %s is y dim\n", ncvarid, ncvars[ncvarid].name);
	  if ( ncdims[ncdimid].dimtype == Z_AXIS )
	    printf("coord var %d %s is z dim\n", ncvarid, ncvars[ncvarid].name);
	  if ( ncdims[ncdimid].dimtype == T_AXIS )
	    printf("coord var %d %s is t dim\n", ncvarid, ncvars[ncvarid].name);

	  if ( ncvars[ncvarid].islon )
	    printf("coord var %d %s is lon\n", ncvarid, ncvars[ncvarid].name);
	  if ( ncvars[ncvarid].islat )
	    printf("coord var %d %s is lat\n", ncvarid, ncvars[ncvarid].name);
	  if ( ncvars[ncvarid].islev )
	    printf("coord var %d %s is lev\n", ncvarid, ncvars[ncvarid].name);
	}
    }
  */
  /* set dim type */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE )
	{
	  int lxdim = 0, lydim = 0, lzdim = 0, ltdim = 0;
	  ndims = ncvars[ncvarid].ndims;
	  for ( i = 0; i < ndims; i++ )
	    {
	      ncdimid = ncvars[ncvarid].dimids[i];
	      if ( ncdims[ncdimid].dimtype == X_AXIS )
		cdfSetDim(ncvars, ncvarid, i, X_AXIS);
	      else if ( ncdims[ncdimid].dimtype == Y_AXIS )
		cdfSetDim(ncvars, ncvarid, i, Y_AXIS);
	      else if ( ncdims[ncdimid].dimtype == Z_AXIS )
		cdfSetDim(ncvars, ncvarid, i, Z_AXIS);
	      else if ( ncdims[ncdimid].dimtype == T_AXIS )
		cdfSetDim(ncvars, ncvarid, i, T_AXIS);
	    }

	  if ( CDI_Debug )
	    {
	      Message(func, "var %d %s", ncvarid, ncvars[ncvarid].name);
	      for ( i = 0; i < ndims; i++ )
		printf("  dim %d type %d  ", i, ncvars[ncvarid].dimtype[i]);
	      printf("\n");
	    }

	  for ( i = 0; i < ndims; i++ )
	    {
	      if      ( ncvars[ncvarid].dimtype[i] == X_AXIS ) lxdim = TRUE;
	      else if ( ncvars[ncvarid].dimtype[i] == Y_AXIS ) lydim = TRUE;
	      else if ( ncvars[ncvarid].dimtype[i] == Z_AXIS ) lzdim = TRUE;
	      else if ( ncvars[ncvarid].dimtype[i] == T_AXIS ) ltdim = TRUE;
	    }

	  for ( i = ndims-1; i >= 0; i-- )
	    {
	      if ( ncvars[ncvarid].dimtype[i] == -1 )
		{
		  /*
		  printf("undef dim: %d %d %d %d %d\n", i, lxdim, lydim, lzdim, ltdim);
		  */
		  if ( lxdim == FALSE )
		    {
		      cdfSetDim(ncvars, ncvarid, i, X_AXIS);
		      lxdim = TRUE;
		    }
		  else if ( lydim == FALSE )
		    {
		      cdfSetDim(ncvars, ncvarid, i, Y_AXIS);
		      lydim = TRUE;
		    }
		  else if ( lzdim == FALSE )
		    {
		      cdfSetDim(ncvars, ncvarid, i, Z_AXIS);
		      lzdim = TRUE;
		    }
		}
	    }	    	  
	}
    }
  /* set coordinate varids (att: associate)  */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE )
	{
	  ndims = ncvars[ncvarid].ndims;
	  for ( i = 0; i < ndims; i++ )
	    {
	      if ( ncvars[ncvarid].varids[i] != -1 )
		{
		  if ( ncvars[ncvars[ncvarid].varids[i]].islon )
		    ncvars[ncvarid].xvarid = ncvars[ncvarid].varids[i];
		  else if ( ncvars[ncvars[ncvarid].varids[i]].islat )
		    ncvars[ncvarid].yvarid = ncvars[ncvarid].varids[i];
		  else if ( ncvars[ncvars[ncvarid].varids[i]].islev )
		    ncvars[ncvarid].zvarid = ncvars[ncvarid].varids[i];
		}
	    }
	}
    }
  
  /* printNCvars(ncvars, nvars); */

  /* define grid */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar && ncvars[ncvarid].gridID == UNDEFID )
	{
	  int xdimid = -1, ydimid = -1;
	  int xvarid = -1, yvarid = -1;
	  int islon = 0, islat = 0;
	  double xinc = 0, yinc = 0;

	  xsize = 0;
	  ysize = 0;

	  ndims = ncvars[ncvarid].ndims;
	  for ( i = 0; i < ndims; i++ )
	    {
	      if ( ncvars[ncvarid].dimtype[i] == X_AXIS )
		xdimid = ncvars[ncvarid].dimids[i];
	      else if ( ncvars[ncvarid].dimtype[i] == Y_AXIS )
		ydimid = ncvars[ncvarid].dimids[i];
	    }
	  
	  /*
	  if ( ncvars[ncvarid].xvarid != UNDEFID )
	    xvarid = ncvars[ncvarid].xvarid;
	  else if ( xdimid != UNDEFID )
	    xvarid = ncdims[xdimid].ncvarid;

	  if ( ncvars[ncvarid].yvarid != UNDEFID )
	    yvarid = ncvars[ncvarid].yvarid;
	  else if ( ydimid != UNDEFID )
	    yvarid = ncdims[ydimid].ncvarid;
	  */
	  
	  if ( xdimid != UNDEFID )
	    xvarid = ncdims[xdimid].ncvarid;
	  if ( xvarid == UNDEFID && ncvars[ncvarid].xvarid != UNDEFID )
	    xvarid = ncvars[ncvarid].xvarid;

	  if ( ydimid != UNDEFID )
	    yvarid = ncdims[ydimid].ncvarid;
	  if ( yvarid == UNDEFID && ncvars[ncvarid].yvarid != UNDEFID )
	    yvarid = ncvars[ncvarid].yvarid;

	  if ( xdimid != UNDEFID )
	    cdf_inq_dimlen(fileID, xdimid, &xsize);

	  if ( ydimid != UNDEFID )
	    cdf_inq_dimlen(fileID, ydimid, &ysize);

	  if ( ydimid == UNDEFID && yvarid != UNDEFID )
	    {
	      if ( ncvars[yvarid].ndims == 1 )
		{
		  ydimid = ncvars[yvarid].dimids[0];
		  cdf_inq_dimlen(fileID, ydimid, &ysize);
		}   
	    }

	  if ( ncvars[ncvarid].gridtype == UNDEFID || ncvars[ncvarid].gridtype == GRID_GENERIC )
	    if ( ydimid == xdimid ) ncvars[ncvarid].gridtype = GRID_CELL;

	  memset(&grid, 0, sizeof(GRID));

	  grid.prec = DATATYPE_REAL8;
	  grid.trunc = ncvars[ncvarid].truncation;
	  
	  if ( ncvars[ncvarid].gridtype == GRID_TRAJECTORY )
	    {
	      if ( ncvars[ncvarid].xvarid == UNDEFID )
		Error(func, "Longitude coordinate undefined for %s!", name);
	      if ( ncvars[ncvarid].yvarid == UNDEFID )
		Error(func, "Latitude coordinate undefined for %s!", name);
	    }
	  else
	    {
	      if ( xvarid != UNDEFID )
		{
		  islon = ncvars[xvarid].islon;
		  if ( ncvars[xvarid].ndims == 2 )
		    {
		      ncvars[ncvarid].gridtype = GRID_CURVILINEAR;
		      size = xsize*ysize;
		    }
		  else
		    {
		      size = xsize;
		    }

		  if ( ncvars[xvarid].xtype == NC_FLOAT ) grid.prec = DATATYPE_REAL4;
		  grid.xvals = (double *) malloc(size*sizeof(double));
		  cdf_get_var_double(fileID, xvarid, grid.xvals);
		  strcpy(grid.xname, ncvars[xvarid].name);
		  strcpy(grid.xlongname, ncvars[xvarid].longname);
		  strcpy(grid.xunits, ncvars[xvarid].units);
		  if ( (len = strlen(grid.xname)) > 2 )
		    if ( grid.xname[len-2] == '_' && isdigit((int) grid.xname[len-1]) )
		      grid.xname[len-2] = 0;

		  if ( islon && xsize > 1 )
		    {
		      xinc = fabs(grid.xvals[0] - grid.xvals[1]);
		      for ( i = 2; i < (int) xsize; i++ )
			if ( (fabs(grid.xvals[i-1] - grid.xvals[i]) - xinc) > (xinc/1000) ) break;
		  
		      if ( i < (int) xsize ) xinc = 0;
		    }
		}
	      if ( yvarid != UNDEFID )
		{
		  islat = ncvars[yvarid].islat;
		  if ( ncvars[yvarid].ndims == 2 )
		    {
		      ncvars[ncvarid].gridtype = GRID_CURVILINEAR;
		      size = xsize*ysize;
		    }
		  else
		    {
		      if ( (int) ysize == 0 ) size = xsize;
		      else                    size = ysize;
		    }

		  if ( ncvars[yvarid].xtype == NC_FLOAT ) grid.prec = DATATYPE_REAL4;
		  grid.yvals = (double *) malloc(size*sizeof(double));
		  cdf_get_var_double(fileID, yvarid, grid.yvals);
		  strcpy(grid.yname, ncvars[yvarid].name);
		  strcpy(grid.ylongname, ncvars[yvarid].longname);
		  strcpy(grid.yunits, ncvars[yvarid].units);
		  if ( (len = strlen(grid.yname)) > 2 )
		    if ( grid.yname[len-2] == '_' && isdigit((int) grid.yname[len-1]) )
		      grid.yname[len-2] = 0;

		  if ( islon && (int) ysize > 1 )
		    {
		      yinc = fabs(grid.yvals[0] - grid.yvals[1]);
		      for ( i = 2; i < (int) ysize; i++ )
			if ( (fabs(grid.yvals[i-1] - grid.yvals[i]) - yinc) > (yinc/1000) ) break;
		  
		      if ( i < (int) ysize ) yinc = 0;
		    }
		}

	      if      ( (int) ysize == 0 ) size = xsize;
	      else if ( (int) xsize == 0 ) size = ysize;
	      else if ( ncvars[ncvarid].gridtype == GRID_CELL ) size = xsize; 
	      else                         size = xsize*ysize;
	    }

	  if ( ncvars[ncvarid].gridtype == UNDEFID ||
	       ncvars[ncvarid].gridtype == GRID_GENERIC )
	    {
	      if ( islat && islon )
		{
		  int lgauss = FALSE;
		  if ( yinc == 0 && (int) ysize > 2 ) /* check if gaussian */
		    {
		      double *yvals, *yw;
		      yvals = (double *) malloc(ysize*sizeof(double));
		      yw    = (double *) malloc(ysize*sizeof(double));
		      gaussaw(yvals, yw, ysize);
		      free(yw);
		      for ( i = 0; i < (int) ysize; i++ )
			yvals[i] = asin(yvals[i])/M_PI*180.0;

		      for ( i = 0; i < (int) ysize; i++ )
			if ( fabs(yvals[i] - grid.yvals[i]) >
			     ((yvals[0] - yvals[1])/500) ) break;
		      
		      if ( i == (int) ysize ) lgauss = TRUE;

		      /* check S->N */
		      if ( lgauss == FALSE )
			{		  
			  for ( i = 0; i < (int) ysize; i++ )
			    if ( fabs(yvals[i] - grid.yvals[ysize-i-1]) >
				 ((yvals[0] - yvals[1])/500) ) break;
		      
			  if ( i == (int) ysize ) lgauss = TRUE;
			}

		      free(yvals);
		    }

		  if ( lgauss )
		    ncvars[ncvarid].gridtype = GRID_GAUSSIAN;
		  else
		    ncvars[ncvarid].gridtype = GRID_LONLAT;
		}
	      else
		ncvars[ncvarid].gridtype = GRID_GENERIC;
	    }

	  switch (ncvars[ncvarid].gridtype)
	    {
	    case GRID_GENERIC:
	    case GRID_LONLAT:
	    case GRID_GAUSSIAN:
	    case GRID_CELL:
	    case GRID_CURVILINEAR:
	      {
		grid.size  = size;
		grid.xsize = xsize;
		grid.ysize = ysize;
		if ( xvarid != UNDEFID )
		  {
		    grid.xdef  = 1;
		    if ( ncvars[xvarid].bounds != UNDEFID )
		      {
			nbdims = ncvars[ncvars[xvarid].bounds].ndims;
			if ( nbdims == 2 || nbdims == 3 )
			  {
			    cdf_inq_dimlen(fileID, ncvars[ncvars[xvarid].bounds].dimids[nbdims-1], &nvertex);
			    grid.nvertex = (int) nvertex;
			    grid.xbounds = (double *) malloc(nvertex*size*sizeof(double));
			    cdf_get_var_double(fileID, ncvars[xvarid].bounds, grid.xbounds);
			  }
		      }
		  }
		if ( yvarid != UNDEFID )
		  {
		    grid.ydef  = 1;
		    if ( ncvars[yvarid].bounds != UNDEFID )
		      {
			nbdims = ncvars[ncvars[yvarid].bounds].ndims;
			if ( nbdims == 2 || nbdims == 3 )
			  {
			    cdf_inq_dimlen(fileID, ncvars[ncvars[yvarid].bounds].dimids[nbdims-1], &nvertex);
			    /*
			    if ( nvertex != grid.nvertex )
			      Warning(func, "nvertex problem! nvertex x %d, nvertex y %d",
				      grid.nvertex, (int) nvertex);
			    */
			    grid.ybounds = (double *) malloc(nvertex*size*sizeof(double));
			    cdf_get_var_double(fileID, ncvars[yvarid].bounds, grid.ybounds);
			  }
		      }
		  }

		if ( ncvars[ncvarid].cellarea != UNDEFID )
		  {
		    grid.area = (double *) malloc(size*sizeof(double));
		    cdf_get_var_double(fileID, ncvars[ncvarid].cellarea, grid.area);
		  }

		break;
	      }
	    case GRID_SPECTRAL:
	      {
		grid.size = size;
		break;
	      }
	    case GRID_TRAJECTORY:
	      {
		grid.size = 1;
		break;
	      }
	    }

	  grid.type = ncvars[ncvarid].gridtype;

	  if ( grid.size == 0 )
	    {
	      Warning(func, "Variable %s has unsupported grid, skipped!", ncvars[ncvarid].name);
	      ncvars[ncvarid].isvar = -1;
	      continue;
	    }

	  if ( ncvars[ncvarid].gmapid >= 0 )
	    {
	      cdf_inq_varnatts(fileID, ncvars[ncvarid].gmapid, &nvatts);

	      for ( iatt = 0; iatt < nvatts; iatt++ )
		{
		  cdf_inq_attname(fileID, ncvars[ncvarid].gmapid, iatt, attname);

		  if ( strcmp(attname, "grid_mapping_name") == 0 )
		    {
		      cdfGetAttText(fileID, ncvars[ncvarid].gmapid, attname, attstring, attstringlen);

		      if ( strcmp(attstring, "rotated_latitude_longitude") == 0 )
			grid.isRotated = TRUE;
		    }
		  else if ( strcmp(attname, "grid_north_pole_latitude") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, &grid.ypole);
		    }
		  else if ( strcmp(attname, "grid_north_pole_longitude") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, &grid.xpole);
		    }
		}

	      grid.angle  = 0.0;
	    }

	  if ( CDI_Debug )
	    Message(func, "grid: type = %d, size = %d, nx = %d, ny %d",
		    grid.type, grid.size, grid.xsize, grid.ysize);

	  ncvars[ncvarid].gridID = varDefGrid(vlistID, grid, 1);

	  gridindex = vlistGridIndex(vlistID, ncvars[ncvarid].gridID);
	  streams[streamID].xdimID[gridindex] = xdimid;
	  streams[streamID].ydimID[gridindex] = ydimid;

	  if ( grid.xvals )   free(grid.xvals);
	  if ( grid.yvals )   free(grid.yvals);
	  if ( grid.xbounds ) free(grid.xbounds);
	  if ( grid.ybounds ) free(grid.ybounds);
	  if ( grid.area )    free(grid.area);

	  if ( CDI_Debug )
	    Message(func, "gridID %d %d %s", ncvars[ncvarid].gridID, ncvarid, ncvars[ncvarid].name);

	  for ( ncvarid2 = ncvarid+1; ncvarid2 < nvars; ncvarid2++ )
	    if ( ncvars[ncvarid2].isvar == TRUE && ncvars[ncvarid2].gridID == UNDEFID )
	      {
		int xdimid2 = -1, ydimid2 = -1;
		ndims = ncvars[ncvarid2].ndims;
		for ( i = 0; i < ndims; i++ )
		  {
		    if ( ncvars[ncvarid2].dimtype[i] == X_AXIS )
		      xdimid2 = ncvars[ncvarid2].dimids[i];
		    else if ( ncvars[ncvarid2].dimtype[i] == Y_AXIS )
		      ydimid2 = ncvars[ncvarid2].dimids[i];
		  }

		if ( xdimid == xdimid2 &&
		     (ydimid == ydimid2 || (xdimid == ydimid && ydimid2 == UNDEFID)) )
		  {
		    if ( CDI_Debug )
		      Message(func, "gridID %d %d %s",
			      ncvars[ncvarid].gridID, ncvarid2, ncvars[ncvarid2].name);
		    ncvars[ncvarid2].gridID = ncvars[ncvarid].gridID;
		  }
	      }
	}
    }

  /* find VCT */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].ndims == 1 )
	{
	  if ( strncmp(ncvars[ncvarid].name, "hyai", 4) == 0 )
	    {
	      vcta_id = ncvarid;
	      nvcth_id = ncvars[ncvarid].dimids[0];
	      continue;
	    }
	  if ( strncmp(ncvars[ncvarid].name, "hybi", 4) == 0 )
	    {
	      vctb_id = ncvarid;
	      nvcth_id = ncvars[ncvarid].dimids[0];
	      continue;
	    }
	}
    }

  /* read VCT */
  if ( nvcth_id != UNDEFID && vcta_id != UNDEFID && vctb_id != UNDEFID )
    {
      cdf_inq_dimlen(fileID, nvcth_id, &vctsize);
      vctsize *= 2;
      vct = (double *) malloc(vctsize*sizeof(double));
      cdf_get_var_double(fileID, vcta_id, vct);
      cdf_get_var_double(fileID, vctb_id, vct+vctsize/2);
    }

  /* define zaxis */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE && ncvars[ncvarid].zaxisID == UNDEFID )
	{
	  int zdimid = UNDEFID;
	  int zvarid = UNDEFID;
	  size_t zsize = 1;
	  double *zvar = NULL;
	  int zaxisType;

	  ndims = ncvars[ncvarid].ndims;
	  for ( i = 0; i < ndims; i++ )
	    {
	      if ( ncvars[ncvarid].dimtype[i] == Z_AXIS )
		zdimid = ncvars[ncvarid].dimids[i];
	    }

	  if ( zdimid != UNDEFID )
	    zvarid = ncdims[zdimid].ncvarid;

	  if ( zdimid != UNDEFID )
	    cdf_inq_dimlen(fileID, zdimid, &zsize);

	  if ( CDI_Debug ) Message(func, "nlevs = %ld", zsize);
    
	  zvar = (double *) malloc(zsize*sizeof(double));

	  zaxisType = UNDEFID;

	  if ( zvarid != UNDEFID ) zaxisType = ncvars[zvarid].zaxistype;
	  
	  if ( zaxisType == UNDEFID )  zaxisType = ZAXIS_GENERIC;

	  zprec = DATATYPE_REAL8;

	  if ( zvarid != UNDEFID )
	    {
	      pname     = ncvars[zvarid].name;
	      plongname = ncvars[zvarid].longname;
	      punits    = ncvars[zvarid].units;
	      if ( ncvars[zvarid].xtype == NC_FLOAT ) zprec = DATATYPE_REAL4;
	      if ( (len = strlen(pname)) > 2 )
		if ( pname[len-2] == '_' && isdigit((int) pname[len-1]) )
		  pname[len-2] = 0;
	      
	      cdf_get_var_double(fileID, zvarid, zvar);
	    }
	  else
	    {
	      pname     = NULL;
	      plongname = NULL;
	      punits    = NULL;

	      if ( zsize == 1 )
		{
		  zaxisType = ZAXIS_SURFACE;
		  zvar[0] = 0;
		  /*
		  if ( zdimid == UNDEFID )
		    zvar[0] = 9999;
		  else
		    zvar[0] = 0;
		  */
		}
	      else
		{
		  for ( ilev = 0; ilev < (int)zsize; ilev++ ) zvar[ilev] = ilev + 1;
		}
	    }

      	  ncvars[ncvarid].zaxisID = varDefZaxis(vlistID, zaxisType, (int) zsize, zvar, 0, NULL, NULL,
						vctsize, vct, pname, plongname, punits, zprec, 1);
	  free(zvar);

	  zaxisindex = vlistZaxisIndex(vlistID, ncvars[ncvarid].zaxisID);
	  streams[streamID].zaxisID[zaxisindex]  = zdimid;	  

	  if ( CDI_Debug )
	    Message(func, "zaxisID %d %d %s", ncvars[ncvarid].zaxisID, ncvarid, ncvars[ncvarid].name);

	  for ( ncvarid2 = ncvarid+1; ncvarid2 < nvars; ncvarid2++ )
	    if ( ncvars[ncvarid2].isvar == TRUE && ncvars[ncvarid2].zaxisID == UNDEFID )
	      {
		int zdimid2 = -1;
		ndims = ncvars[ncvarid2].ndims;
		for ( i = 0; i < ndims; i++ )
		  {
		    if ( ncvars[ncvarid2].dimtype[i] == Z_AXIS )
		      zdimid2 = ncvars[ncvarid2].dimids[i];
		  }
		if ( zdimid == zdimid2 )
		  {
		    if ( CDI_Debug )
		      Message(func, "zaxisID %d %d %s",
			      ncvars[ncvarid].zaxisID, ncvarid2, ncvars[ncvarid2].name);
		    ncvars[ncvarid2].zaxisID = ncvars[ncvarid].zaxisID;
		  }
	      }
	}
    }

  if ( vct ) free(vct);

  /* select vars */
  varids = (int *) malloc(nvars*sizeof(int));
  nvarids = 0;
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE ) varids[nvarids++] = ncvarid;
    }
  nc_nvars = nvars;

  if ( CDI_Debug )
    Message(func, "time varid = %d", streams[streamID].basetime.ncvarid);

  if ( CDI_Debug )
    Message(func, "ntsteps = %d", streams[streamID].ntsteps);

  nvars = nvarids;
  if ( CDI_Debug )
    Message(func, "nvars = %d", nvars);

  if ( nvars == 0 )
    return (CDI_EUFSTRUCT);

  for ( varID1 = 0; varID1 < nvars; varID1++ )
    {
      ncvarid = varids[varID1];

      varID = streamNewVar(streamID, ncvars[ncvarid].gridID, ncvars[ncvarid].zaxisID);
      varID = vlistDefVar(vlistID, ncvars[ncvarid].gridID, ncvars[ncvarid].zaxisID, ncvars[ncvarid].timeID);

      streams[streamID].vars[varID1].level   = NULL;
      streams[streamID].vars[varID1].defmiss = 0;
      streams[streamID].vars[varID1].nlevs   = zaxisInqSize(ncvars[ncvarid].zaxisID);
      streams[streamID].vars[varID1].ncvarid = ncvarid;

      vlistDefVarName(vlistID, varID, ncvars[ncvarid].name);
      if ( ncvars[ncvarid].code != UNDEFID )  vlistDefVarCode(vlistID, varID, ncvars[ncvarid].code);
      if ( ncvars[ncvarid].longname[0] )      vlistDefVarLongname(vlistID, varID, ncvars[ncvarid].longname);
      if ( ncvars[ncvarid].stdname[0] )       vlistDefVarStdname(vlistID, varID, ncvars[ncvarid].stdname);
      if ( ncvars[ncvarid].units[0] )         vlistDefVarUnits(vlistID, varID, ncvars[ncvarid].units);
      if ( ncvars[ncvarid].addoffset != 0 )   vlistDefVarAddoffset(vlistID, varID, ncvars[ncvarid].addoffset);
      if ( ncvars[ncvarid].scalefactor != 1 ) vlistDefVarScalefactor(vlistID, varID, ncvars[ncvarid].scalefactor);

      vlistDefVarDatatype(vlistID, varID, cdfInqDatatype(ncvars[ncvarid].xtype));

      vlistDefVarInstitut(vlistID, varID, instID);
      vlistDefVarModel(vlistID, varID, modelID);
      vlistDefVarTable(vlistID, varID, ncvars[ncvarid].tableID);

      if ( ncvars[ncvarid].defmiss == TRUE ) vlistDefVarMissval(vlistID, varID, ncvars[ncvarid].missval);

      if ( CDI_Debug )
	Message(func, "varID = %d  gridID = %d  zaxisID = %d", varID,
		vlistInqVarGrid(vlistID, varID), vlistInqVarZaxis(vlistID, varID));
    }

  if ( varids ) free(varids);

  for ( varID = 0; varID < nvars; varID++ )
    {
      if ( vlistInqVarCode(vlistID, varID) == -varID-1 )
	{
	  char *pname;
	  pname = vlistInqVarNamePtr(vlistID, varID);
	  len = strlen(pname);
	  if ( len > 3 && isdigit((int) pname[3]) )
	    if ( strncmp("var", pname, 3) == 0 )
	      {
		vlistDefVarCode(vlistID, varID, atoi(pname+3));
		vlistFreeVarName(vlistID, varID);
	      }
	}
      if ( vlistInqVarCode(vlistID, varID) == -varID-1 )
	{
	  char *pname;
	  pname = vlistInqVarNamePtr(vlistID, varID);
	  len = strlen(pname);
	  if ( len > 4 && isdigit((int) pname[4]) )
	    if ( strncmp("code", pname, 4) == 0 )
	      {
		vlistDefVarCode(vlistID, varID, atoi(pname+4));
		vlistFreeVarName(vlistID, varID);
	      }
	}
    }

  for ( varID = 0; varID < nvars; varID++ )
    {
      instID  = vlistInqVarInstitut(vlistID, varID);
      modelID = vlistInqVarModel(vlistID, varID);
      tableID = vlistInqVarTable(vlistID, varID);
      code    = vlistInqVarCode(vlistID, varID);
      if ( cdiDefaultTableID != UNDEFID )
	{
	  if ( tableInqParNamePtr(cdiDefaultTableID, code) )
	    {
	      vlistFreeVarName(vlistID, varID);
	      vlistFreeVarLongname(vlistID, varID);
	      vlistFreeVarUnits(vlistID, varID);

	      if ( tableID != UNDEFID )
		{
		  vlistDefVarName(vlistID, varID, tableInqParNamePtr(cdiDefaultTableID, code));
		  if ( tableInqParLongnamePtr(cdiDefaultTableID, code) )
		    vlistDefVarLongname(vlistID, varID, tableInqParLongnamePtr(cdiDefaultTableID, code));
		  if ( tableInqParUnitsPtr(cdiDefaultTableID, code) )
		    vlistDefVarUnits(vlistID, varID, tableInqParUnitsPtr(cdiDefaultTableID, code));
		}
	      else
		{
		  tableID = cdiDefaultTableID;
		}
	    }
	  if ( cdiDefaultModelID != UNDEFID )
	    modelID = cdiDefaultModelID;
	  if ( cdiDefaultInstID != UNDEFID )
	    instID = cdiDefaultInstID;
	}
      if ( instID  != UNDEFID ) vlistDefVarInstitut(vlistID, varID, instID);
      if ( modelID != UNDEFID ) vlistDefVarModel(vlistID, varID, modelID);
      if ( tableID != UNDEFID ) vlistDefVarTable(vlistID, varID, tableID);
    }

  cdiCreateTimesteps(streamID);

  /* time varID */
  ncvarid = streams[streamID].basetime.ncvarid;

  if ( timehasunit )
    {
      TAXIS *taxis;
      taxis = &streams[streamID].tsteps[0].taxis;

      cdfGetAttText(fileID, ncvarid, "units", attstring, attstringlen);
      if ( splitBasetime(attstring, taxis) == 1 )
	streams[streamID].basetime.ncvarid = UNDEFID;
    }

  if ( time_has_bounds )
    streams[streamID].tsteps[0].taxis.has_bounds = TRUE;

  if ( ncvarid != -1 )
    if ( ncvars[ncvarid].calendar == TRUE )
      {
	cdfGetAttText(fileID, ncvarid, "calendar", attstring, attstringlen);

	if ( strcmp(attstring, "standard")  == 0 ||
	     strcmp(attstring, "gregorian") == 0 )
	  calendar = CALENDAR_STANDARD;
	else if ( strcmp(attstring, "none") == 0 )
	  calendar = CALENDAR_NONE;
	else if ( strncmp(attstring, "360", 3) == 0 )
	  calendar = CALENDAR_360DAYS;
	else if ( strncmp(attstring, "365", 3) == 0 ||
		  strcmp(attstring, "noleap")  == 0 )
	  calendar = CALENDAR_365DAYS;
	else if ( strncmp(attstring, "366", 3)  == 0 ||
		  strcmp(attstring, "all_leap") == 0 )
	  calendar = CALENDAR_366DAYS;
	else if ( strncmp(attstring, "proleptic", 9) == 0 )
	  calendar = CALENDAR_PROLEPTIC;
	else
	  Warning(func, "calendar >%s< unsupported!", attstring);
      }

  if ( streams[streamID].tsteps[0].taxis.type == TAXIS_RELATIVE )
    taxisID = taxisNew(TAXIS_RELATIVE);
  else
    taxisID = taxisNew(TAXIS_ABSOLUTE);

  if ( calendar != UNDEFID )
    {
      TAXIS *taxis;
      taxis = &streams[streamID].tsteps[0].taxis;

      taxis->calendar = calendar;
      taxisDefCalendar(taxisID, calendar);
    }

  vlistDefTaxis(vlistID, taxisID);

  streams[streamID].curTsID = 0;
  streams[streamID].rtsteps = 1;

  (void) cdfInqTimestep(streamID, 0);

  cdfCreateRecords(streamID, 0);

  /* free ncdims */
  free (ncdims);

  /* free ncvars */
  free (ncvars);

#endif

  return (0);
}


int cdfInqTimestep(int streamID, int tsID)
{
  static char func[] = "cdfInqTimestep";
  long nrecs = 0;
#if  defined  (HAVE_LIBNETCDF)
  double timevalue;
  int nctimevarid;
  int nctimeboundsid;
  int fileID;
  size_t index;
  TAXIS *taxis;

  if ( CDI_Debug )
    Message(func, "streamID = %d  tsID = %d", streamID, tsID);

  streamCheckID(func, streamID);
    
  if ( tsID < 0 ) Error(func, "unexpected tsID = %d", tsID);

  if ( tsID < streams[streamID].ntsteps && streams[streamID].ntsteps > 0 )
    {
      cdfCreateRecords(streamID, tsID);

      taxis = &streams[streamID].tsteps[tsID].taxis;
      if ( tsID > 0 )
	ptaxisCopy(taxis, &streams[streamID].tsteps[0].taxis);

      timevalue = tsID;

      nctimevarid = streams[streamID].basetime.ncvarid;
      if ( nctimevarid != UNDEFID )
	{
	  fileID = streamInqFileID(streamID);
	  index  = tsID;
	  cdf_get_var1_double(fileID, nctimevarid, &index, &timevalue);

	  decode_timeval(timevalue, taxis, &taxis->vdate, &taxis->vtime);

	  nctimeboundsid = streams[streamID].basetime.ncvarboundsid;
	  if ( nctimeboundsid != UNDEFID )
	    {
	      size_t start[2], count[2];
	      start[0] = tsID; count[0] = 1; start[1] = 0; count[1] = 1;
	      cdf_get_vara_double(fileID, nctimeboundsid, start, count, &timevalue);

	      decode_timeval(timevalue, taxis, &taxis->vdate_lb, &taxis->vtime_lb);

	      start[0] = tsID; count[0] = 1; start[1] = 1; count[1] = 1;
	      cdf_get_vara_double(fileID, nctimeboundsid, start, count, &timevalue);

	      decode_timeval(timevalue, taxis, &taxis->vdate_ub, &taxis->vtime_ub);
	    }

	}
    }

  streams[streamID].curTsID = tsID;
  nrecs = streams[streamID].tsteps[tsID].nrecs;

#endif
  return ((int) nrecs);
}

void cdfEndDef(int streamID)
{
  int varID, ncvarid;
  int nvars;

  cdfDefGlobalAtts(streamID);
  cdfDefLocalAtts(streamID);
  if ( streams[streamID].accessmode == 0 )
    {
      nvars =  streams[streamID].nvars;

      for ( varID = 0; varID < nvars; varID++ )
	{
	  ncvarid = cdfDefVar(streamID, varID);
	}
      streams[streamID].accessmode = 1;
    }
}

void cdfDefInstitut(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID, instID;
  char *longname;
  size_t len;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  instID  = vlistInqInstitut(vlistID);

  if ( instID != UNDEFID )
    {
      longname = institutInqLongnamePtr(instID);
      if ( longname )
	{
	  len = strlen(longname);
	  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);
	  cdf_put_att_text(fileID, NC_GLOBAL, "institution", len, longname);
	  if ( streams[streamID].ncmode == 2 ) cdf_enddef(fileID);
	}
    }
#endif
}

void cdfDefSource(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID, modelID;
  char *longname;
  size_t len;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  modelID = vlistInqModel(vlistID);

  if ( modelID != UNDEFID )
    {
      longname = modelInqNamePtr(modelID);
      if ( longname )
	{
	  len = strlen(longname);
	  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);
	  cdf_put_att_text(fileID, NC_GLOBAL, "source", len, longname);
	  if ( streams[streamID].ncmode == 2 ) cdf_enddef(fileID);
	}
    }
#endif
}

void cdfDefGlobalAtts(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID, vlistID;
  int iatt;
  size_t len;
  VLIST *vlistptr;

  if ( streams[streamID].globalatts ) return;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  vlistptr = vlist_to_pointer(vlistID);

  if ( vlistptr->natts > 0 )
    {
      if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);
      for ( iatt = 0; iatt < vlistptr->natts; iatt++ )
	{
	  if ( vlistptr->attstrings[iatt] )
	    {
	      len = strlen(vlistptr->attstrings[iatt]);
	      cdf_put_att_text(fileID, NC_GLOBAL, vlistptr->attnames[iatt],
			       len, vlistptr->attstrings[iatt]);
	    }
	}
      if ( streams[streamID].ncmode == 2 ) cdf_enddef(fileID);
    }

  cdfDefSource(streamID);
  cdfDefInstitut(streamID);

  streams[streamID].globalatts = 1;
#endif
}

void cdfDefLocalAtts(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  int varID, instID, fileID;
  char *name;
  size_t len;
  int ncvarid;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  if ( streams[streamID].localatts ) return;
  if ( vlistInqInstitut(vlistID) != UNDEFID ) return;

  streams[streamID].localatts = 1;

  if ( streams[streamID].ncmode == 2 ) cdf_redef(fileID);

  for ( varID = 0; varID < streams[streamID].nvars; varID++ )
    {
      instID = vlistInqVarInstitut(vlistID, varID);
      if ( instID != UNDEFID )
	{
          ncvarid = streams[streamID].vars[varID].ncvarid;
  	  name = institutInqNamePtr(instID);
	  if ( name )
	    {
	      len = strlen(name);
	      cdf_put_att_text(fileID, ncvarid, "institution", len, name);
	    }
	}
      }
  
  if ( streams[streamID].ncmode == 2 ) cdf_enddef(fileID);
#endif
}

void cdfDefHistory(int streamID, int size, char *history)
{
#if  defined  (HAVE_LIBNETCDF)
  int ncid;

  ncid = streams[streamID].fileID;
  cdf_put_att_text(ncid, NC_GLOBAL, "history", (size_t) size, history);
#endif
}

int cdfInqHistorySize(int streamID)
{
  size_t size = 0;
#if  defined  (HAVE_LIBNETCDF)
  int ncid;

  ncid = streams[streamID].fileID;
  if ( streams[streamID].historyID != UNDEFID )
    cdf_inq_attlen(ncid, NC_GLOBAL, "history", &size);

#endif
  return ((int) size);
}


void cdfInqHistoryString(int streamID, char *history)
{
#if  defined  (HAVE_LIBNETCDF)
  int ncid;

  ncid = streams[streamID].fileID;
  if ( streams[streamID].historyID != UNDEFID )
    cdf_get_att_text(ncid, NC_GLOBAL, "history", history);

#endif
}


void cdfDefVars(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefVars";
  int index, gridID, zaxisID, vlistID;
  int nvars, ngrids, nzaxis;
  /*
  int  ncvarid;
  */

  vlistID = streamInqVlist(streamID);
  if ( vlistID == UNDEFID )
    Error(func, "Internal problem! vlist undefined for streamID %d\n", streamID);

  nvars  = vlistNvars(vlistID);
  ngrids = vlistNgrids(vlistID);
  nzaxis = vlistNzaxis(vlistID);
  /*
  if ( vlistHasTime(vlistID) ) cdfDefTime(streamID);
  */
  for ( index = 0; index < ngrids; index++ )
    {
      gridID = vlistGrid(vlistID, index);
      cdfDefGrid(streamID, gridID);
    }

  for ( index = 0; index < nzaxis; index++ )
    {
      zaxisID = vlistZaxis(vlistID, index);
      if ( streams[streamID].zaxisID[index] == UNDEFID ) cdfDefZaxis(streamID, zaxisID);
    }
  /*
    define time first!!!
  for (varID = 0; varID < nvars; varID++ )
    {
      ncvarid = cdfDefVar(streamID, varID);
    }
  */
#endif
}
