#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>  /* modf */
#include <float.h>  /* FLT_EPSILON */
#include <sys/types.h>

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "grib.h"
#include "file.h"
#include "varscan.h"
#include "datetime.h"
#include "taxis.h"
#include "vlist.h"


#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID


typedef struct {
  int code;
  int table;
  int level1;
  int level2;
  int ltype;
} COMPVAR; 


int gribGetGridType(int *isec2)
{
  /*  static char func[] = "gribGetGridType"; */
  int gridtype = 0;

  switch (ISEC2_GridType)
    {
    case   0:
    case  10:
      {
	gridtype = GRID_LONLAT;
	break;
      }
    case   4:
      {
	if ( ISEC2_Reduced )
	  gridtype = GRID_GAUSSIAN_REDUCED;
	else
	  gridtype = GRID_GAUSSIAN;
	break;
      }
    case  50:
      {
	gridtype = GRID_SPECTRAL;
	break;
      }
    case 192:
      {
	gridtype = GRID_GME;
	break;
      }
    default:
      {
	gridtype = GRID_GENERIC;
	break;
      }
    }

  return (gridtype);
}


int gribGetIsRotated(int *isec2)
{
  /*  static char func[] = "gribGetIsRotated"; */
  int isRotated = 0;

  if ( ISEC2_GridType == 10 )
    {
      isRotated = 1;
    }

  return (isRotated);
}


int grbInqRecord(int streamID, int *varID, int *levelID)
{
  static char func[] = "grbInqRecord";
  int status = 0;
  int fileID;
  void *gribbuffer;
  int iret = 0, iword = 0;
  int isec4size = 0;
  int isec0[2], isec1[4096], isec2[4096], isec3[2], isec4[512];
  double fsec2[512], fsec3[2], *fsec4 = NULL;
  int gridID = -1, zaxisID = -1;
  int gridtype;
  int gribsize;
  size_t readsize;
  int vlistID;

  streamCheckID(func, streamID);

  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);

  *varID   = -1;
  *levelID = -1;

  gribsize = gribGetSize(fileID);
  if ( gribsize == 0 ) return (0);

  gribbuffer = streams[streamID].record->buffer;
  gribbuffer = realloc(gribbuffer, gribsize);

  streams[streamID].record->buffer = gribbuffer;
  streams[streamID].record->buffersize = gribsize;

  readsize = gribsize;
  status = gribRead(fileID, (unsigned char *) gribbuffer, &readsize);
  if ( status ) return (-1);

  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, fsec4,
	   isec4size, (int *) gribbuffer, gribsize, &iword, "J", &iret);

  *varID = vlistInqVarID(vlistID, ISEC1_Parameter);

  if ( *varID == UNDEFID )
    Error(func, "code %d undefined", ISEC1_Parameter);

  gridID  = vlistInqVarGrid(vlistID, *varID);
  zaxisID = vlistInqVarZaxis(vlistID, *varID);

  gridtype = gribGetGridType(isec2);

  switch (gridtype)
    {
    case GRID_LONLAT:
      {
	if ( gridInqType(gridID) == GRID_TRAJECTORY )
	  {
	    double xfirst, yfirst;
	    xfirst = ISEC2_FirstLon * 0.001;
	    yfirst = ISEC2_FirstLat * 0.001;
	    gridDefXvals(gridID, &xfirst);
	    gridDefYvals(gridID, &yfirst);
	  }
	break;
      }
    case GRID_GAUSSIAN:
      {
	if ( ISEC4_NumValues != ISEC2_NumLon*ISEC2_NumLat )
	  Error(func, "wrong datasize! isec4len = %d isec2len = %d",
		ISEC4_NumValues, ISEC2_NumLon*ISEC2_NumLat);
	break;
      }
    case GRID_SPECTRAL:
      {
	break;
      }
    case GRID_GME:
      {
	break;
      }
    default:
      {
	Error(func, "%s grid unsupported", gridNamePtr(gridtype));
	break;
      }
    }

  streams[streamID].record->code     = ISEC1_Parameter;
  streams[streamID].record->level    = ISEC1_Level1;
  gribDateTime(isec1, &streams[streamID].record->date, &streams[streamID].record->time);
  streams[streamID].record->gridID   = gridID;
  streams[streamID].record->zaxisID  = zaxisID;
  streams[streamID].record->nrec++;

  *levelID = zaxisInqLevelID(zaxisID, (double) ISEC1_Level1);

  status = 1;

  return (status);
}

int grbDefRecord(int streamID)
{
  static char func[] = "grbDefRecord";
  int fileID;
  int gridID;
  int status = 0;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);
  gridID = streams[streamID].record->gridID;

  return (status);
}


int grbDecodeDataDP(unsigned char *gribbuffer, int gribsize, double *data, int gridsize,
		    int unreduced, int *nmiss)
{
  static char func[] = "grbDecodeDataDP";
  int status = 0;
  int iret = 0, iword = 0;
  int isec0[2], isec1[4096], isec2[4096], isec3[2], isec4[512];
  int izip;
  long unzipsize;
  double fsec2[512], fsec3[2];
  char hoper[2];

  if ( unreduced ) strcpy(hoper, "R");
  else             strcpy(hoper, "D");

  FSEC3_MissVal = cdiDefaultMissval;

  if ( (izip = gribGetZip(gribsize, gribbuffer, &unzipsize)) > 0 )
    {
      Error(func, "SZIP decompression not implemented!\n");
    }

  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, data,
	   gridsize, (int *) gribbuffer, gribsize, &iword, hoper, &iret);

  if ( ISEC1_Sec2Or3Flag & 64 )
    *nmiss = ISEC4_NumValues - ISEC4_NumNonMissValues;
  else
    *nmiss = 0;

  return (status);
}


int grbReadRecord(int streamID, double *data, int *nmiss)
{
  static char func[] = "grbReadRecord";
  int status = 0;
  unsigned char *gribbuffer;
  int fileID;
  int recID, vrecID, tsID, gridID, varID;
  long recsize;
  off_t recpos;
  int gridsize;
  int vlistID;

  streamCheckID(func, streamID);

  gribbuffer = (unsigned char *) streams[streamID].record->buffer;

  vlistID = streamInqVlist(streamID);
  fileID  = streamInqFileID(streamID);
  tsID    = streams[streamID].curTsID;
  vrecID  = streams[streamID].tsteps[tsID].curRecID;
  recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
  recpos  = streams[streamID].tsteps[tsID].records[recID].position;
  recsize = streams[streamID].tsteps[tsID].records[recID].size;
  varID   = streams[streamID].tsteps[tsID].records[recID].varID;

  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);

  fileSetPos(fileID, recpos, SEEK_SET);

  fileRead(fileID, gribbuffer, (size_t) recsize);

  grbDecodeDataDP(gribbuffer, recsize, data, gridsize, streams[streamID].unreduced, nmiss);

  return (status);
}


