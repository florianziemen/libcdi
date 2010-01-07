#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <float.h>  /* FLT_EPSILON */

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "file.h"
#include "varscan.h"
#include "datetime.h"
#include "vlist.h"
#include "stream_grb.h"

#if  defined  (HAVE_LIBCGRIBEX)
#  include "cgribex.h"
#endif

extern int cdiInventoryMode;

typedef struct {
  int param;
  int level1;
  int level2;
  int ltype;
} compvar_t; 


static
int cgribexGetGridType(int *isec2)
{
  /*  static char func[] = "cgribexGetGridType"; */
  int gridtype = 0;

  switch (ISEC2_GridType)
    {
    case  GTYPE_LATLON:
    case  GTYPE_LATLON_ROT:
      {
	gridtype = GRID_LONLAT;
	break;
      }
    case  GTYPE_LCC:
      {
	gridtype = GRID_LCC;
	break;
      }
    case  GTYPE_GAUSSIAN:
      {
	if ( ISEC2_Reduced )
	  gridtype = GRID_GAUSSIAN_REDUCED;
	else
	  gridtype = GRID_GAUSSIAN;
	break;
      }
    case  GTYPE_SPECTRAL:
      {
	gridtype = GRID_SPECTRAL;
	break;
      }
    case  GTYPE_GME:
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

static
int cgribexGetIsRotated(int *isec2)
{
  /*  static char func[] = "cgribexGetIsRotated"; */
  int isRotated = 0;

  if ( ISEC2_GridType == 10 )
    {
      isRotated = 1;
    }

  return (isRotated);
}


static
int cgribexGetZaxisType(int grb_ltype)
{
  int zaxistype = 0;

  switch ( grb_ltype )
    {
    case LTYPE_SURFACE:
      {
	zaxistype = ZAXIS_SURFACE;
	break;
      }
    case LTYPE_MEANSEA:
      {
	zaxistype = ZAXIS_MEANSEA;
	break;
      }
    case LTYPE_99:
    case LTYPE_ISOBARIC:
      {
	zaxistype = ZAXIS_PRESSURE;
	break;
      }
    case LTYPE_HEIGHT:
      {
	zaxistype = ZAXIS_HEIGHT;
	break;
      }
    case LTYPE_ALTITUDE:
      {
	zaxistype = ZAXIS_ALTITUDE;
	break;
      }
    case LTYPE_SIGMA:
      {
	zaxistype = ZAXIS_SIGMA;
	break;
      }
    case LTYPE_HYBRID:
    case LTYPE_HYBRID_LAYER:
      {
	zaxistype = ZAXIS_HYBRID;
	break;
      }
    case LTYPE_LANDDEPTH:
    case LTYPE_LANDDEPTH_LAYER:
      {
	zaxistype = ZAXIS_DEPTH_BELOW_LAND;
	break;
      }
    case LTYPE_ISENTROPIC:
      {
	zaxistype = ZAXIS_ISENTROPIC;
	break;
      }
    case LTYPE_SEADEPTH:
      {
	zaxistype = ZAXIS_DEPTH_BELOW_SEA;
	break;
      }
    default:
      {
	zaxistype = ZAXIS_GENERIC;
	break;
      }
    }

  return (zaxistype);
}

static
int cgribexGetZaxisHasBounds(int grb_ltype)
{
  int lbounds = 0;

  switch (grb_ltype)
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

static
int cgribexGetTimeUnit(int *isec1)
{
  static char func[] = "cgribexGetTimeUnit";
  int timeunit = -1;
  static int lprint = TRUE;

  switch ( ISEC1_TimeUnit )
    {
    case ISEC1_TABLE4_MINUTE:  timeunit = TUNIT_MINUTE;  break;
    case ISEC1_TABLE4_QUARTER: timeunit = TUNIT_QUARTER; break;
    case ISEC1_TABLE4_HOUR:    timeunit = TUNIT_HOUR;    break;
    case ISEC1_TABLE4_DAY:     timeunit = TUNIT_DAY;     break;
    default:
      if ( lprint )
	{
	  Message(func, "Time unit %d unsupported", ISEC1_TimeUnit);
	  lprint = FALSE;
	}
    }

  return (timeunit);
}


static
void cgribexAddRecord(int streamID, int param, int *isec1, int *isec2, double *fsec2,
		      int *isec4, long recsize, off_t position, int prec, int ztype)
{
  static char func[] = "cgribexAddRecord";
  int gridtype;
  int zaxistype;
  int gridID = CDI_UNDEFID, varID;
  int levelID = 0;
  int tsID, recID;
  int level1, level2;
  int numavg;
  int lbounds = 0;
  record_t *record;
  grid_t grid;
  int vlistID;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  tsID    = streamptr->curTsID;
  recID   = recordNewEntry(streamID, tsID);
  record  = &streamptr->tsteps[tsID].records[recID];

  numavg  = ISEC1_AvgNum;

  level1  = ISEC1_Level1;
  level2  = ISEC1_Level2;

  /* fprintf(stderr, "param %d %d %d %d\n", param, level1, level2, ISEC1_LevelType); */

  (*record).size     = recsize;
  (*record).position = position;
  (*record).param    = param;
  (*record).ilevel   = level1;
  (*record).ilevel2  = level2;
  (*record).ltype    = ISEC1_LevelType;

  gridtype = cgribexGetGridType(isec2);

  if ( streamptr->unreduced && gridtype == GRID_GAUSSIAN_REDUCED )
    {
      gridtype = GRID_GAUSSIAN;
      ISEC2_NumLon = 2*ISEC2_NumLat;
      ISEC4_NumValues = ISEC2_NumLon*ISEC2_NumLat;
    }

  memset(&grid, 0, sizeof(grid_t));
  switch (gridtype)
    {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
      {
	if ( ISEC4_NumValues != ISEC2_NumLon*ISEC2_NumLat )
	  Error(func, "numberOfPoints (%d) and gridSize (%d) differ!",
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
    case GRID_LCC:
      {
	if ( ISEC4_NumValues != ISEC2_NumLon*ISEC2_NumLat )
	  Error(func, "numberOfPoints (%d) and gridSize (%d) differ!",
		ISEC4_NumValues, ISEC2_NumLon*ISEC2_NumLat);

	grid.size  = ISEC4_NumValues;
	grid.xsize = ISEC2_NumLon;
	grid.ysize = ISEC2_NumLat;

	grid.lcc_xinc      = ISEC2_Lambert_dx;
	grid.lcc_yinc      = ISEC2_Lambert_dy;
	grid.lcc_originLon = ISEC2_FirstLon * 0.001;
	grid.lcc_originLat = ISEC2_FirstLat * 0.001;
	grid.lcc_lonParY   = ISEC2_Lambert_Lov * 0.001;
	grid.lcc_lat1      = ISEC2_Lambert_LatS1 * 0.001;
	grid.lcc_lat2      = ISEC2_Lambert_LatS2 * 0.001;
	grid.lcc_projflag  = ISEC2_Lambert_ProjFlag;
	grid.lcc_scanflag  = ISEC2_ScanFlag;

	grid.xdef   = 0;	    
	grid.ydef   = 0;

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
	Error(func, "%s grid unsupported!", gridNamePtr(gridtype));
	break;
      }
    }

  grid.isRotated = FALSE;
  if ( cgribexGetIsRotated(isec2) )
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

  zaxistype = cgribexGetZaxisType(ISEC1_LevelType);

  if ( zaxistype == ZAXIS_HYBRID )
    {
      int vctsize = ISEC2_NumVCP;
      double *vctptr = &fsec2[10];

      varDefVCT(vctsize, vctptr);
    }

  lbounds = cgribexGetZaxisHasBounds(ISEC1_LevelType);

  if ( prec > 32 ) prec = DATATYPE_PACK32;
  if ( prec <  0 ) prec = DATATYPE_PACK;

  varAddRecord(recID, param, gridID, zaxistype, lbounds, level1, level2,
	       prec, &varID, &levelID, numavg, ISEC1_LevelType, NULL, NULL, NULL);

  (*record).varID   = varID;
  (*record).levelID = levelID;

  varDefZtype(varID, ztype);

  if ( varInqInst(varID) == CDI_UNDEFID )
    {
      int center, subcenter, instID;
      center    = ISEC1_CenterID;
      subcenter = ISEC1_SubCenterID;
      instID    = institutInq(center, subcenter, NULL, NULL);
      if ( instID == CDI_UNDEFID )
	instID = institutDef(center, subcenter, NULL, NULL);
      varDefInst(varID, instID);
    }

  if ( varInqModel(varID) == CDI_UNDEFID )
    {
      int modelID;
      modelID = modelInq(varInqInst(varID), ISEC1_ModelID, NULL);
      if ( modelID == CDI_UNDEFID )
	modelID = modelDef(varInqInst(varID), ISEC1_ModelID, NULL);
      varDefModel(varID, modelID);
    }

  if ( varInqTable(varID) == CDI_UNDEFID )
    {
      int tableID;

      tableID = tableInq(varInqModel(varID), ISEC1_CodeTable, NULL);

      if ( tableID == CDI_UNDEFID )
	tableID = tableDef(varInqModel(varID), ISEC1_CodeTable, NULL);
      varDefTable(varID, tableID);
    }

  streamptr->tsteps[tsID].nallrecs++;
  streamptr->nrecs++;

  if ( CDI_Debug )
    Message(func, "varID = %d  param = %d  zaxistype = %d  gridID = %d  levelID = %d",
	    varID, param, zaxistype, gridID, levelID);
}


void cgribexScanTimestep1(int streamID)
{
  static char func[] = "cgribexScanTimestep1";
  int *isec0, *isec1, *isec2, *isec3, *isec4;
  double fsec2[512], fsec3[2], *fsec4 = NULL;
  off_t recpos = 0;
  unsigned char *gribbuffer = NULL;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int status;
  int fileID;
  int param = 0;
  int level1 = 0, level2 = 0, vdate = 0, vtime = 0;
  DateTime datetime, datetime0;
  int tsID;
  int varID;
  size_t readsize;
  int nrecords, nrecs, recID;
  int prec;
  long recsize = 0;
  int warn_time = TRUE;
  int warn_numavg = TRUE;
  int taxisID = -1;
  int rdate = 0, rtime = 0, tunit = 0, fcast = 0;
  TAXIS *taxis;
  int vlistID;
  int ztype;
  long unzipsize;
  compvar_t compVar, compVar0;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  streamptr->curTsID = 0;

  isec0 = streamptr->record->sec0;
  isec1 = streamptr->record->sec1;
  isec2 = streamptr->record->sec2;
  isec3 = streamptr->record->sec3;
  isec4 = streamptr->record->sec4;

  tsID  = tstepsNewEntry(streamID);
  taxis = &streamptr->tsteps[tsID].taxis;

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
	  streamptr->ntsteps = 1;
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

      ztype = COMPRESS_NONE;
      if ( gribGetZip(recsize, gribbuffer, &unzipsize) > 0 )
	{
	  ztype = COMPRESS_SZIP;
	  unzipsize += 100; /* need 0 to 1 bytes for rounding of bds */
	  if ( (long) buffersize < unzipsize )
	    {
	      buffersize = unzipsize;
	      gribbuffer = (unsigned char *) realloc(gribbuffer, buffersize);
	    }
	}

      gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, fsec4,
	       ipunp, (int *) gribbuffer, recsize, &iword, "J", &iret);

      param = cdiEncodeParam(ISEC1_Parameter, ISEC1_CodeTable, 255);
      if ( ISEC1_LevelType == 100 ) ISEC1_Level1 *= 100;
      if ( ISEC1_LevelType ==  99 ) ISEC1_LevelType = 100;
      level1   = ISEC1_Level1;
      level2   = ISEC1_Level2;

      gribDateTime(isec1, &vdate, &vtime);

      if ( ISEC4_NumBits > 0 && ISEC4_NumBits <= 32 )
	prec = ISEC4_NumBits;
      else
        prec = DATATYPE_PACK;

      if ( nrecs == 0 )
	{
	  datetime0.date = vdate;
	  datetime0.time = vtime;
	  rdate = gribRefDate(isec1);
	  rtime = gribRefTime(isec1);
	  tunit = cgribexGetTimeUnit(isec1);
	  fcast = gribTimeIsFC(isec1);
	}
      else
	{
	  datetime.date  = vdate;
	  datetime.time  = vtime;
	  compVar.param  = param;
          compVar.level1 = level1;
          compVar.level2 = level2;
          compVar.ltype  = ISEC1_LevelType;
	  for ( recID = 0; recID < nrecs; recID++ )
	    {
	      compVar0.param  = streamptr->tsteps[0].records[recID].param;
	      compVar0.level1 = streamptr->tsteps[0].records[recID].ilevel;
	      compVar0.level2 = streamptr->tsteps[0].records[recID].ilevel2;
	      compVar0.ltype  = streamptr->tsteps[0].records[recID].ltype;

	      if ( memcmp(&compVar0, &compVar, sizeof(compvar_t)) == 0 ) break;
	    }

	  if ( cdiInventoryMode == 1 )
	    {
	      if ( recID < nrecs ) break;
	      if ( warn_time )
		if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) != 0 )
		  {
		    Warning(func, "Inconsistent verification time (param %d level %d)", param, level1);
		    warn_time = FALSE;
		  }
	    }
	  else
	    {
	      if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) != 0 ) break;

	      if ( recID < nrecs )
		{
		  Warning(func, "Param %d level %d already exist, skipped!", param, level1);
		  continue;
		}
	    }
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
	Message(func, "%4d %8d %4d  %8d %8d %6d", nrecs, (int)recpos, param, level1, vdate, vtime);

      cgribexAddRecord(streamID, param, isec1, isec2, fsec2, isec4, recsize, recpos, prec, ztype);
    }

  streamptr->rtsteps = 1;

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
      taxis->unit  = tunit;
    }

  taxis->vdate = datetime0.date;
  taxis->vtime = datetime0.time;

  vlistID = streamInqVlist(streamID);
  vlistDefTaxis(vlistID, taxisID);

  nrecords = streamptr->tsteps[0].nallrecs;
  if ( nrecords < streamptr->tsteps[0].recordSize )
    {
      streamptr->tsteps[0].recordSize = nrecords;
      streamptr->tsteps[0].records =
      (record_t *) realloc(streamptr->tsteps[0].records, nrecords*sizeof(record_t));
    }

  streamptr->tsteps[0].recIDs = (int *) malloc(nrecords*sizeof(int));
  streamptr->tsteps[0].nrecs = nrecords;
  for ( recID = 0; recID < nrecords; recID++ )
    streamptr->tsteps[0].recIDs[recID] = recID;

  streamptr->record->buffer     = gribbuffer;
  streamptr->record->buffersize = buffersize;

  if ( streamptr->ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streamptr->rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streamptr->tsteps[tsID-1].next   = TRUE;
      streamptr->tsteps[tsID].position = recpos;
    }

  if ( streamptr->ntsteps == 1 )
    {
      if ( taxis->vdate == 0 && taxis->vtime == 0 )
	{
	  streamptr->ntsteps = 0;
	  for ( varID = 0; varID < streamptr->nvars; varID++ )
	    {
	      vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	    }
	}
    }
}


