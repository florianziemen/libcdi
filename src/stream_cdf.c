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
  int      ignore;
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
  int      ncoordvars;
  int      coordvarids[4];
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
  int      natts;
  int     *atts;
  int      deflate;
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
  int len, i;
  char *ptu;
  int year, month, day;
  int hour = 0, minute = 0, second = 0;
  int timetype = TAXIS_ABSOLUTE;
  int rdate = -1, rtime = -1;
  int timeunit = -1;

  ptu = timeunits;
  len = (int) strlen(timeunits);

  for ( i = 0; i < len; i++ ) ptu[i] = tolower((int) ptu[i]);

  (*taxis).type = timetype;

  if      ( memcmp(ptu, "sec",    3) == 0 ) timeunit = TUNIT_SECOND;
  else if ( memcmp(ptu, "minute", 6) == 0 ) timeunit = TUNIT_MINUTE;
  else if ( memcmp(ptu, "hour",   4) == 0 ) timeunit = TUNIT_HOUR;
  else if ( memcmp(ptu, "day",    3) == 0 ) timeunit = TUNIT_DAY;
  else if ( memcmp(ptu, "month",  5) == 0 ) timeunit = TUNIT_MONTH;
  else if ( memcmp(ptu, "calendar_month", 14) == 0 ) timeunit = TUNIT_MONTH;
  else if ( memcmp(ptu, "year",   4) == 0 ) timeunit = TUNIT_YEAR;
  else
    {
      Message(func, "Unsupported TIMEUNIT: %s!", ptu);
      return (1);
    }

  while ( ! isspace(*ptu) && *ptu != 0 ) ptu++;
  if ( *ptu )
    {
      while (   isspace(*ptu) ) ptu++;

      if ( memcmp(ptu, "as", 2) == 0 )
	timetype = TAXIS_ABSOLUTE;
      else if ( memcmp(ptu, "since", 5) == 0 )
	timetype = TAXIS_RELATIVE;

      while ( ! isspace(*ptu) && *ptu != 0 ) ptu++;
      if ( *ptu )
	{
	  while (   isspace(*ptu) ) ptu++;

	  if ( timetype == TAXIS_ABSOLUTE )
	    {
	      if ( memcmp(ptu, "%y%m%d.%f", 9) != 0 && timeunit == TUNIT_DAY )
		{
		  Message(func, "Unsupported format %s for TIMEUNIT day!", ptu);
		  timeunit = -1;
		}
	      else if ( memcmp(ptu, "%y%m.%f", 7) != 0 && timeunit == TUNIT_MONTH )
		{
		  Message(func, "Unsupported format %s for TIMEUNIT month!", ptu);
		  timeunit = -1;
		}
	    }
	  else if ( timetype == TAXIS_RELATIVE )
	    {
	      year = atoi(ptu);
	      if ( year < 0 ) ptu++;
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

	      rdate = encode_date(year, month, day);
	      rtime = encode_time(hour, minute);
	      (*taxis).rdate = rdate;
	      (*taxis).rtime = rtime;

	      if ( CDI_Debug )
		Message(func, "rdate = %d  rtime = %d", rdate, rtime);	      
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
static void cdfGetAttInt(int fileID, int ncvarid, char *attname, int attlen, int *attint)
{
  static char func[] = "cdfGetAttInt";
  size_t nc_attlen;
  int *pintatt;

  cdf_inq_attlen(fileID, ncvarid, attname, &nc_attlen);

  if ( (int)nc_attlen > attlen )
    pintatt = (int *) malloc(nc_attlen*sizeof(int));
  else
    pintatt = attint;

  cdf_get_att_int(fileID, ncvarid, attname, pintatt);

  if ( (int)nc_attlen > attlen )
    {
      memcpy(attint, pintatt, attlen*sizeof(int));
      free(pintatt);
    }
}
#endif

#if  defined  (HAVE_LIBNETCDF)
static void cdfGetAttDouble(int fileID, int ncvarid, char *attname, int attlen, double *attdouble)
{
  static char func[] = "cdfGetAttDouble";
  size_t nc_attlen;
  double *pdoubleatt;

  cdf_inq_attlen(fileID, ncvarid, attname, &nc_attlen);

  if ( (int)nc_attlen > attlen )
    pdoubleatt = (double *) malloc(nc_attlen*sizeof(double));
  else
    pdoubleatt = attdouble;

  cdf_get_att_double(fileID, ncvarid, attname, pdoubleatt);

  if ( (int)nc_attlen > attlen )
    {
      memcpy(attdouble, pdoubleatt, attlen*sizeof(double));
      free(pdoubleatt);
    }
}
#endif

#if  defined  (HAVE_LIBNETCDF)
static void cdfGetAttText(int fileID, int ncvarid, char *attname, int attlen, char *atttext)
{
  size_t nc_attlen;
  char attbuf[65636];

  cdf_inq_attlen(fileID, ncvarid, attname, &nc_attlen);

  if ( nc_attlen < sizeof(attbuf) )
    {
      cdf_get_att_text(fileID, ncvarid, attname, attbuf);

      attbuf[nc_attlen++] = 0;

      if ( (int) nc_attlen > attlen ) nc_attlen = attlen;
      memcpy(atttext, attbuf, nc_attlen);
    }
  else
    {
      atttext[0] = 0;
    }
}
#endif

#if  defined  (HAVE_LIBNETCDF)
int cdfInqDatatype(int xtype)
{
  int datatype;

  if      ( xtype == NC_BYTE   )  datatype = DATATYPE_INT8;
  /* else if ( xtype == NC_CHAR   )  datatype = DATATYPE_UINT8; */
  else if ( xtype == NC_SHORT  )  datatype = DATATYPE_INT16;
  else if ( xtype == NC_INT    )  datatype = DATATYPE_INT32;
  else if ( xtype == NC_DOUBLE )  datatype = DATATYPE_FLT64;
  else                            datatype = DATATYPE_FLT32;

  return (datatype);
}
#endif

#if  defined  (HAVE_LIBNETCDF)
int cdfDefDatatype(int datatype)
{
  int xtype;

  if      ( datatype == DATATYPE_INT8  )  xtype = NC_BYTE;
  else if ( datatype == DATATYPE_UINT8 )  xtype = NC_SHORT;
  else if ( datatype == DATATYPE_INT16 )  xtype = NC_SHORT;
  else if ( datatype == DATATYPE_UINT16 ) xtype = NC_INT;
  else if ( datatype == DATATYPE_INT32 )  xtype = NC_INT;
  else if ( datatype == DATATYPE_UINT32 ) xtype = NC_INT;
  else if ( datatype == DATATYPE_FLT64 )  xtype = NC_DOUBLE;
  else                                    xtype = NC_FLOAT;

  return (xtype);
}
#endif


#if  defined  (HAVE_LIBNETCDF)
void defineAttributes(int vlistID, int varID, int fileID, int ncvarID)
{
  static char func[] = "defineAttributes";
  int natts, iatt;
  int atttype, attlen;
  size_t len;
  char attname[1024];
  char atttxt[8192];

  vlistInqNatts(vlistID, varID, &natts);

  for ( iatt = 0; iatt < natts; iatt++ )
    {
      vlistInqAtt(vlistID, varID, iatt, attname, &atttype, &attlen);

      if ( atttype == DATATYPE_TXT )
	{
	  vlistInqAttTxt(vlistID, varID, attname, sizeof(atttxt), atttxt);
	  len = attlen;
	  cdf_put_att_text(fileID, ncvarID, attname, len, atttxt);
	}
      else if ( atttype == DATATYPE_INT )
	{
	  int *attint;
	  attint = (int *) malloc(attlen*sizeof(int));
	  vlistInqAttInt(vlistID, varID, attname, attlen, &attint[0]);
	  len = attlen;
	  cdf_put_att_int(fileID, ncvarID, attname, NC_INT, len, attint);
	  free(attint);
	}
      else if ( atttype == DATATYPE_FLT )
	{
	  double *attflt;
	  attflt = (double *) malloc(attlen*sizeof(double));
	  vlistInqAttFlt(vlistID, varID, attname, attlen, attflt);
	  len = attlen;
	  cdf_put_att_double(fileID, ncvarID, attname, NC_DOUBLE, len, attflt);
	  free(attflt);
	}
    }
}
#endif


int cdfCopyRecord(int streamID2, int streamID1)
{
  static char func[] = "cdfCopyRecord";
  double *data;
  int datasize;
  int tsID1, tsID2, recID1, recID2;
  int ivarID, gridID;
  int nmiss;
  int ierr = 0;
  int vlistID1, vlistID2;
  STREAM *streamptr1;
  STREAM *streamptr2;

  streamptr1 = stream_to_pointer(streamID1);
  streamptr2 = stream_to_pointer(streamID2);

  stream_check_ptr(func, streamptr1);
  stream_check_ptr(func, streamptr2);

  vlistID1 = streamptr1->vlistID;
  vlistID2 = streamptr2->vlistID;

  tsID1 = streamptr1->curTsID;
  tsID2 = streamptr2->curTsID;

  recID1 = streamptr1->tsteps[tsID1].curRecID;
  recID2 = streamptr2->tsteps[tsID2].curRecID;

  ivarID = streamptr1->tsteps[tsID1].records[recID1].varID;

  gridID = vlistInqVarGrid(vlistID1, ivarID);

  datasize = gridInqSize(gridID);      
  /* bug fix for constant netCDF fields */
  if ( datasize < 1048576 ) datasize = 1048576;

  data = (double *) malloc(datasize*sizeof(double));

  streamReadRecord(streamID1, data, &nmiss);
  streamWriteRecord(streamID2, data, nmiss);

  free(data);

  return (ierr);
}

/* not used
int cdfInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "cdfInqRecord";
  int tsID, recID;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  recID = streamptr->tsteps[0].curRecID++;
  printf("cdfInqRecord recID %d %d\n", recID, streamptr->tsteps[0].curRecID);
  printf("cdfInqRecord tsID %d\n", streamptr->curTsID);

  if ( streamptr->tsteps[0].curRecID >= 
       streamptr->tsteps[0].nrecs )
    {
      streamptr->tsteps[0].curRecID = 0;
    }

  *varID   = streamptr->tsteps[0].records[recID].varID;
  *levelID = streamptr->tsteps[0].records[recID].levelID;

  streamptr->record->varID   = *varID;
  streamptr->record->levelID = *levelID;

  if ( CDI_Debug )
    Message(func, "recID = %d  varID = %d  levelID = %d", recID, *varID, *levelID);
  
  return (recID+1);
}
*/
int cdfDefRecord(int streamID)
{
  static char func[] = "cdfDefRecord";
  int ierr = 0;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d", streamID);

  stream_check_ptr(func, streamptr);

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  gridindex = vlistGridIndex(vlistID, gridID);
  lonID = streamptr->xdimID[gridindex];
  latID = streamptr->ydimID[gridindex];

  xlon = gridInqXval(gridID, 0);
  xlat = gridInqYval(gridID, 0);
  tsID = streamptr->curTsID;
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  gridindex = vlistGridIndex(vlistID, gridID);
  lonID = streamptr->xdimID[gridindex];
  latID = streamptr->ydimID[gridindex];

  tsID = streamptr->curTsID;
  index = tsID;

  cdf_get_var1_double(fileID, lonID, &index, &xlon);
  cdf_get_var1_double(fileID, latID, &index, &xlat);

  gridDefXvals(gridID, &xlon);
  gridDefYvals(gridID, &xlat);
#endif
}


#if  defined  (HAVE_LIBNETCDF)
void cdfDefVarDeflate(int ncid, int ncvarid, int deflate_level)
{
  static char func[] = "cdfDefVarDeflate";
#if  defined  (NC_NETCDF4)
  int retval;
  /* Set chunking, shuffle, and deflate. */
  int shuffle = 1;
  int deflate = 1;

  if ( deflate_level < 1 || deflate_level > 9 ) deflate_level = 1;

  if ((retval = nc_def_var_deflate(ncid, ncvarid, shuffle, deflate, deflate_level)))
    {
      Error(func, "nc_def_var_deflate failed, status = %d", retval);
    }
#else
  static int lwarn = TRUE;

  if ( lwarn )
    {
      lwarn = FALSE;
      Warning(func, "Compression failed, netCDF4 not available!");
    }
#endif
}
#endif


void cdfDefMissval(int streamID, int varID, int dtype)
{
#if  defined  (HAVE_LIBNETCDF)
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( streamptr->vars[varID].defmiss == FALSE )
    {
      int fileID;
      int ncvarid;
      double missval;
      int vlistID;
      int xtype;

      vlistID = streamInqVlist(streamID);
      fileID  = streamInqFileID(streamID);
      ncvarid = streamptr->vars[varID].ncvarid;
      missval = vlistInqVarMissval(vlistID, varID);
      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      xtype = cdfDefDatatype(dtype);

      cdf_put_att_double(fileID, ncvarid, "_FillValue", (nc_type) xtype, 1L, &missval);

      if ( cdiNcMissingValue == 1 )
	cdf_put_att_double(fileID, ncvarid, "missing_value", (nc_type) xtype, 1L, &missval);

      if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);

      streamptr->vars[varID].defmiss = TRUE;
    }
#endif
}

void cdfWriteRecord(int streamID, double *data, int nmiss)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfWriteRecord";
  int varID;
  int levelID;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  varID   = streamptr->record->varID;
  levelID = streamptr->record->levelID;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( CDI_Debug ) Message(func, "streamID = %d", streamID);

  tsID    = streamptr->curTsID;
  vrecID  = streamptr->tsteps[tsID].curRecID;
  recID   = streamptr->tsteps[tsID].recIDs[vrecID];
  varID   = streamptr->tsteps[tsID].records[recID].varID;
  levelID = streamptr->tsteps[tsID].records[recID].levelID;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  fileID = streamInqFileID(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d, fileID = %d", streamID, fileID);

  taxis = &streamptr->tsteps[tsID].taxis;

  if ( streamptr->ncmode == 1 )
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  index = tsID;

  timevalue = encode_timeval(taxis->vdate, taxis->vtime, &streamptr->tsteps[0].taxis);
  if ( CDI_Debug ) Message(func, "tsID = %d  timevalue = %f", tsID, timevalue);

  ncvarid = streamptr->basetime.ncvarid;
  cdf_put_var1_double(fileID, ncvarid, &index, &timevalue);

  if ( taxis->has_bounds )
    {
      size_t start[2], count[2];

      ncvarid = streamptr->basetime.ncvarboundsid;

      timevalue = encode_timeval(taxis->vdate_lb, taxis->vtime_lb, &streamptr->tsteps[0].taxis);
      start[0] = tsID; count[0] = 1; start[1] = 0; count[1] = 1;
      cdf_put_vara_double(fileID, ncvarid, start, count, &timevalue);

      timevalue = encode_timeval(taxis->vdate_ub, taxis->vtime_ub, &streamptr->tsteps[0].taxis);
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( streamptr->basetime.ncvarid != UNDEFID ) return;

  fileID = streamInqFileID(streamID);

  if ( streamptr->ncmode == 0 ) streamptr->ncmode = 1;

  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  cdf_def_dim(fileID, "time", NC_UNLIMITED, &streamptr->basetime.ncdimid);

  dims[0] = streamptr->basetime.ncdimid;
  cdf_def_var(fileID, "time", NC_DOUBLE, 1, dims, &time_varid);

  streamptr->basetime.ncvarid = time_varid;

  taxis = &streamptr->tsteps[0].taxis;

  if ( taxis->has_bounds )
    {
      /* fprintf(stderr, "time has bounds\n"); */

      cdf_def_dim(fileID, "tbnds", 2, &dims[1]);
      cdf_def_var(fileID, "time_bnds", NC_DOUBLE, 2, dims, &time_bndsid);

      streamptr->basetime.ncvarboundsid = time_bndsid;

      cdf_put_att_text(fileID, time_varid, "bounds", 9, "time_bnds");
    }

  unitstr[0] = 0;
  if ( streamptr->tsteps[0].taxis.type == TAXIS_ABSOLUTE )
    {
      if ( streamptr->tsteps[0].taxis.unit == TUNIT_YEAR )
	sprintf(unitstr, "year as %s", "%Y.%f");
      else if ( streamptr->tsteps[0].taxis.unit == TUNIT_MONTH )
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

      decode_date(rdate, &year, &month, &day);
      decode_time(rtime, &hour, &minute);

      if ( timeunit == TUNIT_QUARTER ) timeunit = TUNIT_MINUTE;
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

      if      ( taxis->calendar == CALENDAR_STANDARD )  strcpy(calstr, "standard");
      else if ( taxis->calendar == CALENDAR_PROLEPTIC ) strcpy(calstr, "proleptic_gregorian");
      else if ( taxis->calendar == CALENDAR_NONE )      strcpy(calstr, "none");
      else if ( taxis->calendar == CALENDAR_360DAYS )   strcpy(calstr, "360_day");
      else if ( taxis->calendar == CALENDAR_365DAYS )   strcpy(calstr, "365_day");
      else if ( taxis->calendar == CALENDAR_366DAYS )   strcpy(calstr, "366_day");

      len = strlen(calstr);
      if ( len )
	cdf_put_att_text(fileID, time_varid, "calendar", len, calstr);

      if ( taxis->has_bounds )
	if ( len )
	  cdf_put_att_text(fileID, time_bndsid, "calendar", len, calstr);
    }

  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streamptr->xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_SPECTRAL )
	    {
	      dimID = streamptr->xdimID[index];
	      break;
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      dimlen = 2;

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streamptr->xdimID[gridindex] = dimID;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID)/2;

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streamptr->ydimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_SPECTRAL )
	    {
	      dimlen0 = gridInqSize(gridID0)/2;
	      if ( dimlen == dimlen0 )
		{
		  dimID = streamptr->ydimID[index];
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

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streamptr->ydimID[gridindex] = dimID;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( gridInqPrec(gridID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  gridtype = gridInqType(gridID);
  dimlen = gridInqXsize(gridID);
  if ( dimlen != 1 ) Error(func, "Xsize isn't 1 for %s grid!", gridNamePtr(gridtype));

  gridindex = vlistGridIndex(vlistID, gridID);
  ncvarid = streamptr->xdimID[gridindex];

  gridInqXname(gridID, axisname);
  gridInqXlongname(gridID, longname);
  gridInqXstdname(gridID, stdname);
  gridInqXunits(gridID, units);

  if ( ncvarid == UNDEFID )
    {
      dimID = streamptr->basetime.ncvarid;

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
      if ( (len = strlen(longname)) )
	cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
      if ( (len = strlen(units)) )
	cdf_put_att_text(fileID, ncvarid, "units", len, units);
      if ( (len = strlen(stdname)) )
	cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  streamptr->xdimID[gridindex] = ncvarid; /* var ID for trajectory !!! */

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( gridInqPrec(gridID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID = streamInqFileID(streamID);

  gridtype = gridInqType(gridID);
  dimlen = gridInqYsize(gridID);
  if ( dimlen != 1 ) Error(func, "Ysize isn't 1 for %s grid!", gridNamePtr(gridtype));

  gridindex = vlistGridIndex(vlistID, gridID);
  ncvarid = streamptr->ydimID[gridindex];

  gridInqYname(gridID, axisname);
  gridInqYlongname(gridID, longname);
  gridInqYstdname(gridID, stdname);
  gridInqYunits(gridID, units);

  if ( ncvarid == UNDEFID )
    {
      dimID = streamptr->basetime.ncvarid;

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
      if ( (len = strlen(longname)) )
	cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
      if ( (len = strlen(units)) )
	cdf_put_att_text(fileID, ncvarid, "units", len, units);
      if ( (len = strlen(stdname)) )
	cdf_put_att_text(fileID, ncvarid, "standard_name", len, stdname);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  streamptr->ydimID[gridindex] = ncvarid; /* var ID for trajectory !!! */

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
  int index, iz = 0;
  /*  int index2; */
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( gridInqPrec(gridID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

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
      if ( streamptr->xdimID[index] != UNDEFID )
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
		if ( IS_EQUAL(gridInqXval(gridID0, 0), gridInqXval(gridID, 0)) &&
		     IS_EQUAL(gridInqXval(gridID0, dimlen-1), gridInqXval(gridID, dimlen-1)) )
		  {
		    dimID = streamptr->xdimID[index];
		    break;
		  }
	      /*
	      for ( index2 = 0; index2 < index; index2++ )
		if ( streamptr->xdimID[index] == streamptr->xdimID[index2] )
		  break;
	      if ( index2 == index ) iz++;
	      */
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      char axisname0[256];
      char axisname2[256];
      int checkname = FALSE;
      int status;

      /* check that the name is not allready defined */
      checkname = TRUE;
      iz = 0;

      while ( checkname ) 
	{
	  strcpy(axisname2, axisname);
	  if ( iz ) sprintf(&axisname2[strlen(axisname2)], "_%d", iz+1);

	  status = nc_inq_varid(fileID, axisname2, &ncvarid);
	  if ( status != NC_NOERR )
	    {
	      if ( iz )
		{
		  /* check that the name does not exist for other grids */
		  for ( index = 0; index < ngrids; index++ )
		    {
		      gridID0 = vlistGrid(vlistID, index);
		      if ( gridID != gridID0 )
			{
			  gridInqXname(gridID0, axisname0);
			  if ( strcmp(axisname0, axisname2) == 0 ) break;
			}
		    }
		  if ( index == ngrids ) checkname = FALSE;
		}
	      else
		{
		  checkname = FALSE;
		}
	    }

	  if ( checkname ) iz++;

	  if ( iz > 99 ) break;
	}

      if ( iz ) sprintf(&axisname[strlen(axisname)], "_%d", iz+1);

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

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
      streamptr->ncmode = 2;

      if ( ncvarid  != UNDEFID ) cdf_put_var_double(fileID, ncvarid, gridInqXvalsPtr(gridID));
      if ( ncbvarid != UNDEFID ) cdf_put_var_double(fileID, ncbvarid, gridInqXboundsPtr(gridID));
    }

  streamptr->xdimID[gridindex] = dimID;

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
  int index, iz = 0;
  /*  int index2; */
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( gridInqPrec(gridID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

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
      if ( streamptr->ydimID[index] != UNDEFID )
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
		if ( IS_EQUAL(gridInqYval(gridID0, 0), gridInqYval(gridID, 0)) &&
		     IS_EQUAL(gridInqYval(gridID0, dimlen-1), gridInqYval(gridID, dimlen-1)) )
		  {
		    dimID = streamptr->ydimID[index];
		    break;
		  }
	      /*
	      for ( index2 = 0; index2 < index; index2++ )
		if ( streamptr->ydimID[index] == streamptr->ydimID[index2] )
		  break;
	      if ( index2 == index ) iz++;
	      */
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      char axisname0[256];
      char axisname2[256];
      int checkname = FALSE;
      int status;

      /* check that the name is not allready defined */
      checkname = TRUE;
      iz = 0;

      while ( checkname ) 
	{
	  strcpy(axisname2, axisname);
	  if ( iz ) sprintf(&axisname2[strlen(axisname2)], "_%d", iz+1);

	  status = nc_inq_varid(fileID, axisname2, &ncvarid);
	  if ( status != NC_NOERR )
	    {
	      if ( iz )
		{
		  /* check that the name does not exist for other grids */
		  for ( index = 0; index < ngrids; index++ )
		    {
		      gridID0 = vlistGrid(vlistID, index);
		      if ( gridID != gridID0 )
			{
			  gridInqYname(gridID0, axisname0);
			  if ( strcmp(axisname0, axisname2) == 0 ) break;
			}
		    }
		  if ( index == ngrids ) checkname = FALSE;
		}
	      else
		{
		  checkname = FALSE;
		}
	    }

	  if ( checkname ) iz++;

	  if ( iz > 99 ) break;
	}

      if ( iz ) sprintf(&axisname[strlen(axisname)], "_%d", iz+1);

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

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
      streamptr->ncmode = 2;

      if ( ncvarid  != UNDEFID ) cdf_put_var_double(fileID, ncvarid, gridInqYvalsPtr(gridID));
      if ( ncbvarid != UNDEFID ) cdf_put_var_double(fileID, ncbvarid, gridInqYboundsPtr(gridID));
    }

  streamptr->ydimID[gridindex] = dimID;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( gridInqPrec(gridID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

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
      if ( streamptr->xdimID[index] != UNDEFID )
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
		if ( IS_EQUAL(gridInqXval(gridID0, 0), gridInqXval(gridID, 0)) &&
		     IS_EQUAL(gridInqXval(gridID0, xdimlen-1), gridInqXval(gridID, xdimlen-1)) )
		  {
		    xdimID = streamptr->xdimID[index];
		    break;
		  }
	      for ( index2 = 0; index2 < index; index2++ )
		if ( streamptr->xdimID[index] == streamptr->xdimID[index2] )
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

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, xdimname, xdimlen, &xdimID);
      cdf_def_dim(fileID, ydimname, ydimlen, &ydimID);

      if ( gridInqXboundsPtr(gridID) || gridInqYboundsPtr(gridID) )
	{
	  int status;

	  status = nc_inq_dimid(fileID, "nv", &nvdimID);
	  if ( status != NC_NOERR )
	    cdf_def_dim(fileID, "nv", nvertex, &nvdimID);
	}

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

	  streamptr->ncxvarID[gridindex] = ncxvarid;

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

	  streamptr->ncyvarID[gridindex] = ncyvarid;

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

	  streamptr->ncavarID[gridindex] = ncavarid;
	}

      cdf_enddef(fileID);
      streamptr->ncmode = 2;

      if ( ncxvarid  != UNDEFID ) cdf_put_var_double(fileID, ncxvarid,  gridInqXvalsPtr(gridID));
      if ( ncbxvarid != UNDEFID ) cdf_put_var_double(fileID, ncbxvarid, gridInqXboundsPtr(gridID));
      if ( ncyvarid  != UNDEFID ) cdf_put_var_double(fileID, ncyvarid,  gridInqYvalsPtr(gridID));
      if ( ncbyvarid != UNDEFID ) cdf_put_var_double(fileID, ncbyvarid, gridInqYboundsPtr(gridID));
      if ( ncavarid  != UNDEFID ) cdf_put_var_double(fileID, ncavarid,  gridInqAreaPtr(gridID));
    }

  streamptr->xdimID[gridindex] = xdimID;
  streamptr->ydimID[gridindex] = ydimID;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streamptr->xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN_REDUCED )
	    {
	      dimlen0 = gridInqSize(gridID0);

	      if ( dimlen == dimlen0 )
		{
		  dimID = streamptr->xdimID[index];
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

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streamptr->xdimID[gridindex] = dimID;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID);

  if ( gridInqYsize(gridID) == 0 )
    for ( index = 0; index < ngrids; index++ )
      {
	if ( streamptr->xdimID[index] != UNDEFID )
	  {
	    gridID0 = vlistGrid(vlistID, index);
	    gridtype0 = gridInqType(gridID0);
	    if ( gridtype0 == GRID_GENERIC )
	      {
		dimlen0 = gridInqSize(gridID0);
		if ( dimlen == dimlen0 )
		  {
		    dimID = streamptr->xdimID[index];
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
	if ( streamptr->ydimID[index] != UNDEFID )
	  {
	    gridID0 = vlistGrid(vlistID, index);
	    gridtype0 = gridInqType(gridID0);
	    if ( gridtype0 == GRID_GENERIC )
	      {
		dimlen0 = gridInqSize(gridID0);
		if ( dimlen == dimlen0 )
		  {
		    dimID = streamptr->ydimID[index];
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

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streamptr->xdimID[gridindex] = dimID;

#endif
}


void cdfDefCell(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  char axisname[] = "cellX";
  char vertname[] = "nvX";
  char xunits[256];
  char xlongname[256];
  char xstdname[256];
  char yunits[256];
  char ylongname[256];
  char ystdname[256];
  char xaxisname[256];
  char yaxisname[256];
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  size_t len;
  int ncxvarid = UNDEFID, ncyvarid = UNDEFID;
  int ncbxvarid = UNDEFID, ncbyvarid = UNDEFID, ncavarid = UNDEFID;
  int nvertex, nvdimID = -1;
  int vlistID;
  int xtype = NC_DOUBLE;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( gridInqPrec(gridID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID);
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
      if ( streamptr->xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_CELL )
	    {
	      dimlen0 = gridInqSize(gridID0);
	      if ( dimlen == dimlen0 )
		{
		  dimID = streamptr->xdimID[index];
		  break;
		}
	      else
		iz++;	
	    }
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( iz == 0 )
	{
	  axisname[4] = '\0';
	  vertname[2] = '\0';
	}
      else
	{
          sprintf(&axisname[4], "%1d", iz+1);
          sprintf(&vertname[2], "%1d", iz+1);
	}

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, axisname, dimlen, &dimID);

      nvertex = gridInqNvertex(gridID);
      if ( nvertex > 0 ) cdf_def_dim(fileID, vertname, nvertex, &nvdimID);
	
      if ( gridInqXvalsPtr(gridID) )
	{
	  cdf_def_var(fileID, xaxisname, (nc_type) xtype, 1, &dimID, &ncxvarid);
	  if ( (len = strlen(xlongname)) )
	    cdf_put_att_text(fileID, ncxvarid, "long_name", len, xlongname);
	  if ( (len = strlen(xunits)) )
	    cdf_put_att_text(fileID, ncxvarid, "units", len, xunits);
	  if ( (len = strlen(xstdname)) )
	    cdf_put_att_text(fileID, ncxvarid, "standard_name", len, xstdname);

	  streamptr->ncxvarID[gridindex] = ncxvarid;

	  if ( nvdimID != UNDEFID && gridInqXboundsPtr(gridID) )
	    {
	      int dimIDs[2];
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      strcat(xaxisname, "_vertices");
	      cdf_def_var(fileID, xaxisname, (nc_type) xtype, 2, dimIDs, &ncbxvarid);
	      cdf_put_att_text(fileID, ncxvarid, "bounds", strlen(xaxisname), xaxisname);
	    }
	}

      if ( gridInqYvalsPtr(gridID) )
	{
	  cdf_def_var(fileID, yaxisname, (nc_type) xtype, 1, &dimID, &ncyvarid);
	  if ( (len = strlen(ylongname)) )
	    cdf_put_att_text(fileID, ncyvarid, "long_name", len, ylongname);
	  if ( (len = strlen(yunits)) )
	    cdf_put_att_text(fileID, ncyvarid, "units", len, yunits);
	  if ( (len = strlen(ystdname)) )
	    cdf_put_att_text(fileID, ncyvarid, "standard_name", len, ystdname);

	  streamptr->ncyvarID[gridindex] = ncyvarid;

	  if ( nvdimID != UNDEFID && gridInqYboundsPtr(gridID) )
	    {
	      int dimIDs[2];
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      strcat(yaxisname, "_vertices");
	      cdf_def_var(fileID, yaxisname, (nc_type) xtype, 2, dimIDs, &ncbyvarid);
	      cdf_put_att_text(fileID, ncyvarid, "bounds", strlen(yaxisname), yaxisname);
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

	  streamptr->ncavarID[gridindex] = ncavarid;
	}

      cdf_enddef(fileID);
      streamptr->ncmode = 2;

      if ( ncxvarid  != UNDEFID ) cdf_put_var_double(fileID, ncxvarid,  gridInqXvalsPtr(gridID));
      if ( ncbxvarid != UNDEFID ) cdf_put_var_double(fileID, ncbxvarid, gridInqXboundsPtr(gridID));
      if ( ncyvarid  != UNDEFID ) cdf_put_var_double(fileID, ncyvarid,  gridInqYvalsPtr(gridID));
      if ( ncbyvarid != UNDEFID ) cdf_put_var_double(fileID, ncbyvarid, gridInqYboundsPtr(gridID));
      if ( ncavarid  != UNDEFID ) cdf_put_var_double(fileID, ncavarid,  gridInqAreaPtr(gridID));
    }

  streamptr->xdimID[gridindex] = dimID;

#endif
}


void cdfDefVCT(int streamID, int zaxisID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefVCT";
  int type;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  type = zaxisInqType(zaxisID);
  if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
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

      if ( streamptr->vct.ilev > 0 )
	{
	  if ( streamptr->vct.ilev != ilev )
	    Error(func, "more than one VCT for each file unsupported!");
	  return;
	}

      if ( ilev == 0 )
	{
	  Warning(func, "VCT missing");
	  return;
	}

      fileID = streamInqFileID(streamID);

      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      cdf_def_dim(fileID, "mlev", mlev, &ncdimid);
      cdf_def_dim(fileID, "ilev", ilev, &ncdimid2);

      streamptr->vct.mlev   = mlev;
      streamptr->vct.ilev   = ilev;
      streamptr->vct.mlevID = ncdimid;
      streamptr->vct.ilevID = ncdimid2;

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
      streamptr->ncmode = 2;

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
  /*  char zaxisname0[256]; */
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( zaxisInqPrec(zaxisID) == DATATYPE_FLT32 ) xtype = NC_FLOAT;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);

  nzaxis = vlistNzaxis(vlistID);

  dimlen = zaxisInqSize(zaxisID);
  type   = zaxisInqType(zaxisID);

  if ( dimlen == 1 && type == ZAXIS_SURFACE ) return;
  if ( dimlen == 1 && type == ZAXIS_MEANSEA ) return;

  zaxisInqName(zaxisID, axisname);
  /*
  for ( index = 0; index < nzaxis; index++ )
    {
      if ( streamptr->zaxisID[index] != UNDEFID )
	{
	  zaxisID0 = vlistZaxis(vlistID, index);
	  zaxisInqName(zaxisID0, zaxisname0);
	  if ( strcmp(zaxisname0, axisname) == 0 ) ilevel++;
	}
    }
  */
  if ( dimID == UNDEFID )
    {
      char axisname0[256];
      char axisname2[256];
      int checkname = FALSE;
      int status;

      /* check that the name is not allready defined */
      checkname = TRUE;
      ilevel = 0;

      while ( checkname ) 
	{
	  strcpy(axisname2, axisname);
	  if ( ilevel ) sprintf(&axisname2[strlen(axisname2)], "_%d", ilevel+1);

	  status = nc_inq_varid(fileID, axisname2, &ncvarid);
	  if ( status != NC_NOERR )
	    {
	      if ( ilevel )
		{
		  /* check that the name does not exist for other grids */
		  for ( index = 0; index < nzaxis; index++ )
		    {
		      zaxisID0 = vlistZaxis(vlistID, index);
		      if ( zaxisID != zaxisID0 )
			{
			  zaxisInqName(zaxisID0, axisname0);
			  if ( strcmp(axisname0, axisname2) == 0 ) break;
			}
		    }
		  if ( index == nzaxis ) checkname = FALSE;
		}
	      else
		{
		  checkname = FALSE;
		}
	    }

	  if ( checkname ) ilevel++;

	  if ( ilevel > 99 ) break;
	}

      if ( ilevel ) sprintf(&axisname[strlen(axisname)], "_%1d", ilevel+1);

      if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
	{
	  int ilev = zaxisInqVctSize(zaxisID)/2;
	  int mlev = ilev - 1;

	  if ( type == ZAXIS_HYBRID )
	    {
	      if (  mlev != dimlen )
		{
		  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

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
		  streamptr->ncmode = 2;

		  cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
		}
	    }

	  if ( type == ZAXIS_HYBRID_HALF )
	    {
	      if ( ilev != dimlen )
		{
		  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

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
		  streamptr->ncmode = 2;

		  cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
		}
	    }

	  cdfDefVCT(streamID, zaxisID);

	  if ( dimID == UNDEFID )
	    {
	      if ( type == ZAXIS_HYBRID )
		streamptr->zaxisID[zaxisindex] = streamptr->vct.mlevID;
	      else
		streamptr->zaxisID[zaxisindex] = streamptr->vct.ilevID;
	    }
	}
      else
	{
	  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

	  cdf_def_dim(fileID, axisname, dimlen, &dimID);

	  zaxisInqLongname(zaxisID, longname);
	  zaxisInqUnits(zaxisID, units);

	  cdf_def_var(fileID, axisname, (nc_type) xtype, 1, &dimID, &ncvarid);
	  if ( (len = strlen(longname)) )
	    cdf_put_att_text(fileID, ncvarid, "long_name", len, longname);
	  if ( (len = strlen(units)) )
	    cdf_put_att_text(fileID, ncvarid, "units", len, units);
  
	  cdf_enddef(fileID);
	  streamptr->ncmode = 2;

	  cdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
	}
    }

  if ( dimID != UNDEFID )
    streamptr->zaxisID[zaxisindex] = dimID;

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


void cdfDefMapping(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID;
  int ncvarid = UNDEFID;
  int ncerr;

  if ( gridInqType(gridID) == GRID_SINUSOIDAL )
    {
      char varname[] = "sinusoidal";
      char mapname[] = "sinusoidal";

      fileID  = streamInqFileID(streamID);

      cdf_redef(fileID);

      ncerr = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
      if ( ncerr == NC_NOERR )
	{
	  cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", strlen(mapname), mapname);
	  /*
	  cdf_put_att_double(fileID, ncvarid, "grid_north_pole_latitude", NC_DOUBLE, 1L, &ypole);
	  cdf_put_att_double(fileID, ncvarid, "grid_north_pole_longitude", NC_DOUBLE, 1L, &xpole);
	  */
	}

      cdf_enddef(fileID);
    }
  else if ( gridInqType(gridID) == GRID_LAEA )
    {
      char varname[] = "laea";
      char mapname[] = "lambert_azimuthal_equal_area";

      fileID  = streamInqFileID(streamID);

      cdf_redef(fileID);

      ncerr = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
      if ( ncerr == NC_NOERR )
	{
	  double a, lon_0, lat_0;

	  gridInqLaea(gridID, &a, &lon_0, &lat_0);

	  cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", strlen(mapname), mapname);
	  cdf_put_att_double(fileID, ncvarid, "earth_radius", NC_DOUBLE, 1L, &a);
	  cdf_put_att_double(fileID, ncvarid, "longitude_of_projection_origin", NC_DOUBLE, 1L, &lon_0);
	  cdf_put_att_double(fileID, ncvarid, "latitude_of_projection_origin", NC_DOUBLE, 1L, &lat_0);
	}

      cdf_enddef(fileID);
    }
  else if ( gridInqType(gridID) == GRID_LCC2 )
    {
      char varname[] = "Lambert_Conformal";
      char mapname[] = "lambert_conformal_conic";

      fileID  = streamInqFileID(streamID);

      cdf_redef(fileID);

      ncerr = nc_def_var(fileID, varname, (nc_type) NC_CHAR, 0, NULL, &ncvarid);
      if ( ncerr == NC_NOERR )
	{
	  double a, lon_0, lat_0, lat_1, lat_2;

	  gridInqLcc2(gridID, &a, &lon_0, &lat_0, &lat_1, &lat_2);

	  cdf_put_att_text(fileID, ncvarid, "grid_mapping_name", strlen(mapname), mapname);
	  if ( a > 0 )
	    cdf_put_att_double(fileID, ncvarid, "earth_radius", NC_DOUBLE, 1L, &a);
	  cdf_put_att_double(fileID, ncvarid, "longitude_of_central_meridian", NC_DOUBLE, 1L, &lon_0);
	  cdf_put_att_double(fileID, ncvarid, "latitude_of_projection_origin", NC_DOUBLE, 1L, &lat_0);
	  if ( IS_EQUAL(lat_1, lat_2) )
	    cdf_put_att_double(fileID, ncvarid, "standard_parallel", NC_DOUBLE, 1L, &lat_1);
	  else
	    {
	      double lat_1_2[2];
	      lat_1_2[0] = lat_1;
	      lat_1_2[1] = lat_2;
	      cdf_put_att_double(fileID, ncvarid, "standard_parallel", NC_DOUBLE, 2L, lat_1_2);
	    }
	}

      cdf_enddef(fileID);
    }
#endif
}


void cdfDefGrid(int streamID, int gridID)
{
#if  defined  (HAVE_LIBNETCDF)
  static char func[] = "cdfDefGrid";
  int gridtype, size;
  int gridindex;
  int vlistID;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  gridindex = vlistGridIndex(vlistID, gridID);
  if ( streamptr->xdimID[gridindex] != UNDEFID ) return;

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
  else if ( gridtype == GRID_SINUSOIDAL || gridtype == GRID_LAEA || gridtype == GRID_LCC2 )
    {
      cdfDefXaxis(streamID, gridID);
      cdfDefYaxis(streamID, gridID);

      cdfDefMapping(streamID, gridID);
    }
  /*
  else if ( gridtype == GRID_LCC )
    {
      cdfDefLcc(streamID, gridID);
    }
  */
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
  size_t xsize = 0, ysize = 0;
  int code, gridID, zaxisID;
  char varname[9];
  char *name = NULL;
  char *longname = NULL;
  char *stdname = NULL;
  char *units = NULL;
  int dims[4];
  int chunks[4];
  int tableID;
  int ndims = 0;
  int len;
  int timeID;
  int xtype;
  int gridindex, zaxisindex;
  int tablenum;
  int vlistID;
  int iax = 0;
  char axis[5];
  STREAM *streamptr;
  int retval;

  streamptr = stream_to_pointer(streamID);

  fileID  = streamInqFileID(streamID);

  if ( streamptr->vars[varID].ncvarid != UNDEFID )
    return (streamptr->vars[varID].ncvarid);

  vlistID = streamInqVlist(streamID);
  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);
  code    = vlistInqVarCode(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) != GRID_TRAJECTORY )
    {
      xid = streamptr->xdimID[gridindex];
      yid = streamptr->ydimID[gridindex];
      if ( xid != UNDEFID ) cdf_inq_dimlen(fileID, xid, &xsize);
      if ( yid != UNDEFID ) cdf_inq_dimlen(fileID, yid, &ysize);
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streamptr->zaxisID[zaxisindex];

  tid = streamptr->basetime.ncdimid;

  if ( timeID == TIME_VARIABLE )
    {
      if ( tid == UNDEFID ) Error(func, "Internal problem! Time undefined.");
      chunks[ndims] = 1;
      dims[ndims++] = tid;
      axis[iax++] = 't';
    }
  if ( zid != UNDEFID ) axis[iax++] = 'z';
  if ( zid != UNDEFID ) chunks[ndims] = 1;
  if ( zid != UNDEFID ) dims[ndims++] = zid;

  if ( yid != UNDEFID ) chunks[ndims] = (int) ysize;
  if ( yid != UNDEFID ) dims[ndims++] = yid;

  if ( xid != UNDEFID ) chunks[ndims] = (int) xsize;
  if ( xid != UNDEFID ) dims[ndims++] = xid;

  if ( CDI_Debug ) 
    fprintf(stderr, "chunks %d %d %d %d\n", chunks[0], chunks[1], chunks[2], chunks[3]);

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
      int checkname;
      int iz;
      int status;

      if ( code < 0 ) code = -code;

      sprintf(varname, "var%d", code);

      checkname = TRUE;
      iz = 0;

      while ( checkname ) 
	{
	  if ( iz ) sprintf(varname, "var%d_%d", code, iz+1);

	  status = nc_inq_varid(fileID, varname, &ncvarid);
	  if ( status != NC_NOERR )
	    {
	      checkname = FALSE;
	    }

	  if ( checkname ) iz++;

	  if ( iz > 99 ) break;
	}

      name = varname;
      code = 0;
    }

  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  xtype = cdfDefDatatype(vlistInqVarDatatype(vlistID, varID));

  cdf_def_var(fileID, name, (nc_type) xtype, ndims, dims, &ncvarid);

#if  defined  (NC_NETCDF4)
  if ( streamptr->filetype == FILETYPE_NC4 )
    if ((retval = nc_def_var_chunking(fileID, ncvarid, 0, chunks)))
      {
	Error(func, "nc_def_var_chunking failed, status = %d", retval);
      }
#endif

  if ( streamptr->ztype == COMPRESS_ZIP )
    {
      if ( streamptr->filetype == FILETYPE_NC4 )
	{
	  cdfDefVarDeflate(fileID, ncvarid, streamptr->zlevel);
	}
      else
	{
	  static int lwarn = TRUE;

	  if ( lwarn )
	    {
	      lwarn = FALSE;
	      Warning(func, "Compression is only available for netCDF4!");
	    }
	}
    }

  if ( longname && *longname )
    cdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);

  if ( stdname && *stdname )
    cdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);

  if ( units && *units )
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

  if ( gridInqType(gridID) == GRID_SINUSOIDAL )
    {
      char mapping[] = "sinusoidal";
      cdf_put_att_text(fileID, ncvarid, "grid_mapping", strlen(mapping), mapping);
    }

  if ( gridInqType(gridID) == GRID_LAEA )
    {
      char mapping[] = "laea";
      cdf_put_att_text(fileID, ncvarid, "grid_mapping", strlen(mapping), mapping);
    }

  if ( gridInqType(gridID) == GRID_LCC2 )
    {
      char mapping[] = "Lambert_Conformal";
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
      ncxvarID = streamptr->ncxvarID[gridindex];
      ncyvarID = streamptr->ncyvarID[gridindex];
      ncavarID = streamptr->ncavarID[gridindex];
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

      axis[iax++] = '-';
      axis[iax++] = '-';
      cdf_put_att_text(fileID, ncvarid, "axis", iax, axis);
      cdf_put_att_int(fileID, ncvarid, "truncation", NC_INT, 1L, &gridTruncation);
    }

  /*  if ( xtype == NC_BYTE || xtype == NC_SHORT || xtype == NC_INT ) */
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;
      int astype = NC_DOUBLE;

      addoffset    = vlistInqVarAddoffset(vlistID, varID);
      scalefactor  = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = IS_NOT_EQUAL(addoffset, 0);
      lscalefactor = IS_NOT_EQUAL(scalefactor, 1);

      if ( laddoffset || lscalefactor )
	{
	  if ( IS_EQUAL(addoffset, (double) ((float) addoffset)) &&
	       IS_EQUAL(scalefactor, (double) ((float) scalefactor)) )
	    {
	      astype = NC_FLOAT;
	    }

	  if ( xtype == (int) NC_FLOAT ) astype = NC_FLOAT;
	}

      if ( laddoffset )
	cdf_put_att_double(fileID, ncvarid, "add_offset", (nc_type) astype, 1L, &addoffset);

      if ( lscalefactor )
	cdf_put_att_double(fileID, ncvarid, "scale_factor", (nc_type) astype, 1L, &scalefactor);
    }

  streamptr->vars[varID].ncvarid = ncvarid;

  /* Attributes */
  defineAttributes(vlistID, varID, fileID, ncvarid);

  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  tsID = streamptr->curTsID;

  if ( CDI_Debug ) Message(func, "tsID = %d", tsID);

  ncvarid = streamptr->vars[varID].ncvarid;

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
      xid = streamptr->xdimID[gridindex];
      yid = streamptr->ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streamptr->zaxisID[zaxisindex];

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
  laddoffset   = IS_NOT_EQUAL(addoffset, 0);
  lscalefactor = IS_NOT_EQUAL(scalefactor, 1);

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
		  if ( lscalefactor ) data[i] *= scalefactor;
		  if ( laddoffset )   data[i] += addoffset;
		}
	    }
	}
      else
	{
	  for ( i = 0; i < (int) size; i++ )
	    {
	      if ( lscalefactor ) data[i] *= scalefactor;
	      if ( laddoffset )   data[i] += addoffset;
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ntsteps = streamptr->ntsteps;
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
      xid = streamptr->xdimID[gridindex];
      yid = streamptr->ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streamptr->zaxisID[zaxisindex];

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

  if ( streamptr->ncmode == 1 )
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  dtype = vlistInqVarDatatype(vlistID, varID);

  if ( nmiss > 0 ) cdfDefMissval(streamID, varID, dtype);

  /*  if ( dtype == DATATYPE_INT8 || dtype == DATATYPE_INT16 || dtype == DATATYPE_INT32 ) */
    {
      int nvals;
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;
      double missval;

      addoffset    = vlistInqVarAddoffset(vlistID, varID);
      scalefactor  = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = IS_NOT_EQUAL(addoffset, 0);
      lscalefactor = IS_NOT_EQUAL(scalefactor, 1);

      nvals = gridInqSize(gridID)*zaxisInqSize(zaxisID);

      if ( laddoffset || lscalefactor )
	{
	  missval = vlistInqVarMissval(vlistID, varID);

	  if ( nmiss > 0 )
	    {
	      for ( i = 0; i < nvals; i++ )
		{
		  if ( !DBL_IS_EQUAL(data[i], missval) )
		    {
		      if ( laddoffset )   data[i] -= addoffset;
		      if ( lscalefactor ) data[i] /= scalefactor;
		    }
		}
	    }
	  else
	    {
	      for ( i = 0; i < nvals; i++ )
		{
		  if ( laddoffset )   data[i] -= addoffset;
		  if ( lscalefactor ) data[i] /= scalefactor;
		}
	    }
	}

      if ( dtype == DATATYPE_UINT8 || dtype == DATATYPE_INT8 ||
	   dtype == DATATYPE_INT16 || dtype == DATATYPE_INT32 )
	{
	  for ( i = 0; i < nvals; i++ ) data[i] = NINT(data[i]);
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
  int gridsize;
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
  int swapxy = FALSE;
  double missval;
  int laddoffset, lscalefactor;
  double addoffset, scalefactor;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);


  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d  levelID = %d", streamID, varID, levelID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  tsID = streamptr->curTsID;
  if ( CDI_Debug )
    Message(func, "tsID = %d", tsID);

  ncvarid = streamptr->vars[varID].ncvarid;

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridsize = gridInqSize(gridID);
      
  streamptr->numvals += gridsize;

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      cdfReadGridTraj(streamID, gridID);
    }
  else if ( gridInqType(gridID) == GRID_CELL )
    {
      xid = streamptr->xdimID[gridindex];
    }
  else
    {
      xid = streamptr->xdimID[gridindex];
      yid = streamptr->ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streamptr->zaxisID[zaxisindex];
  /*
  printf("2 %d %d %d %s\n", streamID, zaxisindex, streamptr->zaxisID[zaxisindex], vlistInqVarNamePtr(vlistID, varID));
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
	  Error(func, "Unsupported array structure for: %s", name);
	}
    }

  if ( swapyz == FALSE && yid != UNDEFID && xid != UNDEFID )
    {
      if ( xid == dimids[ndimsp-1] && yid == dimids[ndimsp-2] )
	swapxy = FALSE;
      else if ( yid == dimids[ndimsp-1] && xid == dimids[ndimsp-2] )
	swapxy = TRUE;
      else
	{
	  char name[256];
	  vlistInqVarName(vlistID, varID, name);
	  Error(func, "Unsupported array structure for: %s", name);
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
      if ( xid != UNDEFID )
	{
	  start[ndims] = 0;
	  /* count[ndims] = gridInqXsize(gridID); */
	  cdf_inq_dimlen(fileID, xid, &size);
	  count[ndims] = size;
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

      if ( swapxy )
	{
	  if ( xid != UNDEFID )
	    {
	      start[ndims] = 0;
	      /* count[ndims] = gridInqXsize(gridID); */
	      cdf_inq_dimlen(fileID, xid, &size);
	      count[ndims] = size;
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
      else
	{
	  if ( yid != UNDEFID )
	    {
	      start[ndims] = 0;
	      /* count[ndims] = gridInqYsize(gridID); */
	      cdf_inq_dimlen(fileID, yid, &size);
	      count[ndims] = size;
	      ndims++;
	    }
	  if ( xid != UNDEFID )
	    {
	      start[ndims] = 0;
	      /* count[ndims] = gridInqXsize(gridID); */
	      cdf_inq_dimlen(fileID, xid, &size);
	      count[ndims] = size;
	      ndims++;
	    }
	}
    }

  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  cdf_get_vara_double(fileID, ncvarid, start, count, data);

  *nmiss = 0;
  if ( vlistInqVarMissvalUsed(vlistID, varID) == TRUE  )
    {
      missval = vlistInqVarMissval(vlistID, varID);
      for ( i = 0; i < gridsize; i++ )
	if ( DBL_IS_EQUAL(data[i], missval) ) *nmiss += 1;
    }

  addoffset    = vlistInqVarAddoffset(vlistID, varID);
  scalefactor  = vlistInqVarScalefactor(vlistID, varID);
  laddoffset   = IS_NOT_EQUAL(addoffset, 0);
  lscalefactor = IS_NOT_EQUAL(scalefactor, 1);

  if ( laddoffset || lscalefactor )
    {
      missval = vlistInqVarMissval(vlistID, varID);

      if ( *nmiss > 0 )
	{
	  for ( i = 0; i < gridsize; i++ )
	    {
	      if ( !DBL_IS_EQUAL(data[i], missval) )
		{
		  if ( lscalefactor ) data[i] *= scalefactor;
		  if ( laddoffset )   data[i] += addoffset;
		}
	    }
	}
      else
	{
	  for ( i = 0; i < gridsize; i++ )
	    {
	      if ( lscalefactor ) data[i] *= scalefactor;
	      if ( laddoffset )   data[i] += addoffset;
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
  STREAM *streamptr;
  extern int CDF_Debug;

  streamptr = stream_to_pointer(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  ntsteps = streamptr->ntsteps;
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
      xid = streamptr->xdimID[gridindex];
      yid = streamptr->ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streamptr->zaxisID[zaxisindex];

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

  /*  if ( dtype == DATATYPE_INT8 || dtype == DATATYPE_INT16 || dtype == DATATYPE_INT32 ) */
    {
      int nvals;
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;
      double missval;

      addoffset    = vlistInqVarAddoffset(vlistID, varID);
      scalefactor  = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = IS_NOT_EQUAL(addoffset, 0);
      lscalefactor = IS_NOT_EQUAL(scalefactor, 1);

      missval      = vlistInqVarMissval(vlistID, varID);

      nvals = gridInqSize(gridID);

      if ( laddoffset || lscalefactor )
	{
	  if ( nmiss > 0 )
	    {
	      for ( i = 0; i < nvals; i++ )
		{
		  if ( !DBL_IS_EQUAL(data[i], missval) )
		    {
		      if ( laddoffset )   data[i] -= addoffset;
		      if ( lscalefactor ) data[i] /= scalefactor;
		    }
		}
	    }
	  else
	    {
	      for ( i = 0; i < nvals; i++ )
		{
		  if ( laddoffset )   data[i] -= addoffset;
		  if ( lscalefactor ) data[i] /= scalefactor;
		}
	    }
	}

      if ( dtype == DATATYPE_UINT8 || dtype == DATATYPE_INT8 ||
	   dtype == DATATYPE_INT16 || dtype == DATATYPE_INT32 )
	{
	  for ( i = 0; i < nvals; i++ ) data[i] = NINT(data[i]);
	}

      if ( CDF_Debug )
	{
	  double fmin, fmax;
	  fmin =  1.0e200;
	  fmax = -1.0e200;
	  for ( i = 1; i < nvals; ++i )
	    {
	      if ( !DBL_IS_EQUAL(data[i], missval) )
		{
		  if ( data[i] < fmin ) fmin = data[i];
		  if ( data[i] > fmax ) fmax = data[i];
		}
	    }
	  Message(func, "nvals = %d, nmiss = %d, missval = %g, minval = %g, maxval = %g",
		  nvals, nmiss, missval, fmin, fmax);
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID  = streamInqVlist(streamID);

  if ( tsID < 0 || (tsID >= streamptr->ntsteps && tsID > 0) ) return;

  if ( streamptr->tsteps[tsID].nallrecs > 0 ) return;

  if ( tsID == 0 )
    {
      nvars = vlistNvars(vlistID);
      nrecs = vlistNrecs(vlistID);

      streamptr->nrecs += nrecs;

      if ( nrecs > 0 ) records = (RECORD *) malloc(nrecs*sizeof(RECORD));
      streamptr->tsteps[tsID].records    = records;
      streamptr->tsteps[tsID].nrecs      = nrecs;
      streamptr->tsteps[tsID].nallrecs   = nrecs;
      streamptr->tsteps[tsID].recordSize = nrecs;
      streamptr->tsteps[tsID].curRecID   = UNDEFID;

      nvrecs = nrecs; /* use all records at first timestep */
      if ( nvrecs > 0 ) recIDs = (int *) malloc(nvrecs*sizeof(int));
      streamptr->tsteps[tsID].recIDs     = recIDs;
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

      streamptr->nrecs += nvrecs;

      records = (RECORD *) malloc(nrecs*sizeof(RECORD));
      streamptr->tsteps[tsID].records    = records;
      streamptr->tsteps[tsID].nrecs      = nvrecs;
      streamptr->tsteps[tsID].nallrecs   = nrecs;
      streamptr->tsteps[tsID].recordSize = nrecs;
      streamptr->tsteps[tsID].curRecID   = UNDEFID;

      memcpy(streamptr->tsteps[tsID].records,
	     streamptr->tsteps[0].records,
	     nrecs*sizeof(RECORD));

      recIDs = (int *) malloc(nvrecs*sizeof(int));
      streamptr->tsteps[tsID].recIDs     = recIDs;
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

      nvrecs = streamptr->tsteps[1].nrecs;

      streamptr->nrecs += nvrecs;

      records = (RECORD *) malloc(nrecs*sizeof(RECORD));
      streamptr->tsteps[tsID].records    = records;
      streamptr->tsteps[tsID].nrecs      = nvrecs;
      streamptr->tsteps[tsID].nallrecs   = nrecs;
      streamptr->tsteps[tsID].recordSize = nrecs;
      streamptr->tsteps[tsID].curRecID   = UNDEFID;

      memcpy(streamptr->tsteps[tsID].records,
	     streamptr->tsteps[0].records,
	     nrecs*sizeof(RECORD));

      recIDs = (int *) malloc(nvrecs*sizeof(int));
      streamptr->tsteps[tsID].recIDs     = recIDs;

      memcpy(streamptr->tsteps[tsID].recIDs,
	     streamptr->tsteps[1].recIDs,
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
      if ( memcmp(dimname, "time", 4) == 0 ) break;
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
	      if ( memcmp(attname, "units", 5) == 0 )
		{
		  cdfGetAttText(fileID, varid, "units", 256, timeunits);
		  if ( memcmp(timeunits, "sec",    3) == 0 ||
		       memcmp(timeunits, "minute", 6) == 0 ||
		       memcmp(timeunits, "hour",   4) == 0 ||
		       memcmp(timeunits, "day",    3) == 0 ||
		       memcmp(timeunits, "month",  5) == 0 )
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
      if ( ! ncvars[ncvarid].ignore )
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
	      /*
	      if      ( ncvars[ncvarid].tvarid != -1 ) axis[ndim++] = iaxis[0];
	      else if ( ncvars[ncvarid].zvarid != -1 ) axis[ndim++] = iaxis[1];
	      else if ( ncvars[ncvarid].yvarid != -1 ) axis[ndim++] = iaxis[2];
	      else if ( ncvars[ncvarid].xvarid != -1 ) axis[ndim++] = iaxis[3];
	      else
	      */
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

      printf("%3d %3d %3d  %-4s %s\n", ncvarid, ncvars[ncvarid].isvar, ndim-2, axis, ncvars[ncvarid].name);
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
  nc_type xtype, atttype;
  size_t size = 0, xsize, ysize;
  size_t ntsteps;
  int timedimid = -1;
  int *varids;
  int nvarids;
  size_t attlen;
  int  ilev;
  char name[256];
  char attname[256];
  const int attstringlen = 8192; char attstring[8192];
  char *pname, *plongname, *punits;
  double datt;
  int iatt, timehasunit = 0;
  int time_has_bounds = FALSE;
  size_t len;
  int nc_nvars;
  int nvcth_id = UNDEFID, vcta_id = UNDEFID, vctb_id = UNDEFID;
  grid_t grid;
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
  STREAM *streamptr;
  int format = 0;

  streamptr = stream_to_pointer(streamID);
  
  stream_check_ptr(func, streamptr);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d, fileID = %d", streamID, fileID);

#if  defined  (NC_NETCDF4)
  nc_inq_format(fileID, &format);
#endif

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
      ncvars[ncvarid].ignore          = FALSE;
      ncvars[ncvarid].isvar           = UNDEFID;
      ncvars[ncvarid].islon           = FALSE;
      ncvars[ncvarid].islat           = FALSE;
      ncvars[ncvarid].islev           = FALSE;
      ncvars[ncvarid].warn            = FALSE;
      ncvars[ncvarid].timeID          = TIME_CONSTANT;
      ncvars[ncvarid].code            = UNDEFID;
      ncvars[ncvarid].calendar        = FALSE;
      ncvars[ncvarid].bounds          = UNDEFID;
      ncvars[ncvarid].gridID          = UNDEFID;
      ncvars[ncvarid].zaxisID         = UNDEFID;
      ncvars[ncvarid].gridtype        = UNDEFID;
      ncvars[ncvarid].zaxistype       = UNDEFID;
      ncvars[ncvarid].xdim            = UNDEFID;
      ncvars[ncvarid].ydim            = UNDEFID;
      ncvars[ncvarid].zdim            = UNDEFID;
      ncvars[ncvarid].xvarid          = UNDEFID;
      ncvars[ncvarid].yvarid          = UNDEFID;
      ncvars[ncvarid].zvarid          = UNDEFID;
      ncvars[ncvarid].tvarid          = UNDEFID;
      ncvars[ncvarid].ncoordvars      = 0;
      ncvars[ncvarid].coordvarids[0]  = UNDEFID;
      ncvars[ncvarid].coordvarids[1]  = UNDEFID;
      ncvars[ncvarid].coordvarids[2]  = UNDEFID;
      ncvars[ncvarid].coordvarids[3]  = UNDEFID;
      ncvars[ncvarid].cellarea        = UNDEFID;
      ncvars[ncvarid].tableID         = UNDEFID;
      ncvars[ncvarid].xtype           = 0;
      ncvars[ncvarid].ndims           = 0;
      ncvars[ncvarid].gmapid          = UNDEFID;
      ncvars[ncvarid].vlen            = 0;
      ncvars[ncvarid].vdata           = NULL;
      ncvars[ncvarid].truncation      = 0;
      ncvars[ncvarid].positive        = 0;
      ncvars[ncvarid].defmiss         = 0;
      ncvars[ncvarid].missval         = 0;
      ncvars[ncvarid].addoffset       = 0;
      ncvars[ncvarid].scalefactor     = 1;
      ncvars[ncvarid].name[0]         = 0;
      ncvars[ncvarid].longname[0]     = 0;
      ncvars[ncvarid].stdname[0]      = 0;
      ncvars[ncvarid].units[0]        = 0;
      ncvars[ncvarid].natts           = 0;
      ncvars[ncvarid].atts            = NULL;
      ncvars[ncvarid].deflate         = 0;
    }

  /* read global attributtes*/
  for ( iatt = 0; iatt < ngatts; iatt++ )
    {
      cdf_inq_attname(fileID, NC_GLOBAL, iatt, attname);
      cdf_inq_atttype(fileID, NC_GLOBAL, attname, &xtype);
      cdf_inq_attlen(fileID, NC_GLOBAL, attname, &attlen);

      if ( xtype == NC_CHAR )
	{
	  cdfGetAttText(fileID, NC_GLOBAL, attname, attstringlen-1, attstring);
	  attstring[attlen+1] = 0;

	  if ( attlen > 0 && attstring[0] != 0 )
	    {
	      if ( strcmp(attname, "history") == 0 )
		{
		  streamptr->historyID = iatt;
		}
	      else if ( strcmp(attname, "institution") == 0 )
		{
		  instID = institutInq(0, 0, NULL, attstring);
		  if ( instID == UNDEFID )
		    instID = institutDef(0, 0, NULL, attstring);
		}
	      else if ( strcmp(attname, "source") == 0 )
		{
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
		  vlistDefAttTxt(vlistID, CDI_GLOBAL, attname, (int)attlen, attstring);
		}
	    }
	}
      else if ( xtype == NC_INT || xtype == NC_SHORT )
	{
	  int *attint;
	  attint = (int *) malloc(attlen*sizeof(int));
	  cdfGetAttInt(fileID, NC_GLOBAL, attname, attlen, attint);
	  vlistDefAttInt(vlistID, CDI_GLOBAL, attname, (int)attlen, attint);
	  free(attint);
	}
      else if ( xtype == NC_FLOAT || xtype == NC_DOUBLE )
	{
	  double *attflt;
	  attflt = (double *) malloc(attlen*sizeof(double));
	  cdfGetAttDouble(fileID, NC_GLOBAL, attname, attlen, attflt);
	  vlistDefAttFlt(vlistID, CDI_GLOBAL, attname, (int)attlen, attflt);
	  free(attflt);
	}
    }

  /* find time dim */
  if ( unlimdimid >= 0 )
    timedimid = unlimdimid;
  else
    timedimid = cdfTimeDimID(fileID, ndims, nvars);

  streamptr->basetime.ncdimid = timedimid;

  if ( timedimid != UNDEFID )
    cdf_inq_dimlen(fileID, timedimid, &ntsteps);
  else
    ntsteps = 0;

  streamptr->ntsteps = ntsteps;

  if ( CDI_Debug )
    Message(func, "time dimid = %d", streamptr->basetime.ncdimid);

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

#if  defined  (NC_NETCDF4)
      if ( format == NC_FORMAT_NETCDF4_CLASSIC )
	{
	  int shuffle, deflate, deflate_level;
	  nc_inq_var_deflate(fileID, ncvarid, &shuffle, &deflate, &deflate_level);
	  if ( deflate > 0 )
	    ncvars[ncvarid].deflate = 1;
	}
#endif

      if ( nvdims > 0 )
	if ( timedimid == dimidsp[0] )
	  {
	    ncvars[ncvarid].timeID = TIME_VARIABLE;
	    cdfSetDim(ncvars, ncvarid, 0, T_AXIS);
	  }

      for ( iatt = 0; iatt < nvatts; iatt++ )
	{
	  cdf_inq_attname(fileID, ncvarid, iatt, attname);
	  cdf_inq_atttype(fileID, ncvarid, attname, &atttype);
	  cdf_inq_attlen(fileID, ncvarid, attname, &attlen);
	  
	  if ( strcmp(attname, "long_name") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, MAXNAMELEN, ncvars[ncvarid].longname);
      	    }
	  else if ( strcmp(attname, "standard_name") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, MAXNAMELEN, ncvars[ncvarid].stdname);
      	    }
	  else if ( strcmp(attname, "units") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, MAXNAMELEN, ncvars[ncvarid].units);
      	    }
	  else if ( strcmp(attname, "calendar") == 0 )
	    {
	      ncvars[ncvarid].calendar = TRUE;
      	    }
	  else if ( strcmp(attname, "code") == 0 )
	    {
	      cdfGetAttInt(fileID, ncvarid, attname, 1, &ncvars[ncvarid].code);
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "table") == 0 )
	    {
	      cdfGetAttInt(fileID, ncvarid, attname, 1, &tablenum);
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
	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
	      if ( memcmp(attstring, "Triangular", attlen) == 0 )
		ncvars[ncvarid].gridtype = GRID_SPECTRAL;
	    }
	  else if ( strcmp(attname, "grid_type") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);

	      if      ( strcmp(attstring, "gaussian reduced") == 0 )
		ncvars[ncvarid].gridtype = GRID_GAUSSIAN_REDUCED;
	      else if ( strcmp(attstring, "gaussian") == 0 )
		ncvars[ncvarid].gridtype = GRID_GAUSSIAN;
	      else if ( strcmp(attstring, "spectral") == 0 )
		ncvars[ncvarid].gridtype = GRID_SPECTRAL;
	      else if ( strcmp(attstring, "trajectory") == 0 )
		ncvars[ncvarid].gridtype = GRID_TRAJECTORY;
	      else if ( strcmp(attstring, "generic") == 0 )
		ncvars[ncvarid].gridtype = GRID_GENERIC;
	      else if ( strcmp(attstring, "cell") == 0 )
		ncvars[ncvarid].gridtype = GRID_CELL;
	      else if ( strcmp(attstring, "curvilinear") == 0 )
		ncvars[ncvarid].gridtype = GRID_CURVILINEAR;
	      else if ( strcmp(attstring, "sinusoidal") == 0 )
		;
	      else if ( strcmp(attstring, "laea") == 0 )
		;
	      else if ( strcmp(attstring, "lcc2") == 0 )
		;
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
	      cdfGetAttInt(fileID, ncvarid, attname, 1, &ncvars[ncvarid].truncation);
	    }
	  else if ( strcmp(attname, "truncation") == 0 )
	    {
	      cdfGetAttInt(fileID, ncvarid, attname, 1, &ncvars[ncvarid].truncation);
	    }
	  else if ( strcmp(attname, "add_offset") == 0 )
	    {
	      if ( atttype != NC_CHAR )
		{
		  cdfGetAttDouble(fileID, ncvarid, attname, 1, &ncvars[ncvarid].addoffset);
		  /*
		    if ( atttype != NC_BYTE && atttype != NC_SHORT && atttype != NC_INT )
		    if ( ncvars[ncvarid].addoffset != 0 )
		    Warning(func, "attribute add_offset not supported for atttype %d\n", atttype);
		  */
		  cdfSetVar(ncvars, ncvarid, TRUE);
		}
	    }
	  else if ( strcmp(attname, "scale_factor") == 0 )
	    {
	      if ( atttype != NC_CHAR )
		{
		  cdfGetAttDouble(fileID, ncvarid, attname, 1, &ncvars[ncvarid].scalefactor);
		  /*
		    if ( atttype != NC_BYTE && atttype != NC_SHORT && atttype != NC_INT )
		    if ( ncvars[ncvarid].scalefactor != 1 )
		    Warning(func, "attribute scale_factor not supported for atttype %d\n", atttype);
		  */
		  cdfSetVar(ncvars, ncvarid, TRUE);
		}
	    }
	  else if ( strcmp(attname, "bounds") == 0 )
	    {
	      int status, ncboundsid;

	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);

	      status = nc_inq_varid(fileID, attstring, &ncboundsid);

	      if ( status == NC_NOERR )
		{
		  ncvars[ncvarid].bounds = ncboundsid;
		  cdfSetVar(ncvars, ncvars[ncvarid].bounds, FALSE);
		  cdfSetVar(ncvars, ncvarid, FALSE);
		}
	      else
		Warning(func, "%s - %s", nc_strerror(status), attstring);
	    }
	  else if ( strcmp(attname, "cell_measures") == 0 )
	    {
	      char *pstring, *cell_measures = NULL, *cell_var = NULL;

	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
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
	      if ( memcmp(cell_measures, "area", 4) == 0 )
		{
		  int status;
		  int nc_cell_id;

		  status = nc_inq_varid(fileID, cell_var, &nc_cell_id);
		  if ( status == NC_NOERR )
		    {
		      ncvars[ncvarid].cellarea = nc_cell_id;
		      cdfSetVar(ncvars, ncvars[ncvarid].cellarea, FALSE);
		    }
		  else
		    Warning(func, "%s - %s", nc_strerror(status), cell_var);
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
	      
	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
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
	  else if ( strcmp(attname, "associate")  == 0 || 
		    strcmp(attname, "coordinates") == 0 )
	    {
	      int status;
	      char *pstring, *varname = NULL;
	      int lstop = FALSE;
	      int dimvarid;
	      extern int cdiIgnoreAttCoordinates;
	      
	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
	      pstring = attstring;

	      for ( i = 0; i < 4; i++ )
		{
		  while ( isspace((int) *pstring) ) pstring++;
		  if ( *pstring == 0 ) break;
		  varname = pstring;
		  while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
		  if ( *pstring == 0 ) lstop = TRUE;
		  *pstring++ = 0;

		  status = nc_inq_varid(fileID, varname, &dimvarid);
		  if ( status == NC_NOERR )
		    {
		      cdfSetVar(ncvars, dimvarid, FALSE);
		      if ( cdiIgnoreAttCoordinates == FALSE )
			{
			  ncvars[ncvarid].coordvarids[i] = dimvarid;
			  ncvars[ncvarid].ncoordvars++;
			}
		    }
		  else
		    Warning(func, "%s - %s", nc_strerror(status), varname);

		  if ( lstop ) break;
		}
	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "grid_mapping") == 0 )
	    {
	      int status;
	      int nc_gmap_id;
	      
	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);

	      status = nc_inq_varid(fileID, attstring, &nc_gmap_id);
	      if ( status == NC_NOERR )
		{
		  ncvars[ncvarid].gmapid = nc_gmap_id;
		  cdfSetVar(ncvars, ncvars[ncvarid].gmapid, FALSE);
		}
	      else
		Warning(func, "%s - %s", nc_strerror(status), attstring);

	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else if ( strcmp(attname, "positive") == 0 )
	    {	      
	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);

	      if    ( memcmp(attstring, "down", 4) == 0 ) ncvars[ncvarid].positive = -1;
	      else if ( memcmp(attstring, "up", 2) == 0 ) ncvars[ncvarid].positive = 1;

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
	      if ( atttype != NC_CHAR )
		{
		  cdfGetAttDouble(fileID, ncvarid, attname, 1, &ncvars[ncvarid].missval);
		  ncvars[ncvarid].defmiss = TRUE;
		  /* cdfSetVar(ncvars, ncvarid, TRUE); */
		}
	    }
	  else if ( strcmp(attname, "cdi") == 0 )
	    {	      
	      if ( atttype == NC_CHAR )
		{
		  cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
		  if ( memcmp(attstring, "ignore", 6) == 0 ||
		       memcmp(attstring, "Ignore", 6) == 0 )
		    {
		      ncvars[ncvarid].ignore = TRUE;
		      cdfSetVar(ncvars, ncvarid, FALSE);
		    }
		}
	    }
	  else if ( strcmp(attname, "axis") == 0 )
	    {
	      cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
	      /* don't check attlen anymore because the meaning changed from COARDS to CF */
	      /*
	      if ( (int) attlen != nvdims )
		{
		  if ( (int)(attlen-1) == nvdims && attstring[attlen] == 0 ) attlen--;
		  else
		    {
		      if ( nvdims > 0 )
			Warning(func, "axis attribute length %d does not match nvdim %d for %s",
				(int) attlen, nvdims, name);
		      if ( nvdims > 0 )
			{
			  fprintf(stderr, "attlen %d %d %d\n", (int) strlen(attstring), (int) attlen, nvdims);
			  for ( i = 0; i < (int)attlen; i++ )
			    fprintf(stderr, "       %d %d %c\n", i, (int) attstring[i], attstring[i]);
			}
		    }
		}
	      else
	      */
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
	  else
	    {
	      if ( ncvars[ncvarid].natts == 0 )
		ncvars[ncvarid].atts = (int *) malloc(nvatts*sizeof(int));

	      ncvars[ncvarid].atts[ncvars[ncvarid].natts++] = iatt;
	      /*
	      int attrint;
	      double attrflt;
	      nc_type attrtype;
	      cdf_inq_attlen(fileID, ncvarid, attname, &attlen);
	      cdf_inq_atttype(fileID, ncvarid, attname, &attrtype);
	      if ( attlen == 1 && (attrtype == NC_INT || attrtype == NC_SHORT) )
		{
		  cdfGetAttInt(fileID, ncvarid, attname, 1, &attrint);
		  printf("int: %s.%s = %d\n", ncvars[ncvarid].name, attname, attrint);
		}
	      else if ( attlen == 1 && (attrtype == NC_FLOAT || attrtype == NC_DOUBLE) )
		{
		  cdfGetAttDouble(fileID, ncvarid, attname, 1, &attrflt);
		  printf("flt: %s.%s = %g\n", ncvars[ncvarid].name, attname, attrflt);
		}
	      else if ( attrtype == NC_CHAR )
		{
		  cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
		  attstring[attlen] = 0;
		  printf("txt: %s.%s = %s\n", ncvars[ncvarid].name, attname, attstring);
		}
	      else
		printf("att: %s.%s = unknown\n", ncvars[ncvarid].name, attname);
	      */
	    }
	}
    }

  if ( CDI_Debug ) printNCvars(ncvars, nvars);

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
	streamptr->basetime.ncvarid = ncdims[timedimid].ncvarid;
      else
	{
	  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
	    if ( ncvars[ncvarid].ndims == 1 &&
		 timedimid == ncvars[ncvarid].dimids[0] &&
		 ncvars[ncvarid].xtype != NC_CHAR )
	      {
		streamptr->basetime.ncvarid = ncvarid;
		break;
	      }
	}

      /* time varID */
      ncvarid = streamptr->basetime.ncvarid;

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
		      streamptr->basetime.ncvarboundsid = ncvars[ncvarid].bounds;
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
	  Warning(func, "%d dimensional variables unsupported. Skip variable %s",
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
	      if ( memcmp(ncvars[ncvarid].longname, "Longitude", 9) == 0 ||
		   memcmp(ncvars[ncvarid].longname, "longitude", 9) == 0 )
		{
		  ncvars[ncvarid].islon = TRUE;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, X_AXIS);
		  ncdims[ncdimid].dimtype = X_AXIS;
		  continue;
		}
	      else if ( memcmp(ncvars[ncvarid].longname, "Latitude", 8) == 0 || 
			memcmp(ncvars[ncvarid].longname, "latitude", 8) == 0 )
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
	      if ( memcmp(ncvars[ncvarid].units, "degrees_east", 12) == 0 ||
		   memcmp(ncvars[ncvarid].units, "degree_east", 11)  == 0 ||
		   memcmp(ncvars[ncvarid].units, "degree_E", 8)      == 0 ||
		   memcmp(ncvars[ncvarid].units, "degrees_E", 9)     == 0 ||
		   memcmp(ncvars[ncvarid].units, "degreeE", 7)       == 0 ||
		   memcmp(ncvars[ncvarid].units, "degreesE", 8)      == 0 ||
                   memcmp(ncvars[ncvarid].stdname, "longitude", 9)   == 0 ||
		  (memcmp(ncvars[ncvarid].units, "degrees", 7)           == 0 &&
		   memcmp(ncvars[ncvarid].stdname, "grid_longitude", 14) == 0) ||
		  (memcmp(ncvars[ncvarid].units, "radian", 6)            == 0 &&
		   memcmp(ncvars[ncvarid].stdname, "grid_longitude", 14) == 0) )
		{
		  ncvars[ncvarid].islon = TRUE;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, X_AXIS);
		  ncdims[ncdimid].dimtype = X_AXIS;
		}
	      else if ( memcmp(ncvars[ncvarid].units, "degrees_north", 13) == 0 ||
			memcmp(ncvars[ncvarid].units, "degree_north", 12)  == 0 ||
			memcmp(ncvars[ncvarid].units, "degree_N", 8)       == 0 ||
			memcmp(ncvars[ncvarid].units, "degrees_N", 9)      == 0 ||
			memcmp(ncvars[ncvarid].units, "degreeN", 7)        == 0 ||
			memcmp(ncvars[ncvarid].units, "degreesN", 8)       == 0 ||
                        memcmp(ncvars[ncvarid].stdname, "latitude", 8)     == 0 ||
	       	       (memcmp(ncvars[ncvarid].units, "degrees", 7)          == 0 &&
			memcmp(ncvars[ncvarid].stdname, "grid_latitude", 13) == 0) ||
	               (memcmp(ncvars[ncvarid].units, "radian", 6)           == 0 &&
	           	memcmp(ncvars[ncvarid].stdname, "grid_latitude", 13) == 0) )
		{
		  ncvars[ncvarid].islat = TRUE;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, Y_AXIS);
		  ncdims[ncdimid].dimtype = Y_AXIS;
		}
	      else if ( memcmp(ncvars[ncvarid].units, "millibar", 8) == 0 ||
			memcmp(ncvars[ncvarid].units, "mb", 2)       == 0 ||
			memcmp(ncvars[ncvarid].units, "hPa", 3)      == 0 ||
			memcmp(ncvars[ncvarid].units, "Pa", 2)       == 0 )
		{
		  ncvars[ncvarid].zaxistype = ZAXIS_PRESSURE;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "level") == 0 )
		{
		  if      ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer midpoints") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( memcmp(ncvars[ncvarid].longname, "hybrid level at midpoints", 25) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer interfaces") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else if ( memcmp(ncvars[ncvarid].longname, "hybrid level at interfaces", 26) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else
		    ncvars[ncvarid].zaxistype = ZAXIS_GENERIC;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "cm")  == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth below land")         == 0 ||
		       strcmp(ncvars[ncvarid].longname, "depth_below_land")         == 0 ||
		       strcmp(ncvars[ncvarid].longname, "levels below the surface") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_LAND;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "m")   == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth_below_sea") == 0 ||
		       strcmp(ncvars[ncvarid].longname, "depth below sea") == 0 )
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
	      if ( memcmp(ncvars[ncvarid].longname, "Longitude", 9) == 0 ||
		   memcmp(ncvars[ncvarid].longname, "longitude", 9) == 0 )
		{
		  ncvars[ncvarid].islon = TRUE;
		  continue;
		}
	      else if ( memcmp(ncvars[ncvarid].longname, "Latitude", 8) == 0 || 
			memcmp(ncvars[ncvarid].longname, "latitude", 8) == 0 )
		{
		  ncvars[ncvarid].islat = TRUE;
		  continue;
		}
	    }

	  if ( ncvars[ncvarid].units[0] != 0 )
	    {
	      if ( memcmp(ncvars[ncvarid].units, "degrees_east", 12) == 0 ||
		   memcmp(ncvars[ncvarid].units, "degree_east", 11)  == 0 ||
		   memcmp(ncvars[ncvarid].units, "degree_E", 8)      == 0 ||
		   memcmp(ncvars[ncvarid].units, "degrees_E", 9)     == 0 ||
		   memcmp(ncvars[ncvarid].units, "degreeE", 7)       == 0 ||
		   memcmp(ncvars[ncvarid].units, "degreesE", 8)      == 0 ||
                   memcmp(ncvars[ncvarid].stdname, "longitude", 9)   == 0 ||
		  (memcmp(ncvars[ncvarid].units, "degrees", 7)           == 0 &&
		   memcmp(ncvars[ncvarid].stdname, "grid_longitude", 14) == 0) ||
		  (memcmp(ncvars[ncvarid].units, "radian", 6)            == 0 &&
		   memcmp(ncvars[ncvarid].stdname, "grid_longitude", 14) == 0) )
		{
		  ncvars[ncvarid].islon = TRUE;
		  continue;
		}
	      else if ( memcmp(ncvars[ncvarid].units, "degrees_north", 13) == 0 ||
			memcmp(ncvars[ncvarid].units, "degree_north", 12)  == 0 ||
			memcmp(ncvars[ncvarid].units, "degree_N", 8)       == 0 ||
			memcmp(ncvars[ncvarid].units, "degrees_N", 9)      == 0 ||
			memcmp(ncvars[ncvarid].units, "degreeN", 7)        == 0 ||
			memcmp(ncvars[ncvarid].units, "degreesN", 8)       == 0 ||
                        memcmp(ncvars[ncvarid].stdname, "latitude", 8)     == 0 ||
	       	       (memcmp(ncvars[ncvarid].units, "degrees", 7)          == 0 &&
			memcmp(ncvars[ncvarid].stdname, "grid_latitude", 13) == 0) ||
	               (memcmp(ncvars[ncvarid].units, "radian", 6)           == 0 &&
	           	memcmp(ncvars[ncvarid].stdname, "grid_latitude", 13) == 0) )
		{
		  ncvars[ncvarid].islat = TRUE;
		  continue;
		}
	      else if ( memcmp(ncvars[ncvarid].units, "millibar", 8) == 0 ||
			memcmp(ncvars[ncvarid].units, "mb", 2)       == 0 ||
			memcmp(ncvars[ncvarid].units, "hPa", 3)      == 0 ||
			memcmp(ncvars[ncvarid].units, "Pa", 2)       == 0 )
		{
		  ncvars[ncvarid].zaxistype = ZAXIS_PRESSURE;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "level") == 0 )
		{
		  if      ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer midpoints") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( memcmp(ncvars[ncvarid].longname, "hybrid level at midpoints", 25) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer interfaces") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else if ( memcmp(ncvars[ncvarid].longname, "hybrid level at interfaces", 26) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else
		    ncvars[ncvarid].zaxistype = ZAXIS_GENERIC;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "cm")  == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth below land")         == 0 ||
		       strcmp(ncvars[ncvarid].longname, "depth_below_land")         == 0 ||
		       strcmp(ncvars[ncvarid].longname, "levels below the surface") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_LAND;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "m")   == 0 )
		{
		  if ( strcmp(ncvars[ncvarid].longname, "depth_below_sea") == 0 ||
		       strcmp(ncvars[ncvarid].longname, "depth below sea") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_SEA;
		  else if ( strcmp(ncvars[ncvarid].longname, "height") == 0 ||
			    strcmp(ncvars[ncvarid].longname, "height above the surface") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HEIGHT;
		  continue;
		}
	    }
	}
    }

  if ( CDI_Debug ) printNCvars(ncvars, nvars);

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
	      if      ( ncdims[ncdimid].dimtype == X_AXIS ) cdfSetDim(ncvars, ncvarid, i, X_AXIS);
	      else if ( ncdims[ncdimid].dimtype == Y_AXIS ) cdfSetDim(ncvars, ncvarid, i, Y_AXIS);
	      else if ( ncdims[ncdimid].dimtype == Z_AXIS ) cdfSetDim(ncvars, ncvarid, i, Z_AXIS);
	      else if ( ncdims[ncdimid].dimtype == T_AXIS ) cdfSetDim(ncvars, ncvarid, i, T_AXIS);
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

  /* Set coordinate varids (att: associate)  */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE && ncvars[ncvarid].ncoordvars )
	{
	  ndims = ncvars[ncvarid].ndims;
	  ndims = ncvars[ncvarid].ncoordvars;
	  for ( i = 0; i < ndims; i++ )
	    {
	      if ( ncvars[ncvars[ncvarid].coordvarids[i]].islon )
		ncvars[ncvarid].xvarid = ncvars[ncvarid].coordvarids[i];
	      else if ( ncvars[ncvars[ncvarid].coordvarids[i]].islat )
		ncvars[ncvarid].yvarid = ncvars[ncvarid].coordvarids[i];
	      else if ( ncvars[ncvars[ncvarid].coordvarids[i]].islev )
		ncvars[ncvarid].zvarid = ncvars[ncvarid].coordvarids[i];
	    }
	}
    }
  
  if ( CDI_Debug ) printNCvars(ncvars, nvars);

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

	  if ( ncvars[ncvarid].xvarid != UNDEFID )
	    xvarid = ncvars[ncvarid].xvarid;
	  else if ( xdimid != UNDEFID )
	    xvarid = ncdims[xdimid].ncvarid;

	  if ( ncvars[ncvarid].yvarid != UNDEFID )
	    yvarid = ncvars[ncvarid].yvarid;
	  else if ( ydimid != UNDEFID )
	    yvarid = ncdims[ydimid].ncvarid;

	  /*
	  if ( xdimid != UNDEFID )
	    xvarid = ncdims[xdimid].ncvarid;
	  if ( xvarid == UNDEFID && ncvars[ncvarid].xvarid != UNDEFID )
	    xvarid = ncvars[ncvarid].xvarid;

	  if ( ydimid != UNDEFID )
	    yvarid = ncdims[ydimid].ncvarid;
	  if ( yvarid == UNDEFID && ncvars[ncvarid].yvarid != UNDEFID )
	    yvarid = ncvars[ncvarid].yvarid;
	  */

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

	  memset(&grid, 0, sizeof(grid_t));

	  grid.prec  = DATATYPE_FLT64;
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
	      if ( xvarid != UNDEFID && yvarid != UNDEFID )
		{
		  if ( ncvars[xvarid].ndims != ncvars[yvarid].ndims )
		    {
		      Warning(func, "Inconsistent grid structure for variable %s!",
			      ncvars[ncvarid].name);
		      ncvars[ncvarid].xvarid = UNDEFID;
		      ncvars[ncvarid].yvarid = UNDEFID;
		      xvarid = UNDEFID;
		      yvarid = UNDEFID;
		    }

		  if ( ncvars[xvarid].ndims > 2 || ncvars[yvarid].ndims > 2 )
		    {
		      Warning(func, "Unsupported grid structure for variable %s (grid dims > 2)!",
			      ncvars[ncvarid].name);
		      ncvars[ncvarid].xvarid = UNDEFID;
		      ncvars[ncvarid].yvarid = UNDEFID;
		      xvarid = UNDEFID;
		      yvarid = UNDEFID;
		    }
		}

	      if ( xvarid != UNDEFID )
		{
		  islon = ncvars[xvarid].islon;
		  if ( ncvars[xvarid].ndims == 2 )
		    {
		      ncvars[ncvarid].gridtype = GRID_CURVILINEAR;
		      size = xsize*ysize;
		      /* Check size of 2 dimensional coordinate variables */
		      {
			int dimid;
			size_t dimsize1, dimsize2;
			dimid = ncvars[xvarid].dimids[0];
			cdf_inq_dimlen(fileID, dimid, &dimsize1);
			dimid = ncvars[xvarid].dimids[1];
			cdf_inq_dimlen(fileID, dimid, &dimsize2);
			if ( dimsize1*dimsize2 != size )
			  {
			    Warning(func, "Variable %s has unsupported array structure, skipped!",
				    ncvars[ncvarid].name);
			    ncvars[ncvarid].isvar = -1;
			    continue;
			  }
		      }
		    }
		  else
		    {
		      size = xsize;
		      /* Check size of 1 dimensional coordinate variables */
		      {
			int dimid;
			size_t dimsize;
			dimid = ncvars[xvarid].dimids[0];
			cdf_inq_dimlen(fileID, dimid, &dimsize);
			if ( dimsize != size )
			  {
			    Warning(func, "Variable %s has unsupported array structure, skipped!",
				    ncvars[ncvarid].name);
			    ncvars[ncvarid].isvar = -1;
			    continue;
			  }
		      }
		    }

		  if ( ncvars[xvarid].xtype == NC_FLOAT ) grid.prec = DATATYPE_FLT32;
		  grid.xvals = (double *) malloc(size*sizeof(double));
		  cdf_get_var_double(fileID, xvarid, grid.xvals);
		  strcpy(grid.xname, ncvars[xvarid].name);
		  strcpy(grid.xlongname, ncvars[xvarid].longname);
		  strcpy(grid.xunits, ncvars[xvarid].units);
		  /* don't change the name !!! */
		  /*
		  if ( (len = strlen(grid.xname)) > 2 )
		    if ( grid.xname[len-2] == '_' && isdigit((int) grid.xname[len-1]) )
		      grid.xname[len-2] = 0;
		  */
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
		      /* Check size of 2 dimensional coordinate variables */
		      {
			int dimid;
			size_t dimsize1, dimsize2;
			dimid = ncvars[xvarid].dimids[0];
			cdf_inq_dimlen(fileID, dimid, &dimsize1);
			dimid = ncvars[xvarid].dimids[1];
			cdf_inq_dimlen(fileID, dimid, &dimsize2);
			if ( dimsize1*dimsize2 != size )
			  {
			    Warning(func, "Variable %s has unsupported array structure, skipped!",
				    ncvars[ncvarid].name);
			    ncvars[ncvarid].isvar = -1;
			    continue;
			  }
		      }
		    }
		  else
		    {
		      if ( (int) ysize == 0 ) size = xsize;
		      else                    size = ysize;

		      /* Check size of 1 dimensional coordinate variables */
		      {
			int dimid;
			size_t dimsize;
			dimid = ncvars[yvarid].dimids[0];
			cdf_inq_dimlen(fileID, dimid, &dimsize);
			if ( dimsize != size )
			  {
			    Warning(func, "Variable %s has unsupported array structure, skipped!",
				    ncvars[ncvarid].name);
			    ncvars[ncvarid].isvar = -1;
			    continue;
			  }
		      }
		    }

		  if ( ncvars[yvarid].xtype == NC_FLOAT ) grid.prec = DATATYPE_FLT32;
		  grid.yvals = (double *) malloc(size*sizeof(double));
		  cdf_get_var_double(fileID, yvarid, grid.yvals);
		  strcpy(grid.yname, ncvars[yvarid].name);
		  strcpy(grid.ylongname, ncvars[yvarid].longname);
		  strcpy(grid.yunits, ncvars[yvarid].units);
		  /* don't change the name !!! */
		  /*
		  if ( (len = strlen(grid.yname)) > 2 )
		    if ( grid.yname[len-2] == '_' && isdigit((int) grid.yname[len-1]) )
		      grid.yname[len-2] = 0;
		  */
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
		  if ( IS_EQUAL(yinc, 0) && (int) ysize > 2 ) /* check if gaussian */
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

	  if ( ncvars[ncvarid].gmapid >= 0 && ncvars[ncvarid].gridtype != GRID_CURVILINEAR )
	    {
	      cdf_inq_varnatts(fileID, ncvars[ncvarid].gmapid, &nvatts);

	      for ( iatt = 0; iatt < nvatts; iatt++ )
		{
		  cdf_inq_attname(fileID, ncvars[ncvarid].gmapid, iatt, attname);
		  cdf_inq_attlen(fileID, ncvars[ncvarid].gmapid, attname, &attlen);

		  if ( strcmp(attname, "grid_mapping_name") == 0 )
		    {
		      cdfGetAttText(fileID, ncvars[ncvarid].gmapid, attname, attstringlen-1, attstring);

		      if ( strcmp(attstring, "rotated_latitude_longitude") == 0 )
			grid.isRotated = TRUE;
		      else if ( strcmp(attstring, "sinusoidal") == 0 )
			grid.type = GRID_SINUSOIDAL;
		      else if ( strcmp(attstring, "lambert_azimuthal_equal_area") == 0 )
			grid.type = GRID_LAEA;
		      else if ( strcmp(attstring, "lambert_conformal_conic") == 0 )
			grid.type = GRID_LCC2;
		    }
		  else if ( strcmp(attname, "earth_radius") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 1, &datt);
		      grid.laea_a = datt;
		      grid.lcc2_a = datt;
		    }
		  else if ( strcmp(attname, "longitude_of_projection_origin") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 1, &grid.laea_lon_0);
		    }
		  else if ( strcmp(attname, "longitude_of_central_meridian") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 1, &grid.lcc2_lon_0);
		    }
		  else if ( strcmp(attname, "latitude_of_projection_origin") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 1, &datt);
		      grid.laea_lat_0 = datt;
		      grid.lcc2_lat_0 = datt;
		    }
		  else if ( strcmp(attname, "standard_parallel") == 0 )
		    {
		      if ( attlen == 1 )
			{
			  cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 1, &datt);
			  grid.lcc2_lat_1 = datt;
			  grid.lcc2_lat_2 = datt;
			}
		      else
			{
			  double datt2[2];
			  cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 2, datt2);
			  grid.lcc2_lat_1 = datt2[0];
			  grid.lcc2_lat_2 = datt2[1];
			}
		    }
		  else if ( strcmp(attname, "grid_north_pole_latitude") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 1, &grid.ypole);
		    }
		  else if ( strcmp(attname, "grid_north_pole_longitude") == 0 )
		    {
		      cdfGetAttDouble(fileID, ncvars[ncvarid].gmapid, attname, 1, &grid.xpole);
		    }
		}

	      grid.angle  = 0.0;
	    }

	  if ( CDI_Debug )
	    Message(func, "grid: type = %d, size = %d, nx = %d, ny %d",
		    grid.type, grid.size, grid.xsize, grid.ysize);

	  ncvars[ncvarid].gridID = varDefGrid(vlistID, grid, 1);

	  gridindex = vlistGridIndex(vlistID, ncvars[ncvarid].gridID);
	  streamptr->xdimID[gridindex] = xdimid;
	  streamptr->ydimID[gridindex] = ydimid;

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
		    int same_grid = TRUE;

		    if ( xvarid != -1 && ncvars[ncvarid2].xvarid != UNDEFID &&
			 xvarid != ncvars[ncvarid2].xvarid ) same_grid = FALSE;

		    if ( yvarid != -1 && ncvars[ncvarid2].yvarid != UNDEFID &&
			 yvarid != ncvars[ncvarid2].yvarid ) same_grid = FALSE;

		    if ( same_grid )
		      {
			if ( CDI_Debug )
			  Message(func, "gridID %d %d %s",
				  ncvars[ncvarid].gridID, ncvarid2, ncvars[ncvarid2].name);
			ncvars[ncvarid2].gridID = ncvars[ncvarid].gridID;
		      }
		  }
	      }
	}
    }

  /* find VCT */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].ndims == 1 )
	{
	  if ( memcmp(ncvars[ncvarid].name, "hyai", 4) == 0 )
	    {
	      vcta_id = ncvarid;
	      nvcth_id = ncvars[ncvarid].dimids[0];
	      continue;
	    }
	  if ( memcmp(ncvars[ncvarid].name, "hybi", 4) == 0 )
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

	  zprec = DATATYPE_FLT64;

	  if ( zvarid != UNDEFID )
	    {
	      pname     = ncvars[zvarid].name;
	      plongname = ncvars[zvarid].longname;
	      punits    = ncvars[zvarid].units;
	      if ( ncvars[zvarid].xtype == NC_FLOAT ) zprec = DATATYPE_FLT32;
	      /* don't change the name !!! */
	      /*
	      if ( (len = strlen(pname)) > 2 )
		if ( pname[len-2] == '_' && isdigit((int) pname[len-1]) )
		  pname[len-2] = 0;
	      */
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
						vctsize, vct, pname, plongname, punits, zprec, 1, 0);
	  free(zvar);

	  zaxisindex = vlistZaxisIndex(vlistID, ncvars[ncvarid].zaxisID);
	  streamptr->zaxisID[zaxisindex]  = zdimid;	  

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
    Message(func, "time varid = %d", streamptr->basetime.ncvarid);

  if ( CDI_Debug )
    Message(func, "ntsteps = %d", streamptr->ntsteps);

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

#if  defined  (NC_NETCDF4)
      if ( ncvars[ncvarid].deflate )
	vlistDefVarZtype(vlistID, varID, COMPRESS_ZIP);
#endif

      streamptr->vars[varID1].level   = NULL;
      streamptr->vars[varID1].defmiss = 0;
      streamptr->vars[varID1].nlevs   = zaxisInqSize(ncvars[ncvarid].zaxisID);
      streamptr->vars[varID1].ncvarid = ncvarid;

      vlistDefVarName(vlistID, varID, ncvars[ncvarid].name);
      if ( ncvars[ncvarid].code != UNDEFID )  vlistDefVarCode(vlistID, varID, ncvars[ncvarid].code);
      if ( ncvars[ncvarid].longname[0] )      vlistDefVarLongname(vlistID, varID, ncvars[ncvarid].longname);
      if ( ncvars[ncvarid].stdname[0] )       vlistDefVarStdname(vlistID, varID, ncvars[ncvarid].stdname);
      if ( ncvars[ncvarid].units[0] )         vlistDefVarUnits(vlistID, varID, ncvars[ncvarid].units);
      if ( IS_NOT_EQUAL(ncvars[ncvarid].addoffset, 0) )
	vlistDefVarAddoffset(vlistID, varID, ncvars[ncvarid].addoffset);
      if ( IS_NOT_EQUAL(ncvars[ncvarid].scalefactor, 1) )
	vlistDefVarScalefactor(vlistID, varID, ncvars[ncvarid].scalefactor);

      vlistDefVarDatatype(vlistID, varID, cdfInqDatatype(ncvars[ncvarid].xtype));

      vlistDefVarInstitut(vlistID, varID, instID);
      vlistDefVarModel(vlistID, varID, modelID);
      vlistDefVarTable(vlistID, varID, ncvars[ncvarid].tableID);

      if ( ncvars[ncvarid].defmiss == TRUE ) vlistDefVarMissval(vlistID, varID, ncvars[ncvarid].missval);

      if ( CDI_Debug )
	Message(func, "varID = %d  gridID = %d  zaxisID = %d", varID,
		vlistInqVarGrid(vlistID, varID), vlistInqVarZaxis(vlistID, varID));
    }

  for ( varID = 0; varID < nvars; varID++ )
    {
      ncvarid = varids[varID];

      if ( ncvars[ncvarid].natts )
	{
	  int attnum;
	  nc_type attrtype;

	  nvatts = ncvars[ncvarid].natts;
	  for ( iatt = 0; iatt < nvatts; iatt++ )
	    {
	      attnum = ncvars[ncvarid].atts[iatt];
	      cdf_inq_attname(fileID, ncvarid, attnum, attname);
	      cdf_inq_attlen(fileID, ncvarid, attname, &attlen);
	      cdf_inq_atttype(fileID, ncvarid, attname, &attrtype);
	      if ( attrtype == NC_INT || attrtype == NC_SHORT )
		{
		  int *attint;
		  attint = (int *) malloc(attlen*sizeof(int));
		  cdfGetAttInt(fileID, ncvarid, attname, attlen, attint);
		  vlistDefAttInt(vlistID, varID, attname, (int)attlen, attint);
		  if ( CDI_Debug )
		    printf("int: %s.%s = %d\n", ncvars[ncvarid].name, attname, attint[0]);
		  free(attint);
		}
	      else if ( attrtype == NC_FLOAT || attrtype == NC_DOUBLE )
		{
		  double *attflt;
		  attflt = (double *) malloc(attlen*sizeof(double));
		  cdfGetAttDouble(fileID, ncvarid, attname, attlen, attflt);
		  vlistDefAttFlt(vlistID, varID, attname, (int)attlen, attflt);
		  if ( CDI_Debug )
		    printf("flt: %s.%s = %g\n", ncvars[ncvarid].name, attname, attflt[0]);
		  free(attflt);
		}
	      else if ( attrtype == NC_CHAR )
		{
		  cdfGetAttText(fileID, ncvarid, attname, attstringlen-1, attstring);
		  attstring[attlen] = 0;
		  vlistDefAttTxt(vlistID, varID, attname, (int)attlen, attstring);
		  if ( CDI_Debug )
		    printf("txt: %s.%s = %s\n", ncvars[ncvarid].name, attname, attstring);
		}
	      else
		{
		  if ( CDI_Debug )
		    printf("att: %s.%s = unknown\n", ncvars[ncvarid].name, attname);
		}
	    }

	  free(ncvars[ncvarid].atts);
	}
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
	    if ( memcmp("var", pname, 3) == 0 )
	      {
		vlistDefVarCode(vlistID, varID, atoi(pname+3));
		vlistDestroyVarName(vlistID, varID);
	      }
	}
      if ( vlistInqVarCode(vlistID, varID) == -varID-1 )
	{
	  char *pname;
	  pname = vlistInqVarNamePtr(vlistID, varID);
	  len = strlen(pname);
	  if ( len > 4 && isdigit((int) pname[4]) )
	    if ( memcmp("code", pname, 4) == 0 )
	      {
		vlistDefVarCode(vlistID, varID, atoi(pname+4));
		vlistDestroyVarName(vlistID, varID);
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
	      vlistDestroyVarName(vlistID, varID);
	      vlistDestroyVarLongname(vlistID, varID);
	      vlistDestroyVarUnits(vlistID, varID);

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
  ncvarid = streamptr->basetime.ncvarid;

  if ( timehasunit )
    {
      TAXIS *taxis;
      taxis = &streamptr->tsteps[0].taxis;

      cdfGetAttText(fileID, ncvarid, "units", attstringlen-1, attstring);
      if ( splitBasetime(attstring, taxis) == 1 )
	streamptr->basetime.ncvarid = UNDEFID;
    }

  if ( time_has_bounds )
    streamptr->tsteps[0].taxis.has_bounds = TRUE;

  if ( ncvarid != -1 )
    if ( ncvars[ncvarid].calendar == TRUE )
      {
	cdfGetAttText(fileID, ncvarid, "calendar", attstringlen-1, attstring);

	if ( memcmp(attstring, "standard", 8)  == 0 ||
	     memcmp(attstring, "gregorian", 9) == 0 )
	  calendar = CALENDAR_STANDARD;
	else if ( memcmp(attstring, "none", 4) == 0 )
	  calendar = CALENDAR_NONE;
	else if ( memcmp(attstring, "proleptic", 9) == 0 )
	  calendar = CALENDAR_PROLEPTIC;
	else if ( memcmp(attstring, "360", 3) == 0 )
	  calendar = CALENDAR_360DAYS;
	else if ( memcmp(attstring, "365", 3) == 0 ||
		  memcmp(attstring, "noleap", 6)  == 0 )
	  calendar = CALENDAR_365DAYS;
	else if ( memcmp(attstring, "366", 3)  == 0 ||
		  memcmp(attstring, "all_leap", 8) == 0 )
	  calendar = CALENDAR_366DAYS;
	else
	  Warning(func, "calendar >%s< unsupported!", attstring);
      }

  if ( streamptr->tsteps[0].taxis.type == TAXIS_RELATIVE )
    taxisID = taxisCreate(TAXIS_RELATIVE);
  else
    taxisID = taxisCreate(TAXIS_ABSOLUTE);

  if ( calendar != UNDEFID )
    {
      TAXIS *taxis;
      taxis = &streamptr->tsteps[0].taxis;

      taxis->calendar = calendar;
      taxisDefCalendar(taxisID, calendar);
    }
  else if ( streamptr->tsteps[0].taxis.type == TAXIS_RELATIVE )
    {
      TAXIS *taxis;

      calendar = CALENDAR_STANDARD;

      taxis = &streamptr->tsteps[0].taxis;

      taxis->calendar = calendar;
      taxisDefCalendar(taxisID, calendar);
    }

  vlistDefTaxis(vlistID, taxisID);

  streamptr->curTsID = 0;
  streamptr->rtsteps = 1;

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d  tsID = %d", streamID, tsID);

  stream_check_ptr(func, streamptr);
    
  if ( tsID < 0 ) Error(func, "unexpected tsID = %d", tsID);

  if ( tsID < streamptr->ntsteps && streamptr->ntsteps > 0 )
    {
      cdfCreateRecords(streamID, tsID);

      taxis = &streamptr->tsteps[tsID].taxis;
      if ( tsID > 0 )
	ptaxisCopy(taxis, &streamptr->tsteps[0].taxis);

      timevalue = tsID;

      nctimevarid = streamptr->basetime.ncvarid;
      if ( nctimevarid != UNDEFID )
	{
	  fileID = streamInqFileID(streamID);
	  index  = tsID;
	  cdf_get_var1_double(fileID, nctimevarid, &index, &timevalue);

	  decode_timeval(timevalue, taxis, &taxis->vdate, &taxis->vtime);

	  nctimeboundsid = streamptr->basetime.ncvarboundsid;
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

  streamptr->curTsID = tsID;
  nrecs = streamptr->tsteps[tsID].nrecs;

#endif
  return ((int) nrecs);
}


void cdfEndDef(int streamID)
{
  int varID, ncvarid;
  int nvars;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  cdfDefGlobalAtts(streamID);
  cdfDefLocalAtts(streamID);
  if ( streamptr->accessmode == 0 )
    {
      nvars =  streamptr->nvars;

      for ( varID = 0; varID < nvars; varID++ )
	{
	  ncvarid = cdfDefVar(streamID, varID);
	}
      streamptr->accessmode = 1;
    }
}


void cdfDefInstitut(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID, instID;
  char *longname;
  size_t len;
  int vlistID;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  instID  = vlistInqInstitut(vlistID);

  if ( instID != UNDEFID )
    {
      longname = institutInqLongnamePtr(instID);
      if ( longname )
	{
	  len = strlen(longname);
	  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
	  cdf_put_att_text(fileID, NC_GLOBAL, "institution", len, longname);
	  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  modelID = vlistInqModel(vlistID);

  if ( modelID != UNDEFID )
    {
      longname = modelInqNamePtr(modelID);
      if ( longname )
	{
	  len = strlen(longname);
	  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
	  cdf_put_att_text(fileID, NC_GLOBAL, "source", len, longname);
	  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
	}
    }
#endif
}


void cdfDefGlobalAtts(int streamID)
{
#if  defined  (HAVE_LIBNETCDF)
  int fileID, vlistID;
  VLIST *vlistptr;
  STREAM *streamptr;
  int natts;

  streamptr = stream_to_pointer(streamID);

  if ( streamptr->globalatts ) return;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  vlistptr = vlist_to_pointer(vlistID);

  cdfDefSource(streamID);
  cdfDefInstitut(streamID);

  vlistInqNatts(vlistID, CDI_GLOBAL, &natts);

  if ( natts > 0 && streamptr->ncmode == 2 ) cdf_redef(fileID);

  defineAttributes(vlistID, CDI_GLOBAL, fileID, NC_GLOBAL);

  if ( natts > 0 && streamptr->ncmode == 2 ) cdf_enddef(fileID);

  streamptr->globalatts = 1;
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  if ( streamptr->localatts ) return;
  if ( vlistInqInstitut(vlistID) != UNDEFID ) return;

  streamptr->localatts = 1;

  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  for ( varID = 0; varID < streamptr->nvars; varID++ )
    {
      instID = vlistInqVarInstitut(vlistID, varID);
      if ( instID != UNDEFID )
	{
          ncvarid = streamptr->vars[varID].ncvarid;
  	  name = institutInqNamePtr(instID);
	  if ( name )
	    {
	      len = strlen(name);
	      cdf_put_att_text(fileID, ncvarid, "institution", len, name);
	    }
	}
      }
  
  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
#endif
}

void cdfDefHistory(int streamID, int size, char *history)
{
#if  defined  (HAVE_LIBNETCDF)
  int ncid;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  ncid = streamptr->fileID;
  cdf_put_att_text(ncid, NC_GLOBAL, "history", (size_t) size, history);
#endif
}

int cdfInqHistorySize(int streamID)
{
  size_t size = 0;
#if  defined  (HAVE_LIBNETCDF)
  int ncid;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  ncid = streamptr->fileID;
  if ( streamptr->historyID != UNDEFID )
    cdf_inq_attlen(ncid, NC_GLOBAL, "history", &size);

#endif
  return ((int) size);
}


void cdfInqHistoryString(int streamID, char *history)
{
#if  defined  (HAVE_LIBNETCDF)
  int ncid;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  ncid = streamptr->fileID;
  if ( streamptr->historyID != UNDEFID )
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

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
      if ( streamptr->zaxisID[index] == UNDEFID ) cdfDefZaxis(streamID, zaxisID);
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