int gribGetZaxisType(int grbleveltype)
{
  int leveltype = 0;

  switch ( grbleveltype )
    {
    case LTYPE_SURFACE:
      {
	leveltype = ZAXIS_SURFACE;
	break;
      }
    case LTYPE_99:
    case LTYPE_ISOBARIC:
      {
	leveltype = ZAXIS_PRESSURE;
	break;
      }
    case LTYPE_HEIGHT:
      {
	leveltype = ZAXIS_HEIGHT;
	break;
      }
    case LTYPE_ALTITUDE:
      {
	leveltype = ZAXIS_ALTITUDE;
	break;
      }
    case LTYPE_HYBRID:
    case LTYPE_HYBRID_LAYER:
      {
	leveltype = ZAXIS_HYBRID;
	break;
      }
    case LTYPE_LANDDEPTH:
    case LTYPE_LANDDEPTH_LAYER:
      {
	leveltype = ZAXIS_DEPTH_BELOW_LAND;
	break;
      }
    case LTYPE_SEADEPTH:
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


int gribGetZaxisHasBounds(int grbleveltype)
{
  int lbounds = 0;

  switch (grbleveltype)
    {
    case LTYPE_HYBRID_LAYER:
    case LTYPE_LANDDEPTH_LAYER:
      {
	lbounds = 1;
	break;
      }
    }

  return (lbounds);
}


int gribTimeUnit(int *isec1)
{
  static char func[] = "gribTimeUnit";
  int timeunit = -1;
  static int lprint = TRUE;

  switch ( ISEC1_TimeUnit )
    {
    case ISEC1_TABLE4_MINUTE:  timeunit = TUNIT_MINUTE; break;
    case ISEC1_TABLE4_HOUR:    timeunit = TUNIT_HOUR;   break;
    case ISEC1_TABLE4_DAY:     timeunit = TUNIT_DAY;    break;
    default:
      if ( lprint )
	{
	  Message(func, "Time unit %d unsupported", ISEC1_TimeUnit);
	  lprint = FALSE;
	}
    }

  return (timeunit);
}


int grbScanTimestep(int streamID)
{
  static char func[] = "grbScanTimestep";
  int *isec0, *isec1, *isec2, *isec3, *isec4;
  double fsec2[512], fsec3[2], *fsec4 = NULL;
  long recsize = 0;
  off_t recpos = 0;
  unsigned char *gribbuffer;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int status;
  int fileID;
  int rcode = 0, level1 = 0, level2 = 0, vdate = 0, vtime = 0;
  int tsID;
  int vrecID, recID;
  int warn_numavg = TRUE;
  size_t readsize;
  int taxisID = -1;
  TAXIS *taxis;
  int vlistID;
  int rindex, nrecs = 0;
  COMPVAR compVar, compVar0;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);

  if ( CDI_Debug )
    {
      Message(func, "streamID = %d", streamID);
      Message(func, "cts = %d", streams[streamID].curTsID);
      Message(func, "rts = %d", streams[streamID].rtsteps);
      Message(func, "nts = %d", streams[streamID].ntsteps);
    }

  isec0 = streams[streamID].record->sec0;
  isec1 = streams[streamID].record->sec1;
  isec2 = streams[streamID].record->sec2;
  isec3 = streams[streamID].record->sec3;
  isec4 = streams[streamID].record->sec4;

  tsID  = streams[streamID].rtsteps;
  taxis = &streams[streamID].tsteps[tsID].taxis;

  if ( streams[streamID].tsteps[tsID].recordSize == 0 )
    {
      gribbuffer = (unsigned char *) streams[streamID].record->buffer;
      buffersize = streams[streamID].record->buffersize;

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
	  recsize = gribGetSize(fileID);
	  recpos  = fileGetPos(fileID);
	  if ( recsize == 0 )
	    {
	      streams[streamID].ntsteps = streams[streamID].rtsteps + 1;
	      break;
	    }
	  if ( recsize > buffersize )
	    {
	      buffersize = recsize;
	      gribbuffer = (unsigned char *) realloc(gribbuffer, buffersize);
	    }

	  readsize = recsize;
	  status = gribRead(fileID, gribbuffer, &readsize);
	  if ( status )
	    {
	      Error(func, "Inconsistent timestep %d (GRIB record %d/%d)!\n", tsID+1, rindex+1,
		    streams[streamID].tsteps[tsID].recordSize);
	      break;
	    }
	  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, fsec4,
		   ipunp, (int *) gribbuffer, recsize, &iword, "J", &iret);
	  rcode    = ISEC1_Parameter;
	  level1   = ISEC1_Level1;
	  level2   = ISEC1_Level2;
	  if ( ISEC1_LevelType == 100 ) level1 *= 100;

	  gribDateTime(isec1, &vdate, &vtime);

	  if ( rindex == nrecs ) break;

	  if ( rindex == 0 )
	    {
	      taxisID = vlistInqTaxis(vlistID);
	      if ( taxisInqType(taxisID) == TAXIS_RELATIVE )
		{
		  taxis->type  = TAXIS_RELATIVE;
		  taxis->rdate = gribRefDate(isec1);
		  taxis->rtime = gribRefTime(isec1);
		  taxis->unit  = gribTimeUnit(isec1);
		}
	      else
		{
		  taxis->type  = TAXIS_ABSOLUTE;
		}
	      taxis->vdate = vdate;
	      taxis->vtime = vtime;
	    }

	  if ( ISEC1_AvgNum )
	    {
	      if (  taxis->numavg && warn_numavg &&
		   (taxis->numavg != ISEC1_AvgNum) )
		{
	      /*
		  Message(func, "Change numavg from %d to %d not allowed!",
			  streams[streamID].tsteps[tsID].taxis.numavg, ISEC1_AvgNum);
	      */
		  warn_numavg = FALSE;
		}
	      else
		{
		  taxis->numavg = ISEC1_AvgNum;
		}
	    }
	  
	  compVar.code   = rcode;
          compVar.table  = ISEC1_CodeTable;
          compVar.level1 = level1;
          compVar.level2 = level2;
          compVar.ltype  = ISEC1_LevelType;
	  for ( vrecID = 0; vrecID < nrecs; vrecID++ )
	    {
	      recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
	      compVar0.code   = streams[streamID].tsteps[tsID].records[recID].code;
	      compVar0.table  = streams[streamID].tsteps[tsID].records[recID].table;
	      compVar0.level1 = streams[streamID].tsteps[tsID].records[recID].ilevel;
	      compVar0.level2 = streams[streamID].tsteps[tsID].records[recID].ilevel2;
	      compVar0.ltype  = streams[streamID].tsteps[tsID].records[recID].ltype;
	      if ( memcmp(&compVar0, &compVar, sizeof(COMPVAR)) == 0 )
		{
		  streams[streamID].tsteps[tsID].records[recID].used = TRUE;
		  streams[streamID].tsteps[tsID].recIDs[rindex] = recID;
		  break;
		}
	    }
	  if ( vrecID == nrecs )
	    {
	      Warning(func, "1 code %d level %d not found at timestep %d",
		      rcode, level1, tsID+1);
	      return (CDI_EUFSTRUCT);
	    }

	  if ( CDI_Debug )
	    Message(func, "%4d %8d %4d %8d %8d %6d", rindex+1, (int)recpos, rcode, level1, vdate, vtime);

	  compVar0.code   = streams[streamID].tsteps[tsID].records[recID].code;
	  compVar0.table  = streams[streamID].tsteps[tsID].records[recID].table;
	  compVar0.level1 = streams[streamID].tsteps[tsID].records[recID].ilevel;
	  compVar0.level2 = streams[streamID].tsteps[tsID].records[recID].ilevel2;
	  compVar0.ltype  = streams[streamID].tsteps[tsID].records[recID].ltype;

	  if ( memcmp(&compVar0, &compVar, sizeof(COMPVAR)) != 0 )
	    {
	      Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		      tsID, recID,
		      streams[streamID].tsteps[tsID].records[recID].code, rcode,
		      streams[streamID].tsteps[tsID].records[recID].ilevel, level1);
	      Error(func, "Invalid, unsupported or inconsistent record structure");
	    }
	  
	  streams[streamID].tsteps[tsID].records[recID].position = recpos;
	  streams[streamID].tsteps[tsID].records[recID].size = recsize;

	  if ( CDI_Debug )
	    Message(func, "%4d %8d %4d %8d %8d %6d", rindex, (int)recpos, rcode, level1, vdate, vtime);
	}

      for ( vrecID = 0; vrecID < nrecs; vrecID++ )
	{
	  recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
	  if ( ! streams[streamID].tsteps[tsID].records[recID].used ) break;
	}

      if ( vrecID < nrecs )
	{
	  Warning(func, "2 code %d level %d not found at timestep %d",
		  streams[streamID].tsteps[tsID].records[recID].code,
		  streams[streamID].tsteps[tsID].records[recID].ilevel,
		  tsID+1);
	  return (CDI_EUFSTRUCT);
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

      streams[streamID].record->buffer     = gribbuffer;
      streams[streamID].record->buffersize = buffersize;
    }

  if ( nrecs > 0 && nrecs < streams[streamID].tsteps[tsID].nrecs )
    {
      Warning(func, "Incomplete timestep. Stop scanning at timestep %d.\n", tsID);
      streams[streamID].ntsteps = tsID;
    }

  return (streams[streamID].ntsteps);
}


int grbInqTimeSize(int streamID)
{
  static char func[] = "grbInqTimeSize";

  streamCheckID(func, streamID);

  while ( ! streams[streamID].ntsteps )
    grbScanTimestep(streamID);

  return (streams[streamID].ntsteps);
}