int cgribexScanTimestep2(int streamID)
{
  static char func[] = "cgribexScanTimestep2";
  int *isec0, *isec1, *isec2, *isec3, *isec4;
  double fsec2[512], fsec3[2], *fsec4 = NULL;
  off_t recpos = 0;
  unsigned char *gribbuffer = NULL;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int status;
  int fileID;
  int param = 0;
  int level1 = 0, level2 = 0, vdate = 0, vtime = 0;
  DateTime datetime, datetime0;
  int tsID;
  int varID, gridID;
  size_t readsize;
  int nrecords, nrecs, recID, rindex;
  long recsize = 0;
  int warn_numavg = TRUE;
  int taxisID = -1;
  TAXIS *taxis;
  int vlistID;
  long unzipsize;
  compvar_t compVar, compVar0;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  streamptr->curTsID = 1;

  isec0 = streamptr->record->sec0;
  isec1 = streamptr->record->sec1;
  isec2 = streamptr->record->sec2;
  isec3 = streamptr->record->sec3;
  isec4 = streamptr->record->sec4;

  fileID  = streamInqFileID(streamID);
  vlistID = streamInqVlist(streamID);
  taxisID = vlistInqTaxis(vlistID);

  gribbuffer = (unsigned char *) streamptr->record->buffer;
  buffersize = streamptr->record->buffersize;
                                          
  tsID = streamptr->rtsteps;
  if ( tsID != 1 )
    Error(func, "Internal problem! unexpeceted timestep %d", tsID+1);

  taxis = &streamptr->tsteps[tsID].taxis;

  fileSetPos(fileID, streamptr->tsteps[tsID].position, SEEK_SET);

  cdiCreateRecords(streamID, tsID);

  nrecords = streamptr->tsteps[tsID].nallrecs;
  streamptr->tsteps[1].recIDs = (int *) malloc(nrecords*sizeof(int));
  streamptr->tsteps[1].nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    streamptr->tsteps[1].recIDs[recID] = -1;
      
  for ( recID = 0; recID < nrecords; recID++ )
    {
      varID = streamptr->tsteps[0].records[recID].varID;
      streamptr->tsteps[tsID].records[recID].position = 
	streamptr->tsteps[0].records[recID].position;
      streamptr->tsteps[tsID].records[recID].size     = 
	streamptr->tsteps[0].records[recID].size;
    }

  rindex = 0;
  while ( TRUE )
    {
      if ( rindex > nrecords ) break;

      recsize = gribGetSize(fileID);
      recpos  = fileGetPos(fileID);
      if ( recsize == 0 )
	{
	  streamptr->ntsteps = 2;
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

      if ( gribGetZip(recsize, gribbuffer, &unzipsize) > 0 )
	{
	  unzipsize += 100; /* need 0 to 1 bytes for rounding of bds */
	  if ( (long) buffersize < unzipsize )
	    {
	      buffersize = unzipsize;
	      gribbuffer = (unsigned char *) realloc(gribbuffer, buffersize);
	    }
	}

      gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, fsec4,
	       ipunp, (int *) gribbuffer, recsize, &iword, "J", &iret);

      param = cdiEncodeParam(ISEC1_Parameter, ISEC1_CodeTable, 255);
      if ( ISEC1_LevelType == 100 ) ISEC1_Level1 *= 100;
      if ( ISEC1_LevelType ==  99 ) ISEC1_LevelType = 100;
      level1    = ISEC1_Level1;
      level2    = ISEC1_Level2;

      gribDateTime(isec1, &vdate, &vtime);

      if ( rindex == 0 )
	{
	  if ( taxisInqType(taxisID) == TAXIS_RELATIVE )
	    {
	      taxis->type  = TAXIS_RELATIVE;
	      taxis->rdate = gribRefDate(isec1);
	      taxis->rtime = gribRefTime(isec1);
	      taxis->unit  = cgribexGetTimeUnit(isec1);
	    }
	  else
	    {
	      taxis->type  = TAXIS_ABSOLUTE;
	    }
	  taxis->vdate = vdate;
	  taxis->vtime = vtime;

	  datetime0.date = vdate;
	  datetime0.time = vtime;
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

      datetime.date  = vdate;
      datetime.time  = vtime;
      compVar.param  = param;
      compVar.level1 = level1;
      compVar.level2 = level2;
      compVar.ltype  = ISEC1_LevelType;
      for ( recID = 0; recID < nrecords; recID++ )
	{
	  compVar0.param  = streamptr->tsteps[tsID].records[recID].param;
	  compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
	  compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
	  compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

	  if ( memcmp(&compVar0, &compVar, sizeof(compvar_t)) == 0 ) break;
	}

      if ( recID == nrecords )
	{
	  Warning(func, "Param %d level %d not found at timestep %d!", param, level1, tsID+1);
	  return (CDI_EUFSTRUCT);
	}

      if ( cdiInventoryMode == 1 )
	{
	  if ( streamptr->tsteps[tsID].records[recID].used )
	    {
	      break;
	    }
	  else
	    {
	      streamptr->tsteps[tsID].records[recID].used = TRUE;
	      streamptr->tsteps[tsID].recIDs[rindex] = recID;
	    }    
	}
      else
	{
	  if ( streamptr->tsteps[tsID].records[recID].used )
	    {
	      if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) != 0 ) break;

	      Warning(func, "Param %d level %d already exist, skipped!", param, level1);
	      continue;
	    }
	  else
	    {
	      streamptr->tsteps[tsID].records[recID].used = TRUE;
	      streamptr->tsteps[tsID].recIDs[rindex] = recID;
	    }    
	}

      if ( CDI_Debug )
	Message(func, "%4d %8d %4d %8d %8d %6d", rindex+1, (int)recpos, param, level1, vdate, vtime);

      streamptr->tsteps[tsID].records[recID].size = recsize;

      compVar0.param  = streamptr->tsteps[tsID].records[recID].param;
      compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
      compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
      compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

      if ( memcmp(&compVar0, &compVar, sizeof(compvar_t)) != 0 )
	{
	  Message(func, "tsID = %d recID = %d param = %3d new %3d  level = %3d new %3d",
		  tsID, recID,
		  streamptr->tsteps[tsID].records[recID].param, param,
		  streamptr->tsteps[tsID].records[recID].ilevel, level1);
	  return (CDI_EUFSTRUCT);
	}

      streamptr->tsteps[1].records[recID].position = recpos;
      varID = streamptr->tsteps[tsID].records[recID].varID;
      gridID = vlistInqVarGrid(vlistID, varID);
      if ( gridInqSize(gridID) == 1 && gridInqType(gridID) == GRID_LONLAT )
	{
	  if ( IS_NOT_EQUAL(gridInqXval(gridID, 0),ISEC2_FirstLon*0.001) ||
	       IS_NOT_EQUAL(gridInqYval(gridID, 0),ISEC2_FirstLat*0.001) )
	    gridChangeType(gridID, GRID_TRAJECTORY);
	}

      rindex++;
    }

  nrecs = 0;
  for ( recID = 0; recID < nrecords; recID++ )
    {
      if ( ! streamptr->tsteps[tsID].records[recID].used )
	{
	  varID = streamptr->tsteps[tsID].records[recID].varID;
	  vlistDefVarTime(vlistID, varID, TIME_CONSTANT);
	}
      else
	{
	  nrecs++;
	}
    }
  streamptr->tsteps[tsID].nrecs = nrecs;

  streamptr->rtsteps = 2;

  if ( streamptr->ntsteps == -1 )
    {
      tsID = tstepsNewEntry(streamID);
      if ( tsID != streamptr->rtsteps )
	Error(func, "Internal error. tsID = %d", tsID);

      streamptr->tsteps[tsID-1].next   = TRUE;
      streamptr->tsteps[tsID].position = recpos;
    }

  streamptr->record->buffer     = gribbuffer;
  streamptr->record->buffersize = buffersize;

  return (0);
}


