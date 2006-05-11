#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>

#include "dmemory.h"

#include "basetime.h"
#include "cdi.h"
#include "stream_int.h"
#include "stream_hdf.h"
#include "hdf_int.h"
#include "varscan.h"
#include "vlist.h"


#if  defined  (HAVE_LIBHDF5)
#  include "hdf5.h"
#endif

#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID

#define NC_BYTE     1
#define NC_CHAR     1
#define NC_SHORT    2
#define NC_FLOAT    4


void hdfDefTime(int streamID);
int  hdfDefVar(int streamID, int varid);
void hdfDefGlobalAtts(int streamID);
void hdfDefLocalAtts(int streamID);


#define  DIMTYPE_GX  1
#define  DIMTYPE_GY  2
#define  DIMTYPE_Z   3
#define  DIMTYPE_T   4

typedef struct {
  int     ncvarid;
  int     dimtype;
  size_t  len;
  char    name[128];
}
HDFDIM;

typedef struct {
  int     objID;
  int     isvar;
  int     islon;
  int     islat;
  int     islev;
  int     warn;
  int     timeID;
  int     code;
  int     bounds;
  int     gridID;
  int     zaxisID;
  int     gridtype;
  int     zaxistype;
  int     xdim;
  int     ydim;
  int     zdim;
  int     xvarid;
  int     yvarid;
  int     cellarea;
  int     tableID;
  int     truncation;
  int     defmiss;
  int     xtype;
  int     ndims;
  int     dimids[8];
  int     dimtype[8];
  size_t  vlen;
  double  levelscale;
  double *vdata;
  double  missval;
  double  addoffset;
  double  scalefactor;
  char    name[128];
  char    longname[128];
  char    units[128];
}
HDFVAR;

#if defined (XYZ)
static void splitTimevalue(double timevalue, int timeunit, int *date, int *time)
{
  static char func[] = "splitTimevalue";
  int vdate, vtime;
  int hour, minute;

  if ( timeunit == TUNIT_HOUR )
    {
      timevalue /= 24;
      vdate = (int) timevalue;
      vtime = (int) ((timevalue - vdate)*1440 + 0.01);

      hour   = vtime / 60;
      minute = vtime - hour*60;
      vtime  = hour*100 + minute;
    }
  else if ( timeunit == TUNIT_DAY )
    {
      vdate = (int) timevalue;
      vtime = (int) ((timevalue - vdate)*1440 + 0.01);

      hour   = vtime / 60;
      minute = vtime - hour*60;
      vtime  = hour*100 + minute;
    }
  else if ( timeunit == TUNIT_MONTH )
    {
      vdate = (int) timevalue*100;
      vtime = 0;
    }
  else
    {
      Message(func, "Unsupported TIMEUNIT %s!", tunitNamePtr(timeunit));
      return;
    }

  *date = vdate;
  *time = vtime;
}

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

  while ( ! isspace(*ptu) ) ptu++;
  while (   isspace(*ptu) ) ptu++;

  if ( strncmp(ptu, "as", 2) == 0 )
    timetype = TAXIS_ABSOLUTE;
  else if ( strncmp(ptu, "since", 5) == 0 )
    timetype = TAXIS_RELATIVE;

  while ( ! isspace(*ptu) ) ptu++;
  while (   isspace(*ptu) ) ptu++;

  if ( timetype == TAXIS_ABSOLUTE )
    {
      if ( strncmp(ptu, "%Y%m%d.%f", 9) == 0 && timeunit == TUNIT_DAY )
	{
	  (*taxis).type = timetype;
	  (*taxis).unit = timeunit;
	}
      else if ( strncmp(ptu, "%Y%m.%f", 9) == 0 && timeunit == TUNIT_MONTH )
	{ 
	  (*taxis).type = timetype;
	  (*taxis).unit = timeunit;
	}
      else
	{
	  (*taxis).type = timetype;
	  (*taxis).unit = timeunit;
	  /*
	  Message(func, "Unsupported TIMEUNIT %s!", ptu);
	  return (1);
	  */
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

      (*taxis).type = timetype;

      rdate = year*10000 + month*100 + day;
      rtime = hour*100 + minute;
      (*taxis).rdate = rdate;
      (*taxis).rtime = rtime;
      (*taxis).unit  = timeunit;
    }

  if ( CDI_Debug )
    Message(func, "timetype = %d  unit = %d", timetype, timeunit);

  return (0);
}
#endif /*XYZ*/

void hdfDefDimgroup(int streamID)
{
#if  defined  (HAVE_LIBHDF5)
  int fileID;
  int hgid;

  fileID = streamInqFileID(streamID);

  hgid = H5Gcreate(fileID, "coordinates", 0);

  streamDefDimgroupID(streamID, hgid);
#endif
}

#if defined (XYZ)

#if  defined  (HAVE_LIBHDF5)
int hdfInqDatatype(int xtype)
{
  int datatype;

  if      ( xtype == H5T_NATIVE_CHAR  )  datatype = DATATYPE_INT1;
  else if ( xtype == H5T_NATIVE_SHORT )  datatype = DATATYPE_INT2;
  else if ( xtype == H5T_NATIVE_INT   )  datatype = DATATYPE_INT4;
  else if ( xtype == H5T_NATIVE_FLOAT )  datatype = DATATYPE_REAL4;
  else                                   datatype = DATATYPE_REAL8;

  return (datatype);
}
#endif
#endif /*XYZ*/

#if  defined  (HAVE_LIBHDF5)
int hdfDefDatatype(int datatype)
{
  int xtype;

  if      ( datatype == DATATYPE_INT1  )  xtype = H5T_NATIVE_CHAR;
  else if ( datatype == DATATYPE_INT2  )  xtype = H5T_NATIVE_SHORT;
  else if ( datatype == DATATYPE_INT4  )  xtype = H5T_NATIVE_INT;
  else if ( datatype == DATATYPE_REAL8 )  xtype = H5T_NATIVE_DOUBLE;
  else                                    xtype = H5T_NATIVE_FLOAT;

  return (xtype);
}
#endif

#if defined (XYZ)

int hdfCopyRecord(int ostreamID, int istreamID)
{
  static char func[] = "hdfCopyRecord";
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

  data = (double *) malloc(datasize*sizeof(double));

  streamReadRecord(istreamID, data, &nmiss);
  streamWriteRecord(ostreamID, data, nmiss);

  free(data);

  return (ierr);
}

int hdfInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "hdfInqRecord";
  int tsID, recID;

  streamCheckID(func, streamID);

  tsID = 
  recID = streams[streamID].tsteps[0].curRecID++;

  if ( streams[streamID].tsteps[0].curRecID >= 
       streams[streamID].tsteps[0].nrecords )
    {
      streams[streamID].tsteps[0].curRecID = 0;
      /*     return (0);*/
    }

  *varID   = streams[streamID].tsteps[0].records[recID].varID;
  *levelID = streams[streamID].tsteps[0].records[recID].levelID;

  streams[streamID].record->varID = *varID;
  streams[streamID].record->levelID = *levelID;

  if ( CDI_Debug )
    Message(func, "recID = %d  varID = %d  levelID = %d",
	    recID, *varID, *levelID);
  
  return (recID+1);
}
#endif /*XYZ*/

int hdfDefRecord(int streamID)
{
  static char func[] = "hdfDefRecord";
  int ierr = 0;

  if ( CDI_Debug )
    Message(func, "streamID = %d", streamID);

  streamCheckID(func, streamID);

  return (ierr);
}

#if defined (XYZ)
void hdfWriteGridTraj(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  int tsID, fileID;
  int lonID, latID, gridindex;
  size_t index;
  double xlon, xlat;

  fileID = streamInqFileID(streamID);

  gridindex = vlistGridIndex(vlistID, gridID);
  lonID = streams[streamID].xdimID[gridindex];
  latID = streams[streamID].ydimID[gridindex];

  xlon = gridInqXval(gridID, 0);
  xlat = gridInqYval(gridID, 0);
  tsID = streams[streamID].curTsID;
  index = tsID;

  hdf_put_var1_double(fileID, lonID, &index, &xlon);
  hdf_put_var1_double(fileID, latID, &index, &xlat);
#endif
}

void hdfReadGridTraj(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  int tsID, fileID;
  int lonID, latID, gridindex;
  size_t index;
  double xlon, xlat;

  fileID = streamInqFileID(streamID);

  gridindex = vlistGridIndex(vlistID, gridID);
  lonID = streams[streamID].xdimID[gridindex];
  latID = streams[streamID].ydimID[gridindex];

  tsID = streams[streamID].curTsID;
  index = tsID;

  hdf_get_var1_double(fileID, lonID, &index, &xlon);
  hdf_get_var1_double(fileID, latID, &index, &xlat);

  gridDefXvals(gridID, &xlon);
  gridDefYvals(gridID, &xlat);
#endif
}

void hdfDefMissval(int streamID, int varID, int dtype)
{
#if  defined  (HAVE_LIBHDF5)
  if ( streams[streamID].vars[varID].defmiss == FALSE )
    {
      int fileID;
      int ncvarid;
      double missval;

      fileID  = streamInqFileID(streamID);
      ncvarid = streams[streamID].vars[varID].ncvarid;
      missval = vlistInqVarMissval(vlistID, varID);
      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      if ( dtype == DATATYPE_REAL8 )
	hdf_put_att_double(fileID, ncvarid, "_FillValue", H5T_NATIVE_DOUBLE, 1L, &missval);
      else
	hdf_put_att_double(fileID, ncvarid, "_FillValue", NC_FLOAT, 1L, &missval);
      if ( streams[streamID].ncmode == 2 ) hdf_enddef(fileID);

      streams[streamID].vars[varID].defmiss = TRUE;
    }
#endif
}
#endif /*XYZ*/

void hdfWriteRecord(int streamID, double *data, int nmiss)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfWriteRecord";
  int fileID;
  int gridID;
  int zaxisID;
  int zid = UNDEFID;
  int ncvarid;
  int ntsteps;
  size_t size;
  size_t start[4];
  size_t count[4];
  int ndims = 0;
  int idim;
  int varID;
  int timeID;
  int gridindex, zaxisindex;
  int i;
  int dtype;
  int levelID;
  hsize_t hsize[9];
  int status;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  varID   = streams[streamID].record->varID;
  levelID = streams[streamID].record->levelID;
  
  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  fileID  = streamInqFileID(streamID);

  ntsteps = streams[streamID].ntsteps;
  if ( CDI_Debug )
    Message(func, "ntsteps = %d", ntsteps); 

  if ( vlistHasTime(vlistID) ) hdfDefTime(streamID);

  ncvarid = hdfDefVar(streamID, varID);

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  /*
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      hdfWriteGridTraj(streamID, gridID);
    }
  else
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }
  */

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];

  if ( timeID == TIME_VARIABLE )
    {
      start[ndims] = ntsteps - 1;
      count[ndims] = 1;
      hsize[ndims] = ntsteps;
      ndims++;
    }
  /*
  if ( zid != UNDEFID )
    {
      start[ndims] = levelID;
      count[ndims] = 1;
      ndims++;
    }
  if ( yid != UNDEFID )
    {
      start[ndims] = 0;
      hdf_inq_dimlen(fileID, yid, &size);
      count[ndims] = size;
      ndims++;
    }
  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      hdf_inq_dimlen(fileID, xid, &size);
      count[ndims] = size;
      ndims++;
    }
  */
  if ( zaxisID != UNDEFID )
    {
      size_t size = zaxisInqSize(zaxisID);
      if ( ! (zaxisInqType(zaxisID) == ZAXIS_SURFACE && size == 1) )
	{
	  start[ndims] = levelID;
	  count[ndims] = 1;
	  hsize[ndims] = size;
	  ndims++;
	}
    }
  if ( gridID != UNDEFID )
    {
      int xsize, ysize;
      if ( gridInqType(gridID) == GRID_SPECTRAL )
	{
	  xsize = 2;
	  ysize = gridInqSize(gridID)/2;
	}
      else
	{
	  xsize = gridInqXsize(gridID);
	  ysize = gridInqYsize(gridID);
	}
      start[ndims] = 0;
      count[ndims] = ysize;
      hsize[ndims] = ysize;
      ndims++;
      start[ndims] = 0;
      count[ndims] = xsize;
      hsize[ndims] = xsize;
      ndims++;
    }


  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  dtype = vlistInqVarDatatype(vlistID, varID);

  /*
  if ( nmiss > 0 ) hdfDefMissval(streamID, varID, dtype);
  */

  if ( dtype == DATATYPE_INT1 || dtype == DATATYPE_INT2 || dtype == DATATYPE_INT4 )
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;

      addoffset   = vlistInqVarAddoffset(vlistID, varID);
      scalefactor = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = (fabs(addoffset   - 0.0) > 0.0);
      lscalefactor = (fabs(scalefactor - 1.0) > 0.0);

      if ( laddoffset || lscalefactor )
	{
	  gridID   = vlistInqVarGrid(vlistID, varID);
	  size     = gridInqSize(gridID);

	  for ( i = 0; i < (int) size; i++ )
	    {
	      if ( laddoffset )   data[i] = data[i] - addoffset;
	      if ( lscalefactor ) data[i] = data[i] / scalefactor;
	    }
	}
    }

  if ( levelID == 0 )
    status = H5Dextend(ncvarid, hsize);

  hdf_put_vara_double(fileID, ncvarid, start, count, data);
#endif
}
#if defined (XYZ)

int hdfReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "hdfReadRecord";
  int ierr = 0;
  int levelID, varID, tsID, recID, gridID;
  int size, i;
  int laddoffset, lscalefactor;
  double addoffset, scalefactor;

  if ( CDI_Debug ) Message(func, "streamID = %d", streamID);

  tsID    = streams[streamID].curTsID;
  recID   = streams[streamID].tsteps[tsID].curRecID;
  varID   = streams[streamID].tsteps[tsID].records[recID].varID;
  levelID = streams[streamID].tsteps[tsID].records[recID].levelID;

  hdfReadVarSliceDP(streamID, varID, levelID, data, nmiss);

  addoffset   = vlistInqVarAddoffset(vlistID, varID);
  scalefactor = vlistInqVarScalefactor(vlistID, varID);
  laddoffset   = (fabs(addoffset) > 0.0);
  lscalefactor = (fabs(scalefactor - 1.0) > 0.0);

  if ( laddoffset || lscalefactor )
    {
      gridID   = vlistInqVarGrid(vlistID, varID);
      size     = gridInqSize(gridID);
      
      for ( i = 0; i < size; i++ )
	{
	  if ( lscalefactor ) data[i] = data[i] * scalefactor;
	  if ( laddoffset )   data[i] = data[i] + addoffset;
	}
    }

  return (ierr);
}
#endif /*XYZ*/