void grbAddRecord(int streamID, int code, int *isec1, int *isec2, double *fsec2,
		  int *isec4, long recsize, off_t position, int prec)
{
  static char func[] = "grbAddRecord";
  int gridtype;
  int leveltype;
  int gridID = UNDEFID, varID;
  int levelID = 0;
  int tsID, recID;
  int level1, level2;
  int numavg;
  int lbounds = 0;
  RECORD *record;
  GRID grid;
  int vlistID;

  vlistID = streamInqVlist(streamID);
  tsID    = streams[streamID].curTsID;
  recID   = recordNewEntry(streamID, tsID);
  record  = &streams[streamID].tsteps[tsID].records[recID];

  numavg  = ISEC1_AvgNum;

  level1  = ISEC1_Level1;
  if ( ISEC1_LevelType == 100 ) level1 *= 100;
  level2  = ISEC1_Level2;

  /* fprintf(stderr, "code %d %d %d\n", code, level1, level2); */

  (*record).size     = recsize;
  (*record).position = position;
  (*record).code     = code;
  (*record).table    = ISEC1_CodeTable;
  (*record).ilevel   = level1;
  (*record).ilevel2  = level2;
  (*record).ltype    = ISEC1_LevelType;

  gridtype = gribGetGridType(isec2);

  if ( streams[streamID].unreduced && gridtype == GRID_GAUSSIAN_REDUCED )
    {
      gridtype = GRID_GAUSSIAN;
      ISEC2_NumLon = 2*ISEC2_NumLat;
      ISEC4_NumValues = ISEC2_NumLon*ISEC2_NumLat;
    }

  memset(&grid, 0, sizeof(GRID));
  switch (gridtype)
    {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
      {
	if ( ISEC4_NumValues != ISEC2_NumLon*ISEC2_NumLat )
	  Error(func, "wrong datasize! isec4len = %d isec2len = %d",
		ISEC4_NumValues, ISEC2_NumLon*ISEC2_NumLat);
	grid.size  = ISEC4_NumValues;
	grid.xsize = ISEC2_NumLon;
	grid.ysize = ISEC2_NumLat;
	grid.xinc  = 0;
	grid.yinc  = 0;
	grid.xdef  = 0;
	/* if ( ISEC2_FirstLon != 0 || ISEC2_LastLon != 0 ) */
	  {
	    if ( grid.xsize > 1 )
	      {
		if ( ISEC2_ResFlag && ISEC2_LonIncr > 0 )
		  grid.xinc = ISEC2_LonIncr * 0.001;
		else
		  grid.xinc = (ISEC2_LastLon - ISEC2_FirstLon) * 0.001 / (grid.xsize - 1);

		/* correct xinc if necessary */
		if ( ISEC2_FirstLon == 0 && ISEC2_LastLon > 354000 )
		  {
		    double xinc = 360. / grid.xsize;

		    if ( fabs(grid.xinc-xinc) > 0.0 )
		      {
			grid.xinc = xinc;
			if ( CDI_Debug ) Message(func, "set xinc to %g", grid.xinc);
		      }
		  }
	      }
	    grid.xfirst = ISEC2_FirstLon * 0.001;
	    grid.xlast  = ISEC2_LastLon  * 0.001;
	    grid.xdef   = 2;	    
	  }
	grid.ydef  = 0;
	/* if ( ISEC2_FirstLat != 0 || ISEC2_LastLat != 0 ) */
	  {
	    if ( grid.ysize > 1 )
	      {
		if ( ISEC2_ResFlag && ISEC2_LatIncr > 0 )
		  grid.yinc = ISEC2_LatIncr * 0.001;
		else
		  grid.yinc = (ISEC2_LastLat - ISEC2_FirstLat) * 0.001 / (grid.ysize - 1);
	      }
	    grid.yfirst = ISEC2_FirstLat * 0.001;
	    grid.ylast  = ISEC2_LastLat  * 0.001;
	    grid.ydef   = 2;	    
	  }
	break;
      }
    case GRID_GAUSSIAN_REDUCED:
      {
	grid.size   = ISEC4_NumValues;
        grid.rowlon = ISEC2_RowLonPtr;
	grid.ysize  = ISEC2_NumLat;
	grid.xinc  = 0;
	grid.yinc  = 0;
	grid.xdef  = 0;
	/* if ( ISEC2_FirstLon != 0 || ISEC2_LastLon != 0 ) */
	  {
	    if ( grid.xsize > 1 )
	      {
		if ( ISEC2_ResFlag && ISEC2_LonIncr > 0 )
		  grid.xinc = ISEC2_LonIncr * 0.001;
		else
		  grid.xinc = (ISEC2_LastLon - ISEC2_FirstLon) * 0.001 / (grid.xsize - 1);
	      }
	    grid.xfirst = ISEC2_FirstLon * 0.001;
	    grid.xlast  = ISEC2_LastLon  * 0.001;
	    grid.xdef   = 2;	    
	  }
	grid.ydef  = 0;
	/* if ( ISEC2_FirstLat != 0 || ISEC2_LastLat != 0 ) */
	  {
	    if ( grid.ysize > 1 )
	      {
		if ( ISEC2_ResFlag && ISEC2_LatIncr > 0 )
		  grid.yinc = ISEC2_LatIncr * 0.001;
		else
		  grid.yinc = (ISEC2_LastLat - ISEC2_FirstLat) * 0.001 / (grid.ysize - 1);
	      }
	    grid.yfirst = ISEC2_FirstLat * 0.001;
	    grid.ylast  = ISEC2_LastLat  * 0.001;
	    grid.ydef   = 2;	    
	  }
	break;
      }
    case GRID_SPECTRAL:
      {
	grid.size  = ISEC4_NumValues;
	grid.trunc = ISEC2_PentaJ;
	break;
      }
    case GRID_GME:
      {
	grid.size  = ISEC4_NumValues;
	grid.nd    = ISEC2_GME_ND;
	grid.ni    = ISEC2_GME_NI;
	grid.ni2   = ISEC2_GME_NI2;
	grid.ni3   = ISEC2_GME_NI3;
	break;
      }
    case GRID_GENERIC:
      {
	grid.size  = ISEC4_NumValues;
	grid.xsize = 0;
	grid.ysize = 0;
	break;
      }
    default:
      {
	Error(func, "%s grid unsupported", gridNamePtr(gridtype));
	break;
      }
    }

  grid.isRotated = FALSE;
  if ( gribGetIsRotated(isec2) )
    {
      grid.isRotated = TRUE;
      grid.ypole     = - ISEC2_LatSP * 0.001;
      grid.xpole     =   ISEC2_LonSP * 0.001 - 180;
      grid.angle     = 0;
    }

  grid.xvals = NULL;
  grid.yvals = NULL;
  grid.type  = gridtype;

  gridID = varDefGrid(vlistID, grid, 0);

  leveltype = gribGetZaxisType(ISEC1_LevelType);

  if ( leveltype == ZAXIS_HYBRID )
    {
      int vctsize = ISEC2_NumVCP;
      double *vctptr = &fsec2[10];

      varDefVCT(vctsize, vctptr);
    }

  lbounds = gribGetZaxisHasBounds(ISEC1_LevelType);

  if ( prec > DATATYPE_PACK3 ) prec = DATATYPE_PACK3;
  if ( prec < DATATYPE_PACK  ) prec = DATATYPE_PACK;

  varAddRecord(recID, code, gridID, leveltype, lbounds, level1, level2,
	       prec, &varID, &levelID, numavg, ISEC1_CodeTable);

  (*record).varID   = varID;
  (*record).levelID = levelID;

  if ( varInqInst(varID) == UNDEFID )
    {
      int center, subcenter, instID;
      center    = ISEC1_CenterID;
      subcenter = ISEC1_SubCenterID;
      instID    = institutInq(center, subcenter, NULL, NULL);
      if ( instID == UNDEFID )
	instID = institutDef(center, subcenter, NULL, NULL);
      varDefInst(varID, instID);
    }

  if ( varInqModel(varID) == UNDEFID )
    {
      int modelID;
      modelID = modelInq(varInqInst(varID), ISEC1_ModelID, NULL);
      if ( modelID == UNDEFID )
	modelID = modelDef(varInqInst(varID), ISEC1_ModelID, NULL);
      varDefModel(varID, modelID);
    }

  if ( varInqTable(varID) == UNDEFID )
    {
      int tableID;
      tableID = tableInq(varInqModel(varID), ISEC1_CodeTable, NULL);
      if ( tableID == UNDEFID )
	tableID = tableDef(varInqModel(varID), ISEC1_CodeTable, NULL);
      varDefTable(varID, tableID);
    }

  streams[streamID].tsteps[tsID].nallrecs++;
  streams[streamID].nrecs++;

  if ( CDI_Debug )
    Message(func, "varID = %d  code = %d  leveltype = %d  gridID = %d  levelID = %d",
	    varID, code, leveltype, gridID, levelID);
}