int cgribexScanTimestep(int streamID)
{
  static char func[] = "cgribexScanTimestep";
  int *isec0, *isec1, *isec2, *isec3, *isec4;
  double fsec2[512], fsec3[2], *fsec4 = NULL;
  long recsize = 0;
  off_t recpos = 0;
  unsigned char *gribbuffer;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int status;
  int fileID;
  int param = 0;
  int level1 = 0, level2 = 0, vdate = 0, vtime = 0;
  DateTime datetime, datetime0;
  int tsID;
  int vrecID, recID;
  int warn_numavg = TRUE;
  size_t readsize;
  int taxisID = -1;
  TAXIS *taxis;
  int vlistID;
  int rindex, nrecs = 0;
  long unzipsize;
  compvar_t compVar, compVar0;
  stream_t *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  vlistID = streamInqVlist(streamID);

  if ( CDI_Debug )
    {
      Message(func, "streamID = %d", streamID);
      Message(func, "cts = %d", streamptr->curTsID);
      Message(func, "rts = %d", streamptr->rtsteps);
      Message(func, "nts = %d", streamptr->ntsteps);
    }

  isec0 = streamptr->record->sec0;
  isec1 = streamptr->record->sec1;
  isec2 = streamptr->record->sec2;
  isec3 = streamptr->record->sec3;
  isec4 = streamptr->record->sec4;

  tsID  = streamptr->rtsteps;
  taxis = &streamptr->tsteps[tsID].taxis;

  if ( streamptr->tsteps[tsID].recordSize == 0 )
    {
      gribbuffer = (unsigned char *) streamptr->record->buffer;
      buffersize = streamptr->record->buffersize;

      cdiCreateRecords(streamID, tsID);

      nrecs = streamptr->tsteps[1].nrecs;

      streamptr->tsteps[tsID].nrecs = nrecs;
      streamptr->tsteps[tsID].recIDs = (int *) malloc(nrecs*sizeof(int));
      for ( recID = 0; recID < nrecs; recID++ )
	streamptr->tsteps[tsID].recIDs[recID] = streamptr->tsteps[1].recIDs[recID];

      fileID = streamInqFileID(streamID);

      fileSetPos(fileID, streamptr->tsteps[tsID].position, SEEK_SET);

      rindex = 0;
      while ( TRUE )
	{
	  if ( rindex > nrecs ) break;

	  recsize = gribGetSize(fileID);
	  recpos  = fileGetPos(fileID);
	  if ( recsize == 0 )
	    {
	      streamptr->ntsteps = streamptr->rtsteps + 1;
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
		    streamptr->tsteps[tsID].recordSize);
	      break;
	    }

	  if ( gribGetZip(recsize, gribbuffer, &unzipsize) > 0 )
	    {
	      unzipsize += 100; /* need 0 to 1 bytes for rounding of bds */
	      if ( (long) buffersize < unzipsize )
		{
		  buffersize = unzipsize;
		  gribbuffer = (unsigned char *) realloc(gribbuffer, buffersize);
		}
	    }

	  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, fsec4,
		   ipunp, (int *) gribbuffer, recsize, &iword, "J", &iret);

	  param = cdiEncodeParam(ISEC1_Parameter, ISEC1_CodeTable, 255);
	  if ( ISEC1_LevelType == 100 ) ISEC1_Level1 *= 100;
	  if ( ISEC1_LevelType ==  99 ) ISEC1_LevelType = 100;
	  level1   = ISEC1_Level1;
	  level2   = ISEC1_Level2;

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
		  taxis->unit  = cgribexGetTimeUnit(isec1);
		}
	      else
		{
		  taxis->type  = TAXIS_ABSOLUTE;
		}
	      taxis->vdate = vdate;
	      taxis->vtime = vtime;

	      datetime0.date = vdate;
	      datetime0.time = vtime;
	    }

	  if ( ISEC1_AvgNum )
	    {
	      if (  taxis->numavg && warn_numavg &&
		   (taxis->numavg != ISEC1_AvgNum) )
		{
	      /*
		  Message(func, "Change numavg from %d to %d not allowed!",
			  streamptr->tsteps[tsID].taxis.numavg, ISEC1_AvgNum);
	      */
		  warn_numavg = FALSE;
		}
	      else
		{
		  taxis->numavg = ISEC1_AvgNum;
		}
	    }
	  
	  datetime.date  = vdate;
	  datetime.time  = vtime;
	  compVar.param  = param;
          compVar.level1 = level1;
          compVar.level2 = level2;
          compVar.ltype  = ISEC1_LevelType;
	  for ( vrecID = 0; vrecID < nrecs; vrecID++ )
	    {
	      recID   = streamptr->tsteps[1].recIDs[vrecID];
	      compVar0.param  = streamptr->tsteps[tsID].records[recID].param;
	      compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
	      compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
	      compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

	      if ( memcmp(&compVar0, &compVar, sizeof(compvar_t)) == 0 ) break;
	    }

	  if ( vrecID == nrecs )
	    {
	      Warning(func, "Param %d level %d not available at timestep %d!", param, level1, tsID+1);

	      if ( cdiInventoryMode == 1 )
		return (CDI_EUFSTRUCT);
	      else
		continue;
	    }

	  if ( cdiInventoryMode == 1 )
	    {
	      streamptr->tsteps[tsID].records[recID].used = TRUE;
	      streamptr->tsteps[tsID].recIDs[rindex] = recID;
	    }
	  else
	    {
	      if ( streamptr->tsteps[tsID].records[recID].used )
		{
		  if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) != 0 ) break;
		  
		  if ( CDI_Debug )
		    Warning(func, "Param %d level %d already exist, skipped!", param, level1);

		  continue;
		}
	      else
		{
		  streamptr->tsteps[tsID].records[recID].used = TRUE;
		  streamptr->tsteps[tsID].recIDs[rindex] = recID;
		}    
	    }

	  if ( CDI_Debug )
	    Message(func, "%4d %8d %4d %8d %8d %6d", rindex+1, (int)recpos, param, level1, vdate, vtime);

	  compVar0.param  = streamptr->tsteps[tsID].records[recID].param;
	  compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
	  compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
	  compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

	  if ( memcmp(&compVar0, &compVar, sizeof(compvar_t)) != 0 )
	    {
	      Message(func, "tsID = %d recID = %d param = %3d new %3d  level = %3d new %3d",
		      tsID, recID,
		      streamptr->tsteps[tsID].records[recID].param, param,
		      streamptr->tsteps[tsID].records[recID].ilevel, level1);
	      Error(func, "Invalid, unsupported or inconsistent record structure");
	    }
	  
	  streamptr->tsteps[tsID].records[recID].position = recpos;
	  streamptr->tsteps[tsID].records[recID].size = recsize;

	  if ( CDI_Debug )
	    Message(func, "%4d %8d %4d %8d %8d %6d", rindex, (int)recpos, param, level1, vdate, vtime);

	  rindex++;
	}

      for ( vrecID = 0; vrecID < nrecs; vrecID++ )
	{
	  recID   = streamptr->tsteps[tsID].recIDs[vrecID];
	  if ( ! streamptr->tsteps[tsID].records[recID].used ) break;
	}

      if ( vrecID < nrecs )
	{
	  Warning(func, "Param %d level %d not found at timestep %d!",
		  streamptr->tsteps[tsID].records[recID].param,
		  streamptr->tsteps[tsID].records[recID].ilevel,
		  tsID+1);
	  return (CDI_EUFSTRUCT);
	}

      streamptr->rtsteps++;

      if ( streamptr->ntsteps != streamptr->rtsteps )
	{
	  tsID = tstepsNewEntry(streamID);
	  if ( tsID != streamptr->rtsteps )
	    Error(func, "Internal error. tsID = %d", tsID);

	  streamptr->tsteps[tsID-1].next   = 1;
	  streamptr->tsteps[tsID].position = recpos;
	}

      fileSetPos(fileID, streamptr->tsteps[tsID].position, SEEK_SET);
      streamptr->tsteps[tsID].position = recpos;

      streamptr->record->buffer     = gribbuffer;
      streamptr->record->buffersize = buffersize;
    }

  if ( nrecs > 0 && nrecs < streamptr->tsteps[tsID].nrecs )
    {
      Warning(func, "Incomplete timestep. Stop scanning at timestep %d.\n", tsID);
      streamptr->ntsteps = tsID;
    }

  return (streamptr->ntsteps);
}


