#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>

#include "cdi.h"
#include "stream_int.h"
#include "file.h"
#include "varscan.h"
#include "datetime.h"
#include "vlist.h"
#include "stream_grb.h"

#include "gribapi.h"

#if  defined  (HAVE_LIBGRIB_API)
#  if  defined  (HAVE_LIBCGRIBEX)
#  include "cgribex.h"      /* gribGetSize, gribRead, gribGetZip */
#  endif
#  include "grib_api.h"
#endif


extern int cdiInventoryMode;

typedef struct {
  int code;
  int table;
  int level1;
  int level2;
  int ltype;
} compvar2_t; 


#if  defined  (HAVE_LIBGRIB_API)
static
int gribapiGetGridType(grib_handle *gh)
{
  /*  static char func[] = "gribapiGetGridType"; */
  int gridtype = 0;
  int gribgridtype;
  long lpar;

  GRIB_CHECK(grib_get_long(gh, "gridDefinitionTemplateNumber", &lpar), 0);
  gribgridtype = (int) lpar;

  switch (gribgridtype)
    {
    case  GRIBAPI_GTYPE_LATLON:
    case  GRIBAPI_GTYPE_LATLON_ROT:
      {
	gridtype = GRID_LONLAT;
	break;
      }
    case  GRIBAPI_GTYPE_LCC:
      {
	gridtype = GRID_LCC;
	break;
      }
    case  GRIBAPI_GTYPE_GAUSSIAN:
      {
	/*
	if ( ISEC2_Reduced )
	  gridtype = GRID_GAUSSIAN_REDUCED;
	else
	*/
	  gridtype = GRID_GAUSSIAN;
	break;
      }
    case  GRIBAPI_GTYPE_SPECTRAL:
      {
	gridtype = GRID_SPECTRAL;
	break;
      }
    case  GRIBAPI_GTYPE_GME:
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
#endif

#if  defined  (HAVE_LIBGRIB_API)
static
int gribapiGetZaxisType(int grb_ltype)
{
  int zaxistype = 0;

  switch ( grb_ltype )
    {
    case GRIBAPI_LTYPE_SURFACE:
      {
	zaxistype = ZAXIS_SURFACE;
	break;
      }
    case GRIBAPI_LTYPE_MEANSEA:
      {
	zaxistype = ZAXIS_MEANSEA;
	break;
      }
    case GRIBAPI_LTYPE_ISOBARIC:
      {
	zaxistype = ZAXIS_PRESSURE;
	break;
      }
    case GRIBAPI_LTYPE_HEIGHT:
      {
	zaxistype = ZAXIS_HEIGHT;
	break;
      }
    case GRIBAPI_LTYPE_ALTITUDE:
      {
	zaxistype = ZAXIS_ALTITUDE;
	break;
      }
    case GRIBAPI_LTYPE_SIGMA:
      {
	zaxistype = ZAXIS_SIGMA;
	break;
      }
    case GRIBAPI_LTYPE_HYBRID:
      //case GRIBAPI_LTYPE_HYBRID_LAYER:
      {
	zaxistype = ZAXIS_HYBRID;
	break;
      }
    case GRIBAPI_LTYPE_LANDDEPTH:
      //case GRIBAPI_LTYPE_LANDDEPTH_LAYER:
      {
	zaxistype = ZAXIS_DEPTH_BELOW_LAND;
	break;
      }
    case GRIBAPI_LTYPE_ISENTROPIC:
      {
	zaxistype = ZAXIS_ISENTROPIC;
	break;
      }
    case GRIBAPI_LTYPE_SEADEPTH:
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
#endif

#if  defined  (HAVE_LIBGRIB_API)
static
void gribapiAddRecord(int streamID, int code, grib_handle *gh,
		      long recsize, off_t position, int prec, int ztype)
{
  static char func[] = "gribapiAddRecord";
  int gridtype;
  int zaxistype;
  int gridID = CDI_UNDEFID, varID;
  int levelID = 0;
  int tsID, recID;
  int level1, level2;
  int numavg;
  int lbounds = 0;
  RECORD *record;
  grid_t grid;
  int vlistID;
  stream_t *streamptr;
  int leveltype;
  double dlevel;
  long lpar;
  int codetable;
  int status;
  long numberOfPoints;
  size_t datasize;

  streamptr = stream_to_pointer(streamID);

  vlistID = streamInqVlist(streamID);
  tsID    = streamptr->curTsID;
  recID   = recordNewEntry(streamID, tsID);
  record  = &streamptr->tsteps[tsID].records[recID];

  // numavg  = ISEC1_AvgNum;
  numavg  = 0;
  /*
  level1  = ISEC1_Level1;
  level2  = ISEC1_Level2;
  */
  status = grib_get_long(gh, "typeOfFirstFixedSurface", &lpar);
  if ( status == 0 )
    {
      leveltype = (int) lpar;
      GRIB_CHECK(grib_get_double(gh, "level", &dlevel), 0);
      if ( leveltype == 100 ) dlevel *= 100;
      if ( dlevel < -2.e9 || dlevel > 2.e9 ) dlevel = 0;
    }
  else 
    {
      leveltype = 0;
      dlevel = 0;
    }

  level1 = (int) dlevel;
  level2 = 0;

  /* fprintf(stderr, "code %d %d %d %d\n", code, level1, level2, ISEC1_LevelType); */

  (*record).size     = recsize;
  (*record).position = position;
  (*record).code     = code;
  // (*record).table    = ISEC1_CodeTable;
  (*record).table    = 0;
  (*record).ilevel   = level1;
  (*record).ilevel2  = level2;
  (*record).ltype    = leveltype;

  gridtype = gribapiGetGridType(gh);
  /*
  if ( streamptr->unreduced && gridtype == GRID_GAUSSIAN_REDUCED )
    {
      gridtype = GRID_GAUSSIAN;
      ISEC2_NumLon = 2*ISEC2_NumLat;
      ISEC4_NumValues = ISEC2_NumLon*ISEC2_NumLat;
    }
  */
  memset(&grid, 0, sizeof(grid_t));

  GRIB_CHECK(grib_get_size(gh, "values", &datasize), 0);
  GRIB_CHECK(grib_get_long(gh, "numberOfPoints", &numberOfPoints), 0);

  switch (gridtype)
    {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
      {
	int nlon, nlat;

	GRIB_CHECK(grib_get_long(gh, "Ni", &lpar), 0);
	nlon = lpar;
	GRIB_CHECK(grib_get_long(gh, "Nj", &lpar), 0);
	nlat = lpar;

	if ( numberOfPoints != nlon*nlat )
	  Error(func, "wrong datasize! isec4len = %d isec2len = %d",
		(int)numberOfPoints, nlon*nlat);
	grid.size  = numberOfPoints;
	grid.xsize = nlon;
	grid.ysize = nlat;
	grid.xinc  = 0;
	grid.yinc  = 0;
	grid.xdef  = 0;
	/* if ( ISEC2_FirstLon != 0 || ISEC2_LastLon != 0 ) */
	  {
	    if ( grid.xsize > 1 )
	      {
		/*
		if ( ISEC2_ResFlag && ISEC2_LonIncr > 0 )
		  grid.xinc = ISEC2_LonIncr * 0.001;
		else
		  grid.xinc = (ISEC2_LastLon - ISEC2_FirstLon) * 0.001 / (grid.xsize - 1);
		*/
		/* correct xinc if necessary */
		/*
		if ( ISEC2_FirstLon == 0 && ISEC2_LastLon > 354000 )
		  {
		    double xinc = 360. / grid.xsize;

		    if ( fabs(grid.xinc-xinc) > 0.0 )
		      {
			grid.xinc = xinc;
			if ( CDI_Debug ) Message(func, "set xinc to %g", grid.xinc);
		      }
		  }
		*/
	      }
	    /*
	    grid.xfirst = ISEC2_FirstLon * 0.001;
	    grid.xlast  = ISEC2_LastLon  * 0.001;
	    */
	    grid.xdef   = 2;	    
	  }
	grid.ydef  = 0;
	/* if ( ISEC2_FirstLat != 0 || ISEC2_LastLat != 0 ) */
	  {
	    if ( grid.ysize > 1 )
	      {
		/*
		if ( ISEC2_ResFlag && ISEC2_LatIncr > 0 )
		  grid.yinc = ISEC2_LatIncr * 0.001;
		else
		  grid.yinc = (ISEC2_LastLat - ISEC2_FirstLat) * 0.001 / (grid.ysize - 1);
		*/
	      }
	    /*
	    grid.yfirst = ISEC2_FirstLat * 0.001;
	    grid.ylast  = ISEC2_LastLat  * 0.001;
	    */
	    grid.ydef   = 2;	    
	  }
	break;
      }
      /*
    case GRID_GAUSSIAN_REDUCED:
      {
	grid.size   = ISEC4_NumValues;
        grid.rowlon = ISEC2_RowLonPtr;
	grid.ysize  = ISEC2_NumLat;
	grid.xinc  = 0;
	grid.yinc  = 0;
	grid.xdef  = 0;
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
	  Error(func, "wrong datasize! isec4len = %d isec2len = %d",
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
      */
    case GRID_SPECTRAL:
      {
	grid.size  = datasize;
	GRIB_CHECK(grib_get_long(gh, "J", &lpar), 0);
	grid.trunc = lpar;
	break;
      }
      /*
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
      */
    default:
      {
	Error(func, "%s grid unsupported", gridNamePtr(gridtype));
	break;
      }
    }

  grid.isRotated = FALSE;
  /*
  if ( cgribexGetIsRotated(isec2) )
    {
      grid.isRotated = TRUE;
      grid.ypole     = - ISEC2_LatSP * 0.001;
      grid.xpole     =   ISEC2_LonSP * 0.001 - 180;
      grid.angle     = 0;
    }
  */
  grid.xvals = NULL;
  grid.yvals = NULL;
  grid.type  = gridtype;

  gridID = varDefGrid(vlistID, grid, 0);

  zaxistype = gribapiGetZaxisType(leveltype);

  if ( zaxistype == ZAXIS_HYBRID )
    {
      int vctsize;
      double *vctptr;

      GRIB_CHECK(grib_get_long(gh, "NV", &lpar), 0);
      vctsize = lpar;
      vctptr = (double *) malloc(vctsize*sizeof(double));
      GRIB_CHECK(grib_get_double_array(gh, "pv", vctptr, &lpar), 0);
      varDefVCT(vctsize, vctptr);
      free(vctptr);
    }

  //lbounds = cgribexGetZaxisHasBounds(ISEC1_LevelType);

  if ( prec > 32 ) prec = DATATYPE_PACK32;
  if ( prec <  0 ) prec = DATATYPE_PACK;

  codetable = 0;
  varAddRecord(recID, code, gridID, zaxistype, lbounds, level1, level2,
	       prec, &varID, &levelID, numavg, codetable, leveltype);

  (*record).varID   = varID;
  (*record).levelID = levelID;

  varDefZtype(varID, ztype);

  if ( varInqInst(varID) == CDI_UNDEFID )
    {
      long center, subcenter;
      int instID;
      GRIB_CHECK(grib_get_long(gh, "centre", &center), 0);
      GRIB_CHECK(grib_get_long(gh, "subCentre", &subcenter), 0);
      instID    = institutInq((int)center, (int)subcenter, NULL, NULL);
      if ( instID == CDI_UNDEFID )
	instID = institutDef((int)center, (int)subcenter, NULL, NULL);
      varDefInst(varID, instID);
    }

  if ( varInqModel(varID) == CDI_UNDEFID )
    {
      int modelID;
      long processID;
      GRIB_CHECK(grib_get_long(gh, "generatingProcessIdentifier", &processID), 0);
      modelID = modelInq(varInqInst(varID), processID, NULL);
      if ( modelID == CDI_UNDEFID )
	modelID = modelDef(varInqInst(varID), processID, NULL);
      varDefModel(varID, modelID);
    }

  if ( varInqTable(varID) == CDI_UNDEFID )
    {
      int tableID;

      //tableID = tableInq(varInqModel(varID), ISEC1_CodeTable, NULL);
      /*
      if ( tableID == CDI_UNDEFID && cdiPartabPath )
	{
	  char line[8192];
	  char command[4096];
	  FILE *pfp;
	  int nfiles;
	  int max_partab = 4096;
	  char *fnames[4096];
	  int cen, sub, ver, tab, nf, fi;
	  int icen, isub, iver, itab;

	  icen = ISEC1_CenterID;
          isub = ISEC1_SubCenterID;
          iver = ISEC1_ModelID;
          itab = ISEC1_CodeTable;
	  
	  sprintf(command, "cd %s 2> /dev/null; ls * 2> /dev/null", cdiPartabPath);

	  pfp = popen(command, "r");
	  if ( pfp == NULL )
	    SysError(func, "popen %s failed", command);
	  
	  nfiles = 0;
	  while ( _readline_(pfp, line, 8192) )
	    {
	      cen = -1; sub = -1; ver = -1; tab = -1;
	      nf = sscanf(line, "Cen%d_Sub%d_Ver%d_Tab%d", &cen, &sub, &ver, &tab);
	      printf("%d %d %d %d %d\n", nf, cen, sub, ver, tab);
	      if ( nfiles >= max_partab )
		{
		  Warning(func, "Too many parameter tables (limit: %d)", max_partab);
		  break;
		}
	      fnames[nfiles++] = strdupx(line);
	      printf("test: %s\n", line);
	    }

	  pclose(pfp);

	  if ( nfiles )
	    {
	      cen = -1; sub = -1; ver = -1; tab = -1;
	      for ( fi = 0; fi < nfiles; ++fi )
		{
		  nf = sscanf(fnames[fi], "Cen%d_Sub%d_Ver%d_Tab%d", &cen, &sub, &ver, &tab);
		  if ( nf == 4 && cen == icen && sub == isub && ver == iver && tab == itab ) goto END_LABEL;
		}
	      for ( fi = 0; fi < nfiles; ++fi )
		{
		  nf = sscanf(fnames[fi], "Cen%d_Sub%d_Tab%d", &cen, &sub, &tab);
		  if ( nf == 3 && cen == icen && sub == isub && tab == itab ) goto END_LABEL;
		}

	    END_LABEL:

	      if ( fi < nfiles )
		{
		  printf("found table %s\n", fnames[fi]);
		}
	    }
	}
      */
      /*
      if ( tableID == CDI_UNDEFID )
	tableID = tableDef(varInqModel(varID), ISEC1_CodeTable, NULL);
      varDefTable(varID, tableID);
      */
    }

  streamptr->tsteps[tsID].nallrecs++;
  streamptr->nrecs++;

  if ( CDI_Debug )
    Message(func, "varID = %d  code = %d  zaxistype = %d  gridID = %d  levelID = %d",
	    varID, code, zaxistype, gridID, levelID);
}
#endif

void gribapiScanTimestep1(int streamID)
{
  static char func [] = "gribapiScanTimestep1";
#if  defined  (HAVE_LIBGRIB_API)
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
  int warn_time = TRUE;
  int warn_numavg = TRUE;
  int taxisID = -1;
  int rdate = 0, rtime = 0, tunit = 0, fcast = 0;
  TAXIS *taxis;
  int vlistID;
  int ztype;
  long unzipsize;
  compvar2_t compVar, compVar0;
  stream_t *streamptr;
  grib_handle *gh = NULL;
  int leveltype;
  long editionNumber;
  long lpar;
  int bitsPerValue;
  double dlevel = 0;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  streamptr->curTsID = 0;

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

      gh = grib_handle_new_from_message(NULL, (void *) gribbuffer, recsize);
      GRIB_CHECK(grib_set_double(gh, "missingValue", GRIB_MISSVAL), 0);

      GRIB_CHECK(grib_get_long(gh, "editionNumber", &editionNumber), 0);

      if ( editionNumber <= 1 )
	{
	  GRIB_CHECK(grib_get_long(gh, "indicatorOfParameter", &lpar), 0);
	  rcode = (int) lpar;
	  status = grib_get_long(gh, "indicatorOfTypeOfLevel", &lpar);
	  if ( status == 0 )
	    {
	      leveltype = (int) lpar;
	      GRIB_CHECK(grib_get_double(gh, "level", &dlevel), 0);
	      if ( leveltype == 100 ) dlevel *= 100;
	      if ( dlevel < -2.e9 || dlevel > 2.e9 ) dlevel = 0;
	    }
	  else 
	    {
	      leveltype = 0;
	      dlevel = 0;
	    }
	}
      else
	{
	  GRIB_CHECK(grib_get_long(gh, "parameterNumber", &lpar), 0);
	  rcode = (int) lpar;
	  status = grib_get_long(gh, "typeOfFirstFixedSurface", &lpar);
	  if ( status == 0 )
	    {
	      leveltype = (int) lpar;
	      GRIB_CHECK(grib_get_double(gh, "level", &dlevel), 0);
	      if ( leveltype == 100 ) dlevel *= 100;
	      if ( dlevel < -2.e9 || dlevel > 2.e9 ) dlevel = 0;
	    }
	  else 
	    {
	      leveltype = 0;
	      dlevel = 0;
	    }
	}

      level1 = (int) dlevel;
      level2 = 0;

      GRIB_CHECK(grib_get_long(gh, "dataDate", &lpar), 0);
      vdate = (int) lpar;
      GRIB_CHECK(grib_get_long(gh, "dataTime", &lpar), 0);
      vtime = (int) lpar*100;

      GRIB_CHECK(grib_get_long(gh,"bitsPerValue", &lpar),0);
      bitsPerValue = (int) lpar;
      if ( bitsPerValue > 0 && bitsPerValue <= 32 )
	prec = bitsPerValue;
      else
        prec = DATATYPE_PACK;

      if ( nrecs == 0 )
	{
	  datetime0.date = vdate;
	  datetime0.time = vtime;
	  rdate = 0;
	  rtime = 0;
	  tunit = 0;
	  fcast = 0;
	  /*
	  rdate = gribRefDate(isec1);
	  rtime = gribRefTime(isec1);
	  tunit = cgribexGetTimeUnit(isec1);
	  fcast = gribTimeIsFC(isec1);
	  */
	}
      else
	{
	  datetime.date  = vdate;
	  datetime.time  = vtime;
	  compVar.code   = rcode;
          // compVar.table  = ISEC1_CodeTable;
	  compVar.table  = 0;
          compVar.level1 = level1;
          compVar.level2 = level2;
	  compVar.ltype  = leveltype;

	  for ( recID = 0; recID < nrecs; recID++ )
	    {
	      compVar0.code   = streamptr->tsteps[0].records[recID].code;
	      compVar0.table  = streamptr->tsteps[0].records[recID].table;
	      compVar0.level1 = streamptr->tsteps[0].records[recID].ilevel;
	      compVar0.level2 = streamptr->tsteps[0].records[recID].ilevel2;
	      compVar0.ltype  = streamptr->tsteps[0].records[recID].ltype;

	      if ( memcmp(&compVar0, &compVar, sizeof(compvar2_t)) == 0 ) break;
	    }

	  if ( cdiInventoryMode == 1 )
	    {
	      if ( recID < nrecs ) break;
	      if ( warn_time )
		if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) != 0 )
		  {
		    Warning(func, "Inconsistent verification time (code %d level %d)", rcode, level1);
		    warn_time = FALSE;
		  }
	    }
	  else
	    {
	      if ( memcmp(&datetime, &datetime0, sizeof(DateTime)) != 0 ) break;

	      if ( recID < nrecs )
		{
		  Warning(func, "Code %d level %d already exist, skipped!", rcode, level1);
		  continue;
		}
	    }
	}
      /*
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
      */
      nrecs++;

      if ( CDI_Debug )
	Message(func, "%4d %8d %4d  %8d %8d %6d", nrecs, (int)recpos, rcode, level1, vdate, vtime);

      gribapiAddRecord(streamID, rcode, gh, recsize, recpos, prec, ztype);
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
      (RECORD *) realloc(streamptr->tsteps[0].records, nrecords*sizeof(RECORD));
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
#else
  Error(func, "GRIB_API support not compiled in!");
#endif
}


int gribapiScanTimestep2(int streamID)
{
  static char func [] = "gribapiScanTimestep2";
  int status = 0;
#if  defined  (HAVE_LIBGRIB_API)
  off_t recpos = 0;
  unsigned char *gribbuffer = NULL;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int fileID;
  int rcode = 0, level1 = 0, level2 = 0, vdate = 0, vtime = 0;
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
  compvar2_t compVar, compVar0;
  stream_t *streamptr;
  grib_handle *gh = NULL;
  int leveltype;
  long editionNumber;
  long lpar;
  double dlevel = 0;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

  streamptr->curTsID = 1;

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

      gh = grib_handle_new_from_message(NULL, (void *) gribbuffer, recsize);
      GRIB_CHECK(grib_set_double(gh, "missingValue", GRIB_MISSVAL), 0);

      GRIB_CHECK(grib_get_long(gh, "editionNumber", &editionNumber), 0);

      if ( editionNumber <= 1 )
	{
	  GRIB_CHECK(grib_get_long(gh, "indicatorOfParameter", &lpar), 0);
	  rcode = (int) lpar;
	  status = grib_get_long(gh, "indicatorOfTypeOfLevel", &lpar);
	  if ( status == 0 )
	    {
	      leveltype = (int) lpar;
	      GRIB_CHECK(grib_get_double(gh, "level", &dlevel), 0);
	      if ( leveltype == 100 ) dlevel *= 100;
	      if ( dlevel < -2.e9 || dlevel > 2.e9 ) dlevel = 0;
	    }
	  else 
	    {
	      leveltype = 0;
	      dlevel = 0;
	    }
	}
      else
	{
	  GRIB_CHECK(grib_get_long(gh, "parameterNumber", &lpar), 0);
	  rcode = (int) lpar;
	  status = grib_get_long(gh, "typeOfFirstFixedSurface", &lpar);
	  if ( status == 0 )
	    {
	      leveltype = (int) lpar;
	      GRIB_CHECK(grib_get_double(gh, "level", &dlevel), 0);
	      if ( leveltype == 100 ) dlevel *= 100;
	      if ( dlevel < -2.e9 || dlevel > 2.e9 ) dlevel = 0;
	    }
	  else 
	    {
	      leveltype = 0;
	      dlevel = 0;
	    }
	}

      level1 = (int) dlevel;
      level2 = 0;

      GRIB_CHECK(grib_get_long(gh, "dataDate", &lpar), 0);
      vdate = (int) lpar;
      GRIB_CHECK(grib_get_long(gh, "dataTime", &lpar), 0);
      vtime = (int) lpar*100;

      if ( rindex == 0 )
	{
	  if ( taxisInqType(taxisID) == TAXIS_RELATIVE )
	    {
	      taxis->type  = TAXIS_RELATIVE;
	      taxis->rdate = 0;
	      taxis->rtime = 0;
	      taxis->unit  = 0;
	      /*
	      taxis->rdate = gribRefDate(isec1);
	      taxis->rtime = gribRefTime(isec1);
	      taxis->unit  = cgribexGetTimeUnit(isec1);
	      */
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
      /*
      if ( ISEC1_AvgNum )
	{
	  if (  taxis->numavg && warn_numavg &&
		(taxis->numavg != ISEC1_AvgNum) )
	    {
	      warn_numavg = FALSE;
	    }
	  else
	    {
	      taxis->numavg = ISEC1_AvgNum;
	    }
	}
      */
      datetime.date  = vdate;
      datetime.time  = vtime;
      compVar.code   = rcode;
      // compVar.table  = ISEC1_CodeTable;
      compVar.table  = 0;
      compVar.level1 = level1;
      compVar.level2 = level2;
      compVar.ltype  = leveltype;
      for ( recID = 0; recID < nrecords; recID++ )
	{
	  compVar0.code   = streamptr->tsteps[tsID].records[recID].code;
	  compVar0.table  = streamptr->tsteps[tsID].records[recID].table;
	  compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
	  compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
	  compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

	  if ( memcmp(&compVar0, &compVar, sizeof(compvar2_t)) == 0 ) break;
	}

      if ( recID == nrecords )
	{
	  Warning(func, "Code %d level %d not found at timestep %d!", rcode, level1, tsID+1);
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

	      Warning(func, "Code %d level %d already exist, skipped!", rcode, level1);
	      continue;
	    }
	  else
	    {
	      streamptr->tsteps[tsID].records[recID].used = TRUE;
	      streamptr->tsteps[tsID].recIDs[rindex] = recID;
	    }    
	}

      if ( CDI_Debug )
	Message(func, "%4d %8d %4d %8d %8d %6d", rindex+1, (int)recpos, rcode, level1, vdate, vtime);

      streamptr->tsteps[tsID].records[recID].size = recsize;

      compVar0.code   = streamptr->tsteps[tsID].records[recID].code;
      compVar0.table  = streamptr->tsteps[tsID].records[recID].table;
      compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
      compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
      compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

      if ( memcmp(&compVar0, &compVar, sizeof(compvar2_t)) != 0 )
	{
	  Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		  tsID, recID,
		  streamptr->tsteps[tsID].records[recID].code, rcode,
		  streamptr->tsteps[tsID].records[recID].ilevel, level1);
	  return (CDI_EUFSTRUCT);
	}

      streamptr->tsteps[1].records[recID].position = recpos;
      varID = streamptr->tsteps[tsID].records[recID].varID;
      gridID = vlistInqVarGrid(vlistID, varID);
      /*
      if ( gridInqSize(gridID) == 1 && gridInqType(gridID) == GRID_LONLAT )
	{
	  if ( IS_NOT_EQUAL(gridInqXval(gridID, 0),ISEC2_FirstLon*0.001) ||
	       IS_NOT_EQUAL(gridInqYval(gridID, 0),ISEC2_FirstLat*0.001) )
	    gridChangeType(gridID, GRID_TRAJECTORY);
	}
      */
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
#else
  Error(func, "GRIB_API support not compiled in!");
#endif

  return (status);
}


int gribapiScanTimestep(int streamID)
{
  static char func [] = "gribapiScanTimestep";
  int status = 0;
#if  defined  (HAVE_LIBGRIB_API)
  long recsize = 0;
  off_t recpos = 0;
  unsigned char *gribbuffer;
  long buffersize = 0;
  int iret = 0, ipunp = 0, iword = 0;
  int fileID;
  int rcode = 0, level1 = 0, level2 = 0, vdate = 0, vtime = 0;
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
  compvar2_t compVar, compVar0;
  stream_t *streamptr;
  grib_handle *gh = NULL;
  int leveltype;
  long editionNumber;
  long lpar;
  double dlevel = 0;

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

	  gh = grib_handle_new_from_message(NULL, (void *) gribbuffer, recsize);
	  GRIB_CHECK(grib_set_double(gh, "missingValue", GRIB_MISSVAL), 0);

	  GRIB_CHECK(grib_get_long(gh, "editionNumber", &editionNumber), 0);

	  if ( editionNumber <= 1 )
	    {
	      GRIB_CHECK(grib_get_long(gh, "indicatorOfParameter", &lpar), 0);
	      rcode = (int) lpar;
	      status = grib_get_long(gh, "indicatorOfTypeOfLevel", &lpar);
	      if ( status == 0 )
		{
		  leveltype = (int) lpar;
		  GRIB_CHECK(grib_get_double(gh, "level", &dlevel), 0);
		  if ( leveltype == 100 ) dlevel *= 100;
		  if ( dlevel < -2.e9 || dlevel > 2.e9 ) dlevel = 0;
		}
	      else 
		{
		  leveltype = 0;
		  dlevel = 0;
		}
	    }
	  else
	    {
	      GRIB_CHECK(grib_get_long(gh, "parameterNumber", &lpar), 0);
	      rcode = (int) lpar;
	      status = grib_get_long(gh, "typeOfFirstFixedSurface", &lpar);
	      if ( status == 0 )
		{
		  leveltype = (int) lpar;
		  GRIB_CHECK(grib_get_double(gh, "level", &dlevel), 0);
		  if ( leveltype == 100 ) dlevel *= 100;
		  if ( dlevel < -2.e9 || dlevel > 2.e9 ) dlevel = 0;
		}
	      else 
		{
		  leveltype = 0;
		  dlevel = 0;
		}
	    }

	  level1 = (int) dlevel;
	  level2 = 0;

	  GRIB_CHECK(grib_get_long(gh, "dataDate", &lpar), 0);
	  vdate = (int) lpar;
	  GRIB_CHECK(grib_get_long(gh, "dataTime", &lpar), 0);
	  vtime = (int) lpar*100;

	  if ( rindex == nrecs ) break;

	  if ( rindex == 0 )
	    {
	      taxisID = vlistInqTaxis(vlistID);
	      if ( taxisInqType(taxisID) == TAXIS_RELATIVE )
		{
		  taxis->type  = TAXIS_RELATIVE;
		  taxis->rdate = 0;
		  taxis->rtime = 0;
		  taxis->unit  = 0;
		  /*
		    taxis->rdate = gribRefDate(isec1);
		    taxis->rtime = gribRefTime(isec1);
		    taxis->unit  = cgribexGetTimeUnit(isec1);
		  */
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
	  /*
	  if ( ISEC1_AvgNum )
	    {
	      if (  taxis->numavg && warn_numavg &&
		   (taxis->numavg != ISEC1_AvgNum) )
		{
		  warn_numavg = FALSE;
		}
	      else
		{
		  taxis->numavg = ISEC1_AvgNum;
		}
	    }
	  */
	  datetime.date  = vdate;
	  datetime.time  = vtime;
	  compVar.code   = rcode;
	  // compVar.table  = ISEC1_CodeTable;
	  compVar.table  = 0;
          compVar.level1 = level1;
          compVar.level2 = level2;
          compVar.ltype  = leveltype;
	  for ( vrecID = 0; vrecID < nrecs; vrecID++ )
	    {
	      recID   = streamptr->tsteps[1].recIDs[vrecID];
	      compVar0.code   = streamptr->tsteps[tsID].records[recID].code;
	      compVar0.table  = streamptr->tsteps[tsID].records[recID].table;
	      compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
	      compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
	      compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

	      if ( memcmp(&compVar0, &compVar, sizeof(compvar2_t)) == 0 ) break;
	    }

	  if ( vrecID == nrecs )
	    {
	      Warning(func, "Code %d level %d not available at timestep %d!",
		      rcode, level1, tsID+1);

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
		    Warning(func, "Code %d level %d already exist, skipped!", rcode, level1);

		  continue;
		}
	      else
		{
		  streamptr->tsteps[tsID].records[recID].used = TRUE;
		  streamptr->tsteps[tsID].recIDs[rindex] = recID;
		}    
	    }

	  if ( CDI_Debug )
	    Message(func, "%4d %8d %4d %8d %8d %6d", rindex+1, (int)recpos, rcode, level1, vdate, vtime);

	  compVar0.code   = streamptr->tsteps[tsID].records[recID].code;
	  compVar0.table  = streamptr->tsteps[tsID].records[recID].table;
	  compVar0.level1 = streamptr->tsteps[tsID].records[recID].ilevel;
	  compVar0.level2 = streamptr->tsteps[tsID].records[recID].ilevel2;
	  compVar0.ltype  = streamptr->tsteps[tsID].records[recID].ltype;

	  if ( memcmp(&compVar0, &compVar, sizeof(compvar2_t)) != 0 )
	    {
	      Message(func, "tsID = %d recID = %d code = %3d new %3d  level = %3d new %3d",
		      tsID, recID,
		      streamptr->tsteps[tsID].records[recID].code, rcode,
		      streamptr->tsteps[tsID].records[recID].ilevel, level1);
	      Error(func, "Invalid, unsupported or inconsistent record structure");
	    }
	  
	  streamptr->tsteps[tsID].records[recID].position = recpos;
	  streamptr->tsteps[tsID].records[recID].size = recsize;

	  if ( CDI_Debug )
	    Message(func, "%4d %8d %4d %8d %8d %6d", rindex, (int)recpos, rcode, level1, vdate, vtime);

	  rindex++;
	}

      for ( vrecID = 0; vrecID < nrecs; vrecID++ )
	{
	  recID   = streamptr->tsteps[tsID].recIDs[vrecID];
	  if ( ! streamptr->tsteps[tsID].records[recID].used ) break;
	}

      if ( vrecID < nrecs )
	{
	  Warning(func, "Code %d level %d not found at timestep %d!",
		  streamptr->tsteps[tsID].records[recID].code,
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

  status = streamptr->ntsteps;
#else
  Error(func, "GRIB_API support not compiled in!");
#endif

  return (status);
}


int gribapiDecode(unsigned char *gribbuffer, int gribsize, double *data, int gridsize,
		  int unreduced, int *nmiss, int *zip)
{
  static char func[] = "gribapiDecode";
  int status = 0;
#if  defined  (HAVE_LIBGRIB_API)
  long lpar;
  long editionNumber, numberOfPoints;
  size_t datasize, dummy, recsize;
  grib_handle *gh = NULL;

  if ( unreduced )
    {
      static int lwarn = 1;

      if ( lwarn )
	{
	  lwarn = 0;
	  Warning(func, "Conversion of gaussian reduced grids unsupported!\n");
	}
    }

  recsize = gribsize;
  gh = grib_handle_new_from_message(NULL, (void *) gribbuffer, recsize);
  GRIB_CHECK(grib_set_double(gh, "missingValue", GRIB_MISSVAL), 0);

  GRIB_CHECK(grib_get_long(gh, "editionNumber", &editionNumber), 0);

  /* get the size of the values array*/
  GRIB_CHECK(grib_get_size(gh, "values", &datasize), 0);
  GRIB_CHECK(grib_get_long(gh, "numberOfPoints", &numberOfPoints), 0);

  // printf("values_size = %d  numberOfPoints = %ld\n", datasize, numberOfPoints);

  if ( gridsize != (int) datasize )
    Error(func, "Internal problem: gridsize(%d) != datasize(%d)!\n", gridsize, datasize);
  dummy = datasize;
  GRIB_CHECK(grib_get_double_array(gh, "values", data, &dummy), 0);

  int gridtype;
  GRIB_CHECK(grib_get_long(gh, "gridDefinitionTemplateNumber", &lpar), 0);
  gridtype = (int) lpar;

  *nmiss = 0;
  if ( gridtype < 50 || gridtype > 53 )
    {
      GRIB_CHECK(grib_get_long(gh, "numberOfMissing", &lpar), 0);
      *nmiss = (int) lpar;
      // printf("gridtype %d, nmiss %d\n", gridtype, nmiss);
    }
#else
  Error(func, "GRIB_API support not compiled in!");
#endif

  return (status);
}


#if  defined  (HAVE_LIBGRIB_API)
static
void gribapiDefInstitut(grib_handle *gh, int vlistID, int varID)
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
      GRIB_CHECK(grib_set_long(gh, "centre", center), 0);
      GRIB_CHECK(grib_set_long(gh, "subCentre", subcenter), 0);
    }
}

static
void gribapiDefModel(grib_handle *gh, int vlistID, int varID)
{
  int modelID;

  if ( vlistInqModel(vlistID) != CDI_UNDEFID )
    modelID = vlistInqModel(vlistID);
  else
    modelID = vlistInqVarModel(vlistID, varID);

  if ( modelID != CDI_UNDEFID )
    GRIB_CHECK(grib_set_long(gh, "generatingProcessIdentifier", modelInqGribID(modelID)), 0);
}

static
void gribapiDefCode(grib_handle *gh, int code, int codetable)
{
  //ISEC1_CodeTable = codetable;
  if ( code < 0 ) code = -code;
  // GRIB_CHECK(grib_set_long(gh, "indicatorOfParameter", code), 0);
  GRIB_CHECK(grib_set_long(gh, "parameterCategory", 255), 0);
  GRIB_CHECK(grib_set_long(gh, "parameterNumber", code), 0);
}

static
void gribapiDefTime(grib_handle *gh , int date, int time, int numavg, int timeID)
{
  GRIB_CHECK(grib_set_long(gh, "dataDate", date), 0);
  GRIB_CHECK(grib_set_long(gh, "dataTime", time/100), 0);
}

static
void gribapiDefGrid(grib_handle *gh, int gridID)
{
  static char func[] = "gribapiDefGrid";
  int gridtype;
  static short lwarn = TRUE;

  // ISEC1_Sec2Or3Flag = 128;
  
  gridtype = gridInqType(gridID);

  // ISEC1_GridDefinition = 255;

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

  // ISEC2_Reduced = FALSE;

  // ISEC2_ScanFlag = 0;

  switch (gridtype)
    {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
    case GRID_GAUSSIAN_REDUCED:
    case GRID_TRAJECTORY:
      {
	int nlon = 0, nlat;
	double latIncr;
	size_t len;

	if ( gridtype == GRID_GAUSSIAN_REDUCED )
	  {
	    //  ISEC2_Reduced = TRUE;
	    nlon = 0;
	    // gridInqRowlon(gridID, ISEC2_RowLonPtr);
	  }
	else
	  {
	    nlon = gridInqXsize(gridID);
	  }

	nlat = gridInqYsize(gridID);
	if ( gridtype == GRID_GAUSSIAN || gridtype == GRID_GAUSSIAN_REDUCED )
	  GRIB_CHECK(grib_set_long(gh, "gridDefinitionTemplateNumber", GRIBAPI_GTYPE_GAUSSIAN), 0);
	else if ( gridtype == GRID_LONLAT && gridIsRotated(gridID) )
	  GRIB_CHECK(grib_set_long(gh, "gridDefinitionTemplateNumber", GRIBAPI_GTYPE_LATLON_ROT), 0);
	else
	  GRIB_CHECK(grib_set_long(gh, "gridDefinitionTemplateNumber", GRIBAPI_GTYPE_LATLON), 0);

	GRIB_CHECK(grib_set_long(gh, "Ni", nlon), 0);
	GRIB_CHECK(grib_set_long(gh, "Nj", nlat), 0);
	GRIB_CHECK(grib_set_double(gh, "latitudeOfFirstGridPointInDegrees",  gridInqYval(gridID,      0)), 0);
	GRIB_CHECK(grib_set_double(gh, "latitudeOfLastGridPointInDegrees",   gridInqYval(gridID, nlat-1)), 0);
	GRIB_CHECK(grib_set_double(gh, "longitudeOfFirstGridPointInDegrees", gridInqXval(gridID,      0)), 0);
	GRIB_CHECK(grib_set_double(gh, "longitudeOfLastGridPointInDegrees",  gridInqXval(gridID, nlon-1)), 0);
	GRIB_CHECK(grib_set_double(gh, "iDirectionIncrementInDegrees", gridInqXinc(gridID)), 0);
	/*
	if ( fabs(gridInqXinc(gridID)*1000 - ISEC2_LonIncr) > FLT_EPSILON )
	  ISEC2_LonIncr = 0;
	*/
	if ( gridtype == GRID_GAUSSIAN )
	  GRIB_CHECK(grib_set_long(gh, "numberOfParallelsBetweenAPoleAndTheEquator", nlat/2), 0);
	else
	  {
	    latIncr = gridInqYinc(gridID);
	    if ( latIncr < 0 ) latIncr = -latIncr;
	    GRIB_CHECK(grib_set_double(gh, "jDirectionIncrementInDegrees", latIncr), 0);
	    /*
	    if ( fabs(gridInqYinc(gridID)*1000 - ISEC2_LatIncr) > FLT_EPSILON )
	      ISEC2_LatIncr = 0;
	    */
	  }
	/*
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
	*/

	/* East -> West */
	//if ( ISEC2_LastLon < ISEC2_FirstLon ) ISEC2_ScanFlag += 128;

	/* South -> North */
	//if ( ISEC2_LastLat > ISEC2_FirstLat ) ISEC2_ScanFlag += 64;

	GRIB_CHECK(grib_set_string(gh, "typeOfPacking", "grid_simple", &len), 0);

	break;
      }
      /*
    case GRID_LCC:
      {
	double originLon, originLat, lonParY, lat1, lat2, xincm, yincm;
	int xsize, ysize;
	int projflag, scanflag;

	xsize = gridInqXsize(gridID);
	ysize = gridInqYsize(gridID);

	gridInqLCC(gridID, &originLon, &originLat, &lonParY, &lat1, &lat2, &xincm, &yincm,
		   &projflag, &scanflag);

	ISEC2_GridType = GRIBAPI_GTYPE_LCC;
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
      */
    case GRID_SPECTRAL:
      {
	int trunc = gridInqTrunc(gridID);
	size_t len = 15;

	GRIB_CHECK(grib_set_long(gh, "gridDefinitionTemplateNumber", GRIBAPI_GTYPE_SPECTRAL), 0);
	GRIB_CHECK(grib_set_long(gh, "J", trunc), 0);
	GRIB_CHECK(grib_set_long(gh, "K", trunc), 0);
	GRIB_CHECK(grib_set_long(gh, "M", trunc), 0);

	GRIB_CHECK(grib_set_long(gh, "numberOfDataPoints", gridInqSize(gridID)), 0);
	GRIB_CHECK(grib_set_long(gh, "numberOfValues", gridInqSize(gridID)), 0);

	GRIB_CHECK(grib_set_string(gh, "typeOfPacking", "spectral_simple", &len), 0);
	/*
	ISEC2_RepType  = 1;
	ISEC2_RepMode  = 1;
	*/
	break;
      }
      /*
    case GRID_GME:
      {
	ISEC2_GridType   = GRIBAPI_GTYPE_GME;
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
      */
    default:
      Error(func, "%s grid unsupported!", gridNamePtr(gridtype));
    }
}

static
void gribapiDefLevel(grib_handle *gh, int code, int zaxisID, int levelID)
{
  static char func[] = "gribapiDefLevel";
  double level;
  int zaxistype, ltype;
  static int warning = 1;

  zaxistype = zaxisInqType(zaxisID);
  ltype = zaxisInqLtype(zaxisID);

  if ( zaxistype == ZAXIS_GENERIC && ltype == 0 )
    {
      zaxistype = ZAXIS_PRESSURE;
      zaxisChangeType(zaxisID, zaxistype);
    }

  // ISEC2_NumVCP = 0;

  switch (zaxistype)
    {
    case ZAXIS_SURFACE:
      {
	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_SURFACE), 0);
	//GRIB_CHECK(grib_set_long(gh, "scaleFactorOfFirstFixedSurface", 0), 0);
	//GRIB_CHECK(grib_set_long(gh, "scaledValueOfFirstFixedSurface", 0), 0);
	GRIB_CHECK(grib_set_long(gh, "level", (long) zaxisInqLevel(zaxisID, levelID)), 0);
	/*
	ISEC1_Level1    = (int) zaxisInqLevel(zaxisID, levelID);
	ISEC1_Level2    = 0;
	*/
	break;
      }
    case ZAXIS_MEANSEA:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_MEANSEA), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    case ZAXIS_HYBRID:
    case ZAXIS_HYBRID_HALF:
      {
	int vctsize;

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_HYBRID), 0);
	if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
	  {
	    GRIB_CHECK(grib_set_long(gh, "level", (long) zaxisInqLevel(zaxisID, levelID)), 0);
	    Error(func, "hybrid_half model level code missing!");
	    /*
	    ISEC1_Level1    = (int) zaxisInqLbound(zaxisID, levelID);
	    ISEC1_Level2    = (int) zaxisInqUbound(zaxisID, levelID);
	    */
	  }
	else
	  {
	    GRIB_CHECK(grib_set_long(gh, "level", (long) zaxisInqLevel(zaxisID, levelID)), 0);
	  }

	vctsize = zaxisInqVctSize(zaxisID);
	if ( vctsize == 0 && warning )
	  {
	    Warning(func, "VCT missing. ( code = %d, zaxisID = %d )", code, zaxisID);
	    warning = 0;
	  }
	GRIB_CHECK(grib_set_long(gh, "PVPresent", 1), 0);
	GRIB_CHECK(grib_set_double_array(gh, "pv", zaxisInqVctPtr(zaxisID), vctsize), 0);

	break;
      }
    case ZAXIS_PRESSURE:
      {
	char units[128];

	level = zaxisInqLevel(zaxisID, levelID);
	if ( level < 0 )
	  Warning(func, "pressure level of %f Pa is below 0.", level);

	zaxisInqUnits(zaxisID, units);
	if ( memcmp(units, "hPa", 3) == 0 || memcmp(units, "mb",2 ) == 0 )
	  level = level;
	else
	  level /= 100;

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_ISOBARIC), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    case ZAXIS_HEIGHT:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_HEIGHT), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    case ZAXIS_ALTITUDE:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_ALTITUDE), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    case ZAXIS_SIGMA:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_SIGMA), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    case ZAXIS_DEPTH_BELOW_LAND:
      {
	if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
	  {
	    GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_LANDDEPTH), 0);
	    GRIB_CHECK(grib_set_double(gh, "level", zaxisInqLbound(zaxisID, levelID)), 0);
	    GRIB_CHECK(grib_set_long(gh, "typeOfSecondFixedSurface", GRIBAPI_LTYPE_LANDDEPTH), 0);
	    GRIB_CHECK(grib_set_double(gh, "level", zaxisInqUbound(zaxisID, levelID)), 0);
	    /*
	    ISEC1_LevelType = GRIBAPI_LTYPE_LANDDEPTH_LAYER;
	    ISEC1_Level1    = (int) zaxisInqLbound(zaxisID, levelID);
	    ISEC1_Level2    = (int) zaxisInqUbound(zaxisID, levelID);
	    */
	  }
	else
	  {
	    level = zaxisInqLevel(zaxisID, levelID);

	    GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_LANDDEPTH), 0);
	    GRIB_CHECK(grib_set_double(gh, "level", level), 0);
	  }

	break;
      }
    case ZAXIS_DEPTH_BELOW_SEA:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_SEADEPTH), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    case ZAXIS_ISENTROPIC:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", GRIBAPI_LTYPE_ISENTROPIC), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    case ZAXIS_GENERIC:
      {
	level = zaxisInqLevel(zaxisID, levelID);

	GRIB_CHECK(grib_set_long(gh, "typeOfFirstFixedSurface", ltype), 0);
	GRIB_CHECK(grib_set_double(gh, "level", level), 0);

	break;
      }
    default:
      {
	Error(func, "zaxistype >%s< unsupported", zaxisNamePtr(zaxistype));
	break;
      }
    }
}
#endif