void grbScanTimestep1(int streamID)
{
  static char func[] = "grbScanTimestep1";
  int *isec0, *isec1, *isec2, *isec3, *isec4;
  double fsec2[512], fsec3[2], *fsec4 = NULL;
  off_t recpos = 0;
  unsigned char *gribbuffer = NULL;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int status;
  int fileID;
  int rcode = 0, level1 = 0, level2 = 0, vdate = 0, vtime = 0;
  DateTime datetime, datetime0;
  int tsID;
  int varID;
  size_t readsize;
  int nrecords, nrecs, recID;
  int prec;
  long recsize = 0;
  int warn_numavg = TRUE;
  int taxisID = -1;
  int rdate = 0, rtime = 0, tunit = 0, fcast = 0;
  TAXIS *taxis;
  int vlistID;
  COMPVAR compVar, compVar0;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 0;

  isec0 = streams[streamID].record->sec0;
  isec1 = streams[streamID].record->sec1;
  isec2 = streams[streamID].record->sec2;
  isec3 = streams[streamID].record->sec3;
  isec4 = streams[streamID].record->sec4;

  tsID  = tstepsNewEntry(streamID);
  taxis = &streams[streamID].tsteps[tsID].taxis;

  if ( tsID != 0 )
    Error(func, "Internal problem! tstepsNewEntry returns %d", tsID);

  fileID = streamInqFileID(streamID);

  nrecs = 0;
  while ( TRUE )
    {
      recsize = gribGetSize(fileID);
      recpos  = fileGetPos(fileID);

      if ( recsize == 0 )
	{
	  streams[streamID].ntsteps = 1;
	  break;
	}
      if ( recsize > buffersize )
	{
	  buffersize = recsize;
	  gribbuffer = (unsigned char *) realloc(gribbuffer, buffersize);
	}

      readsize = recsize;
      status = gribRead(fileID, gribbuffer, &readsize);
      if ( status ) break;

      gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, fsec4,
	       ipunp, (int *) gribbuffer, recsize, &iword, "J", &iret);

      rcode    = ISEC1_Parameter;
      level1   = ISEC1_Level1;
      level2   = ISEC1_Level2;
      if ( ISEC1_LevelType == 100 ) level1 *= 100;

      switch (ISEC4_NumBits)
	{
	case 8:
	  prec = DATATYPE_PACK1;
          break;
	case 16:
	  prec = DATATYPE_PACK2;
          break;
	case 24:
	  prec = DATATYPE_PACK3;
          break;
	default:
	  prec = DATATYPE_PACK;
          break;
	}

      gribDateTime(isec1, &vdate, &vtime);

      if ( nrecs == 0 )
	{
	  datetime0.date = vdate;
	  datetime0.time = vtime;
	  rdate = gribRefDate(isec1);
	  rtime = gribRefTime(isec1);
	  tunit = gribTimeUnit(isec1);
	  fcast = gribTimeIsFC(isec1);
	}
      else
	{
	  datetime.date  = vdate;
	  datetime.time  = vtime;
	  compVar.code   = rcode;
          compVar.table  = ISEC1_CodeTable;
          compVar.level1 = level1;
          compVar.level2 = level2;
          compVar.ltype  = ISEC1_LevelType;
	  for ( recID = 0; recID < nrecs; recID++ )
	    {
	      compVar0.code   = streams[streamID].tsteps[0].records[recID].code;
	      compVar0.table  = streams[streamID].tsteps[0].records[recID].table;
	      compVar0.level1 = streams[streamID].tsteps[0].records[recID].ilevel;
	      compVar0.level2 = streams[streamID].tsteps[0].records[recID].ilevel2;
	      compVar0.ltype  = streams[streamID].tsteps[0].records[recID].ltype;

	      if ( memcmp(&compVar0, &compVar, sizeof(COMPVAR)) == 0 ) break;
	    }
	  if ( recID < nrecs ) break;
	  if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) )
	    Warning(func, "Inconsistent verification time for code %d level %d", rcode, level1);
	}

      if ( ISEC1_AvgNum )
	{
	  if (  taxis->numavg && warn_numavg && (taxis->numavg != ISEC1_AvgNum) )
	    {
	      Message(func, "Change numavg from %d to %d not allowed!",
		      taxis->numavg, ISEC1_AvgNum);
	      warn_numavg = FALSE;
	    }
	  else
	    {
	      taxis->numavg = ISEC1_AvgNum;
	    }
	}

      nrecs++;

      if ( CDI_Debug )
	Message(func, "%4d %8d %4d  %8d %8d %6d", nrecs, (int)recpos, rcode, level1, vdate, vtime);

      grbAddRecord(streamID, rcode, isec1, isec2, fsec2, isec4, recsize, recpos, prec);
    }

  streams[streamID].rtsteps = 1;

  cdiGenVars(streamID);

  if ( fcast )
    {
      taxisID = taxisCreate(TAXIS_RELATIVE);
      taxis->type  = TAXIS_RELATIVE;
      taxis->rdate = rdate;
      taxis->rtime = rtime;
      taxis->unit  = tunit;
    }
  else
    {
      taxisID = taxisCreate(TAXIS_ABSOLUTE);
      taxis->type  = TAXIS_ABSOLUTE;
    }

  taxis->vdate = datetime0.date;
  taxis->vtime = datetime0.time;

  vlistID = streamInqVlist(streamID);
  vlistDefTaxis(vlistID, taxisID);

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

  streams[streamID].record->buffer     = gribbuffer;
  streams[streamID].record->buffersize = buffersize;

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