void hdfDefTimeValue(int streamID, int date, int time)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfDefTimeValue";
  int fileID, tsID;
  int hour, minute;
  double timevalue;
  int timevarid;
  size_t index;
  hsize_t hsize[1];
  int status;

  fileID = streamInqFileID(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d, fileID = %d", streamID, fileID);

  tsID = streams[streamID].curTsID;

  hour   = time / 100;
  minute = time - hour*100;

  if ( streams[streamID].tsteps[0].taxis.type == TAXIS_ABSOLUTE )
    {
      if ( streams[streamID].tsteps[0].taxis.unit == TUNIT_MONTH )
	{
	  timevalue = date/100 + 0.5;
	}
      else
	timevalue = date + (hour*60 + minute)/1440.;
    }
  else
    timevalue = vtime2timeval(date, time, &streams[streamID].tsteps[0].taxis);

  if ( CDI_Debug )
    Message(func, "tsID = %d  timevalue = %f", tsID, timevalue);

  timevarid = streams[streamID].basetime.ncvarid;
  /*
  if ( streams[streamID].ncmode == 1 )
    {
      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }
  */
  index = tsID;

  hsize[0] = tsID+1;
  status = H5Dextend(timevarid, hsize);

  hdf_put_var1_double(fileID, timevarid, &index, &timevalue);
  /*
printf("fileID = %d %d %d %f\n", fileID, timevarid, index, timevalue);
  */
#endif
}

void hdfDefTimestep(int streamID, int tsID)
{
  int vdate, vtime;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  vdate = streams[streamID].tsteps[tsID].taxis.vdate;
  vtime = streams[streamID].tsteps[tsID].taxis.vtime;

  if ( vlistHasTime(vlistID) ) hdfDefTime(streamID);

  hdfDefTimeValue(streamID, vdate, vtime);
}

void hdfDefTime(int streamID)
{
#if  defined  (HAVE_LIBHDF5)
  int fileID;
  int varid;
  int dims[1];
  int year, month, day, hour, minute;
  char unitstring[80];
  int plistid;
  hsize_t cdims[1];

  if ( streams[streamID].basetime.ncvarid != UNDEFID ) return;

  /*  fileID = streamInqFileID(streamID); */
  fileID = streamInqDimgroupID(streamID);

  if ( streams[streamID].ncmode == 0 ) streams[streamID].ncmode = 1;

  /*
  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
  hdf_def_dim(fileID, "time", -1, &streams[streamID].basetime.ncdimid);
  dims[0] = streams[streamID].basetime.ncdimid;
  */
  dims[0] = -1;
  cdims[0] = 1;
  plistid = H5Pcreate(H5P_DATASET_CREATE);
  H5Pset_chunk(plistid, 1, cdims);
  hdf_def_var(fileID, "time", H5T_NATIVE_DOUBLE, 1, dims, &varid, plistid);
  H5Pclose(plistid);

  streams[streamID].basetime.ncvarid = varid;

  if ( streams[streamID].tsteps[0].taxis.type == TAXIS_ABSOLUTE )
    {
      if ( streams[streamID].tsteps[0].taxis.unit == TUNIT_MONTH )
	sprintf(unitstring, "month as %s", "%Y%m.%f");
      else
	sprintf(unitstring, "day as %s", "%Y%m%d.%f");
    }
  else
    {
      int rdate, rtime;
      int timeunit;
      TAXIS *taxis;
      taxis = &streams[streamID].tsteps[0].taxis;
      timeunit = (*taxis).unit;
      rdate  = (*taxis).rdate;
      rtime  = (*taxis).rtime;
      if ( rdate == -1 )
	{
	  rdate  = (*taxis).vdate;
	  rtime  = (*taxis).vtime;
	}
      year   =  rdate / 10000;
      month  = (rdate - year*10000) / 100;
      day    =  rdate - year*10000 - month*100;
      hour   =  rtime / 100;
      minute =  rtime - hour*100;
      sprintf(unitstring, "%s since %d-%d-%d %02d:%02d",
	      tunitNamePtr(timeunit), year, month, day, hour, minute);
    }

  hdf_put_att_text(fileID, varid, "units", strlen(unitstring), unitstring);
  /*
  if ( streams[streamID].ncmode == 2 ) hdf_enddef(fileID);
  */
#endif
}

#if defined (XYZ)

void hdfDefComplex(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char axisname[] = "complex";
  int index;
  int dimID = UNDEFID;
  int gridID0, gridtype0, gridindex;
  int ngrids;
  int fileID;
  int dimlen;

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

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_dim(fileID, axisname, dimlen, &dimID);

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = dimID;

#endif
}

void hdfDefSpc(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
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

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_dim(fileID, axisname, dimlen, &dimID);

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].ydimID[gridindex] = dimID;

#endif
}

void hdfDefTrajLon(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfDefTrajLon";
  char unitstring[] = "degrees_east";
  char longname[] = "longitude";
  char stdname[] = "longitude";
  char axisname[] = "tlon";
  int gridtype, gridindex;
  int dimID = UNDEFID;
  int fileID;
  int dimlen;
  int ncvarid;

  fileID = streamInqFileID(streamID);

  gridtype = gridInqType(gridID);
  dimlen = gridInqXsize(gridID);
  if ( dimlen != 1 ) Error(func, "Xsize isn't 1 for %s grid!", gridNamePtr(gridtype));

  gridindex = vlistGridIndex(vlistID, gridID);
  ncvarid = streams[streamID].xdimID[gridindex];

  if ( ncvarid == UNDEFID )
    {
      dimID = streams[streamID].basetime.ncvarid;

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_var(fileID, axisname, H5T_NATIVE_DOUBLE, 1, &dimID, &ncvarid, H5P_DEFAULT);
      hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
      hdf_put_att_text(fileID, ncvarid, "units", strlen(unitstring), unitstring);
      hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  streams[streamID].xdimID[gridindex] = ncvarid; /* var ID for trajectory !!! */

#endif
}

void hdfDefTrajLat(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfDefTrajLat";
  char unitstring[] = "degrees_north";
  char longname[] = "latitude";
  char stdname[] = "latitude";
  char axisname[] = "tlat";
  int gridtype, gridindex;
  int dimID = UNDEFID;
  int fileID;
  int dimlen;
  int ncvarid;

  fileID = streamInqFileID(streamID);

  gridtype = gridInqType(gridID);
  dimlen = gridInqYsize(gridID);
  if ( dimlen != 1 ) Error(func, "Ysize isn't 1 for %s grid!", gridNamePtr(gridtype));

  gridindex = vlistGridIndex(vlistID, gridID);
  ncvarid = streams[streamID].ydimID[gridindex];

  if ( ncvarid == UNDEFID )
    {
      dimID = streams[streamID].basetime.ncvarid;

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_var(fileID, axisname, H5T_NATIVE_DOUBLE, 1, &dimID, &ncvarid, H5P_DEFAULT);
      hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
      hdf_put_att_text(fileID, ncvarid, "units", strlen(unitstring), unitstring);
      hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  streams[streamID].ydimID[gridindex] = ncvarid; /* var ID for trajectory !!! */

#endif
}
#endif /*XYZ*/

void hdfDefLon(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char unitstring[] = "degrees_east";
  char longname[] = "longitude";
  char stdname[] = "longitude";
  char axisname[5] = "lonX";
  int index, index2, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int ncvarid;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqDimgroupID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqXsize(gridID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].xdimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN ||
	       gridtype0 == GRID_LONLAT   ||
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
      if ( iz == 0 ) axisname[3] = '\0';
      else           sprintf(&axisname[3], "%1d", iz+1);
      /*
      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
      */
      /*
      hdf_def_dim(fileID, axisname, dimlen, &dimID);
      */
      if ( gridInqXvalsPtr(gridID) )
	{
	  hdf_def_var(fileID, axisname, H5T_NATIVE_DOUBLE, 1, &dimlen, &ncvarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
	  hdf_put_att_text(fileID, ncvarid, "units", strlen(unitstring), unitstring);
	  hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);
	}
      /*
      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
      */
      if ( gridInqXvalsPtr(gridID) )
	hdf_put_var_double(fileID, ncvarid, gridInqXvalsPtr(gridID));
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = ncvarid;

#endif
}

void hdfDefLat(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char unitstring[] = "degrees_north";
  char longname[] = "latitude";
  char stdname[] = "latitude";
  char axisname[5] = "latX";
  int index, index2, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int ncvarid;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqDimgroupID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqYsize(gridID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].ydimID[index] != UNDEFID )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN ||
	       gridtype0 == GRID_LONLAT   ||
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
      if ( iz == 0 ) axisname[3] = '\0';
      else           sprintf(&axisname[3], "%1d", iz+1);
      /*
      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
      */
      /*
      hdf_def_dim(fileID, axisname, dimlen, &dimID);
      */
      if ( gridInqYvalsPtr(gridID) )
	{
	  hdf_def_var(fileID, axisname, H5T_NATIVE_DOUBLE, 1, &dimlen, &ncvarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
	  hdf_put_att_text(fileID, ncvarid, "units", strlen(unitstring), unitstring);
	  hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);
	}
      /*
      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
      */
      if ( gridInqYvalsPtr(gridID) )
	hdf_put_var_double(fileID, ncvarid, gridInqYvalsPtr(gridID));
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].ydimID[gridindex] = ncvarid;

#endif
}

#if defined (XYZ)

void hdfDefLonRot(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char unitstring[] = "degrees";
  char longname[] = "longitude in rotated pole grid";
  char stdname[] = "grid_longitude";
  char axisname[6] = "rlonX";
  int index, index2, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int ncvarid;
  double north_pole;

  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqXsize(gridID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].xdimID[index] != UNDEFID && gridIsRotated(gridID) )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN ||
	       gridtype0 == GRID_LONLAT   ||
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
      if ( iz == 0 ) axisname[4] = '\0';
      else           sprintf(&axisname[4], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_dim(fileID, axisname, dimlen, &dimID);

      if ( gridInqXvalsPtr(gridID) )
	{
	  hdf_def_var(fileID, axisname, H5T_NATIVE_DOUBLE, 1, &dimID, &ncvarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
	  hdf_put_att_text(fileID, ncvarid, "units", strlen(unitstring), unitstring);
	  hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);
	  north_pole = gridInqXpole(gridID);
	  hdf_put_att_double(fileID, ncvarid, "north_pole", H5T_NATIVE_DOUBLE, 1L, &north_pole);
	}

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      if ( gridInqXvalsPtr(gridID) )
	hdf_put_var_double(fileID, ncvarid, gridInqXvalsPtr(gridID));
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = dimID;

#endif
}