int cgribexDecode(unsigned char *gribbuffer, int gribsize, double *data, int gridsize,
		  int unreduced, int *nmiss, int *zip)
{
  static char func[] = "cgribexDecode";
  int status = 0;
#if  defined  (HAVE_LIBCGRIBEX)
  int iret = 0, iword = 0;
  int isec0[2], isec1[4096], isec2[4096], isec3[2], isec4[512];
  int izip;
  long unzipsize;
  double fsec2[512], fsec3[2];
  char hoper[2];

  *zip = 0;

  if ( unreduced ) strcpy(hoper, "R");
  else             strcpy(hoper, "D");

  FSEC3_MissVal = cdiDefaultMissval;

  if ( (izip = gribGetZip(gribsize, gribbuffer, &unzipsize)) > 0 )
    {
          *zip = izip;
	  if ( izip == 128 ) /* szip */
	    {
	      unsigned char *itmpbuffer = NULL;
	      size_t itmpbuffersize = 0;

	      if ( unzipsize < (long) gribsize )
		{
		  fprintf(stderr, "Decompressed size smaller than compressed size (in %d; out %ld)!\n",
			  gribsize, unzipsize);
		  return (status);
		}

	      if ( itmpbuffersize < (size_t) gribsize )
		{
		  itmpbuffersize = gribsize;
		  itmpbuffer = (unsigned char *) realloc(itmpbuffer, itmpbuffersize);
		}

	      memcpy(itmpbuffer, gribbuffer, itmpbuffersize);

	      unzipsize += 100; /* need 0 to 1 bytes for rounding of bds */

	      gribsize = gribUnzip(gribbuffer, unzipsize, itmpbuffer, gribsize);

	      if ( gribsize <= 0 )
		Error(func, "Decompression problem!\n");
	   
	      free(itmpbuffer);
	    }
	  else
	    {
	      Error(func, "Decompression for %d not implemented!\n", izip);
	    }
    }

  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, data,
	   gridsize, (int *) gribbuffer, gribsize, &iword, hoper, &iret);

  if ( ISEC1_Sec2Or3Flag & 64 )
    *nmiss = ISEC4_NumValues - ISEC4_NumNonMissValues;
  else
    *nmiss = 0;