int grbScanTimestep2(int streamID)
{
  static char func[] = "grbScanTimestep2";
  int *isec0, *isec1, *isec2, *isec3, *isec4;
  double fsec2[512], fsec3[2], *fsec4 = NULL;
  off_t recpos = 0;
  unsigned char *gribbuffer = NULL;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int status;
  int fileID;
  int rcode = 0, level1 = 0, level2 = 0, vdate = 0, vtime = 0;
  int tsID;
  int varID, gridID;
  size_t readsize;
  int nrecords, nrecs, recID, rindex;
  long recsize = 0;
  int warn_numavg = TRUE;
  int taxisID = -1;
  int nextstep;
  TAXIS *taxis;
  int vlistID;
  COMPVAR compVar, compVar0;

  streamCheckID(func, streamID);

  streams[streamID].curTsID = 1;

  isec0 = streams[streamID].record->sec0;
  isec1 = streams[streamID].record->sec1;
  isec2 = streams[streamID].record->sec2;
  isec3 = streams[streamID].record->sec3;
  isec4 = streams[streamID].record->sec4;

  fileID  = streamInqFileID(streamID);
  vlistID = streamInqVlist(streamID);
  taxisID = vlistInqTaxis(vlistID);

  gribbuffer = (unsigned char *) streams[streamID].record->buffer;
  buffersize = streams[streamID].record->buffersize;
                                          
  tsID = streams[streamID].rtsteps;
  if ( tsID != 1 )
    Error(func, "Internal problem! unexpeceted timestep %d", tsID+1);

  taxis = &streams[streamID].tsteps[tsID].taxis;

  fileSetPos(fileID, streams[streamID].tsteps[tsID].position, SEEK_SET);

  cdiCreateRecords(streamID, tsID);

  nrecords = streams[streamID].tsteps[tsID].nallrecs;
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
      recsize = gribGetSize(fileID);
      recpos  = fileGetPos(fileID);
      if ( recsize == 0 )
	{
	  streams[streamID].ntsteps = 2;
	  break;
	}
      if ( recsize > buffersize )
	{
	  buffersize = recsize;
	  gribbuffer = (unsigned char *) realloc(gribbuffer, (size_t)buffersize);
	}

      readsize = recsize;
      status = gribRead(fileID, gribbuffer, &readsize);
      if ( status ) break;

      gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, fsec4,
	       ipunp, (int *) gribbuffer, recsize, &iword, "J", &iret);

      rcode     = ISEC1_Parameter;
      level1    = ISEC1_Level1;
      level2    = ISEC1_Level2;
      if ( ISEC1_LevelType == 100 ) level1 *= 100;

      gribDateTime(isec1, &vdate, &vtime);

      if ( rindex == 0 )
	{
	  if ( taxisInqType(taxisID) == TAXIS_RELATIVE )
	    {
	      taxis->type  = TAXIS_RELATIVE;
	      taxis->rdate = gribRefDate(isec1);
	      taxis->rtime = gribRefTime(isec1);
	      taxis->unit  = gribTimeUnit(isec1);
	    }
	  else
	    {
	      taxis->type  = TAXIS_ABSOLUTE;
	    }
	  taxis->vdate = vdate;
	  taxis->vtime = vtime;
	}

      if ( ISEC1_AvgNum )
	{
	  if (  taxis->numavg && warn_numavg &&
		(taxis->numavg != ISEC1_AvgNum) )
	    {
	  /*
	      Message(func, "change numavg from %d to %d not allowed!",
		      taxis->numavg, ISEC1_AvgNum);
	  */
	      warn_numavg = FALSE;
	    }
	  else
	    {
	      taxis->numavg = ISEC1_AvgNum;
	    }
	}

      compVar.code   = rcode;
      compVar.table  = ISEC1_CodeTable;
      compVar.level1 = level1;
      compVar.level2 = level2;
      compVar.ltype  = ISEC1_LevelType;
      nextstep = FALSE;
      for ( recID = 0; recID < nrecords; recID++ )
	{
	  compVar0.code   = streams[streamID].tsteps[tsID].records[recID].code;
	  compVar0.table  = streams[streamID].tsteps[tsID].records[recID].table;
	  compVar0.level1 = streams[streamID].tsteps[tsID].records[recID].ilevel;
	  compVar0.level2 = streams[streamID].tsteps[tsID].records[recID].ilevel2;
	  compVar0.ltype  = streams[streamID].tsteps[tsID].records[recID].ltype;
	  if ( memcmp(&compVar0, &compVar, sizeof(COMPVAR)) == 0 )
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
	  Warning(func, "code %d level %d not found at timestep %d", rcode, level1, tsID+1);
	  return (CDI_EUFSTRUCT);
	}

      if ( nextstep ) break;

      if ( CDI_Debug )
	Message(func, "%4d %8d %4d %8d %8d %6d", rindex+1, (int)recpos, rcode, level1, vdate, vtime);

      streams[streamID].tsteps[tsID].records[recID].size = recsize;

      compVar0.code   = streams[streamID].tsteps[tsID].records[recID].code;
      compVar0.table  = streams[streamID].tsteps[tsID].records[recID].table;
      compVar0.level1 = streams[streamID].tsteps[tsID].records[recID].ilevel;
      compVar0.level2 = streams[streamID].tsteps[tsID].records[recID].ilevel2;
      compVar0.ltype  = streams[streamID].tsteps[tsID].records[recID].ltype;

      if ( memcmp(&compVar0, &compVar, sizeof(COMPVAR)) != 0 )
	{
	  Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		  tsID, recID,
		  streams[streamID].tsteps[tsID].records[recID].code, rcode,
		  streams[streamID].tsteps[tsID].records[recID].ilevel, level1);
	  return (CDI_EUFSTRUCT);
	}

      streams[streamID].tsteps[1].records[recID].position = recpos;
      varID = streams[streamID].tsteps[tsID].records[recID].varID;
      gridID = vlistInqVarGrid(vlistID, varID);
      if ( gridInqSize(gridID) == 1 && gridInqType(gridID) == GRID_LONLAT )
	{
	  if ( gridInqXval(gridID, 0) != ISEC2_FirstLon*0.001 ||
	       gridInqYval(gridID, 0) != ISEC2_FirstLat*0.001 )
	    gridChangeType(gridID, GRID_TRAJECTORY);
	}
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

  streams[streamID].record->buffer     = gribbuffer;
  streams[streamID].record->buffersize = buffersize;

  return (0);
}


int grbInqContents(int streamID)
{
  static char func[] = "grbInqContents";
  int fileID;
  int status = 0;

  streamCheckID(func, streamID);

  fileID = streamInqFileID(streamID);

  streams[streamID].curTsID = 0;

  grbScanTimestep1(streamID);
 
  if ( streams[streamID].ntsteps == -1 ) status = grbScanTimestep2(streamID);

  fileSetPos(fileID, 0, SEEK_SET);

  return (status);
}