void hdfDefLatRot(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char unitstring[] = "degrees";
  char longname[] = "latitude in rotated pole grid";
  char stdname[] = "grid_latitude";
  char axisname[6] = "rlatX";
  int index, index2, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int ncvarid;
  double north_pole;

  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqYsize(gridID);

  for ( index = 0; index < ngrids; index++ )
    {
      if ( streams[streamID].ydimID[index] != UNDEFID && gridIsRotated(gridID) )
	{
	  gridID0 = vlistGrid(vlistID, index);
	  gridtype0 = gridInqType(gridID0);
	  if ( gridtype0 == GRID_GAUSSIAN ||
	       gridtype0 == GRID_LONLAT   ||
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
      if ( iz == 0 ) axisname[4] = '\0';
      else           sprintf(&axisname[4], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_dim(fileID, axisname, dimlen, &dimID);

      if ( gridInqYvalsPtr(gridID) )
	{
	  hdf_def_var(fileID, axisname, H5T_NATIVE_DOUBLE, 1, &dimID, &ncvarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
	  hdf_put_att_text(fileID, ncvarid, "units", strlen(unitstring), unitstring);
	  hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);
	  north_pole = gridInqYpole(gridID);
	  hdf_put_att_double(fileID, ncvarid, "north_pole", H5T_NATIVE_DOUBLE, 1L, &north_pole);
	}

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      if ( gridInqYvalsPtr(gridID) )
	hdf_put_var_double(fileID, ncvarid, gridInqYvalsPtr(gridID));
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].ydimID[gridindex] = dimID;

#endif
}

void hdfDefRgrid(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char axisname[7] = "rgridX";
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;

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
      if ( iz == 0 ) axisname[5] = '\0';
      else           sprintf(&axisname[5], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_dim(fileID, axisname, dimlen, &dimID);

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = dimID;

#endif
}

void hdfDefGdim(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char axisname[7] = "xydimX";
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;

  fileID  = streamInqFileID(streamID);

  ngrids = vlistNgrids(vlistID);

  dimlen = gridInqSize(gridID);

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

  if ( dimID == UNDEFID )
    {
      if ( iz == 0 ) axisname[5] = '\0';
      else           sprintf(&axisname[5], "%1d", iz+1);

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_dim(fileID, axisname, dimlen, &dimID);

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  gridindex = vlistGridIndex(vlistID, gridID);
  streams[streamID].xdimID[gridindex] = dimID;

#endif
}

void hdfDefCell(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  char axisname[6] = "cellX";
  int index, iz = 0;
  int gridID0, gridtype0, gridindex;
  int dimID = UNDEFID;
  int ngrids;
  int fileID;
  int dimlen, dimlen0;
  int ncxvarid = UNDEFID, ncyvarid = UNDEFID, ncavarid = UNDEFID;
  int ncbxvarid, ncbyvarid;
  int nvertex, nvdimID = -1;

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

      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

      hdf_def_dim(fileID, axisname, dimlen, &dimID);

      nvertex = gridInqNvertex(gridID);
      if ( nvertex > 0 )
	hdf_def_dim(fileID, "nv", nvertex, &nvdimID);
	
      if ( gridInqXvalsPtr(gridID) )
	{
	  char xaxisname[] = "lon";
	  char unitstring[] = "degrees_east";
	  char longname[] = "longitude";

	  hdf_def_var(fileID, xaxisname, H5T_NATIVE_DOUBLE, 1, &dimID, &ncxvarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncxvarid, "long_name", strlen(longname), longname);
	  hdf_put_att_text(fileID, ncxvarid, "units", strlen(unitstring), unitstring);

	  streams[streamID].ncxvarID[gridindex] = ncxvarid;

	  if ( nvdimID != -1 && gridInqXboundsPtr(gridID) )
	    {
	      char bxaxisname[] = "lon_vertices";
	      int dimIDs[2];
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      hdf_def_var(fileID, bxaxisname, H5T_NATIVE_DOUBLE, 2, dimIDs, &ncbxvarid, H5P_DEFAULT);
	      hdf_put_att_text(fileID, ncxvarid, "bounds", strlen(bxaxisname), bxaxisname);
	    }
	}

      if ( gridInqYvalsPtr(gridID) )
	{
	  char yaxisname[] = "lat";
	  char unitstring[] = "degrees_north";
	  char longname[] = "latitude";

	  hdf_def_var(fileID, yaxisname, H5T_NATIVE_DOUBLE, 1, &dimID, &ncyvarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncyvarid, "long_name", strlen(longname), longname);
	  hdf_put_att_text(fileID, ncyvarid, "units", strlen(unitstring), unitstring);

	  streams[streamID].ncyvarID[gridindex] = ncyvarid;

	  if ( nvdimID != -1 && gridInqYboundsPtr(gridID) )
	    {
	      char byaxisname[] = "lat_vertices";
	      int dimIDs[2];
	      dimIDs[0] = dimID;
	      dimIDs[1] = nvdimID;
	      hdf_def_var(fileID, byaxisname, H5T_NATIVE_DOUBLE, 2, dimIDs, &ncbyvarid, H5P_DEFAULT);
	      hdf_put_att_text(fileID, ncyvarid, "bounds", strlen(byaxisname), byaxisname);
	    }
	}

      if ( gridInqAreaPtr(gridID) )
	{
	  char yaxisname[] = "cell_area";
	  char unitstring[] = "m2";
	  char longname[] = "area of grid cell";
	  char stdname[] = "area";

	  hdf_def_var(fileID, yaxisname, H5T_NATIVE_DOUBLE, 1, &dimID, &ncavarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncavarid, "long_name", strlen(longname), longname);
	  hdf_put_att_text(fileID, ncavarid, "standard_name", strlen(stdname), stdname);
	  hdf_put_att_text(fileID, ncavarid, "units", strlen(unitstring), unitstring);

	  streams[streamID].ncavarID[gridindex] = ncavarid;
	}

      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;

      if ( gridInqXvalsPtr(gridID) )
	hdf_put_var_double(fileID, ncxvarid, gridInqXvalsPtr(gridID));

      if ( gridInqYvalsPtr(gridID) )
	hdf_put_var_double(fileID, ncyvarid, gridInqYvalsPtr(gridID));

      if ( gridInqXboundsPtr(gridID) )
	hdf_put_var_double(fileID, ncbxvarid, gridInqXboundsPtr(gridID));

      if ( gridInqYboundsPtr(gridID) )
	hdf_put_var_double(fileID, ncbyvarid, gridInqYboundsPtr(gridID));

      if ( gridInqAreaPtr(gridID) )
	hdf_put_var_double(fileID, ncavarid, gridInqAreaPtr(gridID));
    }

  streams[streamID].xdimID[gridindex] = dimID;

#endif
}
#endif /*XYZ*/

void hdfDefVCT(int streamID, int zaxisID)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfDefVCT";
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
      int ncvarid, ncvarid2;
      int hyaiid, hybiid, hyamid, hybmid;
      int *ilevels = (int *) malloc(ilev*sizeof(int));
      const double *vctptr = zaxisInqVctPtr(zaxisID);
      double mval;
      char tmpname[128];

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

      fileID = streamInqDimgroupID(streamID);
      /*
      if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
      */
      /*
      hdf_def_dim(fileID, "mlev", mlev, &ncdimid);
      hdf_def_dim(fileID, "ilev", ilev, &ncdimid2);
      */

      streams[streamID].vct.mlev   = mlev;
      streams[streamID].vct.ilev   = ilev;
      /*
      streams[streamID].vct.mlevID = ncdimid;
      streams[streamID].vct.ilevID = ncdimid2;
      */

      hdf_def_var(fileID, "mlev", H5T_NATIVE_INT,    1, &mlev, &ncvarid, H5P_DEFAULT);
      hdf_def_var(fileID, "ilev", H5T_NATIVE_INT,    1, &ilev, &ncvarid2, H5P_DEFAULT);
      hdf_def_var(fileID, "hyai", H5T_NATIVE_DOUBLE, 1, &ilev, &hyaiid, H5P_DEFAULT);
      hdf_def_var(fileID, "hybi", H5T_NATIVE_DOUBLE, 1, &ilev, &hybiid, H5P_DEFAULT);
      hdf_def_var(fileID, "hyam", H5T_NATIVE_DOUBLE, 1, &mlev, &hyamid, H5P_DEFAULT);
      hdf_def_var(fileID, "hybm", H5T_NATIVE_DOUBLE, 1, &mlev, &hybmid, H5P_DEFAULT);

      strcpy(tmpname, "hybrid level at layer midpoints");
      hdf_put_att_text(fileID, ncvarid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid_sigma_pressure");
      hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "level");
      hdf_put_att_text(fileID, ncvarid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "down");
      hdf_put_att_text(fileID, ncvarid, "positive", strlen(tmpname), tmpname);
      strcpy(tmpname, "hyam hybm (mlev=hyam+hybm*aps)");
      hdf_put_att_text(fileID, ncvarid, "formula", strlen(tmpname), tmpname);
      strcpy(tmpname, "ap: hyam b: hybm ps: aps");
      hdf_put_att_text(fileID, ncvarid, "formula_terms", strlen(tmpname), tmpname);
      strcpy(tmpname, "ilev");
      hdf_put_att_text(fileID, ncvarid, "borders", strlen(tmpname), tmpname);

      strcpy(tmpname, "hybrid level at layer interfaces");
      hdf_put_att_text(fileID, ncvarid2, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid_sigma_pressure");
      hdf_put_att_text(fileID, ncvarid2, "standard_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "level");
      hdf_put_att_text(fileID, ncvarid2, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "down");
      hdf_put_att_text(fileID, ncvarid2, "positive", strlen(tmpname), tmpname);
      strcpy(tmpname, "hyai hybi (ilev=hyai+hybi*aps)");
      hdf_put_att_text(fileID, ncvarid2, "formula", strlen(tmpname), tmpname);
      strcpy(tmpname, "ap: hyai b: hybi ps: aps");
      hdf_put_att_text(fileID, ncvarid2, "formula_terms", strlen(tmpname), tmpname);

      strcpy(tmpname, "hybrid A coefficient at layer interfaces");
      hdf_put_att_text(fileID, hyaiid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "Pa");
      hdf_put_att_text(fileID, hyaiid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid B coefficient at layer interfaces");
      hdf_put_att_text(fileID, hybiid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "1");
      hdf_put_att_text(fileID, hybiid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid A coefficient at layer midpoints");
      hdf_put_att_text(fileID, hyamid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "Pa");
      hdf_put_att_text(fileID, hyamid, "units", strlen(tmpname), tmpname);
      strcpy(tmpname, "hybrid B coefficient at layer midpoints");
      hdf_put_att_text(fileID, hybmid, "long_name", strlen(tmpname), tmpname);
      strcpy(tmpname, "1");
      hdf_put_att_text(fileID, hybmid, "units", strlen(tmpname), tmpname);
      /*
      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
      */
      for ( i = 0; i < ilev; i++ ) ilevels[i] = i+1;
      hdf_put_var_int(fileID, ncvarid,  ilevels);
      hdf_put_var_int(fileID, ncvarid2, ilevels);
      free(ilevels);

      hdf_put_var_double(fileID, hyaiid, vctptr);
      hdf_put_var_double(fileID, hybiid, vctptr+ilev);

      for ( i = 0; i < mlev; i++ )
	{
	  start = i;
	  mval = (vctptr[i] + vctptr[i+1]) * 0.5;
	  hdf_put_vara_double(fileID, hyamid, &start, &count, &mval);
	  mval = (vctptr[ilev+i] + vctptr[ilev+i+1]) * 0.5;
	  hdf_put_vara_double(fileID, hybmid, &start, &count, &mval);	  
	}
    }

#endif
}

void hdfDefLev(int streamID, int zaxisID)
{
#if  defined  (HAVE_LIBHDF5)
  char units[128];
  char longname[128];
  char tmpname[128];
  char axisname[5] = "levX";
  int index, index2;
  int zaxisID0, zaxistype0;
  int dimID = UNDEFID;
  int fileID;
  int dimlen, dimlen0;
  int levelID;
  int ncvarid;
  int type;
  int nzaxis;
  int ilevel = 0;
  int vlistID;
  int zaxisindex;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqDimgroupID(streamID);

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);

  nzaxis = vlistNzaxis(vlistID);

  dimlen = zaxisInqSize(zaxisID);
  type   = zaxisInqType(zaxisID);

  if ( dimlen == 1 && type == ZAXIS_SURFACE ) return;

  for ( index = 0; index < nzaxis; index++ )
    {
      if ( streams[streamID].zaxisID[index] != UNDEFID )
	{
	  zaxisID0 = vlistZaxis(vlistID, index);
	  zaxistype0 = zaxisInqType(zaxisID0);

	  dimlen0 = zaxisInqSize(zaxisID0);
	  if ( dimlen == dimlen0 && type == zaxistype0 )
	    {
	      for ( levelID = 0; levelID < dimlen; levelID++ )
		{
		  if ( fabs(zaxisInqLevel(zaxisID0, levelID) - zaxisInqLevel(zaxisID, levelID)) )
		    break;
		}
	      if ( levelID == dimlen )
		{
		  dimID = streams[streamID].zaxisID[index];
		  break;
		}
	    }
	  for ( index2 = 0; index2 < index; index2++ )
	    if ( streams[streamID].zaxisID[index] == streams[streamID].zaxisID[index2] )
	      break;
	  if ( index2 == index ) ilevel++;
	}
    }

  if ( dimID == UNDEFID )
    {
      if ( ilevel == 0 ) axisname[3] = '\0';
      else               sprintf(&axisname[3], "%1d", ilevel+1);

      if ( type == ZAXIS_HYBRID || type == ZAXIS_HYBRID_HALF )
	{
	  size_t ilev = zaxisInqVctSize(zaxisID)/2;
	  size_t mlev = ilev - 1;

	  if ( type == ZAXIS_HYBRID )
	    {
	      if ( (int) mlev != dimlen )
		{
		  /*
		  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
		  */
		  /*
		  hdf_def_dim(fileID, axisname, dimlen, &dimID);
		  */
		  hdf_def_var(fileID, axisname, H5T_NATIVE_INT, 1, &dimlen,  &ncvarid, H5P_DEFAULT);

		  strcpy(tmpname, "hybrid level at layer midpoints");
		  hdf_put_att_text(fileID, ncvarid, "long_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hybrid_sigma_pressure");
		  hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "level");
		  hdf_put_att_text(fileID, ncvarid, "units", strlen(tmpname), tmpname);
		  strcpy(tmpname, "down");
		  hdf_put_att_text(fileID, ncvarid, "positive", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hyam hybm (mlev=hyam+hybm*aps)");
		  hdf_put_att_text(fileID, ncvarid, "formula", strlen(tmpname), tmpname);
		  strcpy(tmpname, "ap: hyam b: hybm ps: aps");
		  hdf_put_att_text(fileID, ncvarid, "formula_terms", strlen(tmpname), tmpname);
		  strcpy(tmpname, "ilev");
		  hdf_put_att_text(fileID, ncvarid, "borders", strlen(tmpname), tmpname);
		  /*
		  hdf_enddef(fileID);
		  streams[streamID].ncmode = 2;
		  */
		  hdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
		}
	    }

	  if ( type == ZAXIS_HYBRID_HALF )
	    {
	      if ( (int) ilev != dimlen )
		{
		  /*
		  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
		  */
		  /*
		  hdf_def_dim(fileID, axisname, dimlen, &dimID);
		  */
		  hdf_def_var(fileID, axisname, H5T_NATIVE_INT, 1, &dimlen,  &ncvarid, H5P_DEFAULT);

		  strcpy(tmpname, "hybrid level at layer interfaces");
		  hdf_put_att_text(fileID, ncvarid, "long_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hybrid_sigma_pressure");
		  hdf_put_att_text(fileID, ncvarid, "standard_name", strlen(tmpname), tmpname);
		  strcpy(tmpname, "level");
		  hdf_put_att_text(fileID, ncvarid, "units", strlen(tmpname), tmpname);
		  strcpy(tmpname, "down");
		  hdf_put_att_text(fileID, ncvarid, "positive", strlen(tmpname), tmpname);
		  strcpy(tmpname, "hyai hybi (ilev=hyai+hybi*aps)");
		  hdf_put_att_text(fileID, ncvarid, "formula", strlen(tmpname), tmpname);
		  strcpy(tmpname, "ap: hyai b: hybi ps: aps");
		  hdf_put_att_text(fileID, ncvarid, "formula_terms", strlen(tmpname), tmpname);
		  /*
		  hdf_enddef(fileID);
		  streams[streamID].ncmode = 2;
		  */
		  hdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
		}
	    }

	  hdfDefVCT(streamID, zaxisID);

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
	  /*
	  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
	  */
	  /*
	  hdf_def_dim(fileID, axisname, dimlen, &dimID);
	  */

	  zaxisInqLongname(zaxisID, longname);
	  zaxisInqUnits(zaxisID, units);

	  hdf_def_var(fileID, axisname, H5T_NATIVE_DOUBLE, 1, &dimlen, &ncvarid, H5P_DEFAULT);
	  hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
	  if ( strlen(units) )
	    hdf_put_att_text(fileID, ncvarid, "units", strlen(units), units);
	  /*
	  hdf_enddef(fileID);
	  streams[streamID].ncmode = 2;
	  */
	  hdf_put_var_double(fileID, ncvarid, zaxisInqLevelsPtr(zaxisID));
	}
    }

  if ( dimID != UNDEFID )
    streams[streamID].zaxisID[zaxisindex] = dimID;

#endif
}

void hdfDefGrid(int streamID, int gridID)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfDefGrid";
  int gridtype, size;
  int gridindex;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  gridindex = vlistGridIndex(vlistID, gridID);
  if ( streams[streamID].xdimID[gridindex] != UNDEFID ) return;

  gridtype = gridInqType(gridID);
  size = gridInqSize(gridID);

  if ( CDI_Debug )
    Message(func, "gridtype = %d  size = %d", gridtype, size);

  if ( gridtype == GRID_GAUSSIAN || gridtype == GRID_LONLAT )
    {
      /*
      if ( gridIsRotated(gridID) )
	{
	  hdfDefLonRot(streamID, gridID);
	  hdfDefLatRot(streamID, gridID);
	}
      else
      */
	{
	  hdfDefLon(streamID, gridID);
	  hdfDefLat(streamID, gridID);
	}
    }
  else if ( gridtype == GRID_GENERIC )
    {   
      int xsize, ysize;
      xsize = gridInqXsize(gridID);
      ysize = gridInqYsize(gridID);
      /*
      if ( xsize == 0 || ysize == 0 )
	{
	  hdfDefGdim(streamID, gridID);
	}
      else
      */
	{
	  hdfDefLon(streamID, gridID);
	  hdfDefLat(streamID, gridID);
	}
    }
  /*
  else if ( gridtype == GRID_GAUSSIAN_REDUCED )
    {
      hdfDefRgrid(streamID, gridID);
    }
  */
  else if ( gridtype == GRID_SPECTRAL )
    {
      /*
      hdfDefComplex(streamID, gridID);
      hdfDefSpc(streamID, gridID);
      */
    }
  /*
  else if ( gridtype == GRID_TRAJECTORY )
    {
      hdfDefTrajLon(streamID, gridID);
      hdfDefTrajLat(streamID, gridID);
    }
  else if ( gridtype == GRID_CELL )
    {
      hdfDefCell(streamID, gridID);
    }
  */
  else
    {
      Error(func, "%s grid unsupported!", gridNamePtr(gridtype));
    }
#endif
}

void hdfDefZaxis(int streamID, int zaxisID)
{
#if  defined  (HAVE_LIBHDF5)
  int vlistID, zaxisindex;

  vlistID    = streamInqVlist(streamID);
  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);

  if ( streams[streamID].zaxisID[zaxisindex] != UNDEFID ) return;

  hdfDefLev(streamID, zaxisID);
#endif
}

int hdfDefVar(int streamID, int varID)
{
  int ncvarid = -1;
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfDefVar";
  int fileID;
  int xid = UNDEFID, yid = UNDEFID, zid = UNDEFID, tid = UNDEFID; /* varid !!! */
  int code, gridID, zaxisID;
  char varname[9];
  char *name = NULL;
  char *longname = NULL;
  char *units = NULL;
  int dims[4];
  int tableID;
  int ndims = 0;
  int len;
  int timeID;
  int xtype;
  int gridindex, zaxisindex;
  int tablenum;
  hsize_t cdims[9];
  int plistid;
  int compressLevel;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);

  if ( streams[streamID].vars[varID].ncvarid != UNDEFID )
    return (streams[streamID].vars[varID].ncvarid);

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);
  code    = vlistInqVarCode(vlistID, varID);

  hdfDefGrid(streamID, gridID);

  hdfDefZaxis(streamID, zaxisID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) != GRID_TRAJECTORY )
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];

  tid = streams[streamID].basetime.ncvarid;
  /* printf("%d %d %d %d\n", tid, zid, yid, xid); */

  if ( timeID == TIME_VARIABLE )
    {
      if ( tid == UNDEFID ) Error(func, "Internal problem! Time undefined.");

      cdims[ndims] = 1;
      /* dims[ndims++] = tid; */
      dims[ndims++] = -1;
    }
  /*
  if ( zid != UNDEFID ) dims[ndims++] = zid;
  if ( yid != UNDEFID ) dims[ndims++] = yid;
  if ( xid != UNDEFID ) dims[ndims++] = xid;
  */
  if ( zaxisID != UNDEFID )
    {
      size_t size = zaxisInqSize(zaxisID);
      if ( ! (zaxisInqType(zaxisID) == ZAXIS_SURFACE && size == 1) )
	{
	  cdims[ndims] = 1;
	  dims[ndims++] = zaxisInqSize(zaxisID);
	}
    }
  if ( gridID != UNDEFID )
    {
      int xsize, ysize;
      if ( gridInqType(gridID) == GRID_SPECTRAL )
	{
	  xsize = 2;
	  ysize = gridInqSize(gridID)/2;
	}
      else
	{
	  xsize = gridInqXsize(gridID);
	  ysize = gridInqYsize(gridID);
	}
      cdims[ndims] = ysize;
      dims[ndims]  = ysize;
      ndims++;
      cdims[ndims] = xsize;
      dims[ndims]  = xsize;
      ndims++;
    }


  tableID  = vlistInqVarTable(vlistID, varID);

  name     = vlistInqVarNamePtr(vlistID, varID);
  longname = vlistInqVarLongnamePtr(vlistID, varID);
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
  /*
  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
  */
  xtype = hdfDefDatatype(vlistInqVarDatatype(vlistID, varID));

  plistid = H5Pcreate(H5P_DATASET_CREATE);
  H5Pset_chunk(plistid, ndims, cdims);

  compressLevel = streams[streamID].compress;
  if ( compressLevel )
    H5Pset_deflate(plistid, compressLevel);

  hdf_def_var(fileID, name, xtype, ndims, dims, &ncvarid, plistid);

  H5Pclose(plistid);

  if ( longname )
    hdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);

  if ( units )
    hdf_put_att_text(fileID, ncvarid, "units", strlen(units), units);

  /*
  if ( code > 0 )
    hdf_put_att_int(fileID, ncvarid, "code", H5T_NATIVE_INT, 1L, &code);
  */

  if ( tableID != UNDEFID )
    {
      tablenum = tableInqNum(tableID);
      /*
      if ( tablenum > 0 )
	hdf_put_att_int(fileID, ncvarid, "table", H5T_NATIVE_INT, 1L, &tablenum);
      */
    }

  if ( gridInqType(gridID) != GRID_GENERIC && gridInqType(gridID) != GRID_LONLAT )
    {
      len = strlen(gridNamePtr(gridInqType(gridID)));
      if ( len > 0 )
	hdf_put_att_text(fileID, ncvarid, "grid_type", len,
			 gridNamePtr(gridInqType(gridID)));
    }

  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      hdf_put_att_text(fileID, ncvarid, "coordinates", 9, "tlon tlat" );
    }
  /*
  if ( gridInqType(gridID) == GRID_CELL )
    {
      char coordinates[128] = "";
      char cellarea[128] = "area: ";
      int ncxvarID, ncyvarID, ncavarID;
      int gridindex;
      size_t len;

      gridindex = vlistGridIndex(vlistID, gridID);
      ncxvarID = streams[streamID].ncxvarID[gridindex];
      ncyvarID = streams[streamID].ncyvarID[gridindex];
      ncavarID = streams[streamID].ncavarID[gridindex];
      if ( ncxvarID != CDI_UNDEFID )
	hdf_inq_varname(fileID, ncxvarID, coordinates);
      len = strlen(coordinates);
      if ( ncyvarID != CDI_UNDEFID )
	{
	  if ( len ) coordinates[len++] = ' ';
	  hdf_inq_varname(fileID, ncyvarID, coordinates+len);
	}
      len = strlen(coordinates);
      if ( len )
	hdf_put_att_text(fileID, ncvarid, "coordinates", len, coordinates);

      if ( ncavarID != CDI_UNDEFID )
	{
	  len = strlen(cellarea);
	  hdf_inq_varname(fileID, ncavarID, cellarea+len);
	  len = strlen(cellarea);
	  hdf_put_att_text(fileID, ncvarid, "cell_measures", len, cellarea);
	}
    }
  */
  if ( gridInqType(gridID) == GRID_SPECTRAL )
    {
      /*
      int gridTruncation = gridInqTrunc(gridID);
      hdf_put_att_int(fileID, ncvarid, "truncation", H5T_NATIVE_INT, 1L, &gridTruncation);
      */
    }

  if ( xtype == NC_BYTE || xtype == NC_SHORT || xtype == H5T_NATIVE_INT )
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;

      addoffset   = vlistInqVarAddoffset(vlistID, varID);
      scalefactor = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = (fabs(addoffset - 0.0) > 0.0);
      lscalefactor = (fabs(scalefactor - 1.0) > 0.0);
      /*
      if ( laddoffset )
	hdf_put_att_double(fileID, ncvarid, "add_offset", NC_FLOAT, 1L, &addoffset);

      if ( lscalefactor )
	hdf_put_att_double(fileID, ncvarid, "scale_factor", NC_FLOAT, 1L, &scalefactor);
      */
    }

  streams[streamID].vars[varID].ncvarid = ncvarid;
  /*
  if ( streams[streamID].ncmode == 2 ) hdf_enddef(fileID);
  */
