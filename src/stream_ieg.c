#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "dmemory.h"

#include "error.h"
#include "file.h"
#include "cdi.h"
#include "stream_int.h"
#include "varscan.h"
#include "datetime.h"
#include "ieg.h"
#include "vlist.h"


#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID

#define SINGLE_PRECISION  4
#define DOUBLE_PRECISION  8


#if defined (HAVE_LIBIEG)

int iegInqDatatype(int prec)
{
  int datatype;

  if ( prec == DOUBLE_PRECISION ) datatype = DATATYPE_REAL8;
  else                            datatype = DATATYPE_REAL4;

  return (datatype);
}


int iegDefDatatype(int datatype)
{
  int prec;

  if ( datatype != DATATYPE_REAL4 && datatype != DATATYPE_REAL8 )
    datatype = DATATYPE_REAL4;

  if ( datatype == DATATYPE_REAL8 ) prec = DOUBLE_PRECISION;
  else                              prec = SINGLE_PRECISION;

  return (prec);
}


int iegInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "iegInqRecord";
  int status;
  int fileID;
  int icode, ilevel;
  int zaxisID = -1;
  int vlistID;
  IEGREC *iegp;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  iegp    = streams[streamID].record->iegp;

  *varID   = -1;
  *levelID = -1;

  status = iegRead(fileID, iegp);
  if ( status != 0 ) return (0);

  icode  = IEG_P_Parameter(iegp->ipdb);
  if ( IEG_P_LevelType(iegp->ipdb) == IEG_LTYPE_HYBRID_LAYER )
    ilevel = IEG_P_Level1(iegp->ipdb);
  else
    ilevel = IEG_P_Level2(iegp->ipdb);

  *varID = vlistInqVarID(vlistID, icode);

  if ( *varID == UNDEFID ) Error(func, "code %d undefined", icode);

  zaxisID = vlistInqVarZaxis(vlistID, *varID);

  *levelID = zaxisInqLevelID(zaxisID, (double) ilevel);
  
  return (1);
}


int iegReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "iegReadRecord";
  int vlistID, fileID;
  int status;
  int recID, vrecID, tsID;
  off_t recpos;
  int varID, gridID;
  int i, size;
  double missval;
  IEGREC *iegp;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  tsID    = streams[streamID].curTsID;
  vrecID  = streams[streamID].tsteps[tsID].curRecID;
  recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
  recpos  = streams[streamID].tsteps[tsID].records[recID].position;
  varID   = streams[streamID].tsteps[tsID].records[recID].varID;
  iegp    = streams[streamID].record->iegp;

  fileSetPos(fileID, recpos, SEEK_SET);

  status = iegRead(fileID, iegp);
  if ( status != 0 ) return (0);

  iegInqDataDP(iegp, data);

  gridID  = vlistInqVarGrid(vlistID, varID);
  size    = gridInqSize(gridID);
  missval = vlistInqVarMissval(vlistID, varID);

  *nmiss = 0;
  for ( i = 0; i < size; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }

  return (1);
}


int iegGetZaxisType(int iegleveltype)
{
  int leveltype = 0;

  switch ( iegleveltype )
    {
    case IEG_LTYPE_SURFACE:
      {
	leveltype = ZAXIS_SURFACE;
	break;
      }
    case IEG_LTYPE_99:
    case IEG_LTYPE_ISOBARIC:
      {
	leveltype = ZAXIS_PRESSURE;
	break;
      }
    case IEG_LTYPE_HEIGHT:
      {
	leveltype = ZAXIS_HEIGHT;
	break;
      }
    case IEG_LTYPE_ALTITUDE:
      {
	leveltype = ZAXIS_ALTITUDE;
	break;
      }
    case IEG_LTYPE_HYBRID:
    case IEG_LTYPE_HYBRID_LAYER:
      {
	leveltype = ZAXIS_HYBRID;
	break;
      }
    case IEG_LTYPE_LANDDEPTH:
    case IEG_LTYPE_LANDDEPTH_LAYER:
      {
	leveltype = ZAXIS_DEPTH_BELOW_LAND;
	break;
      }
    case IEG_LTYPE_SEADEPTH:
      {
	leveltype = ZAXIS_DEPTH_BELOW_SEA;
	break;
      }
    default:
      {
	leveltype = ZAXIS_GENERIC;
	break;
      }
    }

  return (leveltype);
}


void iegDefTime(int *pdb, int date, int time, int timeID)
{
  int year, month, day, hour, minute;
  int timetype = -1;

  if ( timeID != -1 ) timetype = taxisInqType(timeID);

  if ( timetype == TAXIS_ABSOLUTE || timetype == TAXIS_RELATIVE )
    {
      year    =  date / 10000;
      month   = (date - year*10000) / 100;
      day     =  date - year*10000 - month*100;
      hour    =  time / 100;
      minute  =  time - hour*100;

      IEG_P_Year(pdb)     = year;
      IEG_P_Month(pdb)    = month;
      IEG_P_Day(pdb)      = day;
      IEG_P_Hour(pdb)     = hour;
      IEG_P_Minute(pdb)   = minute;

      pdb[15] = 1;
      pdb[16] = 0;
      pdb[17] = 0;
      pdb[18] = 10;
      pdb[36] = 1;
    }

  pdb[5] = 128;
}