int grbInqTimestep(int streamID, int tsID)
{
  static char func[] = "grbInqTimestep";
  int ntsteps, nrecs;

  streamCheckID(func, streamID);

  if ( tsID == 0 && streams[streamID].rtsteps == 0 )
    Error(func, "Call to cdiInqContents missing!");

  if ( CDI_Debug )
    Message(func, "tsid = %d rtsteps = %d", tsID, streams[streamID].rtsteps);
  
  ntsteps = UNDEFID;
  while ( (tsID + 1) > streams[streamID].rtsteps && ntsteps == UNDEFID )
    {
      ntsteps = grbScanTimestep(streamID);
      if ( ntsteps == CDI_EUFSTRUCT )
	{
	  streams[streamID].ntsteps = streams[streamID].rtsteps;
	  break;
	}
    }

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


void grbReadVarDP(int streamID, int varID, double *data, int *nmiss)
{
  static char func[] = "grbReadVarDP";
  int fileID;
  int levelID, nlevs, gridID, gridsize;
  unsigned char *gribbuffer;
  int tsID, recID;
  long recsize;
  off_t recpos, currentfilepos;
  int imiss;
  int vlistID;

  streamCheckID(func, streamID);

  gribbuffer = (unsigned char *) streams[streamID].record->buffer;

  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;

  nlevs    = streams[streamID].vars[varID].nlevs;
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d gridsize = %d", nlevs, gridID, gridsize);

  currentfilepos = fileGetPos(fileID);

  *nmiss = 0;
  for ( levelID = 0; levelID < nlevs; levelID++ )
    {
      recID   = streams[streamID].vars[varID].level[levelID];
      recpos  = streams[streamID].tsteps[tsID].records[recID].position;
      recsize = streams[streamID].tsteps[tsID].records[recID].size;

      fileSetPos(fileID, recpos, SEEK_SET);

      fileRead(fileID, gribbuffer, recsize);

      grbDecodeDataDP(gribbuffer, recsize, &data[levelID*gridsize], gridsize, streams[streamID].unreduced, &imiss);

      *nmiss += imiss;
    }

  fileSetPos(fileID, currentfilepos, SEEK_SET);
}

void grbReadVarSliceDP(int streamID, int varID, int levelID, double *data, int *nmiss)
{
  static char func[] = "grbReadVarSliceDP";
  int fileID;
  int gridID, gridsize;
  unsigned char *gribbuffer;
  long recsize;
  off_t recpos, currentfilepos;
  int tsID, recID;
  int vlistID;

  streamCheckID(func, streamID);

  gribbuffer = (unsigned char *) streams[streamID].record->buffer;

  vlistID  = streamInqVlist(streamID);
  gridID   = vlistInqVarGrid(vlistID, varID);
  gridsize = gridInqSize(gridID);
  tsID     = streams[streamID].curTsID;

  if ( CDI_Debug )
    Message(func, "gridID = %d gridsize = %d", gridID, gridsize);

  fileID = streamInqFileID(streamID);

  currentfilepos = fileGetPos(fileID);

  recID   = streams[streamID].vars[varID].level[levelID];
  recpos  = streams[streamID].tsteps[tsID].records[recID].position;
  recsize = streams[streamID].tsteps[tsID].records[recID].size;

  if ( recsize == 0 )
    Error(func, "Internal problem! Recordsize is zero for record %d at timestep %d",
	  recID+1, tsID+1);

  fileSetPos(fileID, recpos, SEEK_SET);

  fileRead(fileID, gribbuffer, recsize);

  grbDecodeDataDP(gribbuffer, recsize, data, gridsize, streams[streamID].unreduced, nmiss);

  fileSetPos(fileID, currentfilepos, SEEK_SET);
}

void grbDefInstitut(int *isec1, int center, int subcenter)
{
  ISEC1_CenterID    = center;
  ISEC1_SubCenterID = subcenter;
}


void grbDefModel(int *isec1, int model)
{
  ISEC1_ModelID = model;
}


void grbDefCode(int *isec1, int code, int codetable)
{
  ISEC1_CodeTable = codetable;
  if ( code < 0 ) code = -code;
  ISEC1_Parameter = code;
}


void grbDefTime(int *isec1, int date, int time, int numavg, int timeID)
{
  int year, month, day, hour, minute;
  int century = 0;
  int timetype = -1;

  if ( timeID != -1 ) timetype = taxisInqType(timeID);

  if ( timetype == TAXIS_RELATIVE )
    {
      sUnit tunit = {0, 1};
      int rdate, rtime;
      double value;
      
      rdate   = taxisInqRdate(timeID);
      rtime   = taxisInqRtime(timeID);
      year    =  rdate / 10000;
      month   = (rdate - year*10000) / 100;
      day     =  rdate - year*10000 - month*100;
      hour    =  rtime / 100;
      minute  =  rtime - hour*100;
      century =  year / 100;

      ISEC1_Year   = year - century*100;
      ISEC1_Month  = month;
      ISEC1_Day    = day;
      ISEC1_Hour   = hour;
      ISEC1_Minute = minute;
      /*
      printf("year %d, month %d, day %d, hour %d, minute %d\n", year, month, day, hour, minute);
      */
      InvCalendar(year, month, day, hour, minute, 0.0, tunit, &value);
      tunit.origin = value;
      switch (taxisInqTunit(timeID))
	{
	case TUNIT_MINUTE:  tunit.factor =    60; ISEC1_TimeUnit = ISEC1_TABLE4_MINUTE; break;
	case TUNIT_HOUR:    tunit.factor =  3600; ISEC1_TimeUnit = ISEC1_TABLE4_HOUR; break;
	case TUNIT_DAY:     tunit.factor = 86400; ISEC1_TimeUnit = ISEC1_TABLE4_DAY; break;
	default:            tunit.factor =  3600; ISEC1_TimeUnit = ISEC1_TABLE4_HOUR; break;
	}
      year   =  date / 10000;
      month  = (date - year*10000) / 100;
      day    =  date - year*10000 - month*100;
      hour   =  time / 100;
      minute =  time - hour*100;
      /*
      printf("date time %d %d\n", date, time);
      printf("year %d, month %d, day %d, hour %d, minute %d\n", year, month, day, hour, minute);
      */
      InvCalendar(year, month, day, hour, minute, 0.0, tunit, &value);
      
      if ( value > 255 ) timetype = TAXIS_ABSOLUTE;
      
      ISEC1_TimeRange = 0;
      ISEC1_TimePeriod1 = (int) value;
    }

  if ( timetype == TAXIS_ABSOLUTE )
    {
      year    =  date / 10000;
      month   = (date - year*10000) / 100;
      day     =  date - year*10000 - month*100;
      hour    =  time / 100;
      minute  =  time - hour*100;
      century =  year / 100;

      ISEC1_Year     = year - century*100;
      ISEC1_Month    = month;
      ISEC1_Day      = day;
      ISEC1_Hour     = hour;
      ISEC1_Minute   = minute;

      ISEC1_TimeUnit = 0;

      if ( numavg > 0 )
	ISEC1_TimeRange = 0;
      else
	ISEC1_TimeRange = 10;

      ISEC1_TimePeriod1 = 0;
    }

  ISEC1_TimePeriod2    = 0;
  ISEC1_AvgNum         = numavg;
  ISEC1_AvgMiss        = 0;
  ISEC1_Century        = century + 1;
  ISEC1_DecScaleFactor = 0;
}


void grbDefGrid(int *isec1, int *isec2, int gridID)
{
  static char func[] = "grbDefGrid";
  int gridtype;

  ISEC1_Sec2Or3Flag = 128;
  
  gridtype = gridInqType(gridID);

  ISEC1_GridDefinition = 255;

  if ( gridtype == GRID_GENERIC )
    {
      int xsize, ysize;

      xsize = gridInqXsize(gridID);
      ysize = gridInqYsize(gridID);

      if ( (ysize == 32  || ysize == 48 || ysize == 64 ||
	    ysize == 96  || ysize == 160) && 
	   (xsize == 2*ysize || xsize == 1) )
	{
	  gridtype = GRID_GAUSSIAN;
	  gridChangeType(gridID, gridtype);
	}
      else if ( xsize == 1 && ysize == 1 )
	{
	  gridtype = GRID_LONLAT;
	  gridChangeType(gridID, gridtype);
	}
    }
  else if ( gridtype == GRID_CURVILINEAR )
    {
      gridtype = GRID_LONLAT;
    }

  ISEC2_Reduced = FALSE;

  switch (gridtype)
    {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
    case GRID_GAUSSIAN_REDUCED:
    case GRID_TRAJECTORY:
      {
	int nlon = 0, nlat;

	if ( gridtype == GRID_GAUSSIAN_REDUCED )
	  {
	    ISEC2_Reduced = TRUE;
	    nlon = 0;
	    gridInqRowlon(gridID, ISEC2_RowLonPtr);
	  }
	else
	  {
	    nlon = (int) gridInqXsize(gridID);
	  }

	nlat = (int) gridInqYsize(gridID);
	if ( gridtype == GRID_GAUSSIAN || gridtype == GRID_GAUSSIAN_REDUCED )
	  ISEC2_GridType = 4;
	else if ( gridtype == GRID_LONLAT && gridIsRotated(gridID) )
	  ISEC2_GridType = 10;
	else
	  ISEC2_GridType = 0;

	ISEC2_NumLon   = nlon;
	ISEC2_NumLat   = nlat;
	ISEC2_FirstLat = NINT(gridInqYval(gridID,      0)*1000);
	ISEC2_LastLat  = NINT(gridInqYval(gridID, nlat-1)*1000);
	ISEC2_FirstLon = NINT(gridInqXval(gridID,      0)*1000);
	ISEC2_LastLon  = NINT(gridInqXval(gridID, nlon-1)*1000);
	ISEC2_LonIncr  = NINT(gridInqXinc(gridID)*1000);
	if ( fabs(gridInqXinc(gridID)*1000 - ISEC2_LonIncr) > FLT_EPSILON )
	  ISEC2_LonIncr = 0;

	if ( gridtype == GRID_GAUSSIAN )
	  ISEC2_NumPar = nlat/2;
	else
	  {
	    ISEC2_LatIncr = NINT(gridInqYinc(gridID)*1000);
	    if ( fabs(gridInqYinc(gridID)*1000 - ISEC2_LatIncr) > FLT_EPSILON )
	      ISEC2_LatIncr = 0;
	  }

	if ( ISEC2_LatIncr == 0 || ISEC2_LonIncr == 0 )
	  ISEC2_ResFlag = 0;
	else
	  ISEC2_ResFlag = 128;

	if ( gridIsRotated(gridID) )
	  {
	    ISEC2_LatSP = - NINT(gridInqYpole(gridID) * 1000);
	    ISEC2_LonSP =   NINT((gridInqXpole(gridID) + 180) * 1000);
	  }

	break;
      }
    case GRID_SPECTRAL:
      {
	ISEC2_GridType = 50;
	ISEC2_PentaJ  = gridInqTrunc(gridID);
	ISEC2_PentaK  = ISEC2_PentaJ;
	ISEC2_PentaM  = ISEC2_PentaJ;
	ISEC2_RepType = 1;
	ISEC2_RepMode = 1;
	break;
      }
    case GRID_GME:
      {
	ISEC2_GridType = 192;
	ISEC2_GME_ND  = gridInqGMEnd(gridID);
	ISEC2_GME_NI  = gridInqGMEni(gridID);
	ISEC2_GME_NI2 = gridInqGMEni2(gridID);
	ISEC2_GME_NI3 = gridInqGMEni3(gridID);
	ISEC2_GME_AFlag  = 0;
	ISEC2_GME_LatPP  = 90000;
	ISEC2_GME_LonPP  = 0;
	ISEC2_GME_LonMPL = 0;
	ISEC2_GME_BFlag  = 0;
	break;
      }
    default:
      Error(func, "%s grid unsupported!", gridNamePtr(gridtype));
    }

  ISEC2_ScanFlag = 0;
}


void grbDefLevel(int *isec1, int *isec2, double *fsec2, int zaxisID, int levelID)
{
  static char func[] = "grbDefLevel";
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

  ISEC2_NumVCP = 0;

  switch (leveltype)
    {
    case ZAXIS_SURFACE:
      {
	ISEC1_LevelType = LTYPE_SURFACE;
	ISEC1_Level1    = (int) zaxisInqLevel(zaxisID, levelID);
	ISEC1_Level2    = 0;
	break;
      }
    case ZAXIS_HYBRID:
      {
	int vctsize;

	if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
	  {
	    ISEC1_LevelType = LTYPE_HYBRID_LAYER;
	    ISEC1_Level1    = (int) zaxisInqLbound(zaxisID, levelID);
	    ISEC1_Level2    = (int) zaxisInqUbound(zaxisID, levelID);
	  }
	else
	  {
	    ISEC1_LevelType = LTYPE_HYBRID;
	    ISEC1_Level1    = (int) zaxisInqLevel(zaxisID, levelID);
	    ISEC1_Level2    = 0;
	  }

	vctsize = zaxisInqVctSize(zaxisID);
	if ( vctsize == 0 && warning )
	  {
	    Warning(func, "VCT missing. ( code = %d, zaxisID = %d )",
		    ISEC1_Parameter, zaxisID);
	    warning = 0;
	  }
	if ( vctsize > 255 )
	  {
	    ISEC2_NumVCP = 0;
	    if ( vct_warning )
	      {
		Warning(func, "VCT size of %d is to large (maximum is 255). Set to 0!", vctsize);
		vct_warning = 0;
	      }
	  }
	else
	  {
	    ISEC2_NumVCP = vctsize;
	    memcpy(&fsec2[10], zaxisInqVctPtr(zaxisID), vctsize*sizeof(double));
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
	    ISEC1_LevelType = LTYPE_99;
	    ISEC1_Level1    = ilevel;
	    ISEC1_Level2    = 0;
	  }
	else
	  {
	    ISEC1_LevelType = LTYPE_ISOBARIC;
	    ISEC1_Level1    = ilevel/100;
	    ISEC1_Level2    = 0;
	  }
	break;
      }
    case ZAXIS_HEIGHT:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	ISEC1_LevelType = LTYPE_HEIGHT;
	ISEC1_Level1    = ilevel;
	ISEC1_Level2    = 0;

	break;
      }
    case ZAXIS_ALTITUDE:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	ISEC1_LevelType = LTYPE_ALTITUDE;
	ISEC1_Level1    = ilevel;
	ISEC1_Level2    = 0;

	break;
      }
    case ZAXIS_DEPTH_BELOW_LAND:
      {
	if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
	  {
	    ISEC1_LevelType = LTYPE_LANDDEPTH_LAYER;
	    ISEC1_Level1    = (int) zaxisInqLbound(zaxisID, levelID);
	    ISEC1_Level2    = (int) zaxisInqUbound(zaxisID, levelID);
	  }
	else
	  {
	    level = zaxisInqLevel(zaxisID, levelID);

	    ilevel = (int) level;
	    ISEC1_LevelType = LTYPE_LANDDEPTH;
	    ISEC1_Level1    = ilevel;
	    ISEC1_Level2    = 0;
	  }

	break;
      }
    case ZAXIS_DEPTH_BELOW_SEA:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	ISEC1_LevelType = LTYPE_SEADEPTH;
	ISEC1_Level1    = ilevel;
	ISEC1_Level2    = 0;

	break;
      }
    case ZAXIS_ISENTROPIC:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	ISEC1_LevelType = 113;
	ISEC1_Level1    = ilevel;
	ISEC1_Level2    = 0;

	break;
      }
    default:
      {
	Error(func, "leveltype >%s< unsupported", zaxisNamePtr(leveltype));
	break;
      }
    }
}