#endif
  return (ncvarid);
}

#if defined (XYZ)

void hdfReadVarDP(int streamID, int varID, double *data, int *nmiss)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfReadVarDP";
  int fileID;
  int gridID;
  int zaxisID;
  int xid = UNDEFID, yid = UNDEFID, zid = UNDEFID;
  int ncvarid;
  int tsID;
  size_t start[4];
  size_t count[4];
  int ndims = 0;
  int idim;
  int timeID;
  int gridindex, zaxisindex;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

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
      hdfReadGridTraj(streamID, gridID);
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

  hdf_get_vara_double(fileID, ncvarid, start, count, data);
#endif
}

void hdfWriteVarDP(int streamID, int varID, double *data, int nmiss)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfWriteVarDP";
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

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  fileID = streamInqFileID(streamID);

  ntsteps = streams[streamID].ntsteps;
  if ( CDI_Debug )
    Message(func, "ntsteps = %d", ntsteps); 

  if ( vlistHasTime(vlistID) ) hdfDefTime(streamID);

  ncvarid = hdfDefVar(streamID, varID);

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      hdfWriteGridTraj(streamID, gridID);
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
      hdf_inq_dimlen(fileID, yid, &size);
      /*      count[ndims] = gridInqYsize(gridID); */
      count[ndims] = size;
      ndims++;
    }
  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      hdf_inq_dimlen(fileID, xid, &size);
      /*      count[ndims] = gridInqXsize(gridID); */
      count[ndims] = size;
      ndims++;
    }

  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  if ( streams[streamID].ncmode == 1 )
    {
      hdf_enddef(fileID);
      streams[streamID].ncmode = 2;
    }

  dtype = vlistInqVarDatatype(vlistID, varID);

  if ( nmiss > 0 ) hdfDefMissval(streamID, varID, dtype);

  if ( dtype == DATATYPE_INT1 || dtype == DATATYPE_INT2 || dtype == DATATYPE_INT4 )
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;

      addoffset   = vlistInqVarAddoffset(vlistID, varID);
      scalefactor = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = (fabs(addoffset   - 0.0) > 0.0);
      lscalefactor = (fabs(scalefactor - 1.0) > 0.0);

      if ( laddoffset || lscalefactor )
	{
	  gridID   = vlistInqVarGrid(vlistID, varID);
	  size     = gridInqSize(gridID);

	  for ( i = 0; i < size; i++ )
	    {
	      if ( laddoffset )   data[i] = data[i] - addoffset;
	      if ( lscalefactor ) data[i] = data[i] / scalefactor;
	    }
	}
    }

  hdf_put_vara_double(fileID, ncvarid, start, count, data);
#endif
}

int hdfReadVarSliceDP(int streamID, int varID, int levelID, double *data, int *nmiss)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfReadVarSliceDP";
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

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d  levelID = %d", streamID, varID, levelID);

  fileID = streamInqFileID(streamID);

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
      hdfReadGridTraj(streamID, gridID);
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
      start[ndims] = levelID;
      count[ndims] = 1;
      ndims++;
    }
  if ( yid != UNDEFID )
    {
      start[ndims] = 0;
      /* count[ndims] = gridInqYsize(gridID); */
      hdf_inq_dimlen(fileID, yid, &size);
      count[ndims] = size;
      ndims++;
    }
  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      /* count[ndims] = gridInqXsize(gridID); */
      hdf_inq_dimlen(fileID, xid, &size);
      count[ndims] = size;
      ndims++;
    }

  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  hdf_get_vara_double(fileID, ncvarid, start, count, data);

  *nmiss = 0;
  if ( vlistInqVarMissvalUsed(vlistID, varID) == TRUE  )
    {
      int i;
      double missval;

      missval = vlistInqVarMissval(vlistID, varID);
      gridID  = vlistInqVarGrid(vlistID, varID);
      size    = gridInqSize(gridID);

      for ( i = 0; i < size; i++ )
	if ( DBL_IS_EQUAL(data[i], missval) ) *nmiss += 1;
    }

#endif
  return (0);
}
#endif /*XYZ*/

int hdfWriteVarSliceDP(int streamID, int varID, int levelID, double *data, int nmiss)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfWriteVarSliceDP";
  int fileID;
  int gridID;
  int zaxisID;
  int zid = UNDEFID;
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
  hsize_t hsize[9];
  int status;
  int vlistID;
    
  if ( CDI_Debug )
    Message(func, "streamID = %d  varid = %d", streamID, varID);

  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);

  ntsteps   = streams[streamID].ntsteps;
  if ( CDI_Debug )
    Message(func, "ntsteps = %d", ntsteps);
  /*
  if ( vlistHasTime(vlistID) ) hdfDefTime(streamID);
  */
  ncvarid = hdfDefVar(streamID, varID);

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  timeID  = vlistInqVarTime(vlistID, varID);

  gridindex = vlistGridIndex(vlistID, gridID);
  /*
  if ( gridInqType(gridID) == GRID_TRAJECTORY )
    {
      hdfWriteGridTraj(streamID, gridID);
    }
  else
    {
      xid = streams[streamID].xdimID[gridindex];
      yid = streams[streamID].ydimID[gridindex];
    }
  */

  zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  zid = streams[streamID].zaxisID[zaxisindex];

  if ( timeID == TIME_VARIABLE )
    {
      start[ndims] = ntsteps - 1;
      count[ndims] = 1;
      hsize[ndims] = ntsteps;
      ndims++;
    }
  /*
  if ( zid != UNDEFID )
    {
      start[ndims] = levelID;
      count[ndims] = 1;
      ndims++;
    }
  if ( yid != UNDEFID )
    {
      start[ndims] = 0;
      hdf_inq_dimlen(fileID, yid, &size);
      count[ndims] = size;
      ndims++;
    }
  if ( xid != UNDEFID )
    {
      start[ndims] = 0;
      hdf_inq_dimlen(fileID, xid, &size);
      count[ndims] = size;
      ndims++;
    }
  */
  if ( zaxisID != UNDEFID )
    {
      size_t size = zaxisInqSize(zaxisID);
      if ( ! (zaxisInqType(zaxisID) == ZAXIS_SURFACE && size == 1) )
	{
	  start[ndims] = levelID;
	  count[ndims] = 1;
	  hsize[ndims] = size;
	  ndims++;
	}
    }
  if ( gridID != UNDEFID )
    {
      start[ndims] = 0;
      count[ndims] = gridInqYsize(gridID);
      hsize[ndims] = gridInqYsize(gridID);
      ndims++;
      start[ndims] = 0;
      count[ndims] = gridInqXsize(gridID);
      hsize[ndims] = gridInqXsize(gridID);
      ndims++;
    }


  if ( CDI_Debug )
    for (idim = 0; idim < ndims; idim++)
      Message(func, "dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);

  dtype = vlistInqVarDatatype(vlistID, varID);
  /*
  if ( nmiss > 0 ) hdfDefMissval(streamID, varID, dtype);
  */
  if ( dtype == DATATYPE_INT1 || dtype == DATATYPE_INT2 || dtype == DATATYPE_INT4 )
    {
      int laddoffset, lscalefactor;
      double addoffset, scalefactor;

      addoffset   = vlistInqVarAddoffset(vlistID, varID);
      scalefactor = vlistInqVarScalefactor(vlistID, varID);
      laddoffset   = (fabs(addoffset   - 0.0) > 0.0);
      lscalefactor = (fabs(scalefactor - 1.0) > 0.0);

      if ( laddoffset || lscalefactor )
	{
	  gridID   = vlistInqVarGrid(vlistID, varID);
	  size     = gridInqSize(gridID);

	  for ( i = 0; i < (int) size; i++ )
	    {
	      if ( laddoffset )   data[i] = data[i] - addoffset;
	      if ( lscalefactor ) data[i] = data[i] / scalefactor;
	    }
	}
    }

  if ( levelID == 0 )
    {
      status = H5Dextend(ncvarid, hsize);
      if ( status < 0 )
	{
	  H5Eprint(NULL);
	  Error(func, "write problem");
	}
    }

  hdf_put_vara_double(fileID, ncvarid, start, count, data);

#endif
  return (0);
}
#if defined (XYZ)
void hdfCreateRecords(int streamID, int tsID)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfCreateRecords";
  int varID, levelID, recordID;
  int nrecords = 0,  nvars, nlev;
  RECORD *records;

  if ( tsID < 0 || (tsID >= streams[streamID].ntsteps && tsID > 0) ) return;

  if ( streams[streamID].tsteps[tsID].nrecords > 0 ) return;

  nvars    = streams[streamID].nvars;

  for ( varID = 0; varID < nvars; varID++ )
    {
      if ( tsID > 0 && vlistInqVarTime(vlistID, varID) == TIME_CONSTANT )
	continue;
      nlev = streams[streamID].vars[varID].nlevs;
      nrecords += nlev;
    }

  streams[streamID].nrecs += nrecords;

  records = (RECORD *) malloc(nrecords*sizeof(RECORD));
  streams[streamID].tsteps[tsID].records    = records;
  streams[streamID].tsteps[tsID].nrecords   = nrecords;
  streams[streamID].tsteps[tsID].recordSize = nrecords;
  streams[streamID].tsteps[tsID].curRecID   = UNDEFID;

  recordID = 0;
  for ( varID = 0; varID < nvars; varID++ )
    {
      if ( tsID > 0 && vlistInqVarTime(vlistID, varID) == TIME_CONSTANT )
	continue;

      nlev = streams[streamID].vars[varID].nlevs;
      
      for ( levelID = 0; levelID < nlev; levelID++ )
	{
	  recordInitEntry(&records[recordID]);
	  records[recordID].varID   = varID;
	  records[recordID].levelID = levelID;
	  recordID++;
	}
    }
#endif
}