#else
  Error(func, "CGRIBEX support not compiled in!");
#endif

  return (status);
}


#if  defined  (HAVE_LIBCGRIBEX)
static
void cgribexDefInstitut(int *isec1, int vlistID, int varID)
{
  int instID;

  if ( vlistInqInstitut(vlistID) != CDI_UNDEFID )
    instID = vlistInqInstitut(vlistID);
  else
    instID = vlistInqVarInstitut(vlistID, varID);

  if ( instID != CDI_UNDEFID )
    {
      int center, subcenter;
      center    = institutInqCenter(instID);
      subcenter = institutInqSubcenter(instID);
      ISEC1_CenterID    = center;
      ISEC1_SubCenterID = subcenter;
    }
}

static
void cgribexDefModel(int *isec1, int vlistID, int varID)
{
  int modelID;

  if ( vlistInqModel(vlistID) != CDI_UNDEFID )
    modelID = vlistInqModel(vlistID);
  else
    modelID = vlistInqVarModel(vlistID, varID);

  if ( modelID != CDI_UNDEFID )
    ISEC1_ModelID = modelInqGribID(modelID);
}

static
void cgribexDefParam(int *isec1, int param)
{
  static char func[] = "cgribexDefParam";
  int pdis, pcat, pnum;
  static int lwarn = TRUE;

  cdiDecodeParam(param, &pnum, &pcat, &pdis);

  if ( pdis != 255 && lwarn )
    {
      Warning(func, "Can not convert GRIB2 parameter number (%d) to GRIB1", param);
      lwarn = FALSE;
    }
    
  if ( pnum < 0 ) pnum = -pnum;

  ISEC1_CodeTable = pcat;
  ISEC1_Parameter = pnum;
}