void grbDefMask(int *isec3)
{
}


void grbDefData(int datatype, int *isec4, int gridID)
{
  ISEC4_NumValues = gridInqSize(gridID);

  if ( datatype != UNDEFID )
    {
      if ( datatype != DATATYPE_PACK  && datatype != DATATYPE_PACK1 &&
	   datatype != DATATYPE_PACK2 && datatype != DATATYPE_PACK3 )
	{
	  if   ( datatype == DATATYPE_REAL8 ) datatype = DATATYPE_PACK3;
	  else                           datatype = DATATYPE_PACK2;
	}
      datatype *= 8;
      if ( datatype == 8 || datatype == 16 || datatype == 24 )
	ISEC4_NumBits = datatype;
      else
	ISEC4_NumBits = 16;
    }
  else
    ISEC4_NumBits = 16;
}

void grbDefaultSec0(int *isec0)
{
  ISEC0_GRIB_Len     = 0;
  ISEC0_GRIB_Version = 0;
}

void grbDefaultSec1(int *isec1)
{
  ISEC1_CenterID    = 0;
  ISEC1_SubCenterID = 0;
  ISEC1_LocalFLag   = 0;
}

void grbDefaultSec4(int *isec4)
{
  isec4[2] = 0;
  isec4[3] = 0;
  isec4[4] = 0;
  isec4[5] = 0;
  isec4[6] = 0;
  isec4[7] = 0;
  isec4[8] = 0;
  isec4[9] = 0;
  isec4[10] = 0;
}


void grbWriteVarDP(int streamID, int varID, const double *data, int nmiss)
{
  static char func[] = "grbWriteVarDP";
  int fileID;
  int levelID, nlevs, gridID;
  int zaxisID;
  int isec0[2], isec1[4096], isec2[4096], isec3[2], isec4[512];
  double fsec2[512], fsec3[2];
  unsigned char *gribbuffer = NULL;
  size_t gribbuffersize = 0;
  long datasize, gribsize;
  int iret = 0, iword = 0;
  int date, time, code;
  int numavg = 0;
  int tsID;
  int instID, modelID, tableID;
  int datatype;
  int vlistID;

  if ( CDI_Debug ) Message(func, "streamID = %d  varID = %d", streamID, varID);

  memset(isec1, 0, 32*sizeof(int));
  fsec2[0] = 0; fsec2[1] = 0;

  fileID  = streamInqFileID(streamID);
  vlistID = streamInqVlist(streamID);
  tsID    = streams[streamID].curTsID;

  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);

  nlevs    = zaxisInqSize(zaxisID);

  code     = vlistInqVarCode(vlistID, varID);
  date     = streams[streamID].tsteps[tsID].taxis.vdate;
  time     = streams[streamID].tsteps[tsID].taxis.vtime;
  if ( vlistInqVarAverage(vlistID, varID) )
    numavg = streams[streamID].tsteps[tsID].taxis.numavg;
  datasize = gridInqSize(gridID);

  if ( CDI_Debug )
    Message(func, "nlevs = %d gridID = %d datasize = %d",
	    nlevs, gridID, datasize);

  gribbuffersize = datasize*4+3000;
  gribbuffer = (unsigned char *) malloc(gribbuffersize);
  gribsize   = gribbuffersize / sizeof(int);

  grbDefaultSec0(isec0);
  grbDefaultSec1(isec1);
  grbDefaultSec4(isec4);

  if ( vlistInqInstitut(vlistID) != UNDEFID )
    instID = vlistInqInstitut(vlistID);
  else
    instID = vlistInqVarInstitut(vlistID, varID);

  if ( instID != UNDEFID )
    {
      int center, subcenter;
      center    = institutInqCenter(instID);
      subcenter = institutInqSubcenter(instID);
      grbDefInstitut(isec1, center, subcenter);
    }

  if ( vlistInqModel(vlistID) != UNDEFID )
    modelID = vlistInqModel(vlistID);
  else
    modelID = vlistInqVarModel(vlistID, varID);

  if ( modelID != UNDEFID )
    grbDefModel(isec1, modelInqGribID(modelID));

  tableID = vlistInqVarTable(vlistID, varID);
  datatype = vlistInqVarDatatype(vlistID, varID);

  grbDefCode(isec1, code, tableInqNum(tableID));
  grbDefTime(isec1, date, time, numavg, vlistInqTaxis(vlistID));
  grbDefGrid(isec1, isec2, gridID);
  grbDefData(datatype, isec4, gridID);

  if ( nmiss > 0 )
    {
      FSEC3_MissVal = vlistInqVarMissval(vlistID, varID);
      ISEC1_Sec2Or3Flag |= 64;
    }

  for ( levelID = 0;  levelID < nlevs; levelID++ )
    {
      grbDefLevel(isec1, isec2, fsec2, zaxisID, levelID);
      grbDefMask(isec3);

      gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, (double*) &data[levelID*datasize],
	       datasize, (int *) gribbuffer, gribsize, &iword, "C", &iret);

      if ( iret ) Error(func, "Problem during GRIB encode (errno = %d)!", iret);;
      
      gribWrite(fileID, gribbuffer, iword*sizeof(int));
    }

  if ( gribbuffer ) free(gribbuffer);
}