int hdfTimeDimID(int fileID, int ndims, int nvars)
{
  int dimid = 0;
#if  defined  (HAVE_LIBHDF5)
  char dimname[80];
  char timeunits[128];
  char attname[128];
  char name[128];
  int xtype;
  int nvdims, nvatts;
  int dimids[9];
  int varid, iatt;

  for ( dimid = 0; dimid < ndims; dimid++ )
    {
      hdf_inq_dimname(fileID, dimid, dimname);
      if ( strncmp(dimname, "time", 4) == 0 ) break;
    }

  if ( dimid == ndims ) dimid = UNDEFID;

  for ( varid = 0; varid < nvars; varid++ )
    {
      if ( dimid != UNDEFID ) break;

      hdf_inq_var(fileID, varid, name, &xtype, &nvdims, dimids, &nvatts);
      if ( nvdims == 1 )
	{
	  for ( iatt = 0; iatt < nvatts; iatt++ )
	    {
	      hdf_inq_attname(fileID, varid, iatt, attname);
	      if ( strncmp(attname, "units", 5) == 0 )
		{
		  hdf_get_att_text(fileID, varid, "units", timeunits);
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
#endif /*XYZ*/

void hdfSetVar(HDFVAR *ncvars, int ncvarid, int isvar)
{
  static char func[] = "hdfSetVar";

  if ( isvar != TRUE && isvar != FALSE )
    Error(func, "Internal problem! isvar = %d", isvar);

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

void hdfSetDim(HDFVAR *ncvars, int ncvarid, int dimid, int dimtype)
{
  static char func[] = "hdfSetDim";

  if ( ncvars[ncvarid].dimtype[dimid] != UNDEFID &&
       ncvars[ncvarid].dimtype[dimid] != dimtype )
    {
      Warning(func, "Inconsistent dimension definition for %s! dimid %d  oldtype %d  newtype %d",
	      ncvars[ncvarid].name, dimid, ncvars[ncvarid].dimtype[dimid], dimtype);
    }

  ncvars[ncvarid].dimtype[dimid] = dimtype;
}

void printHDFvars(HDFVAR *hdfvars, int nvars)
{
  int hdfvarid;

  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      printf("%3d %3d %s\n", hdfvarid, hdfvars[hdfvarid].isvar, hdfvars[hdfvarid].name);
    }
}
/*
 * Operator function.
 */
#if  defined  (HAVE_LIBHDF5)
herr_t group_info(hid_t loc_id, const char *name, void *opdata)
{
  hid_t did;  /* dataset identifier  */
  hid_t tid;  /* datatype identifier */
  H5T_class_t t_class;
  hid_t pid;  /* data_property identifier */
  hsize_t chunk_dims_out[5];
  int i;

  int  rank_chunk;

  /* avoid warnings */
  opdata = opdata;

  /*
   * Open the datasets using their names.
   */
  did = H5Dopen(loc_id, name);
 
  /*
   * Display dataset name.
   */
  printf("\n");
  printf("Name : ");
  puts(name);
  
  /*    
   * Display dataset information.
   */
  tid = H5Dget_type(did);  /* get datatype*/
  pid = H5Dget_create_plist(did); /* get creation property list */
  
  /*
   * Check if dataset is chunked.
   */
  if(H5D_CHUNKED == H5Pget_layout(pid)){
    /*
     * get chunking information: rank and dimensions.
     */
    rank_chunk = H5Pget_chunk(pid, 5, chunk_dims_out);
    printf("chunk rank %d, dimensions ", rank_chunk);
    for ( i = 0; i < rank_chunk; i++)
      {
	printf("%lu ", (unsigned long)(chunk_dims_out[i]));
	if ( i + 1 < rank_chunk ) printf(" x ");
      }
    printf("\n");
  } 
  else{
    t_class = H5Tget_class(tid);
    if(t_class < 0){ 
      puts(" Invalid datatype.\n");
    }
    else {
      if(t_class == H5T_INTEGER)
      puts(" Datatype is 'H5T_NATIVE_INTEGER'.\n");
      if(t_class == H5T_FLOAT)
      puts(" Datatype is 'H5T_NATIVE_FLOAT'.\n");
      if(t_class == H5T_STRING)
      puts(" Datatype is 'H5T_NATIVE_STRING'.\n");
      if(t_class == H5T_BITFIELD)
      puts(" Datatype is 'H5T_NATIVE_BITFIELD'.\n");
      if(t_class == H5T_OPAQUE)
      puts(" Datatype is 'H5T_NATIVE_OPAQUE'.\n");
      if(t_class == H5T_COMPOUND)
      puts(" Datatype is 'H5T_NATIVE_COMPOUND'.\n");
    }
  }
    

  H5Dclose(did);
  H5Pclose(pid);
  H5Tclose(tid);

  return 0;
 }
#endif
/*
 * Operator function.
 */
#if  defined  (HAVE_LIBHDF5)
herr_t file_info(hid_t loc_id, const char *name, void *opdata)
{
    /* avoid warnings */
    loc_id = loc_id;
    opdata = opdata;

    /*
     * Display group name. The name is passed to the function by 
     * the Library. Some magic :-)
     */
    printf("\n");
    printf("Name : ");
    puts(name);
   
    return 0;
 }
#endif

void hdfOpenVars(int fileID, HDFVAR *hdfvars, int nvars)
{
#if  defined  (HAVE_LIBHDF5)
  char name[128];
  int groupID, varID;
  int status;
  hsize_t num_obj, idx;
  int hdfvarid = 0;

  groupID = H5Gopen(fileID, "/");
  status = H5Gget_num_objs(groupID, &num_obj);
  printf("num_obj %d\n", (int) num_obj);
  for ( idx = 0; idx < num_obj; idx++ )
    {
      if ( H5Gget_objtype_by_idx(groupID, idx) == H5G_DATASET )
	{
	  H5Gget_objname_by_idx(groupID, idx, name, (size_t) 128);
	  printf("  name: %s %s\n", "/", name);
	  varID = H5Dopen(groupID, name);
	  /*	  hdfvars[hdfvarid].groupID = groupID; */
	  hdfvars[hdfvarid].objID = varID;
	  strcpy(hdfvars[hdfvarid].name, name);
	  hdfvarid++;
	}
      else if ( H5Gget_objtype_by_idx(groupID, idx) == H5G_GROUP )
	{
	  hsize_t num_obj2, idx2;
	  int groupID2;
	  char name2[128];
	  H5Gget_objname_by_idx(groupID, idx, name, (size_t) 128);
	  groupID2 = H5Gopen(fileID, name);
	  status = H5Gget_num_objs(groupID2, &num_obj2);
	  for ( idx2 = 0; idx2 < num_obj2; idx2++ )
	    {
	      if ( H5Gget_objtype_by_idx(groupID2, idx2) == H5G_DATASET )
		{
		  H5Gget_objname_by_idx(groupID2, idx2, name2, (size_t) 128);
		  printf("  name: %s %s\n", name, name2);
		  varID = H5Dopen(groupID2, name2);
		  /*	  hdfvars[hdfvarid].groupID = groupID; */
		  hdfvars[hdfvarid].objID = varID;
		  strcpy(hdfvars[hdfvarid].name, name2);
		  hdfvarid++;
		}
	      else if ( H5Gget_objtype_by_idx(groupID2, idx2) == H5G_GROUP )
		{
		  printf("group level2 not supported\n");
		}  
	    }
	  H5Gclose(groupID2);
	  printf("num_obj2 %d\n", (int) num_obj2);
	}
    }
  printf("num_obj  %d\n", (int) num_obj);
  printf("hdfvarid %d\n", (int) hdfvarid);

  H5Gclose(groupID);
#endif
}

void hdfInqContents(int streamID)
{
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfInqContents";
  int fileID;
  int ndims, hdfvarid;
  int hdfobjID;
  int nvars, ngatts, unlimdimid;
  int ncdimid;
  int nvdims, nvatts;
  int *dimidsp;
  int xtype;
  size_t ntsteps = 0;
  int timedimid = -1;
  int iatt;
  size_t attlen;
  char name[128];
  char attname[128];
  char atttext[1024];
  int tablenum;
  int instID  = UNDEFID;
  int modelID = UNDEFID;
  HDFVAR *hdfvars;
  int i;
  int status;
  int groupID;
  hsize_t num_obj, idx;
  hsize_t dims[9], maxdims[9];
  int varspaceid;
  int vlistID;
  
  streamCheckID(func, streamID);

  fileID  = streamInqFileID(streamID);
  vlistID = streamInqVlist(streamID);

  if ( CDI_Debug )
    Message(func, "streamID = %d, fileID = %d", streamID, fileID);

  groupID = H5Gopen(fileID, "/");
  status = H5Gget_num_objs(groupID, &num_obj);
  printf("num_obj %d\n", (int) num_obj);
  for ( idx = 0; idx < num_obj; idx++ )
    {
      if ( H5Gget_objtype_by_idx(groupID, idx) == H5G_DATASET )
	{
	  H5Gget_objname_by_idx(groupID, idx, name, (size_t) 128);
	  printf("  name: %s %s\n", "/", name);
	  hdfvarid = H5Dopen(groupID, name);

	  varspaceid = H5Dget_space(hdfvarid);
	  ndims = H5Sget_simple_extent_dims(varspaceid, dims, maxdims);
	  for ( i = 0; i < ndims; i++ )
	    printf("    id %d  dim %4d  maxdim %4d\n", i, (int) dims[i], (int) maxdims[i]);
	}
      else if ( H5Gget_objtype_by_idx(groupID, idx) == H5G_GROUP )
	{
	  hsize_t num_obj2, idx2;
	  int groupID2;
	  char name2[128];
	  H5Gget_objname_by_idx(groupID, idx, name, (size_t) 128);
	  groupID2 = H5Gopen(fileID, name);
	  status = H5Gget_num_objs(groupID2, &num_obj2);
	  for ( idx2 = 0; idx2 < num_obj2; idx2++ )
	    {
	      if ( H5Gget_objtype_by_idx(groupID2, idx2) == H5G_DATASET )
		{
		  H5Gget_objname_by_idx(groupID2, idx2, name2, (size_t) 128);
		  printf("  name: %s %s\n", name, name2);
		  hdfvarid = H5Dopen(groupID2, name2);

		  varspaceid = H5Dget_space(hdfvarid);
		  ndims = H5Sget_simple_extent_dims(varspaceid, dims, maxdims);
		  for ( i = 0; i < ndims; i++ )
		    printf("    id %d  dim %4d  maxdim %4d\n", i, (int) dims[i], (int) maxdims[i]);
		}
	      else if ( H5Gget_objtype_by_idx(groupID2, idx2) == H5G_GROUP )
		{
		  printf("group level2 not supported\n");
		}  
	    }
	  H5Gclose(groupID2);
	  printf("num_obj2 %d\n", (int) num_obj2);
	}
    }
  printf("num_obj %d\n", (int) num_obj);

  H5Gclose(groupID);
  hdfvarid = H5Dopen(fileID, "//coordinates/lon");
  H5Iget_name(hdfvarid, name, (size_t) 128 );
  printf("name : >%s<\n", name);
  H5Dclose(hdfvarid);


  nvars  = hdf_inq_nvars(fileID);
  ngatts = hdf_inq_ngatts(fileID);
  ndims  = 0;
  unlimdimid = -1;
  printf("nvars = %d\n", nvars);
  printf("ngatts = %d\n", ngatts);

  /* hdf_inq(fileID, &ndims , &nvars, &ngatts, &unlimdimid); */

  /* alloc hdfvars */
  /* ncdims = (HDFDIM *) malloc(ndims*sizeof(HDFDIM)); */

  /* alloc hdfvars */
  hdfvars = (HDFVAR *) malloc(nvars*sizeof(HDFVAR));

  /* init ncdims */
  /*
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      ncdims[ncdimid].hdfvarid     = UNDEFID;
      ncdims[ncdimid].dimtype      = UNDEFID;
      ncdims[ncdimid].len          = 0;
      ncdims[ncdimid].name[0]      = 0;
    }
  */
  /* init hdfvars */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      hdfvars[hdfvarid].objID        = UNDEFID;
      hdfvars[hdfvarid].isvar        = UNDEFID;
      hdfvars[hdfvarid].islon        = FALSE;
      hdfvars[hdfvarid].islat        = FALSE;
      hdfvars[hdfvarid].islev        = FALSE;
      hdfvars[hdfvarid].warn         = FALSE;
      hdfvars[hdfvarid].timeID       = TIME_CONSTANT;
      hdfvars[hdfvarid].code         = UNDEFID;
      hdfvars[hdfvarid].bounds       = UNDEFID;
      hdfvars[hdfvarid].gridID       = UNDEFID;
      hdfvars[hdfvarid].zaxisID      = UNDEFID;
      hdfvars[hdfvarid].gridtype     = UNDEFID;
      hdfvars[hdfvarid].zaxistype    = UNDEFID;
      hdfvars[hdfvarid].xdim         = UNDEFID;
      hdfvars[hdfvarid].ydim         = UNDEFID;
      hdfvars[hdfvarid].zdim         = UNDEFID;
      hdfvars[hdfvarid].xvarid       = UNDEFID;
      hdfvars[hdfvarid].yvarid       = UNDEFID;
      hdfvars[hdfvarid].cellarea     = UNDEFID;
      hdfvars[hdfvarid].tableID      = UNDEFID;
      hdfvars[hdfvarid].vlen         = 0;
      hdfvars[hdfvarid].vdata        = NULL;
      hdfvars[hdfvarid].truncation   = 0;
      hdfvars[hdfvarid].defmiss      = 0;
      hdfvars[hdfvarid].levelscale   = 1;
      hdfvars[hdfvarid].missval      = 0;
      hdfvars[hdfvarid].addoffset    = 0;
      hdfvars[hdfvarid].scalefactor  = 1;
      hdfvars[hdfvarid].name[0]      = 0;
      hdfvars[hdfvarid].longname[0]  = 0;
      hdfvars[hdfvarid].units[0]     = 0;
    }

  hdfOpenVars(fileID, hdfvars, nvars);

  /* read global attributtes*/
  for ( iatt = 0; iatt < ngatts; iatt++ )
    {
      hdf_inq_attname(fileID, HDF_GLOBAL, iatt, attname);
      if ( strcmp(attname, "history") == 0 )
	{
	  streams[streamID].historyID = iatt;
	}
      if ( strcmp(attname, "institution") == 0 )
	{
	  hdf_inq_attlen(fileID, HDF_GLOBAL, attname, &attlen);
	  hdf_get_att_text(fileID, HDF_GLOBAL, attname, atttext);
	  atttext[attlen] = 0;
	  instID = institutInq(0, 0, NULL, atttext);
	  if ( instID == UNDEFID )
	    instID = institutDef(0, 0, NULL, atttext);
	}
      if ( strcmp(attname, "source") == 0 )
	{
	  hdf_inq_attlen(fileID, HDF_GLOBAL, attname, &attlen);
	  hdf_get_att_text(fileID, HDF_GLOBAL, attname, atttext);
	  atttext[attlen] = 0;
	  modelID = modelInq(-1, 0, atttext);
	}
    }

  /* find time dim */
  /*
  if ( unlimdimid >= 0 )
    timedimid = unlimdimid;
  else
    timedimid = hdfTimeDimID(fileID, ndims, nvars);
  */
  streams[streamID].basetime.ncdimid = timedimid;
  /*
  if ( timedimid != UNDEFID )
    hdf_inq_dimlen(fileID, timedimid, &ntsteps);
  else
    ntsteps = 0;
  */
  streams[streamID].ntsteps = ntsteps;

  if ( CDI_Debug )
    Message(func, "time dimid = %d", streams[streamID].basetime.ncdimid);


  /* read ncdims */
  /*
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      hdf_inq_dimlen(fileID, ncdimid, &ncdims[ncdimid].len);
      hdf_inq_dimname(fileID, ncdimid, ncdims[ncdimid].name);
      if ( timedimid == ncdimid )
	ncdims[ncdimid].dimtype = DIMTYPE_T;
    }
  */
  /* read hdfvars */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      dimidsp = hdfvars[hdfvarid].dimids;

      hdfobjID = hdfvars[hdfvarid].objID;
      printf("hdfobjid %d %d\n", hdfvarid, hdfobjID);
      H5Iget_name(hdfobjID, name, (size_t) 128 );
      printf("name : >%s<\n", name);
      /*
      hdf_inq_var(fileID, hdfobjID, name, &xtype, &nvdims, dimidsp, &nvatts);
      */
      nvdims = hdf_inq_varndims(hdfobjID);
      continue;

      for ( ncdimid = 0; ncdimid < nvdims; ncdimid++ )
	hdfvars[hdfvarid].dimtype[ncdimid] = -1;

      hdfvars[hdfvarid].xtype = xtype;
      hdfvars[hdfvarid].ndims = nvdims;

      if ( nvdims > 0 )
	if ( timedimid == dimidsp[0] )
	  {
	    hdfvars[hdfvarid].timeID = TIME_VARIABLE;
	    hdfSetDim(hdfvars, hdfvarid, 0, DIMTYPE_T);
	  }

      for ( iatt = 0; iatt < nvatts; iatt++ )
	{
	  hdf_inq_attname(fileID, hdfvarid, iatt, attname);
	  
	  if ( strcmp(attname, "long_name") == 0 )
	    {
	      hdf_inq_attlen(fileID, hdfvarid, attname, &attlen);
	      hdf_get_att_text(fileID, hdfvarid, attname, hdfvars[hdfvarid].longname);
	      hdfvars[hdfvarid].longname[attlen] = 0;
      	    }
	  else if ( strcmp(attname, "units") == 0 )
	    {
	      hdf_inq_attlen(fileID, hdfvarid, attname, &attlen);
	      hdf_get_att_text(fileID, hdfvarid, attname, hdfvars[hdfvarid].units);
	      hdfvars[hdfvarid].units[attlen] = 0;
      	    }
	  else if ( strcmp(attname, "code") == 0 )
	    {
	      hdf_get_att_int(fileID, hdfvarid, attname, &hdfvars[hdfvarid].code);
	      hdfSetVar(hdfvars, hdfvarid, TRUE);
	    }
	  else if ( strcmp(attname, "table") == 0 )
	    {
	      hdf_get_att_int(fileID, hdfvarid, attname, &tablenum);
	      if ( tablenum > 0 )
		{
		  hdfvars[hdfvarid].tableID = tableInq(modelID, tablenum, NULL);
		  if ( hdfvars[hdfvarid].tableID == CDI_UNDEFID )
		    hdfvars[hdfvarid].tableID = tableDef(modelID, tablenum, NULL);
		}
	      hdfSetVar(hdfvars, hdfvarid, TRUE);
	    }
	  else if ( strcmp(attname, "grid_type") == 0 )
	    {
	      hdf_inq_attlen(fileID, hdfvarid, attname, &attlen);
	      hdf_get_att_text(fileID, hdfvarid, attname, atttext);
	      atttext[attlen] = 0;
	      if      ( strncmp(atttext, "gaussian", attlen) == 0 )
		hdfvars[hdfvarid].gridtype = GRID_GAUSSIAN;
	      else if ( strncmp(atttext, "gaussian reduced", attlen) == 0 )
		hdfvars[hdfvarid].gridtype = GRID_GAUSSIAN_REDUCED;
	      else if ( strncmp(atttext, "spectral", 8) == 0 )
		hdfvars[hdfvarid].gridtype = GRID_SPECTRAL;
	      else if ( strncmp(atttext, "trajectory", attlen) == 0 )
		hdfvars[hdfvarid].gridtype = GRID_TRAJECTORY;
	      else if ( strncmp(atttext, "generic", attlen) == 0 )
		hdfvars[hdfvarid].gridtype = GRID_GENERIC;
	      else if ( strncmp(atttext, "cell", attlen) == 0 )
		hdfvars[hdfvarid].gridtype = GRID_CELL;
	      else
		{
		  static int warn = 0;
		  if ( ! warn )
		    {
		      warn = 1;
		      Warning(func, "Gridtype %s unsupported!", atttext);
		    }
		  hdfvars[hdfvarid].gridtype = GRID_GENERIC;
		}

	      hdfSetVar(hdfvars, hdfvarid, TRUE);
	    }
	  else if ( strcmp(attname, "truncation") == 0 )
	    {
	      hdf_get_att_int(fileID, hdfvarid, "truncation", &hdfvars[hdfvarid].truncation);
	    }
	  else if ( strcmp(attname, "add_offset") == 0 )
	    {
	      hdf_get_att_double(fileID, hdfvarid, "add_offset", &hdfvars[hdfvarid].addoffset);
	      if ( xtype != NC_BYTE && xtype != NC_SHORT && xtype != H5T_NATIVE_INT )
		if ( hdfvars[hdfvarid].addoffset != 0 )
		  Warning(func, "attribute add_offset not supported for xtype %d\n", xtype);
	      hdfSetVar(hdfvars, hdfvarid, TRUE);
	    }
	  else if ( strcmp(attname, "scale_factor") == 0 )
	    {
	      hdf_get_att_double(fileID, hdfvarid, "scale_factor", &hdfvars[hdfvarid].scalefactor);	      
	      if ( xtype != NC_BYTE && xtype != NC_SHORT && xtype != H5T_NATIVE_INT )
		if ( hdfvars[hdfvarid].scalefactor != 1 )
		  Warning(func, "attribute scale_factor not supported for xtype %d\n", xtype);
	      hdfSetVar(hdfvars, hdfvarid, TRUE);
	    }
	  else if ( strcmp(attname, "bounds") == 0 )
	    {
	      hdf_inq_attlen(fileID, hdfvarid, attname, &attlen);
	      hdf_get_att_text(fileID, hdfvarid, attname, atttext);
	      atttext[attlen] = 0;

	      hdf_inq_varid(fileID, atttext, &hdfvars[hdfvarid].bounds);
	      hdfSetVar(hdfvars, hdfvars[hdfvarid].bounds, FALSE);

	      hdfSetVar(hdfvars, hdfvarid, FALSE);
	    }
	  else if ( strcmp(attname, "cell_measures") == 0 )
	    {
	      char *pstring, *cell_measures = NULL, *cell_var = NULL;

	      hdf_inq_attlen(fileID, hdfvarid, attname, &attlen);
	      hdf_get_att_text(fileID, hdfvarid, attname, atttext);
	      atttext[attlen] = 0;
	      pstring = atttext;

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
		  hdf_inq_varid(fileID, cell_var, &hdfvars[hdfvarid].cellarea);
		  hdfSetVar(hdfvars, hdfvars[hdfvarid].cellarea, FALSE);
		}
	      else
		{
		  Warning(func, "%s has unexpected contents: %s", attname, cell_measures);
		}
	      hdfSetVar(hdfvars, hdfvarid, TRUE);
	    }
	  else if ( strcmp(attname, "coordinates") == 0 )
	    {
	      char *pstring, *xvarname = NULL, *yvarname = NULL;
	      
	      hdf_inq_attlen(fileID, hdfvarid, attname, &attlen);
	      hdf_get_att_text(fileID, hdfvarid, attname, atttext);
	      atttext[attlen] = 0;
	      pstring = atttext;

	      while ( isspace((int) *pstring) ) pstring++;
	      xvarname = pstring;
	      while ( isalnum((int) *pstring) ) pstring++;
	      *pstring++ = 0;
	      while ( isspace((int) *pstring) ) pstring++;
	      yvarname = pstring;
	      while ( isalnum((int) *pstring) ) pstring++;
	      *pstring++ = 0;
	      /*
	      printf("xvarname >%s<\n", xvarname);
	      printf("yvarname >%s<\n", yvarname);
	      */
	      hdf_inq_varid(fileID, xvarname, &hdfvars[hdfvarid].xvarid);
	      hdf_inq_varid(fileID, yvarname, &hdfvars[hdfvarid].yvarid);

	      hdfSetVar(hdfvars, hdfvars[hdfvarid].xvarid, FALSE);
	      hdfSetVar(hdfvars, hdfvars[hdfvarid].yvarid, FALSE);
	      hdfSetVar(hdfvars, hdfvarid, TRUE);
	    }
	  else if ( strcmp(attname, "_FillValue") == 0 )
	    {
	      hdf_get_att_double(fileID, hdfvarid, "_FillValue", &hdfvars[hdfvarid].missval);
	      hdfvars[hdfvarid].defmiss = TRUE;
	      /* hdfSetVar(hdfvars, hdfvarid, TRUE); */
	    }
	  else if ( strcmp(attname, "axis") == 0 )
	    {
	      hdf_inq_attlen(fileID, hdfvarid, attname, &attlen);
	      hdf_get_att_text(fileID, hdfvarid, attname, atttext);
	      atttext[attlen] = 0;
	      if ( attlen != (size_t) nvdims )
		Warning(func, "axis attribute length %d does not match nvdim %d", attlen, nvdims);
	      else
		{
		  while ( attlen-- )
		    {
		      if ( atttext[attlen] == 't' )
			{
			  if ( attlen != 0 )
			    Warning(func, "axis attribute 't' not on first position");
			  hdfSetDim(hdfvars, hdfvarid, attlen, DIMTYPE_T);
			}
		      else if ( atttext[attlen] == 'z' )
			{
			  hdfvars[hdfvarid].zdim = dimidsp[attlen];
			  hdfSetDim(hdfvars, hdfvarid, attlen, DIMTYPE_Z);
			}
		      else if ( atttext[attlen] == 'y' )
			{
			  hdfvars[hdfvarid].ydim = dimidsp[attlen];
			  hdfSetDim(hdfvars, hdfvarid, attlen, DIMTYPE_GY);
			}
		      else if ( atttext[attlen] == 'x' )
			{
			  hdfvars[hdfvarid].xdim = dimidsp[attlen];
			  hdfSetDim(hdfvars, hdfvarid, attlen, DIMTYPE_GX);
			}
		    }
		}
	    }
	}
    }

  printHDFvars(hdfvars, nvars);

  printf(">>>baustelle<<<\n");
  exit(EXIT_FAILURE);
#if defined (XYZ)
  /* find coordinate vars */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
	{
	  if ( hdfvars[hdfvarid].ndims == 1 && hdfvars[hdfvarid].isvar != TRUE )
	    {
	      hdfSetVar(hdfvars, hdfvarid, FALSE);
	      if ( ncdimid == hdfvars[hdfvarid].dimids[0] && ncdims[ncdimid].hdfvarid == UNDEFID )
		if ( strcmp(hdfvars[hdfvarid].name, ncdims[ncdimid].name) == 0 )
		  {
		    ncdims[ncdimid].hdfvarid = hdfvarid;
		  }
	    }
	}
    }

  /* find time var */
  if ( timedimid != UNDEFID )
    {
      if ( ncdims[timedimid].hdfvarid != UNDEFID )
	streams[streamID].basetime.hdfvarid = ncdims[timedimid].hdfvarid;
      else
	{
	  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
	    if ( hdfvars[hdfvarid].ndims == 1 && timedimid == hdfvars[hdfvarid].dimids[0] )
	      {
		streams[streamID].basetime.hdfvarid = hdfvarid;
		break;
	      }
	}
      if ( streams[streamID].basetime.hdfvarid == UNDEFID )
	Warning(func, "time variable not found!");
      else
	{
	  if ( hdfvars[streams[streamID].basetime.hdfvarid].units[0] != 0 )
	    timehasunit = TRUE;
	}
    }

  /* check hdfvars */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      if ( timedimid != UNDEFID )
	if ( hdfvars[hdfvarid].ndims > 1 && timedimid == hdfvars[hdfvarid].dimids[0] )
	  hdfSetVar(hdfvars, hdfvarid, TRUE);

      if ( hdfvars[hdfvarid].isvar == -1 && hdfvars[hdfvarid].ndims == 0 )
	hdfSetVar(hdfvars, hdfvarid, FALSE);

      if ( hdfvars[hdfvarid].isvar == -1 && hdfvars[hdfvarid].ndims > 1 )
	hdfSetVar(hdfvars, hdfvarid, TRUE);
	  
      if ( hdfvars[hdfvarid].isvar == -1 )
	Warning(func, "variable %s has unknown type, skipped!", hdfvars[hdfvarid].name);
      if ( hdfvars[hdfvarid].ndims > 4 )
	Warning(func, "%d dimension variables unsupported. Skip variable %s",
		hdfvars[hdfvarid].ndims, hdfvars[hdfvarid].name);

      if ( hdfvars[hdfvarid].xtype == NC_CHAR )
	hdfvars[hdfvarid].isvar = -1;
    }

  /* verify coordinate vars */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      hdfvarid = ncdims[ncdimid].hdfvarid;
      if ( hdfvarid != -1 )
	{
	  if ( hdfvars[hdfvarid].dimids[0] == timedimid )
	    {
	      ncdims[ncdimid].dimtype = DIMTYPE_T;
	      continue;
	    }

	  if ( hdfvars[hdfvarid].longname[0] != 0 )
	    {
	      if ( strcmp(hdfvars[hdfvarid].longname, "Longitude") == 0 ||
		   strcmp(hdfvars[hdfvarid].longname, "longitude") == 0 )
		{
		  hdfvars[hdfvarid].islon = TRUE;
		  hdfSetVar(hdfvars, hdfvarid, FALSE);
		  hdfSetDim(hdfvars, hdfvarid, 0, DIMTYPE_GX);
		  ncdims[ncdimid].dimtype = DIMTYPE_GX;
		  continue;
		}
	      else if ( strcmp(hdfvars[hdfvarid].longname, "Latitude") == 0 || 
			strcmp(hdfvars[hdfvarid].longname, "latitude") == 0 )
		{
		  hdfvars[hdfvarid].islat = TRUE;
		  hdfSetVar(hdfvars, hdfvarid, FALSE);
		  hdfSetDim(hdfvars, hdfvarid, 0, DIMTYPE_GY);
		  ncdims[ncdimid].dimtype = DIMTYPE_GY;
		  continue;
		}
	    }

	  if ( hdfvars[hdfvarid].units[0] != 0 )
	    {
	      if ( strcmp(hdfvars[hdfvarid].units, "degrees_east") == 0 ||
		   strcmp(hdfvars[hdfvarid].units, "degree_E")     == 0 ||
		   strcmp(hdfvars[hdfvarid].units, "degrees_E")    == 0 ||
		   strcmp(hdfvars[hdfvarid].units, "degreeE")      == 0 ||
		   strcmp(hdfvars[hdfvarid].units, "degreesE")     == 0 )
		{
		  hdfvars[hdfvarid].islon = TRUE;
		  hdfSetVar(hdfvars, hdfvarid, FALSE);
		  hdfSetDim(hdfvars, hdfvarid, 0, DIMTYPE_GX);
		  ncdims[ncdimid].dimtype = DIMTYPE_GX;
		}
	      else if ( strcmp(hdfvars[hdfvarid].units, "degrees_north") == 0 ||
			strcmp(hdfvars[hdfvarid].units, "degree_N")      == 0 ||
			strcmp(hdfvars[hdfvarid].units, "degrees_N")     == 0 ||
			strcmp(hdfvars[hdfvarid].units, "degreeN")       == 0 ||
			strcmp(hdfvars[hdfvarid].units, "degreesN")      == 0 )
		{
		  hdfvars[hdfvarid].islat = TRUE;
		  hdfSetVar(hdfvars, hdfvarid, FALSE);
		  hdfSetDim(hdfvars, hdfvarid, 0, DIMTYPE_GY);
		  ncdims[ncdimid].dimtype = DIMTYPE_GY;
		}
	      else if ( strcmp(hdfvars[hdfvarid].units, "millibar") == 0 ||
			strcmp(hdfvars[hdfvarid].units, "hPa")      == 0 )
		{
		  hdfvars[hdfvarid].zaxistype = ZAXIS_PRESSURE;
		  hdfvars[hdfvarid].levelscale = 100;
		  /*  for ( ilev = 0; ilev < nlevs; ilev++ ) dlevels[ilev] *= 100; */
		}
	      else if ( strcmp(hdfvars[hdfvarid].units, "Pa") == 0 )
		{
		  hdfvars[hdfvarid].zaxistype = ZAXIS_PRESSURE;
		  hdfvars[hdfvarid].levelscale = 1;
		}
	      else if ( strcmp(hdfvars[hdfvarid].units, "level") == 0)
		{
		  if      ( strcmp(hdfvars[hdfvarid].longname, "hybrid level at layer midpoints") == 0 )
		    hdfvars[hdfvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strcmp(hdfvars[hdfvarid].longname, "hybrid level at layer interfaces") == 0 )
		    hdfvars[hdfvarid].zaxistype = ZAXIS_HYBRID_HALF;
		}
	      else if ( strcmp(hdfvars[hdfvarid].units, "cm")  == 0 )
		{
		  if ( strcmp(hdfvars[hdfvarid].longname, "depth below land")         == 0 ||
		       strcmp(hdfvars[hdfvarid].longname, "levels below the surface") == 0 )
		    hdfvars[hdfvarid].zaxistype = ZAXIS_DEPTH_BELOW_LAND;
		}
	      else if ( strcmp(hdfvars[hdfvarid].units, "m")   == 0 )
		{
		  if ( strcmp(hdfvars[hdfvarid].longname, "depth below sea") == 0 )
		    hdfvars[hdfvarid].zaxistype = ZAXIS_DEPTH_BELOW_SEA;
		  else if ( strcmp(hdfvars[hdfvarid].longname, "height") == 0 )
		    hdfvars[hdfvarid].zaxistype = ZAXIS_HEIGHT;
		}
	    }

	  if ( hdfvars[hdfvarid].zaxistype != UNDEFID )
	    {
	      hdfSetVar(hdfvars, hdfvarid, FALSE);
	      hdfSetDim(hdfvars, hdfvarid, 0, DIMTYPE_Z);
	      ncdims[ncdimid].dimtype = DIMTYPE_Z;
	    }
	}
    }
  /*
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      hdfvarid = ncdims[ncdimid].hdfvarid;
      if ( hdfvarid != -1 )
	{
	  printf("coord var %d %s %s\n", hdfvarid, hdfvars[hdfvarid].name, hdfvars[hdfvarid].units);
	  if ( ncdims[ncdimid].dimtype == DIMTYPE_GX )
	    printf("coord var %d %s is x dim\n", hdfvarid, hdfvars[hdfvarid].name);
	  if ( ncdims[ncdimid].dimtype == DIMTYPE_GY )
	    printf("coord var %d %s is y dim\n", hdfvarid, hdfvars[hdfvarid].name);
	  if ( ncdims[ncdimid].dimtype == DIMTYPE_Z )
	    printf("coord var %d %s is z dim\n", hdfvarid, hdfvars[hdfvarid].name);
	  if ( ncdims[ncdimid].dimtype == DIMTYPE_T )
	    printf("coord var %d %s is t dim\n", hdfvarid, hdfvars[hdfvarid].name);

	  if ( hdfvars[hdfvarid].islon )
	    printf("coord var %d %s is lon\n", hdfvarid, hdfvars[hdfvarid].name);
	  if ( hdfvars[hdfvarid].islat )
	    printf("coord var %d %s is lat\n", hdfvarid, hdfvars[hdfvarid].name);
	  if ( hdfvars[hdfvarid].islev )
	    printf("coord var %d %s is lev\n", hdfvarid, hdfvars[hdfvarid].name);
	}
    }
  */
  /* set dim type */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      if ( hdfvars[hdfvarid].isvar == TRUE )
	{
	  int lxdim = 0, lydim = 0, lzdim = 0, ltdim = 0;
	  ndims = hdfvars[hdfvarid].ndims;
	  for ( i = 0; i < ndims; i++ )
	    {
	      ncdimid = hdfvars[hdfvarid].dimids[i];
	      if ( ncdims[ncdimid].dimtype == DIMTYPE_GX )
		hdfSetDim(hdfvars, hdfvarid, i, DIMTYPE_GX);
	      else if ( ncdims[ncdimid].dimtype == DIMTYPE_GY )
		hdfSetDim(hdfvars, hdfvarid, i, DIMTYPE_GY);
	      else if ( ncdims[ncdimid].dimtype == DIMTYPE_Z )
		hdfSetDim(hdfvars, hdfvarid, i, DIMTYPE_Z);
	      else if ( ncdims[ncdimid].dimtype == DIMTYPE_T )
		hdfSetDim(hdfvars, hdfvarid, i, DIMTYPE_T);
	    }

	  if ( CDI_Debug )
	    {
	      Message(func, "var %d %s", hdfvarid, hdfvars[hdfvarid].name);
	      for ( i = 0; i < ndims; i++ )
		printf("  dim %d type %d  ", i, hdfvars[hdfvarid].dimtype[i]);
	      printf("\n");
	    }

	  for ( i = 0; i < ndims; i++ )
	    {
	      if ( hdfvars[hdfvarid].dimtype[i] == DIMTYPE_GX )
		{
		  lxdim = TRUE;
		}
	      else if ( hdfvars[hdfvarid].dimtype[i] == DIMTYPE_GY )
		{
		  lydim = TRUE;
		}
	      else if ( hdfvars[hdfvarid].dimtype[i] == DIMTYPE_Z )
		{
		  lzdim = TRUE;
		}
	      else if ( hdfvars[hdfvarid].dimtype[i] == DIMTYPE_T )
		{
		  ltdim = TRUE;
		}
	    }

	  for ( i = ndims-1; i >= 0; i-- )
	    {
	      if ( hdfvars[hdfvarid].dimtype[i] == -1 )
		{
		  /*
		  printf("undef dim: %d %d %d %d %d\n", i, lxdim, lydim, lzdim, ltdim);
		  */
		  if ( lxdim == FALSE )
		    {
		      hdfSetDim(hdfvars, hdfvarid, i, DIMTYPE_GX);
		      lxdim = TRUE;
		    }
		  else if ( lydim == FALSE )
		    {
		      hdfSetDim(hdfvars, hdfvarid, i, DIMTYPE_GY);
		      lydim = TRUE;
		    }
		  else if ( lzdim == FALSE )
		    {
		      hdfSetDim(hdfvars, hdfvarid, i, DIMTYPE_Z);
		      lzdim = TRUE;
		    }
		}
	    }	    	  
	}
    }

  /* define grid */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      if ( hdfvars[hdfvarid].isvar == TRUE && hdfvars[hdfvarid].gridID == UNDEFID )
	{
	  int xdimid = -1, ydimid = -1;
	  int xvarid = -1, yvarid = -1;
	  int islon = 0, islat = 0;
	  double *xvar = NULL, *yvar = NULL;

	  xsize = 0;
	  ysize = 0;

	  ndims = hdfvars[hdfvarid].ndims;
	  for ( i = 0; i < ndims; i++ )
	    {
	      if ( hdfvars[hdfvarid].dimtype[i] == DIMTYPE_GX )
		xdimid = hdfvars[hdfvarid].dimids[i];
	      else if ( hdfvars[hdfvarid].dimtype[i] == DIMTYPE_GY )
		ydimid = hdfvars[hdfvarid].dimids[i];
	    }
	  
	  /*
	  if ( hdfvars[hdfvarid].xvarid != UNDEFID )
	    xvarid = hdfvars[hdfvarid].xvarid;
	  else if ( xdimid != UNDEFID )
	    xvarid = ncdims[xdimid].hdfvarid;

	  if ( hdfvars[hdfvarid].yvarid != UNDEFID )
	    yvarid = hdfvars[hdfvarid].yvarid;
	  else if ( ydimid != UNDEFID )
	    yvarid = ncdims[ydimid].hdfvarid;
	  */
	  
	  if ( xdimid != UNDEFID )
	    xvarid = ncdims[xdimid].hdfvarid;
	  if ( xvarid == UNDEFID && hdfvars[hdfvarid].xvarid != UNDEFID )
	    xvarid = hdfvars[hdfvarid].xvarid;

	  if ( ydimid != UNDEFID )
	    yvarid = ncdims[ydimid].hdfvarid;
	  if ( yvarid == UNDEFID && hdfvars[hdfvarid].yvarid != UNDEFID )
	    yvarid = hdfvars[hdfvarid].yvarid;


	  if ( xdimid != UNDEFID )
	    hdf_inq_dimlen(fileID, xdimid, &xsize);

	  if ( ydimid != UNDEFID )
	    hdf_inq_dimlen(fileID, ydimid, &ysize);

	  memset(&grid, 0, sizeof(GRID));

	  grid.trunc = hdfvars[hdfvarid].truncation;
	  
	  if ( hdfvars[hdfvarid].gridtype == GRID_TRAJECTORY )
	    {
	      if ( hdfvars[hdfvarid].xvarid == UNDEFID )
		Error(func, "Longitude coordinate undefined for %s!", name);
	      if ( hdfvars[hdfvarid].yvarid == UNDEFID )
		Error(func, "Latitude coordinate undefined for %s!", name);
	    }
	  else
	    {
	      if ( xvarid != UNDEFID )
		{
		  islon = hdfvars[xvarid].islon;
		  xvar = (double *) malloc(xsize*sizeof(double));
		  hdf_get_var_double(fileID, xvarid, xvar);
		}
	      if ( yvarid != UNDEFID )
		{
		  islat = hdfvars[yvarid].islat;
		  if ( ysize == 0 ) size = xsize;
		  else              size = ysize;
		  yvar = (double *) malloc(size*sizeof(double));
		  hdf_get_var_double(fileID, yvarid, yvar);
		  latinc = 0.0;
		  if ( ysize > 2 )
		    {
		      latinc = fabs(yvar[0] - yvar[1]);
		      for ( i = 2; i < ysize; i++ )
			if ( (fabs(yvar[i-1] - yvar[i]) - latinc) > 0.0 ) break;
		  
		      if ( i < ysize ) latinc = 0.0;
		    }
		}

	      if ( ysize == 0 ) size = xsize;
	      else              size = xsize*ysize;
	    }

	  if ( hdfvars[hdfvarid].gridtype == UNDEFID )
	    {
	      if ( islat && islon && latinc > 0.0 )
		hdfvars[hdfvarid].gridtype = GRID_LONLAT;
	      else
		hdfvars[hdfvarid].gridtype = GRID_GENERIC;
	    }

	  grid.xvals   = NULL;
	  grid.yvals   = NULL;
	  grid.xbounds = NULL;
	  grid.ybounds = NULL;
	  grid.area    = NULL;

	  switch (hdfvars[hdfvarid].gridtype)
	    {
	    case GRID_GENERIC:
	    case GRID_LONLAT:
	    case GRID_GAUSSIAN:
	    case GRID_CELL:
	      {
		grid.size  = size;
		grid.xsize = xsize;
		grid.ysize = ysize;
		if ( xvarid != UNDEFID )
		  {
		    grid.xdef  = TRUE;
		    grid.xvals = xvar;
		    if ( hdfvars[xvarid].bounds != UNDEFID )
		      {
			if ( hdfvars[hdfvars[xvarid].bounds].ndims == 2 )
			  {
			    size_t nvertex;
			    hdf_inq_dimlen(fileID, hdfvars[hdfvars[xvarid].bounds].dimids[1], &nvertex);
			    grid.nvertex = (int) nvertex;
			    grid.xbounds = (double *) malloc(nvertex*size*sizeof(double));
			    hdf_get_var_double(fileID, hdfvars[xvarid].bounds, grid.xbounds);
			  }
		      }
		  }
		if ( yvarid != UNDEFID )
		  {
		    grid.ydef  = TRUE;
		    grid.yvals = yvar;
		    if ( hdfvars[yvarid].bounds != UNDEFID )
		      {
			if ( hdfvars[hdfvars[yvarid].bounds].ndims == 2 )
			  {
			    size_t nvertex;
			    hdf_inq_dimlen(fileID, hdfvars[hdfvars[yvarid].bounds].dimids[1], &nvertex);
			    if ( nvertex != grid.nvertex )
			      Warning(func, "nvertex problem! nvertex x %d, nvertex y %d",
				      grid.nvertex, (int) nvertex);
			    grid.ybounds = (double *) malloc(nvertex*size*sizeof(double));
			    hdf_get_var_double(fileID, hdfvars[yvarid].bounds, grid.ybounds);
			  }
		      }
		  }

		if ( hdfvars[hdfvarid].cellarea != UNDEFID )
		  {
		    grid.area = (double *) malloc(size*sizeof(double));
		    hdf_get_var_double(fileID, hdfvars[hdfvarid].cellarea, grid.area);
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

	  grid.type = hdfvars[hdfvarid].gridtype;

	  hdfvars[hdfvarid].gridID = varDefGrid(vlistID, grid, 0);

	  gridindex = vlistGridIndex(vlistID, hdfvars[hdfvarid].gridID);
	  streams[streamID].xdimID[gridindex] = xdimid;
	  streams[streamID].ydimID[gridindex] = ydimid;

	  if ( xvar ) free(xvar);
	  if ( yvar ) free(yvar);
	  if ( grid.xbounds ) free(grid.xbounds);
	  if ( grid.ybounds ) free(grid.ybounds);
	  if ( grid.area )    free(grid.area);

	  if ( CDI_Debug )
	    Message(func, "gridID %d %d %s",
		    hdfvars[hdfvarid].gridID, hdfvarid, hdfvars[hdfvarid].name);

	  for ( hdfvarid2 = hdfvarid+1; hdfvarid2 < nvars; hdfvarid2++ )
	    if ( hdfvars[hdfvarid2].isvar == TRUE && hdfvars[hdfvarid2].gridID == UNDEFID )
	      {
		int xdimid2 = -1, ydimid2 = -1;
		ndims = hdfvars[hdfvarid2].ndims;
		for ( i = 0; i < ndims; i++ )
		  {
		    if ( hdfvars[hdfvarid2].dimtype[i] == DIMTYPE_GX )
		      xdimid2 = hdfvars[hdfvarid2].dimids[i];
		    else if ( hdfvars[hdfvarid2].dimtype[i] == DIMTYPE_GY )
		      ydimid2 = hdfvars[hdfvarid2].dimids[i];
		  }
		if ( xdimid == xdimid2 && ydimid == ydimid2 )
		  {
		    if ( CDI_Debug )
		      Message(func, "gridID %d %d %s",
			      hdfvars[hdfvarid].gridID, hdfvarid2, hdfvars[hdfvarid2].name);
		    hdfvars[hdfvarid2].gridID = hdfvars[hdfvarid].gridID;
		  }
	      }
	}
    }

  /* find VCT */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      if ( hdfvars[hdfvarid].ndims == 1 )
	{
	  if ( strncmp(hdfvars[hdfvarid].name, "hyai", 4) == 0 )
	    {
	      vcta_id = hdfvarid;
	      nvcth_id = hdfvars[hdfvarid].dimids[0];
	      continue;
	    }
	  if ( strncmp(hdfvars[hdfvarid].name, "hybi", 4) == 0 )
	    {
	      vctb_id = hdfvarid;
	      nvcth_id = hdfvars[hdfvarid].dimids[0];
	      continue;
	    }
	}
    }

  /* read VCT */
  if ( nvcth_id != UNDEFID && vcta_id != UNDEFID && vctb_id != UNDEFID )
    {
      hdf_inq_dimlen(fileID, nvcth_id, &vctsize);
      vctsize *= 2;
      vct = (double *) malloc(vctsize*sizeof(double));
      hdf_get_var_double(fileID, vcta_id, vct);
      hdf_get_var_double(fileID, vctb_id, vct+vctsize/2);
    }

  /* define zaxis */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      if ( hdfvars[hdfvarid].isvar == TRUE && hdfvars[hdfvarid].zaxisID == UNDEFID )
	{
	  int zdimid = -1;
	  int zvarid = -1;
	  size_t zsize = 1;
	  double *zvar = NULL;
	  int zaxisType;

	  ndims = hdfvars[hdfvarid].ndims;
	  for ( i = 0; i < ndims; i++ )
	    {
	      if ( hdfvars[hdfvarid].dimtype[i] == DIMTYPE_Z )
		zdimid = hdfvars[hdfvarid].dimids[i];
	    }

	  if ( zdimid != UNDEFID )
	    zvarid = ncdims[zdimid].hdfvarid;

	  if ( zdimid != UNDEFID )
	    hdf_inq_dimlen(fileID, zdimid, &zsize);

	  if ( CDI_Debug ) Message(func, "nlevs = %ld", zsize);
    
	  zvar = (double *) malloc(zsize*sizeof(double));

	  zaxisType = UNDEFID;

	  if ( zvarid != UNDEFID ) zaxisType = hdfvars[zvarid].zaxistype;
	  
	  if ( zaxisType == UNDEFID )  zaxisType = ZAXIS_GENERIC;

	  if ( zvarid != UNDEFID )
	    {
	      hdf_get_var_double(fileID, zvarid, zvar);
	      if ( zaxisType == ZAXIS_PRESSURE )
		for ( ilev = 0; ilev < zsize; ilev++ ) zvar[ilev] *= hdfvars[zvarid].levelscale;
	    }
	  else
	    {
	      if ( zsize == 1 )
		{
		  zaxisType = ZAXIS_SURFACE;
		  zvar[0] = 0;
		}
	      else
		{
		  for ( ilev = 0; ilev < zsize; ilev++ ) zvar[ilev] = ilev + 1;
		}
	    }

      	  hdfvars[hdfvarid].zaxisID = varDefZaxis(streamID, zaxisType, (int) zsize, zvar, 0, NULL, NULL,
						  vctsize, vct, NULL, NULL, NULL, 0, 1);
	  free(zvar);

	  zaxisindex = vlistZaxisIndex(vlistID, hdfvars[hdfvarid].zaxisID);
	  streams[streamID].zaxisID[zaxisindex]  = zdimid;

	  if ( CDI_Debug )
	    Message(func, "zaxisID %d %d %s",
		    hdfvars[hdfvarid].zaxisID, hdfvarid, hdfvars[hdfvarid].name);

	  for ( hdfvarid2 = hdfvarid+1; hdfvarid2 < nvars; hdfvarid2++ )
	    if ( hdfvars[hdfvarid2].isvar == TRUE && hdfvars[hdfvarid2].zaxisID == UNDEFID )
	      {
		int zdimid2 = -1;
		ndims = hdfvars[hdfvarid2].ndims;
		for ( i = 0; i < ndims; i++ )
		  {
		    if ( hdfvars[hdfvarid2].dimtype[i] == DIMTYPE_Z )
		      zdimid2 = hdfvars[hdfvarid2].dimids[i];
		  }
		if ( zdimid == zdimid2 )
		  {
		    if ( CDI_Debug )
		      Message(func, "zaxisID %d %d %s",
			      hdfvars[hdfvarid].zaxisID, hdfvarid2, hdfvars[hdfvarid2].name);
		    hdfvars[hdfvarid2].zaxisID = hdfvars[hdfvarid].zaxisID;
		  }
	      }
	}
    }

  if ( vct ) free(vct);

  /* select vars */
  for ( hdfvarid = 0; hdfvarid < nvars; hdfvarid++ )
    {
      if ( hdfvars[hdfvarid].isvar == TRUE ) varids[nvarids++] = hdfvarid;
    }
  nc_nvars = nvars;

  if ( CDI_Debug )
    Message(func, "time varid = %d", streams[streamID].basetime.hdfvarid);

  if ( CDI_Debug )
    Message(func, "ntsteps = %d", streams[streamID].ntsteps);

  nvars = nvarids;
  if ( CDI_Debug )
    Message(func, "nvars = %d", nvars);

  for ( varID1 = 0; varID1 < nvars; varID1++ )
    {
      hdfvarid = varids[varID1];

      varID = streamNewVar(streamID, hdfvars[hdfvarid].gridID, hdfvars[hdfvarid].zaxisID);
      varID = vlistDefVar(vlistID, hdfvars[hdfvarid].gridID, hdfvars[hdfvarid].zaxisID, hdfvars[hdfvarid].timeID);

      streams[streamID].vars[varID1].level   = NULL;
      streams[streamID].vars[varID1].defmiss = 0;
      streams[streamID].vars[varID1].nlevs   = zaxisInqSize(hdfvars[hdfvarid].zaxisID);
      streams[streamID].vars[varID1].hdfvarid = hdfvarid;

      vlistDefVarName(vlistID, varID, hdfvars[hdfvarid].name);
      if ( hdfvars[hdfvarid].code != UNDEFID )  vlistDefVarCode(vlistID, varID, hdfvars[hdfvarid].code);
      if ( hdfvars[hdfvarid].longname[0] )      vlistDefVarLongname(vlistID, varID, hdfvars[hdfvarid].longname);
      if ( hdfvars[hdfvarid].units[0] )         vlistDefVarUnits(vlistID, varID, hdfvars[hdfvarid].units);
      if ( hdfvars[hdfvarid].addoffset != 0 )   vlistDefVarAddoffset(vlistID, varID, hdfvars[hdfvarid].addoffset);
      if ( hdfvars[hdfvarid].scalefactor != 1 ) vlistDefVarScalefactor(vlistID, varID, hdfvars[hdfvarid].scalefactor);

      vlistDefVarDatatype(vlistID, varID, hdfInqDatatype(hdfvars[hdfvarid].xtype));

      vlistDefVarInstitut(vlistID, varID, instID);
      vlistDefVarModel(vlistID, varID, modelID);
      vlistDefVarTable(vlistID, varID, hdfvars[hdfvarid].tableID);

      if ( hdfvars[hdfvarid].defmiss == TRUE ) vlistDefVarMissval(vlistID, varID, hdfvars[hdfvarid].missval);

      if ( CDI_Debug )
	Message(func, "varID = %d  gridID = %d  zaxisID = %d", varID,
		vlistInqVarGrid(vlistID, varID), vlistInqVarZaxis(vlistID, varID));
    }

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
		streamFreeVarName(vlistID, varID);
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
		streamFreeVarName(vlistID, varID);
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
	      streamFreeVarName(vlistID, varID);
	      streamFreeVarLongname(vlistID, varID);
	      streamFreeVarUnits(vlistID, varID);

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

  if ( timehasunit )
    {
      TAXIS *taxis;
      taxis = &streams[streamID].tsteps[0].taxis;

      hdf_inq_attlen(fileID, streams[streamID].basetime.hdfvarid, "units", &attlen);
      hdf_get_att_text(fileID, streams[streamID].basetime.hdfvarid, "units", atttext);
      atttext[attlen] = 0;
      if ( splitBasetime(atttext, taxis) == 1 )
	streams[streamID].basetime.hdfvarid = UNDEFID;	
    }

  if ( streams[streamID].tsteps[0].taxis.type == TAXIS_RELATIVE )
    taxisID = taxisCreate(TAXIS_RELATIVE);
  else
    taxisID = taxisCreate(TAXIS_ABSOLUTE);

  vlistDefTaxis(vlistID, taxisID);

  streams[streamID].curTsID    = 0;
  streams[streamID].rtsteps = 1;

  (void) hdfInqTimestep(streamID, 0);

  hdfCreateRecords(streamID, 0);

  /* free ncdims */
  free (ncdims);