void iegDefGrid(int *gdb, int gridID)
{
  static char func[] = "iegDefGrid";
  int gridtype;

  gridtype = gridInqType(gridID);

  if ( gridtype == GRID_GENERIC )
    {
      gridtype = GRID_LONLAT;
      gridChangeType(gridID, gridtype);
    }
  else if ( gridtype == GRID_CURVILINEAR )
    {
      gridtype = GRID_LONLAT;
    }

  if ( gridtype == GRID_LONLAT )
    {
      int nlon, nlat;

      nlon = (int) gridInqXsize(gridID);

      nlat = (int) gridInqYsize(gridID);

      if ( gridtype == GRID_LONLAT && gridIsRotated(gridID) )
	IEG_G_GridType(gdb) = 10;
      else
	IEG_G_GridType(gdb) = 0;

      IEG_G_NumLon(gdb)   = nlon;
      IEG_G_NumLat(gdb)   = nlat;
      IEG_G_FirstLat(gdb) = NINT(gridInqYval(gridID,      0)*1000);
      IEG_G_LastLat(gdb)  = NINT(gridInqYval(gridID, nlat-1)*1000);
      IEG_G_FirstLon(gdb) = NINT(gridInqXval(gridID,      0)*1000);
      IEG_G_LastLon(gdb)  = NINT(gridInqXval(gridID, nlon-1)*1000);
      IEG_G_LonIncr(gdb)  = NINT(gridInqXinc(gridID)*1000);
      if ( fabs(gridInqXinc(gridID)*1000 - IEG_G_LonIncr(gdb)) > FLT_EPSILON )
	IEG_G_LonIncr(gdb) = 0;

      IEG_G_LatIncr(gdb) = NINT(gridInqYinc(gridID)*1000);
      if ( fabs(gridInqYinc(gridID)*1000 - IEG_G_LatIncr(gdb)) > FLT_EPSILON )
	IEG_G_LatIncr(gdb) = 0;

      if ( gridIsRotated(gridID) )
	{
	  IEG_G_LatSP(gdb) = - NINT(gridInqYpole(gridID) * 1000);
	  IEG_G_LonSP(gdb) =   NINT((gridInqXpole(gridID) + 180) * 1000);
	  IEG_G_Size(gdb)  = 42;
	}
      else
	{
	  IEG_G_Size(gdb)  = 32;
	}
    }
  else
    {
      Error(func, "%s grid unsupported!", gridNamePtr(gridtype));
    }

  IEG_G_ResFlag(gdb) = 128;
  IEG_G_ScanFlag(gdb) = 64;
}