void grbWriteVarSliceDP(int streamID, int varID, int levelID, const double *data, int nmiss)
{
  static char func[] = "grbWriteVarSliceDP";
  int fileID;
  int gridID;
  int zaxisID;
  int isec0[2], isec1[4096], isec2[4096], isec3[2], isec4[512];
  double fsec2[512], fsec3[2];
  unsigned char *gribbuffer = NULL;
  size_t gribbuffersize = 0;
  long datasize, gribsize;
  int iret = 0, iword = 0;
  int date, time, code;
  int numavg = 0;
  int tsID;
  int instID, modelID, tableID;
  int datatype;
  int vlistID;

  memset(isec1, 0, 32*sizeof(int));
  fsec2[0] = 0; fsec2[1] = 0;

  vlistID  = streamInqVlist(streamID);
  fileID   = streamInqFileID(streamID);
  tsID     = streams[streamID].curTsID;

  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);

  if ( CDI_Debug )
    Message(func, "gridID = %d zaxisID = %d", gridID, zaxisID);

  code     = vlistInqVarCode(vlistID, varID);
  date     = streams[streamID].tsteps[tsID].taxis.vdate;
  time     = streams[streamID].tsteps[tsID].taxis.vtime;
  if ( vlistInqVarAverage(vlistID, varID) )
    numavg = streams[streamID].tsteps[tsID].taxis.numavg;
  datasize = gridInqSize(gridID);

  gribbuffersize = datasize*4+3000;
  gribbuffer = (unsigned char *) malloc(gribbuffersize);
  gribsize   = gribbuffersize / sizeof(int);

  grbDefaultSec0(isec0);
  grbDefaultSec1(isec1);
  grbDefaultSec4(isec4);

  if ( vlistInqInstitut(vlistID) != UNDEFID )
    instID = vlistInqInstitut(vlistID);
  else
    instID = vlistInqVarInstitut(vlistID, varID);

  if ( instID != UNDEFID )
    {
      int center, subcenter;
      center    = institutInqCenter(instID);
      subcenter = institutInqSubcenter(instID);
      grbDefInstitut(isec1, center, subcenter);
    }

  if ( vlistInqModel(vlistID) != UNDEFID )
    modelID = vlistInqModel(vlistID);
  else
    modelID = vlistInqVarModel(vlistID, varID);

  if ( modelID != UNDEFID )
    grbDefModel(isec1, modelInqGribID(modelID));

  tableID = vlistInqVarTable(vlistID, varID);
  datatype = vlistInqVarDatatype(vlistID, varID);

  grbDefCode(isec1, code, tableInqNum(tableID));
  grbDefTime(isec1, date, time, numavg, vlistInqTaxis(vlistID));
  grbDefGrid(isec1, isec2, gridID);
  grbDefLevel(isec1, isec2, fsec2, zaxisID, levelID);
  grbDefMask(isec3);
  grbDefData(datatype, isec4, gridID);

  if ( nmiss > 0 )
    {
      FSEC3_MissVal = vlistInqVarMissval(vlistID, varID);
      ISEC1_Sec2Or3Flag |= 64;
    }

  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, (double*) data,
	   datasize, (int *) gribbuffer, gribsize, &iword, "C", &iret);

  if ( iret ) Error(func, "Problem during GRIB encode (errno = %d)!", iret);;

  gribWrite(fileID, gribbuffer, iword*sizeof(int));

  if ( gribbuffer ) free(gribbuffer);
}


int grbCopyRecord(int ostreamID, int istreamID)
{
  static char func[] = "grbCopyRecord";
  int ifileID, ofileID;
  int tsID, recID, vrecID;
  long recsize;
  long bufsize;
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

  bufsize = recsize == (recsize>>3)<<3 ? recsize : (1+(recsize>>3))<<3;

  buffer = (char *) malloc(bufsize);

  fileRead(ifileID, buffer, recsize);
  
  while ( recsize & 7 ) buffer[recsize++] = 0;

  fileWrite(ofileID, buffer, recsize);

  free(buffer);

  return (status);
}


int grbWriteRecord(int streamID, const double *data, int nmiss)
{
  static char func[] = "grbWriteRecord";
  int fileID;
  int status = 0;
  int levelID, gridID;
  int zaxisID;
  int isec0[2], isec1[4096], isec2[4096], isec3[2], isec4[512];
  double fsec2[512], fsec3[2];
  unsigned char *gribbuffer = NULL;
  size_t gribbuffersize = 0;
  long datasize, gribsize;
  int iret = 0, iword = 0;
  int date, time, code;
  int numavg = 0;
  int tsID;
  int instID, modelID, tableID;
  int varID;
  int datatype;
  int vlistID;

  streamCheckID(func, streamID);

  memset(isec1, 0, 32*sizeof(int));
  fsec2[0] = 0; fsec2[1] = 0;

  varID    = streams[streamID].record->varID;
  levelID  = streams[streamID].record->levelID;

  fileID   = streamInqFileID(streamID);
  vlistID  = streamInqVlist(streamID);
  tsID     = streams[streamID].curTsID;

  gridID   = vlistInqVarGrid(vlistID, varID);
  zaxisID  = vlistInqVarZaxis(vlistID, varID);

  if ( CDI_Debug )
    Message(func, "gridID = %d zaxisID = %d", gridID, zaxisID);

  code     = vlistInqVarCode(vlistID, varID);
  date     = streams[streamID].tsteps[tsID].taxis.vdate;
  time     = streams[streamID].tsteps[tsID].taxis.vtime;
  if ( vlistInqVarAverage(vlistID, varID) )
    numavg = streams[streamID].tsteps[tsID].taxis.numavg;
  datasize = gridInqSize(gridID);

  gribbuffersize = datasize*4+3000;
  gribbuffer = (unsigned char *) malloc(gribbuffersize);
  gribsize   = gribbuffersize / sizeof(int);

  grbDefaultSec0(isec0);
  grbDefaultSec1(isec1);
  grbDefaultSec4(isec4);

  if ( vlistInqInstitut(vlistID) != UNDEFID )
    instID = vlistInqInstitut(vlistID);
  else
    instID = vlistInqVarInstitut(vlistID, varID);

  if ( instID != UNDEFID )
    {
      int center, subcenter;
      center    = institutInqCenter(instID);
      subcenter = institutInqSubcenter(instID);
      grbDefInstitut(isec1, center, subcenter);
    }

  if ( vlistInqModel(vlistID) != UNDEFID )
    modelID = vlistInqModel(vlistID);
  else
    modelID = vlistInqVarModel(vlistID, varID);

  if ( modelID != UNDEFID )
    grbDefModel(isec1, modelInqGribID(modelID));

  tableID = vlistInqVarTable(vlistID, varID);
  datatype = vlistInqVarDatatype(vlistID, varID);

  grbDefCode(isec1, code, tableInqNum(tableID));
  grbDefTime(isec1, date, time, numavg, vlistInqTaxis(vlistID));
  grbDefGrid(isec1, isec2, gridID);
  grbDefLevel(isec1, isec2, fsec2, zaxisID, levelID);
  grbDefMask(isec3);
  grbDefData(datatype, isec4, gridID);

  if ( nmiss > 0 )
    {
      FSEC3_MissVal = vlistInqVarMissval(vlistID, varID);
      ISEC1_Sec2Or3Flag |= 64;
    }

  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, (double*) data,
	   datasize, (int *) gribbuffer, gribsize, &iword, "C", &iret);

  if ( iret ) Error(func, "Problem during GRIB encode (errno = %d)!", iret);;

  /*  gribPrintSec2DP(isec0, isec2, fsec2); */
  status = gribWrite(fileID, gribbuffer, iword*sizeof(int));

  if ( gribbuffer ) free(gribbuffer);

  return (status);
}


void streamInqGinfo(int streamID, int *intnum, float *fltnum)
{
  static char func[] = "streamInqGinfo";
  int recID, vrecID, tsID;
  int filetype;
  void *gribbuffer;
  long recsize;
  off_t recpos;

  streamCheckID(func, streamID);

  filetype = streams[streamID].filetype;

  if ( filetype == FILETYPE_GRB )
    {
      tsID    = streams[streamID].curTsID;
      vrecID  = streams[streamID].tsteps[tsID].curRecID;
      recID   = streams[streamID].tsteps[tsID].recIDs[vrecID];
      recpos  = streams[streamID].tsteps[tsID].records[recID].position;
      recsize = streams[streamID].tsteps[tsID].records[recID].size;

      gribbuffer = streams[streamID].record->buffer;

      gribGinfo(recpos, recsize, (unsigned char *) gribbuffer, intnum, fltnum);
    }
}