static
void cgribexDefTime(int *isec1, int date, int time, int numavg, int timeID)
{
  int year, month, day, hour, minute, second;
  int century = 0;
  int timetype = -1;

  if ( timeID != -1 ) timetype = taxisInqType(timeID);

  if ( timetype == TAXIS_RELATIVE )
    {
      int factor = 1;
      int rdate, rtime;
      int julday1, secofday1, julday2, secofday2, days, secs;
      int ival;
      int calendar;
      
      calendar = taxisInqCalendar(timeID);
      rdate   = taxisInqRdate(timeID);
      rtime   = taxisInqRtime(timeID);
      cdiDecodeDate(rdate, &year, &month, &day);
      cdiDecodeTime(rtime, &hour, &minute, &second);

      century =  year / 100;

      ISEC1_Year = year - century*100;

      if ( year < 0 )
	{
	  century = -century;
	  ISEC1_Year = -ISEC1_Year;
	}

      if ( ISEC1_Year == 0 )
	{
	  ISEC1_Year = 100;
	  century -= 1;
	}

      century += 1;
      if ( year < 0 ) century = -century;

      ISEC1_Month  = month;
      ISEC1_Day    = day;
      ISEC1_Hour   = hour;
      ISEC1_Minute = minute;
      /*
      printf("year %d, month %d, day %d, hour %d, minute %d\n", year, month, day, hour, minute);
      */
      encode_juldaysec(calendar, year, month, day, hour, minute, &julday1, &secofday1);

      switch (taxisInqTunit(timeID))
	{
	case TUNIT_MINUTE:  factor =    60; ISEC1_TimeUnit = ISEC1_TABLE4_MINUTE;  break;
	case TUNIT_QUARTER: factor =   900; ISEC1_TimeUnit = ISEC1_TABLE4_QUARTER; break;
	case TUNIT_HOUR:    factor =  3600; ISEC1_TimeUnit = ISEC1_TABLE4_HOUR;    break;
	case TUNIT_DAY:     factor = 86400; ISEC1_TimeUnit = ISEC1_TABLE4_DAY;     break;
	default:            factor =  3600; ISEC1_TimeUnit = ISEC1_TABLE4_HOUR;    break;
	}
      cdiDecodeDate(date, &year, &month, &day);
      cdiDecodeTime(time, &hour, &minute, &second);
      /*
      printf("date time %d %d\n", date, time);
      printf("year %d, month %d, day %d, hour %d, minute %d\n", year, month, day, hour, minute);
      */
      encode_juldaysec(calendar, year, month, day, hour, minute, &julday2, &secofday2);

      (void) julday_sub(julday1, secofday1, julday2, secofday2, &days, &secs);
      /* ival = (int) ((days*86400.0 + secs)/factor); */

      if ( (int) fmod(days*86400.0 + secs, factor) )
	ival = -1;
      else
	ival = (int) ((days*86400.0 + secs)/factor);
      
      if ( ival < 0 || ival > 255 ) timetype = TAXIS_ABSOLUTE;
      
      ISEC1_TimeRange = 0;
      ISEC1_TimePeriod1 = ival;
    }

  if ( timetype == TAXIS_ABSOLUTE )
    {
      cdiDecodeDate(date, &year, &month, &day);
      cdiDecodeTime(time, &hour, &minute, &second);

      century =  year / 100;

      ISEC1_Year = year - century*100;

      if ( year < 0 )
	{
	  century = -century;
	  ISEC1_Year = -ISEC1_Year;
	}

      if ( ISEC1_Year == 0 )
	{
	  ISEC1_Year = 100;
	  century -= 1;
	}

      century += 1;
      if ( year < 0 ) century = -century;

      ISEC1_Month    = month;
      ISEC1_Day      = day;
      ISEC1_Hour     = hour;
      ISEC1_Minute   = minute;

      /* ISEC1_TimeUnit = 0; */
      switch (taxisInqTunit(timeID))
	{
	case TUNIT_MINUTE:  ISEC1_TimeUnit = ISEC1_TABLE4_MINUTE;  break;
	case TUNIT_QUARTER: ISEC1_TimeUnit = ISEC1_TABLE4_QUARTER; break;
	case TUNIT_HOUR:    ISEC1_TimeUnit = ISEC1_TABLE4_HOUR;    break;
	case TUNIT_DAY:     ISEC1_TimeUnit = ISEC1_TABLE4_DAY;     break;
	}

      if ( numavg > 0 )
	ISEC1_TimeRange = 0;
      else
	ISEC1_TimeRange = 10;

      ISEC1_TimePeriod1 = 0;
    }

  ISEC1_TimePeriod2    = 0;
  ISEC1_AvgNum         = numavg;
  ISEC1_AvgMiss        = 0;
  ISEC1_Century        = century;
  ISEC1_DecScaleFactor = 0;
}