void iegDefLevel(int *pdb, int *gdb, double *vct, int zaxisID, int levelID)
{
  static char func[] = "iegDefLevel";
  double level;
  int ilevel, leveltype;
  static int warning = 1;
  static int vct_warning = 1;

  leveltype = zaxisInqType(zaxisID);

  if ( leveltype == ZAXIS_GENERIC )
    {
      leveltype = ZAXIS_PRESSURE;
      zaxisChangeType(zaxisID, leveltype);
    }

  /*  IEG_G_NumVCP(gdb) = 0; */

  switch (leveltype)
    {
    case ZAXIS_SURFACE:
      {
	IEG_P_LevelType(pdb) = IEG_LTYPE_SURFACE;
	IEG_P_Level1(pdb)    = 0;
	IEG_P_Level2(pdb)    = (int) zaxisInqLevel(zaxisID, levelID);
	break;
      }
    case ZAXIS_HYBRID:
      {
	int vctsize;

	if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
	  {
	    IEG_P_LevelType(pdb) = IEG_LTYPE_HYBRID_LAYER;
	    IEG_P_Level1(pdb)    = (int) zaxisInqLbound(zaxisID, levelID);
	    IEG_P_Level2(pdb)    = (int) zaxisInqUbound(zaxisID, levelID);
	  }
	else
	  {
	    IEG_P_LevelType(pdb) = IEG_LTYPE_HYBRID;
	    IEG_P_Level1(pdb)    = 0;
	    IEG_P_Level2(pdb)    = (int) zaxisInqLevel(zaxisID, levelID);
	  }

	vctsize = zaxisInqVctSize(zaxisID);
	if ( vctsize == 0 && warning )
	  {
	    Warning(func, "VCT missing. ( code = %d, zaxisID = %d )",
		    IEG_P_Parameter(pdb), zaxisID);
	    warning = 0;
	  }
	if ( vctsize > 100 )
	  {
	    /*	    IEG_G_NumVCP(gdb) = 0; */
	    if ( vct_warning )
	      {
		Warning(func, "VCT size of %d is to large (maximum is 100). Set to 0!", vctsize);
		vct_warning = 0;
	      }
	  }
	else
	  {
	    IEG_G_Size(gdb) += (vctsize*4);
	    memcpy(vct, zaxisInqVctPtr(zaxisID), vctsize/2*sizeof(double));
	    memcpy(vct+50, zaxisInqVctPtr(zaxisID)+vctsize/2, vctsize/2*sizeof(double));
	  }
	break;
      }
    case ZAXIS_PRESSURE:
      {
	double dum;
	char units[128];

	level = zaxisInqLevel(zaxisID, levelID);
	if ( level < 0 )
	  Warning(func, "pressure level of %f Pa is below 0.", level);

	zaxisInqUnits(zaxisID, units);
	if ( strncmp(units, "hPa", 3) == 0 ||  strncmp(units, "mb",2 ) == 0 )
	  level = level*100;

	ilevel = (int) level;
	if ( level < 32768 && (level < 100 || modf(level/100, &dum) > 0) )
	  {
	    IEG_P_LevelType(pdb) = IEG_LTYPE_99;
	    IEG_P_Level1(pdb)    = 0;
	    IEG_P_Level2(pdb)    = ilevel;
	  }
	else
	  {
	    IEG_P_LevelType(pdb) = IEG_LTYPE_ISOBARIC;
	    IEG_P_Level1(pdb)    = 0;
	    IEG_P_Level2(pdb)    = ilevel/100;
	  }
	break;
      }
    case ZAXIS_HEIGHT:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	IEG_P_LevelType(pdb) = IEG_LTYPE_HEIGHT;
	IEG_P_Level1(pdb)    = 0;
	IEG_P_Level2(pdb)    = ilevel;

	break;
      }
    case ZAXIS_ALTITUDE:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	IEG_P_LevelType(pdb) = IEG_LTYPE_ALTITUDE;
	IEG_P_Level1(pdb)    = 0;
	IEG_P_Level2(pdb)    = ilevel;

	break;
      }
    case ZAXIS_DEPTH_BELOW_LAND:
      {
	if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
	  {
	    IEG_P_LevelType(pdb) = IEG_LTYPE_LANDDEPTH_LAYER;
	    IEG_P_Level1(pdb)    = (int) zaxisInqLbound(zaxisID, levelID);
	    IEG_P_Level2(pdb)    = (int) zaxisInqUbound(zaxisID, levelID);
	  }
	else
	  {
	    level = zaxisInqLevel(zaxisID, levelID);

	    ilevel = (int) level;
	    IEG_P_LevelType(pdb) = IEG_LTYPE_LANDDEPTH;
	    IEG_P_Level1(pdb)    = 0;
	    IEG_P_Level2(pdb)    = ilevel;
	  }

	break;
      }
    case ZAXIS_DEPTH_BELOW_SEA:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	IEG_P_LevelType(pdb) = IEG_LTYPE_SEADEPTH;
	IEG_P_Level1(pdb)    = 0;
	IEG_P_Level2(pdb)    = ilevel;

	break;
      }
    case ZAXIS_ISENTROPIC:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	IEG_P_LevelType(pdb) = 113;
	IEG_P_Level1(pdb)    = 0;
	IEG_P_Level2(pdb)    = ilevel;

	break;
      }
    default:
      {
	Error(func, "leveltype >%s< unsupported", zaxisNamePtr(leveltype));
	break;
      }
    }
}


int iegCopyRecord(int ostreamID, int istreamID)
{
  static char func[] = "iegCopyRecord";
  int ifileID, ofileID;
  int tsID, recID, vrecID;
  long recsize;
  off_t recpos;
  int status = 0;
  char *buffer;

  streamCheckID(func, istreamID);
  streamCheckID(func, ostreamID);

  ifileID = streamInqFileID(istreamID);
  ofileID = streamInqFileID(ostreamID);

  tsID    = streams[istreamID].curTsID;
  vrecID  = streams[istreamID].tsteps[tsID].curRecID;
  recID   = streams[istreamID].tsteps[tsID].recIDs[vrecID];
  recpos  = streams[istreamID].tsteps[tsID].records[recID].position;
  recsize = streams[istreamID].tsteps[tsID].records[recID].size;

  fileSetPos(ifileID, recpos, SEEK_SET);

  buffer = (char *) malloc(recsize);

  fileRead(ifileID, buffer, recsize);

  fileWrite(ofileID, buffer, recsize);

  free(buffer);

  return (status);
}


int iegDefRecord(int streamID)
{
  static char func[] = "iegDefRecord";
  int status = 0;
  int fileID, vlistID;
  int gridID;
  int date, time;
  int datatype;
  int i;
  int varID, levelID, tsID, zaxisID;
  int byteorder;
  IEGREC *iegp;

  streamCheckID(func, streamID);

  fileID  = streamInqFileID(streamID);
  vlistID = streamInqVlist(streamID);
  iegp    = streams[streamID].record->iegp;
  byteorder = streams[streamID].byteorder;

  varID   = streams[streamID].record->varID;
  levelID = streams[streamID].record->levelID;
  tsID    = streams[streamID].curTsID;

  gridID  = vlistInqVarGrid(vlistID, varID);
  zaxisID = vlistInqVarZaxis(vlistID, varID);
  
  iegInit(iegp);
  for ( i = 0; i < 37; i++ ) iegp->ipdb[i] = -1;

  iegp->byteswap = getByteswap(byteorder);

  IEG_P_Parameter(iegp->ipdb) = vlistInqVarCode(vlistID, varID);
  date     = streams[streamID].tsteps[tsID].taxis.vdate;
  time     = streams[streamID].tsteps[tsID].taxis.vtime;

  iegDefTime(iegp->ipdb, date, time, vlistInqTaxis(vlistID));
  iegDefGrid(iegp->igdb, gridID);
  iegDefLevel(iegp->ipdb, iegp->igdb, iegp->vct, zaxisID, levelID);

  datatype = streams[streamID].record->prec;

  iegp->dprec = iegDefDatatype(datatype);

  return (status);
}