#endif /*XYZ*/

  /* free hdfvars */
  free (hdfvars);

#endif
}

#if defined (XYZ)

int hdfInqTimestep(int streamID, int tsID)
{
  static char func[] = "hdfInqTimestep";
  long nrecs = 0;
#if  defined  (HAVE_LIBHDF5)
  double timevalue;
  int nctimevarid;
  int fileID;
  size_t index;
  int vdate = 0, vtime = 0;

  if ( CDI_Debug )
    Message(func, "streamID = %d  tsID = %d", streamID, tsID);

  streamCheckID(func, streamID);
    
  if ( tsID < 0 )
    {
      Error(func, "unexpected tsID = %d", tsID);
    }
  else if ( tsID == 0 && streams[streamID].ntsteps == 0 )
    {
      streams[streamID].curTsID = tsID;
      nrecs = streams[streamID].tsteps[tsID].nrecords;
    }
  else if ( tsID < streams[streamID].ntsteps )
    {
      hdfCreateRecords(streamID, tsID);
      if ( tsID > 0 )
	ptaxisCopy(&streams[streamID].tsteps[tsID].taxis,
		     &streams[streamID].tsteps[0].taxis);

      timevalue = tsID;

      nctimevarid = streams[streamID].basetime.hdfvarid;
      if ( nctimevarid != UNDEFID )
	{
	  fileID = streamInqFileID(streamID);
	  index  = tsID;
	  hdf_get_var1_double(fileID, nctimevarid, &index, &timevalue);
	}

      if ( streams[streamID].tsteps[0].taxis.type == TAXIS_ABSOLUTE )
	{
	  int timeunit;
	  timeunit = streams[streamID].tsteps[0].taxis.unit;
	  splitTimevalue(timevalue, timeunit, &vdate, &vtime);
	}
      else
	timeval2vtime(timevalue, &streams[streamID].tsteps[tsID].taxis, &vdate, &vtime);

      streams[streamID].tsteps[tsID].taxis.vdate = vdate;
      streams[streamID].tsteps[tsID].taxis.vtime = vtime;

      streams[streamID].curTsID = tsID;
      nrecs = streams[streamID].tsteps[tsID].nrecords;
    }

#endif
  return ((int) nrecs);
}