static
void cgribexDefGrid(int *isec1, int *isec2, int gridID)
{
  static char func[] = "cgribexDefGrid";
  int gridtype;
  static short lwarn = TRUE;

  memset(isec2, 0, 16*sizeof(int));

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
      else if ( gridInqXvals(gridID, NULL) && gridInqYvals(gridID, NULL) )
	{
	  gridtype = GRID_LONLAT;
	  gridChangeType(gridID, gridtype);
	}
    }
  else if ( gridtype == GRID_CURVILINEAR )
    {
      if ( lwarn )
	{
	  lwarn = FALSE;
	  Warning(func, "Curvilinear grids are unsupported in GRIB format! Created wrong GDS!");
	}
      gridtype = GRID_LONLAT;
    }

  ISEC2_Reduced = FALSE;

  ISEC2_ScanFlag = 0;

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
	    nlon = gridInqXsize(gridID);
	  }

	nlat = gridInqYsize(gridID);
	if ( gridtype == GRID_GAUSSIAN || gridtype == GRID_GAUSSIAN_REDUCED )
	  ISEC2_GridType = GTYPE_GAUSSIAN;
	else if ( gridtype == GRID_LONLAT && gridIsRotated(gridID) )
	  ISEC2_GridType = GTYPE_LATLON_ROT;
	else
	  ISEC2_GridType = GTYPE_LATLON;

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

	    if ( ISEC2_LatIncr < 0 ) ISEC2_LatIncr = -ISEC2_LatIncr;
	  }

	if ( ISEC2_NumLon > 1 && ISEC2_NumLat == 1 ) 
	  if ( ISEC2_LonIncr != 0 && ISEC2_LatIncr == 0 ) ISEC2_LatIncr = ISEC2_LonIncr;

	if ( ISEC2_NumLon == 1 && ISEC2_NumLat > 1 ) 
	  if ( ISEC2_LonIncr == 0 && ISEC2_LatIncr != 0 ) ISEC2_LonIncr = ISEC2_LatIncr;

	if ( ISEC2_LatIncr == 0 || ISEC2_LonIncr == 0 )
	  ISEC2_ResFlag = 0;
	else
	  ISEC2_ResFlag = 128;

	if ( gridIsRotated(gridID) )
	  {
	    ISEC2_LatSP = - NINT(gridInqYpole(gridID) * 1000);
	    ISEC2_LonSP =   NINT((gridInqXpole(gridID) + 180) * 1000);
	  }

	/* East -> West */
	if ( ISEC2_LastLon < ISEC2_FirstLon ) ISEC2_ScanFlag += 128;

	/* South -> North */
	if ( ISEC2_LastLat > ISEC2_FirstLat ) ISEC2_ScanFlag += 64;

	break;
      }
    case GRID_LCC:
      {
	double originLon, originLat, lonParY, lat1, lat2, xincm, yincm;
	int xsize, ysize;
	int projflag, scanflag;

	xsize = gridInqXsize(gridID);
	ysize = gridInqYsize(gridID);

	gridInqLCC(gridID, &originLon, &originLat, &lonParY, &lat1, &lat2, &xincm, &yincm,
		   &projflag, &scanflag);

	ISEC2_GridType = GTYPE_LCC;
	ISEC2_NumLon   = xsize;
	ISEC2_NumLat   = ysize;
	ISEC2_FirstLon = NINT(originLon * 1000);
	ISEC2_FirstLat = NINT(originLat * 1000);
	ISEC2_Lambert_Lov    = NINT(lonParY * 1000);
	ISEC2_Lambert_LatS1  = NINT(lat1 * 1000);
	ISEC2_Lambert_LatS2  = NINT(lat2 * 1000);
	ISEC2_Lambert_dx     = NINT(xincm);
	ISEC2_Lambert_dy     = NINT(yincm);
	ISEC2_Lambert_LatSP  = 0;
	ISEC2_Lambert_LatSP  = 0;
	ISEC2_Lambert_ProjFlag = projflag;
	ISEC2_ScanFlag = scanflag;

	break;
      }
    case GRID_SPECTRAL:
      {
	ISEC2_GridType = GTYPE_SPECTRAL;
	ISEC2_PentaJ   = gridInqTrunc(gridID);
	ISEC2_PentaK   = ISEC2_PentaJ;
	ISEC2_PentaM   = ISEC2_PentaJ;
	ISEC2_RepType  = 1;
	ISEC2_RepMode  = 1;
	break;
      }
    case GRID_GME:
      {
	ISEC2_GridType   = GTYPE_GME;
	ISEC2_GME_ND     = gridInqGMEnd(gridID);
	ISEC2_GME_NI     = gridInqGMEni(gridID);
	ISEC2_GME_NI2    = gridInqGMEni2(gridID);
	ISEC2_GME_NI3    = gridInqGMEni3(gridID);
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
}