int iegWriteRecord(int streamID, const double *data)
{
  static char func[] = "iegWriteRecord";
  int fileID;
  int status = 0;
  int i, gridsize, gridID;
  double refval;
  IEGREC *iegp;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);
  iegp   = streams[streamID].record->iegp;
  gridID = streams[streamID].record->gridID;
  
  gridsize = gridInqSize(gridID);

  refval = data[0];
  for ( i = 1; i < gridsize; i++ )
    if ( data[i] < refval ) refval = data[i];

  iegp->refval = refval;

  iegDefDataDP(iegp, data);

  iegWrite(fileID, iegp);

  return (status);
}


void iegAddRecord(int streamID, int code, int *pdb, int *gdb, double *vct,
		  long recsize, off_t position, int prec)
{
  static char func[] = "iegAddRecord";
  int leveltype;
  int gridID = UNDEFID;
  int levelID = 0;
  int tsID, recID, varID;
  int datatype;
  int level1, level2;
  int gridtype;
  int lbounds = 0;
  RECORD *record;
  GRID grid;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  tsID    = streams[streamID].curTsID;
  recID   = recordNewEntry(streamID, tsID);
  record  = &streams[streamID].tsteps[tsID].records[recID];

  if ( IEG_P_LevelType(pdb) == IEG_LTYPE_HYBRID_LAYER )
    {
      level1 = IEG_P_Level1(pdb);
      level2 = IEG_P_Level2(pdb);
    }
  else
    {
      level1 = IEG_P_Level2(pdb);
      level2 = 0;
    }
  /*
  fprintf(stderr, "%d %d %d %d\n", code, IEG_P_LevelType(pdb), IEG_P_Level1(pdb), IEG_P_Level2(pdb));
  */
  (*record).size     = recsize;
  (*record).position = position;
  (*record).code     = code;
  (*record).table    = IEG_P_CodeTable(pdb);
  (*record).ilevel   = level1;
  (*record).ilevel2  = level2;
  (*record).ltype    = IEG_P_LevelType(pdb);

  if ( IEG_G_GridType(gdb) == 0 || IEG_G_GridType(gdb) == 10 )
    gridtype = GRID_LONLAT;
  else
    gridtype = GRID_GENERIC;

  memset(&grid, 0, sizeof(GRID));
  grid.type  = gridtype; 
  grid.size  = IEG_G_NumLon(gdb)*IEG_G_NumLat(gdb);
  grid.xsize = IEG_G_NumLon(gdb);
  grid.ysize = IEG_G_NumLat(gdb);
  grid.xfirst= IEG_G_FirstLon(gdb) * 0.001;
  grid.xlast = IEG_G_LastLon(gdb) * 0.001;
  grid.xinc  = IEG_G_LonIncr(gdb) * 0.001;
  grid.xdef  = 2;	    
  grid.yfirst= IEG_G_FirstLat(gdb) * 0.001;
  grid.ylast = IEG_G_LastLat(gdb) * 0.001;
  grid.yinc  = IEG_G_LatIncr(gdb) * 0.001;
  grid.ydef  = 2;	    
  grid.xvals = NULL;
  grid.yvals = NULL;

  grid.isRotated = FALSE;
  if ( IEG_G_GridType(gdb) == 10 )
    {
      grid.isRotated = TRUE;
      grid.ypole     = - IEG_G_LatSP(gdb) * 0.001;
      grid.xpole     =   IEG_G_LonSP(gdb) * 0.001 - 180;
      grid.angle     = 0;
    }

  gridID = varDefGrid(vlistID, grid, 0);

  leveltype = iegGetZaxisType(IEG_P_LevelType(pdb));
  
  if ( leveltype == ZAXIS_HYBRID )
    {
      int i;
      double tmpvct[100];
      int vctsize = IEG_G_NumVCP(gdb);

      for ( i = 0; i < vctsize/2; i++ ) tmpvct[i] = vct[i];
      for ( i = 0; i < vctsize/2; i++ ) tmpvct[i+vctsize/2] = vct[i+50];

      varDefVCT(vctsize, tmpvct);
    }

  if ( IEG_P_LevelType(pdb) == IEG_LTYPE_HYBRID_LAYER ) lbounds = 1;

  datatype = iegInqDatatype(prec);

  varAddRecord(recID, code, gridID, leveltype, lbounds, level1, level2,
	       datatype, &varID, &levelID, 0, 0);

  (*record).varID   = varID;
  (*record).levelID = levelID;

  streams[streamID].tsteps[tsID].nallrecs++;
  streams[streamID].nrecs++;

  if ( CDI_Debug )
    Message(func, "varID = %d gridID = %d levelID = %d",
	    varID, gridID, levelID);
}