void hdfEndDef(int streamID)
{
  int varID, hdfvarid;
  int nvars;

  hdfDefGlobalAtts(streamID);
  hdfDefLocalAtts(streamID);
  if ( streams[streamID].accessmode == 0 )
    {
      nvars =  streams[streamID].nvars;

      for ( varID = 0; varID < nvars; varID++ )
	{
	  hdfvarid = hdfDefVar(streamID, varID);
	}
      streams[streamID].accessmode = 1;
    }
}

void hdfDefInstitut(int streamID)
{
#if  defined  (HAVE_LIBHDF5)
  int fileID, instID;
  char *longname;
  size_t len;

  fileID  = streamInqFileID(streamID);
  instID  = vlistInqInstitut(vlistID);

  if ( instID != UNDEFID )
    {
      longname = institutInqLongnamePtr(instID);
      if ( longname )
	{
	  len = strlen(longname);
	  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
	  hdf_put_att_text(fileID, HDF_GLOBAL, "institution", len, longname);
	  if ( streams[streamID].ncmode == 2 ) hdf_enddef(fileID);
	}
    }
#endif
}

void hdfDefSource(int streamID)
{
#if  defined  (HAVE_LIBHDF5)
  int fileID, modelID;
  char *longname;
  size_t len;

  fileID  = streamInqFileID(streamID);
  modelID = vlistInqModel(vlistID);

  if ( modelID != UNDEFID )
    {
      longname = modelInqNamePtr(modelID);
      if ( longname )
	{
	  len = strlen(longname);
	  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);
	  hdf_put_att_text(fileID, HDF_GLOBAL, "source", len, longname);
	  if ( streams[streamID].ncmode == 2 ) hdf_enddef(fileID);
	}
    }