size_t gribapiEncode(int varID, int levelID, int vlistID, int gridID, int zaxisID, int date, int time, int numavg, 
		     long datasize, const double *data, int nmiss, unsigned char *gribbuffer, size_t gribbuffersize)
{
  static char func[] = "gribapiEncode";
  size_t nbytes = 0;
#if  defined  (HAVE_LIBGRIB_API)
  size_t recsize = 0;
  void *dummy = NULL;
  long edition = 2;
  int datatype;
  int code, tableID;
  long bitsPerValue;
  grib_handle *gh = NULL;

  code    = vlistInqVarCode(vlistID, varID);
  tableID = vlistInqVarTable(vlistID, varID);
  datatype = vlistInqVarDatatype(vlistID, varID);

  gh = grib_handle_new_from_template(NULL, "GRIB2");
  if ( gh == NULL ) Error(func, "grib_handle_new_from_template failed!");
  
  GRIB_CHECK(grib_set_long(gh, "editionNumber", edition), 0);

  gribapiDefInstitut(gh, vlistID, varID);
  gribapiDefModel(gh, vlistID, varID);

  gribapiDefCode(gh, code, tableInqNum(tableID));
  gribapiDefTime(gh, date, time, numavg, vlistInqTaxis(vlistID));
  gribapiDefGrid(gh, gridID);
  gribapiDefLevel(gh, code, zaxisID, levelID);

  // GRIB_CHECK(grib_set_double(gh, "missingValue", GRIB_MISSVAL), 0);

  bitsPerValue = grbBitsPerValue(datatype);
  GRIB_CHECK(grib_set_long(gh, "bitsPerValue", bitsPerValue), 0);

  GRIB_CHECK(grib_set_double_array(gh, "values", data, datasize), 0);

  /* get the size of coded message  */
  GRIB_CHECK(grib_get_message(gh, (const void **)&dummy, &recsize), 0);
  //recsize += 512; /* add some space for possible filling */

  if ( recsize > gribbuffersize )
    Error(func, "Internal problem: GRIB buffer to small (size = %ld, needed = %ld", gribbuffersize, recsize);

  /* get a copy of the coded message */
  GRIB_CHECK(grib_get_message_copy(gh, gribbuffer, &recsize), 0);

  grib_handle_delete(gh);

  nbytes = recsize;
#else
  Error(func, "GRIB_API support not compiled in!");
#endif

  return (nbytes);
}