void iegCmpRecord(int streamID, int tsID, int recID, off_t position, int code,
		  int level, int xsize, int ysize)
{
  static char func[] = "iegCmpRecord";
  int varID = 0;
  int levelID = 0;
  RECORD *record;

  record  = &streams[streamID].tsteps[tsID].records[recID];

  if ( code != (*record).code || level != (*record).ilevel )
    Error(func, "inconsistent timestep");

  (*record).position = position;
  /*
  varID   = (*record).varID;
  levelID = (*record).levelID;

  streams[streamID].vars[varID].level[levelID] = recID;

  streams[streamID].tsteps[tsID].nallrecs++;
  streams[streamID].nrecs++;
  */
  if ( CDI_Debug )
    Message(func, "varID = %d levelID = %d", varID, levelID);
}


void iegDateTime(int *pdb, int *date, int *time)
{
  int ryear, rmonth, rday, rhour, rminute;

  ryear   = IEG_P_Year(pdb);

  rmonth  = IEG_P_Month(pdb);
  rday    = IEG_P_Day(pdb);

  rhour   = IEG_P_Hour(pdb);
  rminute = IEG_P_Minute(pdb);

  if ( rminute == -1 ) rminute = 0;

  *date = ryear*10000 + rmonth*100 + rday;
  *time = rhour*100 + rminute;
}


void iegScanTimestep1(int streamID)
{  
  static char func[] = "iegScanTimestep1";
  int prec = 0;
  int status;
  int fileID;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  DateTime datetime, datetime0;
  long levelcode, levelcode0 = 0;
  int tsID;
  int varID;
  long recsize;
  off_t recpos;
  int nrecords, nrecs, recID;
  int taxisID = -1;
  TAXIS *taxis;
  int vlistID;
  IEGREC *iegp;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 0;

  iegp  = streams[streamID].record->iegp;
  tsID  = tstepsNewEntry(streamID);
  taxis = &streams[streamID].tsteps[tsID].taxis;

  if ( tsID != 0 )
    Error(func, "Internal problem! tstepsNewEntry returns %d", tsID);

  fileID = streamInqFileID(streamID);

  nrecs = 0;
  while ( TRUE )
    {
      recpos = fileGetPos(fileID);
      status = iegRead(fileID, iegp);
      if ( status != 0 )
	{
	  streams[streamID].ntsteps = 1;
	  break;
	}
      recsize = fileGetPos(fileID) - recpos;

      prec   = iegp->dprec;
      rcode  = IEG_P_Parameter(iegp->ipdb);
      if ( IEG_P_LevelType(iegp->ipdb) == IEG_LTYPE_HYBRID_LAYER )
	rlevel = IEG_P_Level1(iegp->ipdb);
      else
	rlevel = IEG_P_Level2(iegp->ipdb);

      iegDateTime(iegp->ipdb, &vdate, &vtime);

      if ( nrecs == 0 )
	{
	  datetime0.date = vdate;
	  datetime0.time = vtime;
	}
      else
	{
	  datetime.date = vdate;
	  datetime.time = vtime;
	  levelcode = rlevel*1000 + rcode;
	  for ( recID = 0; recID < nrecs; recID++ )
	    {
	      levelcode0 = streams[streamID].tsteps[0].records[recID].ilevel*1000 +
	 	           streams[streamID].tsteps[0].records[recID].code;
	      if ( levelcode == levelcode0 ) break;
	    }
	  if ( recID < nrecs ) break;
	  if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) )
	    Warning(func, "Inconsistent verification time for code %d level %d", rcode, rlevel);
	}

      nrecs++;

      if ( CDI_Debug )
	Message(func, "%4d%8d%4d%8d%8d%6d", nrecs, (int)recpos, rcode, rlevel, vdate, vtime);

      iegAddRecord(streamID, rcode, iegp->ipdb, iegp->igdb, iegp->vct, recsize, recpos, prec);
    }

  streams[streamID].rtsteps = 1;

  cdiGenVars(streamID);

  taxisID = taxisCreate(TAXIS_ABSOLUTE);
  taxis->type  = TAXIS_ABSOLUTE;
  taxis->vdate = datetime0.date;
  taxis->vtime = datetime0.time;

  vlistID = streamInqVlist(streamID);
  vlistDefTaxis(vlistID, taxisID);

  cdiCheckContents(streamID);

  nrecords = streams[streamID].tsteps[0].nallrecs;
  if ( nrecords < streams[streamID].tsteps[0].recordSize )
    {
      streams[streamID].tsteps[0].recordSize = nrecords;
      streams[streamID].tsteps[0].records =
	(RECORD *) realloc(streams[streamID].tsteps[0].records, nrecords*sizeof(RECORD));
    }

  streams[streamID].tsteps[0].recIDs = (int *) malloc(nrecords*sizeof(int));
  streams[streamID].tsteps[0].nrecs = nrecords;
  for ( recID = 0; recID < nrecords; recID++ )
    streams[streamID].tsteps[0].recIDs[recID] = recID;

  if ( streams[streamID].ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streams[streamID].rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streams[streamID].tsteps[tsID-1].next   = TRUE;
      streams[streamID].tsteps[tsID].position = recpos;
    }

  if ( streams[streamID].ntsteps == 1 )
    {
      if ( taxis->vdate == 0 && taxis->vtime == 0 )
	{
	  streams[streamID].ntsteps = 0;
	  for ( varID = 0; varID < streams[streamID].nvars; varID++ )
	    {
	      vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	    }
	}
    }
}