static
void cgribexDefLevel(int *isec1, int *isec2, double *fsec2, int zaxisID, int levelID)
{
  static char func[] = "cgribexDefLevel";
  double level;
  int ilevel, zaxistype, ltype;
  static int warning = 1;
  static int vct_warning = 1;

  zaxistype = zaxisInqType(zaxisID);
  ltype = zaxisInqLtype(zaxisID);

  if ( zaxistype == ZAXIS_GENERIC && ltype == 0 )
    {
      zaxistype = ZAXIS_PRESSURE;
      zaxisChangeType(zaxisID, zaxistype);
      zaxisDefUnits(zaxisID, "Pa");
    }

  ISEC2_NumVCP = 0;

  switch (zaxistype)
    {
    case ZAXIS_SURFACE:
      {
	ISEC1_LevelType = LTYPE_SURFACE;
	ISEC1_Level1    = (int) zaxisInqLevel(zaxisID, levelID);
	ISEC1_Level2    = 0;
	break;
      }
    case ZAXIS_MEANSEA:
      {
	ISEC1_LevelType = LTYPE_MEANSEA;
	ISEC1_Level1    = (int) zaxisInqLevel(zaxisID, levelID);
	ISEC1_Level2    = 0;
	break;
      }
    case ZAXIS_HYBRID:
    case ZAXIS_HYBRID_HALF:
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
	    Warning(func, "VCT missing. ( param = %d, zaxisID = %d )",
		    ISEC1_Parameter, zaxisID);
	    warning = 0;
	  }
	if ( vctsize > 255 )
	  {
	    ISEC2_NumVCP = 0;
	    if ( vct_warning )
	      {
		Warning(func, "VCT size of %d is too large (maximum is 255). Set to 0!", vctsize);
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
	  Warning(func, "Pressure level of %f Pa is below zero!", level);

	zaxisInqUnits(zaxisID, units);
	if ( memcmp(units, "Pa", 2) != 0 ) level *= 100;

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
    case ZAXIS_SIGMA:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	ISEC1_LevelType = LTYPE_SIGMA;
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
	ISEC1_LevelType = LTYPE_ISENTROPIC;
	ISEC1_Level1    = ilevel;
	ISEC1_Level2    = 0;

	break;
      }
    case ZAXIS_GENERIC:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	ilevel = (int) level;
	ISEC1_LevelType = ltype;
	ISEC1_Level1    = ilevel;
	ISEC1_Level2    = 0;

	break;
      }
    default:
      {
	Error(func, "zaxistype >%s< unsupported", zaxisNamePtr(zaxistype));
	break;
      }
    }
}

static
void cgribexDefMask(int *isec3)
{
}

static
void cgribexDefaultSec0(int *isec0)
{
  ISEC0_GRIB_Len     = 0;
  ISEC0_GRIB_Version = 0;
}

static
void cgribexDefaultSec1(int *isec1)
{
  ISEC1_CenterID    = 0;
  ISEC1_SubCenterID = 0;
  ISEC1_LocalFLag   = 0;
}

static
void cgribexDefaultSec4(int *isec4)
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
#endif


size_t cgribexEncode(int varID, int levelID, int vlistID, int gridID, int zaxisID, int date, int time, int numavg, 
		     long datasize, const double *data, int nmiss, unsigned char *gribbuffer, size_t gribbuffersize)
{
  static char func[] = "cgribexEncode";
  size_t nbytes = 0;
#if  defined  (HAVE_LIBCGRIBEX)
  long gribsize;
  int iret = 0, iword = 0;
  int isec0[2], isec1[4096], isec2[4096], isec3[2], isec4[512];
  double fsec2[512], fsec3[2];
  int datatype;
  int code, tabnum;
  int param;

  memset(isec1, 0, 32*sizeof(int));
  fsec2[0] = 0; fsec2[1] = 0;

  gribsize = gribbuffersize / sizeof(int);
  param    = vlistInqVarParam(vlistID, varID);
  code     = vlistInqVarCode(vlistID, varID);
  tabnum   = tableInqNum(vlistInqVarTable(vlistID, varID));

  if ( param == CDI_UNDEFPARAM ) param = cdiEncodeParam(code, tabnum, 255);

  cgribexDefaultSec0(isec0);
  cgribexDefaultSec1(isec1);
  cgribexDefaultSec4(isec4);

  cgribexDefInstitut(isec1, vlistID, varID);
  cgribexDefModel(isec1, vlistID, varID);

  datatype = vlistInqVarDatatype(vlistID, varID);

  cgribexDefParam(isec1, param);
  cgribexDefTime(isec1, date, time, numavg, vlistInqTaxis(vlistID));
  cgribexDefGrid(isec1, isec2, gridID);
  cgribexDefLevel(isec1, isec2, fsec2, zaxisID, levelID);
  cgribexDefMask(isec3);

  ISEC4_NumBits   = grbBitsPerValue(datatype);
  ISEC4_NumValues = gridInqSize(gridID);

  if ( nmiss > 0 )
    {
      FSEC3_MissVal = vlistInqVarMissval(vlistID, varID);
      ISEC1_Sec2Or3Flag |= 64;
    }

  gribExDP(isec0, isec1, isec2, fsec2, isec3, fsec3, isec4, (double*) data,
	   datasize, (int *) gribbuffer, gribsize, &iword, "C", &iret);

  if ( iret ) Error(func, "Problem during GRIB encode (errno = %d)!", iret);

  nbytes = iword*sizeof(int);
#else
  Error(func, "CGRIBEX support not compiled in!");
#endif

  return (nbytes);
}