#endif
}

void hdfDefGlobalAtts(int streamID)
{
  if ( streams[streamID].globalatts ) return;

  hdfDefSource(streamID);
  hdfDefInstitut(streamID);

  streams[streamID].globalatts = 1;
}

void hdfDefLocalAtts(int streamID)
{
#if  defined  (HAVE_LIBHDF5)
  int varID, instID, fileID;
  char *name;
  size_t len;
  int hdfvarid;

  fileID  = streamInqFileID(streamID);

  if ( streams[streamID].localatts ) return;
  if ( vlistInqInstitut(vlistID) != UNDEFID ) return;

  streams[streamID].localatts = 1;

  if ( streams[streamID].ncmode == 2 ) hdf_redef(fileID);

  for ( varID = 0; varID < streams[streamID].nvars; varID++ )
    {
      instID = vlistInqVarInstitut(vlistID, varID);
      if ( instID != UNDEFID )
	{
          hdfvarid = streams[streamID].vars[varID].hdfvarid;
  	  name = institutInqNamePtr(instID);
	  if ( name )
	    {
	      len = strlen(name);
	      hdf_put_att_text(fileID, hdfvarid, "institution", len, name);
	    }
	}
      }
  
  if ( streams[streamID].ncmode == 2 ) hdf_enddef(fileID);
#endif
}

void hdfDefHistory(int streamID, int size, char *history)
{
#if  defined  (HAVE_LIBHDF5)
  int ncid;

  ncid = streams[streamID].fileID;
  hdf_put_att_text(ncid, HDF_GLOBAL, "history", (size_t) size, history);
#endif
}

int hdfInqHistorySize(int streamID)
{
  size_t size = 0;
#if  defined  (HAVE_LIBHDF5)
  int ncid;

  ncid = streams[streamID].fileID;
  if ( streams[streamID].historyID != UNDEFID )
    hdf_inq_attlen(ncid, HDF_GLOBAL, "history", &size);

#endif
  return ((int) size);
}

void hdfInqHistoryString(int streamID, char *history)
{
#if  defined  (HAVE_LIBHDF5)
  int ncid;

  ncid = streams[streamID].fileID;
  if ( streams[streamID].historyID != UNDEFID )
    hdf_get_att_text(ncid, HDF_GLOBAL, "history", history);

#endif
}
#endif /*XYZ*/