int iegScanTimestep2(int streamID)
{  
  static char func[] = "iegScanTimestep2";
  int status;
  int fileID;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  long levelcode, levelcode0 = 0;
  int tsID;
  int varID;
  long recsize;
  off_t recpos = 0;
  int nrecords, nrecs, recID, rindex;
  int nextstep;
  TAXIS *taxis;
  int vlistID;
  IEGREC *iegp;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 1;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  iegp    = streams[streamID].record->iegp;

  tsID = streams[streamID].rtsteps;
  if ( tsID != 1 )
    Error(func, "Internal problem! unexpeceted timestep %d", tsID+1);

  taxis = &streams[streamID].tsteps[tsID].taxis;

  fileSetPos(fileID, streams[streamID].tsteps[tsID].position, SEEK_SET);

  cdiCreateRecords(streamID, tsID);

  nrecords = streams[streamID].tsteps[0].nallrecs;
  streams[streamID].tsteps[1].recIDs = (int *) malloc(nrecords*sizeof(int));
  streams[streamID].tsteps[1].nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    streams[streamID].tsteps[1].recIDs[recID] = -1;

  for ( recID = 0; recID < nrecords; recID++ )
    {
      varID = streams[streamID].tsteps[0].records[recID].varID;
      streams[streamID].tsteps[tsID].records[recID].position = 
	streams[streamID].tsteps[0].records[recID].position;
      streams[streamID].tsteps[tsID].records[recID].size     = 
	streams[streamID].tsteps[0].records[recID].size;
    }

  for ( rindex = 0; rindex <= nrecords; rindex++ )
    {
      recpos = fileGetPos(fileID);
      status = iegRead(fileID, iegp);
      if ( status != 0 )
	{
	  streams[streamID].ntsteps = 2;
	  break;
	}
      recsize = fileGetPos(fileID) - recpos;

      rcode  = IEG_P_Parameter(iegp->ipdb);
      if ( IEG_P_LevelType(iegp->ipdb) == IEG_LTYPE_HYBRID_LAYER )
	rlevel = IEG_P_Level1(iegp->ipdb);
      else
	rlevel = IEG_P_Level2(iegp->ipdb);

      iegDateTime(iegp->ipdb, &vdate, &vtime);

      if ( rindex == 0 )
	{
	  taxis->type  = TAXIS_ABSOLUTE;
	  taxis->vdate = vdate;
	  taxis->vtime = vtime;
	}

      levelcode = rlevel*1000 + rcode;
      nextstep = FALSE;
      for ( recID = 0; recID < nrecords; recID++ )
	{
	  levelcode0 = streams[streamID].tsteps[tsID].records[recID].ilevel*1000 +
	               streams[streamID].tsteps[tsID].records[recID].code;
	  if ( levelcode == levelcode0 )
	    {
	      if ( streams[streamID].tsteps[tsID].records[recID].used )
		{
		  nextstep = TRUE;
		}
	      else
		{
		  streams[streamID].tsteps[tsID].records[recID].used = TRUE;
		  streams[streamID].tsteps[tsID].recIDs[rindex] = recID;
		}
	      break;
	    }
	}
      if ( recID == nrecords )
	{
	  Warning(func, "code %d level %d not found at timestep %d", rcode, rlevel, tsID+1);
	  return (CDI_EUFSTRUCT);
	}

      if ( nextstep ) break;

      if ( CDI_Debug )
	Message(func, "%4d%8d%4d%8d%8d%6d", rindex+1, (int)recpos, rcode, rlevel, vdate, vtime);

      streams[streamID].tsteps[tsID].records[recID].size = recsize;

      levelcode0 = streams[streamID].tsteps[tsID].records[recID].ilevel*1000 +
	           streams[streamID].tsteps[tsID].records[recID].code;

      if ( levelcode != levelcode0 )
	{
	  Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		  tsID, recID,
		  streams[streamID].tsteps[tsID].records[recID].code, rcode,
		  streams[streamID].tsteps[tsID].records[recID].ilevel, rlevel);
	  return (CDI_EUFSTRUCT);
	}

      streams[streamID].tsteps[1].records[recID].position = recpos;
    }

  nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    {
      if ( ! streams[streamID].tsteps[tsID].records[recID].used )
	{
	  varID = streams[streamID].tsteps[tsID].records[recID].varID;
	  vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	}
      else
	{
	  nrecs++;
	}
    }
  streams[streamID].tsteps[tsID].nrecs = nrecs;

  streams[streamID].rtsteps = 2;

  if ( streams[streamID].ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streams[streamID].rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streams[streamID].tsteps[tsID-1].next   = TRUE;
      streams[streamID].tsteps[tsID].position = recpos;
    }

  return (0);
}


int iegInqContents(int streamID)
{
  static char func[] = "iegInqContents";
  int fileID;
  int status = 0;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);

  streams[streamID].curTsID = 0;

  iegScanTimestep1(streamID);
 
  if ( streams[streamID].ntsteps == -1 ) status = iegScanTimestep2(streamID);

  fileSetPos(fileID, 0, SEEK_SET);

  return (status);
}


int iegScanTimestep(int streamID)
{
  static char func[] = "iegScanTimestep";
  int status;
  int fileID;
  int tsID;
  int rcode = 0, rlevel = 0, vdate = 0, vtime = 0;
  long levelcode, levelcode0 = 0;
  long recsize = 0;
  off_t recpos = 0;
  int recID;
  TAXIS *taxis;
  int rindex, nrecs = 0;
  IEGREC *iegp;

  streamCheckID(func, streamID);

  if ( CDI_Debug )
    {
      Message(func, "streamID = %d", streamID);
      Message(func, "cts = %d", streams[streamID].curTsID);
      Message(func, "rts = %d", streams[streamID].rtsteps);
      Message(func, "nts = %d", streams[streamID].ntsteps);
    }

  if ( streams[streamID].rtsteps == 0 )
    Error(func, "Internal problem! Missing contents.");

  iegp  = streams[streamID].record->iegp;
  tsID  = streams[streamID].rtsteps;
  taxis = &streams[streamID].tsteps[tsID].taxis;

  if ( streams[streamID].tsteps[tsID].recordSize == 0 )
    {
      cdiCreateRecords(streamID, tsID);

      nrecs = streams[streamID].tsteps[1].nrecs;

      streams[streamID].tsteps[tsID].nrecs = nrecs;
      streams[streamID].tsteps[tsID].recIDs = (int *) malloc(nrecs*sizeof(int));
      for ( recID = 0; recID < nrecs; recID++ )
	streams[streamID].tsteps[tsID].recIDs[recID] = streams[streamID].tsteps[1].recIDs[recID];

      fileID = streamInqFileID(streamID);

      fileSetPos(fileID, streams[streamID].tsteps[tsID].position, SEEK_SET);

      for ( rindex = 0; rindex <= nrecs; rindex++ )
	{
	  recpos = fileGetPos(fileID);
	  status = iegRead(fileID, iegp);
	  if ( status != 0 )
	    {
	      streams[streamID].ntsteps = streams[streamID].rtsteps + 1;
	      break;
	    }
	  recsize = fileGetPos(fileID) - recpos;

	  rcode  = IEG_P_Parameter(iegp->ipdb);
	  if ( IEG_P_LevelType(iegp->ipdb) == IEG_LTYPE_HYBRID_LAYER )
	    rlevel = IEG_P_Level1(iegp->ipdb);
	  else
	    rlevel = IEG_P_Level2(iegp->ipdb);

	  iegDateTime(iegp->ipdb, &vdate, &vtime);

	  if ( rindex == nrecs ) break;
	  recID = streams[streamID].tsteps[tsID].recIDs[rindex];

	  if ( rindex == 0 )
	    {
	      taxis->type  = TAXIS_ABSOLUTE;
	      taxis->vdate = vdate;
	      taxis->vtime = vtime;
	    }

	  levelcode  = rlevel*1000 + rcode;
	  levelcode0 = streams[streamID].tsteps[tsID].records[recID].ilevel*1000 +
	               streams[streamID].tsteps[tsID].records[recID].code;

	  if ( levelcode != levelcode0 )
	    {
	      Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		      tsID, recID,
		      streams[streamID].tsteps[tsID].records[recID].code, rcode,
		      streams[streamID].tsteps[tsID].records[recID].ilevel, rlevel);
	      Error(func, "Invalid, unsupported or inconsistent record structure");
	    }

	  streams[streamID].tsteps[tsID].records[recID].position = recpos;
	  streams[streamID].tsteps[tsID].records[recID].size = recsize;

	  if ( CDI_Debug )
	    Message(func, "%4d%8d%4d%8d%8d%6d", rindex, (int)recpos, rcode, rlevel, vdate, vtime);
	}

      streams[streamID].rtsteps++;

      if ( streams[streamID].ntsteps != streams[streamID].rtsteps )
	{
	  tsID = tstepsNewEntry(streamID);
	  if ( tsID != streams[streamID].rtsteps )
	    Error(func, "Internal error. tsID = %d", tsID);

	  streams[streamID].tsteps[tsID-1].next   = 1;
	  streams[streamID].tsteps[tsID].position = recpos;
	}

      fileSetPos(fileID, streams[streamID].tsteps[tsID].position, SEEK_SET);
      streams[streamID].tsteps[tsID].position = recpos;
    }

  if ( nrecs > 0 && nrecs < streams[streamID].tsteps[tsID].nrecs )
    {
      Warning(func, "Incomplete timestep. Stop scanning at timestep %d.\n", tsID);
      streams[streamID].ntsteps = tsID;
    }

  return (streams[streamID].ntsteps);
}


int iegInqTimestep(int streamID, int tsID)
{
  static char func[] = "iegInqTimestep";
  int ntsteps, nrecs;

  streamCheckID(func, streamID);

  if ( tsID == 0 && streams[streamID].rtsteps == 0 )
    Error(func, "Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message(func, "tsID = %d rtsteps = %d", tsID, streams[streamID].rtsteps);
  
  ntsteps = UNDEFID;
  while ( ( tsID + 1 ) > streams[streamID].rtsteps && ntsteps == UNDEFID )
    ntsteps = iegScanTimestep(streamID);

  if ( tsID >= streams[streamID].ntsteps && streams[streamID].ntsteps != CDI_UNDEFID )
    {
      nrecs = 0;
    }
  else
    {
      streams[streamID].curTsID = tsID;
      nrecs = streams[streamID].tsteps[tsID].nrecs;
    }

  return (nrecs);
}


void iegReadVarDP(int streamID, int varID, double *data, int *nmiss)
{
  static char func[] = "iegReadVarDP";
  int vlistID, fileID;
  int levID, nlevs, gridID, gridsize;
  off_t recpos, currentfilepos;
  int tsid;
  int recID;
  int i;
  int status;
  double missval;
  IEGREC *iegp;

  iegp     = streams[streamID].record->iegp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  nlevs    = streams[streamID].vars[varID].nlevs;
  missval  = vlistInqVarMissval(vlistID, varID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsid     = streams[streamID].curTsID;

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  for (levID = 0; levID < nlevs; levID++)
    {
      recID = streams[streamID].vars[varID].level[levID];
      recpos = streams[streamID].tsteps[tsid].records[recID].position;
      fileSetPos(fileID, recpos, SEEK_SET);
      status = iegRead(fileID, iegp);
      iegInqDataDP(iegp, &data[levID*gridsize]);
    }
  fileSetPos(fileID, currentfilepos, SEEK_SET);

  *nmiss = 0;
  for ( i = 0; i < nlevs*gridsize; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }
}


void iegReadVarSliceDP(int streamID, int varID, int levID, double *data, int *nmiss)
{
  static char func[] = "iegReadVarSliceDP";
  int vlistID, fileID;
  int nlevs, gridID, gridsize;
  off_t recpos, currentfilepos;
  int tsid;
  int recID;
  int i;
  int status;
  double missval;
  IEGREC *iegp;

  iegp     = streams[streamID].record->iegp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  nlevs    = streams[streamID].vars[varID].nlevs;
  missval  = vlistInqVarMissval(vlistID, varID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsid     = streams[streamID].curTsID;

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d",
	     nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  recID = streams[streamID].vars[varID].level[levID];
  recpos = streams[streamID].tsteps[tsid].records[recID].position;
  fileSetPos(fileID, recpos, SEEK_SET);
  status = iegRead(fileID, iegp);
  iegInqDataDP(iegp, data);

  fileSetPos(fileID, currentfilepos, SEEK_SET);

  *nmiss = 0;
  for ( i = 0; i < gridsize; i++ )
    if ( DBL_IS_EQUAL(data[i], missval) || DBL_IS_EQUAL(data[i], (float)missval) )
      {
	data[i] = missval;
	(*nmiss)++;
      }
}


void iegWriteVarDP(int streamID, int varID, const double *data)
{
  static char func[] = "iegWriteVarDP";
  int fileID;
  int levID, nlevs, gridID, gridsize;
  int zaxisID;
  double level;
  int datatype;
  int tsID;
  int vlistID;
  int i;
  int code, date, time, datasize;
  IEGREC *iegp;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  iegp     = streams[streamID].record->iegp;

  iegInit(iegp);
  for ( i = 0; i < 37; i++ ) iegp->ipdb[i] = -1;

  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  nlevs    = zaxisInqSize(zaxisID);

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  code     = vlistInqVarCode(vlistID, varID);
  date     = streams[streamID].tsteps[tsID].taxis.vdate;
  time     = streams[streamID].tsteps[tsID].taxis.vtime;
  datasize = gridInqSize(gridID);

  datatype = vlistInqVarDatatype(vlistID, varID);

  iegp->dprec = iegDefDatatype(datatype);

  for ( levID = 0;  levID < nlevs; levID++ )
    {
      level = zaxisInqLevel(zaxisID, levID);

      /*  header[1] = (int) level; */
      iegDefDataDP(iegp, &data[levID*gridsize]);
      iegWrite(fileID, iegp);
    }
}


void iegWriteVarSliceDP(int streamID, int varID, int levID, const double *data)
{
  static char func[] = "iegWriteVarSliceDP";
  int fileID;
  int gridID;
  int zaxisID;
  double level;
  int datatype;
  int tsID;
  int vlistID;
  int code, date, time, datasize;
  IEGREC *iegp;

  iegp     = streams[streamID].record->iegp;
  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;
  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);
  level    = zaxisInqLevel(zaxisID, levID);

  if ( CDI_Debug )
    Message(func, "gridID = %d zaxisID = %d", gridID, zaxisID);

  code = vlistInqVarCode(vlistID, varID);
  date = streams[streamID].tsteps[tsID].taxis.vdate;
  time = streams[streamID].tsteps[tsID].taxis.vtime;
  datasize = gridInqSize(gridID);

  datatype = vlistInqVarDatatype(vlistID, varID);

  iegp->dprec = iegDefDatatype(datatype);

  iegDefDataDP(iegp, data);
  iegWrite(fileID, iegp);
}

#endif /* HAVE_LIBIEG */
