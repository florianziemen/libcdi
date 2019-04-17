#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "stream_cdf.h"
#include "cdf.h"
#include "cdf_int.h"
#include "vlist.h"
#include "vlist_var.h"


void cdfDefVarDeflate(int ncid, int ncvarid, int deflate_level)
{
#if  defined(HAVE_NETCDF4)
  int retval;
  /* Set chunking, shuffle, and deflate. */
  const int shuffle = 1;
  const int deflate = 1;

  if ( deflate_level < 1 || deflate_level > 9 ) deflate_level = 1;

  if ( (retval = nc_def_var_deflate(ncid, ncvarid, shuffle, deflate, deflate_level)) )
    {
      Error("nc_def_var_deflate failed, status = %d", retval);
    }
#else
  static bool lwarn = true;
  if ( lwarn )
    {
      lwarn = false;
      Warning("Deflate compression failed, NetCDF4 not available!");
    }
#endif
}

nc_type cdfDefDatatype(int datatype, stream_t *streamptr)
{
  nc_type xtype = NC_FLOAT;

  if ( streamptr->filetype == CDI_FILETYPE_NC4 )
    {
      if      ( datatype == CDI_DATATYPE_INT8   ) xtype = NC_BYTE;
      else if ( datatype == CDI_DATATYPE_INT16  ) xtype = NC_SHORT;
      else if ( datatype == CDI_DATATYPE_INT32  ) xtype = NC_INT;
#ifdef  HAVE_NETCDF4
      else if ( datatype == CDI_DATATYPE_UINT8  ) xtype = NC_UBYTE;
      else if ( datatype == CDI_DATATYPE_UINT16 ) xtype = NC_USHORT;
      else if ( datatype == CDI_DATATYPE_UINT32 ) xtype = NC_UINT;
      else if ( datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64 )
        Error("CDI library does not support complex numbers with NetCDF4/HDF5!");
#else
      else if ( datatype == CDI_DATATYPE_UINT8  ) xtype = NC_SHORT;
      else if ( datatype == CDI_DATATYPE_UINT16 ) xtype = NC_INT;
      else if ( datatype == CDI_DATATYPE_UINT32 ) xtype = NC_INT;
      else if ( datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64 )
        Error("CDI library does not support complex numbers with NetCDF4 classic!");
#endif
      else if ( datatype == CDI_DATATYPE_FLT64  ) xtype = NC_DOUBLE;
      else if ( datatype == CDI_DATATYPE_FLT32  ) xtype = NC_FLOAT;
    }
  else
    {
      if      ( datatype == CDI_DATATYPE_INT8   ) xtype = NC_BYTE;
      else if ( datatype == CDI_DATATYPE_INT16  ) xtype = NC_SHORT;
      else if ( datatype == CDI_DATATYPE_INT32  ) xtype = NC_INT;
      else if ( datatype == CDI_DATATYPE_UINT8  ) xtype = NC_SHORT;
      else if ( datatype == CDI_DATATYPE_UINT16 ) xtype = NC_INT;
      else if ( datatype == CDI_DATATYPE_UINT32 ) xtype = NC_INT;
      else if ( datatype == CDI_DATATYPE_FLT64  ) xtype = NC_DOUBLE;
      else if ( datatype == CDI_DATATYPE_FLT32  ) xtype = NC_FLOAT;
      else if ( datatype == CDI_DATATYPE_CPX32 || datatype == CDI_DATATYPE_CPX64 )
        Error("CDI library does not support complex numbers with NetCDF classic!");
    }

  return xtype;
}

static
void cdfDefVarMissval(stream_t *streamptr, int varID, int dtype, int lcheck)
{
  if ( streamptr->vars[varID].defmiss == false )
    {
      const int vlistID = streamptr->vlistID;
      const int fileID  = streamptr->fileID;
      const int ncvarid = streamptr->vars[varID].ncvarid;
      const double missval = vlistInqVarMissval(vlistID, varID);

      if ( lcheck && streamptr->ncmode == 2 ) cdf_redef(fileID);

      nc_type xtype = cdfDefDatatype(dtype, streamptr);
      if ( xtype == NC_BYTE && missval > 127 && missval < 256 ) xtype = NC_INT;

      if ( lcheck == 0 ||
           streamptr->ncmode != 2 ||
           streamptr->filetype == CDI_FILETYPE_NC ||
           streamptr->filetype == CDI_FILETYPE_NC2||
           streamptr->filetype == CDI_FILETYPE_NC5 )
        cdf_put_att_double(fileID, ncvarid, "_FillValue", xtype, 1, &missval);

      cdf_put_att_double(fileID, ncvarid, "missing_value", xtype, 1, &missval);

      if ( lcheck && streamptr->ncmode == 2 ) cdf_enddef(fileID);

      streamptr->vars[varID].defmiss = true;
    }
}

static
void cdfDefInstitut(const stream_t *streamptr)
{
  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;
  const int instID  = vlistInqInstitut(vlistID);

  if ( instID != CDI_UNDEFID )
    {
      const char *longname = institutInqLongnamePtr(instID);
      if ( longname )
	{
	  const size_t len = strlen(longname);
	  if ( len > 0 )
	    {
	      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
	      cdf_put_att_text(fileID, NC_GLOBAL, "institution", len, longname);
	      if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
	    }
	}
    }
}

static
void cdfDefSource(const stream_t *streamptr)
{
  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;
  const int modelID = vlistInqModel(vlistID);

  if ( modelID != CDI_UNDEFID )
    {
      const char *longname = modelInqNamePtr(modelID);
      if ( longname )
	{
          size_t len = strlen(longname);
	  if ( len > 0 )
	    {
	      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);
	      cdf_put_att_text(fileID, NC_GLOBAL, "source", len, longname);
	      if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
	    }
	}
    }
}

static inline
void *resizeBuf(void **buf, size_t *bufSize, size_t reqSize)
{
  if ( reqSize > *bufSize )
    {
      *buf = Realloc(*buf, reqSize);
      *bufSize = reqSize;
    }
  return *buf;
}


void cdfDefineAttributes(int cdiID, int varID, int fileID, int ncvarID)
{
  int atttype, attlen;
  size_t len;
  char attname[CDI_MAX_NAME+1];
  void *attBuf = NULL;
  size_t attBufSize = 0;

  int natts;
  cdiInqNatts(cdiID, varID, &natts);

  for ( int iatt = 0; iatt < natts; ++iatt )
    {
      cdiInqAtt(cdiID, varID, iatt, attname, &atttype, &attlen);

      if ( attlen == 0 ) continue;

      if ( atttype == CDI_DATATYPE_TXT )
        {
          size_t attSize = (size_t)attlen*sizeof(char);
          char *atttxt = (char *)resizeBuf(&attBuf, &attBufSize, attSize);
          cdiInqAttTxt(cdiID, varID, attname, attlen, atttxt);
          len = (size_t)attlen;
          cdf_put_att_text(fileID, ncvarID, attname, len, atttxt);
        }
      else if ( atttype == CDI_DATATYPE_INT8  || atttype == CDI_DATATYPE_UINT8  ||
                atttype == CDI_DATATYPE_INT16 || atttype == CDI_DATATYPE_UINT16 ||
                atttype == CDI_DATATYPE_INT32 || atttype == CDI_DATATYPE_UINT32 )
        {
          size_t attSize = (size_t)attlen*sizeof(int);
          int *attint = (int *)resizeBuf(&attBuf, &attBufSize, attSize);
          cdiInqAttInt(cdiID, varID, attname, attlen, &attint[0]);
          len = (size_t)attlen;
          nc_type xtype = (atttype == CDI_DATATYPE_INT8)  ? NC_BYTE :
                          (atttype == CDI_DATATYPE_INT16) ? NC_SHORT :
#if  defined  (HAVE_NETCDF4)
                          (atttype == CDI_DATATYPE_UINT8)  ? NC_UBYTE :
                          (atttype == CDI_DATATYPE_UINT16) ? NC_USHORT :
                          (atttype == CDI_DATATYPE_UINT32) ? NC_UINT :
#endif
                          NC_INT;
          cdf_put_att_int(fileID, ncvarID, attname, xtype, len, attint);
        }
      else if ( atttype == CDI_DATATYPE_FLT32 || atttype == CDI_DATATYPE_FLT64 )
        {
          size_t attSize = (size_t)attlen * sizeof(double);
          double *attflt = (double *)resizeBuf(&attBuf, &attBufSize, attSize);
          cdiInqAttFlt(cdiID, varID, attname, attlen, attflt);
          len = (size_t)attlen;
          if ( atttype == CDI_DATATYPE_FLT32 )
            {
              float attflt_sp[8];
              float *pattflt_sp = (len > 8) ? (float*) malloc(len*sizeof(float)) : attflt_sp;
              for ( size_t i = 0; i < len; ++i ) pattflt_sp[i] = (float)attflt[i];
              cdf_put_att_float(fileID, ncvarID, attname, NC_FLOAT, len, pattflt_sp);
              if (len > 8) free(pattflt_sp);
            }
          else
            cdf_put_att_double(fileID, ncvarID, attname, NC_DOUBLE, len, attflt);
        }
    }

  Free(attBuf);
}

static
void cdfDefGlobalAtts(stream_t *streamptr)
{
  if ( streamptr->globalatts ) return;

  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;

  cdfDefSource(streamptr);
  cdfDefInstitut(streamptr);

  int natts;
  cdiInqNatts(vlistID, CDI_GLOBAL, &natts);

  if ( natts > 0 && streamptr->ncmode == 2 ) cdf_redef(fileID);

  cdfDefineAttributes(vlistID, CDI_GLOBAL, fileID, NC_GLOBAL);

  if ( natts > 0 && streamptr->ncmode == 2 ) cdf_enddef(fileID);

  streamptr->globalatts = 1;
}

static
void cdfDefLocalAtts(stream_t *streamptr)
{
  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;

  if ( streamptr->localatts ) return;
  if ( vlistInqInstitut(vlistID) != CDI_UNDEFID ) return;

  streamptr->localatts = 1;

  if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

  for ( int varID = 0; varID < streamptr->nvars; varID++ )
    {
      const int instID = vlistInqVarInstitut(vlistID, varID);
      if ( instID != CDI_UNDEFID )
	{
          const int ncvarid = streamptr->vars[varID].ncvarid;
  	  const char *name = institutInqNamePtr(instID);
	  if ( name )
	    {
              const size_t len = strlen(name);
	      cdf_put_att_text(fileID, ncvarid, "institution", len, name);
	    }
	}
      }

  if ( streamptr->ncmode == 2 ) cdf_enddef(fileID);
}

static
void cdf_get_gmapvarname(int gridID, char *gmapvarname)
{
  int pgridID = gridID;
  char mapping[CDI_MAX_NAME]; mapping[0] = 0;
  cdiGridInqKeyStr(pgridID, CDI_KEY_MAPNAME, CDI_MAX_NAME, mapping);

  if ( !mapping[0] )
    {
      int projID = gridInqProj(gridID);
      if ( projID != CDI_UNDEFID )
        {
          pgridID = projID;
          cdiGridInqKeyStr(pgridID, CDI_KEY_MAPNAME, CDI_MAX_NAME, mapping);
        }
    }

  if ( mapping[0] )
    cdiGridInqKeyStr(pgridID, CDI_KEY_MAPPING, CDI_MAX_NAME, gmapvarname);
}

static
int nc_grid_index(stream_t *streamptr, int gridID)
{
  int index = 0;
  int vlistID = streamptr->vlistID;
  int ngrids = vlistNgrids(vlistID);
  for ( index = 0; index < ngrids; ++index )
    if ( streamptr->ncgrid[index].gridID == gridID ) break;

  assert(index < ngrids);

  return index;
}

static
void cdfDefVarCompression(const stream_t *streamptr, int ncvarid, bool lchunk)
{
  if ( streamptr->comptype == CDI_COMPRESS_ZIP )
    {
      if ( lchunk && (streamptr->filetype == CDI_FILETYPE_NC4 || streamptr->filetype == CDI_FILETYPE_NC4C) )
        {
          cdfDefVarDeflate(streamptr->fileID, ncvarid, streamptr->complevel);
        }
      else
        {
          if ( lchunk )
            {
              static bool lwarn = true;
              if ( lwarn )
                {
                  lwarn = false;
                  Warning("Deflate compression is only available for NetCDF4!");
                }
            }
        }
    }
}

static
void cdfDefVarPacking(const stream_t *streamptr, int ncvarid, nc_type xtype, int vlistID, int varID)
{
  /*  if ( xtype == NC_BYTE || xtype == NC_SHORT || xtype == NC_INT ) */
    {
      const double addoffset   = vlistInqVarAddoffset(vlistID, varID);
      const double scalefactor = vlistInqVarScalefactor(vlistID, varID);
      const bool laddoffset   = IS_NOT_EQUAL(addoffset, 0);
      const bool lscalefactor = IS_NOT_EQUAL(scalefactor, 1);

      if ( laddoffset || lscalefactor )
        {
          nc_type astype = (xtype == NC_FLOAT) ? NC_FLOAT : NC_DOUBLE;
          if ( (astype == NC_DOUBLE) &&
               IS_EQUAL(addoffset,   (double) ((float) addoffset)) &&
               IS_EQUAL(scalefactor, (double) ((float) scalefactor)) )
            {
              astype = NC_FLOAT;
            }

          cdf_put_att_double(streamptr->fileID, ncvarid, "add_offset",   astype, 1, &addoffset);
          cdf_put_att_double(streamptr->fileID, ncvarid, "scale_factor", astype, 1, &scalefactor);
        }
    }
}

static
void cdfAppendCoordinates(int fileID, int ncvarid, char coordinates[CDI_MAX_NAME])
{
  if (ncvarid != CDI_UNDEFID)
    {
      const size_t len = strlen(coordinates);
      if (len) coordinates[len + 1] = ' ';
      cdf_inq_varname(fileID, ncvarid, coordinates+len);
    }
}

static
void cdfDefineCoordinates(const stream_t *streamptr, int ncvarid, int nczvarID, int gridtype, int gridID, int gridindex, int xid, int yid, size_t gridsize, char axis[5], size_t iax)
{
  const int fileID  = streamptr->fileID;

  char coordinates[CDI_MAX_NAME]; coordinates[0] = 0;

  if (nczvarID != CDI_UNDEFID) cdfAppendCoordinates(fileID, nczvarID, coordinates);

  if ( gridtype != GRID_GENERIC && gridtype != GRID_LONLAT && gridtype != GRID_GAUSSIAN &&
       gridtype != GRID_PROJECTION && gridtype != GRID_CURVILINEAR && gridtype != GRID_CHARXY )
    {
      const size_t len = strlen(gridNamePtr(gridtype));
      if ( len > 0 ) cdf_put_att_text(fileID, ncvarid, "CDI_grid_type", len, gridNamePtr(gridtype));
    }

  char gmapvarname[CDI_MAX_NAME]; gmapvarname[0] = 0;
  cdf_get_gmapvarname(gridID, gmapvarname);
  if ( gmapvarname[0] ) cdf_put_att_text(fileID, ncvarid, "grid_mapping", strlen(gmapvarname), gmapvarname);

  if ( gridtype == GRID_TRAJECTORY )
    {
      cdf_put_att_text(fileID, ncvarid, "coordinates", 9, "tlon tlat" );
    }
  else if ( gridtype == GRID_LONLAT && xid == CDI_UNDEFID && yid == CDI_UNDEFID && gridsize == 1 )
    {
      const int ncxvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_X];
      const int ncyvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_Y];
      cdfAppendCoordinates(fileID, ncyvarID, coordinates);
      cdfAppendCoordinates(fileID, ncxvarID, coordinates);
    }
  else if ( gridtype == GRID_GAUSSIAN_REDUCED )
    {
      const int ncxvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_X];
      const int ncyvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_Y];
      cdfAppendCoordinates(fileID, ncyvarID, coordinates);
      cdfAppendCoordinates(fileID, ncxvarID, coordinates);
    }
  else if ( gridtype == GRID_UNSTRUCTURED || gridtype == GRID_CURVILINEAR )
    {
      char cellarea[CDI_MAX_NAME] = "area: ";
      const int ncxvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_X];
      const int ncyvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_Y];
      const int ncavarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_A];
      // CMOR order: coordinates = "lat lon"
      if ( cdiCoordinatesLonLat )
        {
          cdfAppendCoordinates(fileID, ncxvarID, coordinates);
          cdfAppendCoordinates(fileID, ncyvarID, coordinates);
        }
      else
        {
          cdfAppendCoordinates(fileID, ncyvarID, coordinates);
          cdfAppendCoordinates(fileID, ncxvarID, coordinates);
        }

      if ( ncavarID != CDI_UNDEFID )
        {
          size_t len = strlen(cellarea);
          cdf_inq_varname(fileID, ncavarID, cellarea+len);
          len = strlen(cellarea);
          cdf_put_att_text(fileID, ncvarid, "cell_measures", len, cellarea);
        }

      if ( gridtype == GRID_UNSTRUCTURED )
        {
          int position = gridInqPosition(gridID);
          if ( position > 0 )
            cdf_put_att_int(fileID, ncvarid, "number_of_grid_in_reference", NC_INT, 1, &position);
        }
    }
  else if ( gridtype == GRID_SPECTRAL || gridtype == GRID_FOURIER )
    {
      axis[iax++] = '-';
      axis[iax++] = '-';
      cdf_put_att_text(fileID, ncvarid, "axis", iax, axis);
      int gridTruncation = gridInqTrunc(gridID);
      cdf_put_att_int(fileID, ncvarid, "truncation", NC_INT, 1, &gridTruncation);
    }
  else if ( gridtype == GRID_CHARXY )
    {
      if ( gridInqXIsc(gridID) )
        {
          const int ncxvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_X];
          cdfAppendCoordinates(fileID, ncxvarID, coordinates);
        }
      else if ( gridInqYIsc(gridID) )
        {
          const int ncyvarID = streamptr->ncgrid[gridindex].ncIDs[CDF_VARID_Y];
          cdfAppendCoordinates(fileID, ncyvarID, coordinates);
        }
    }

  size_t len = strlen(coordinates);
  if ( len ) cdf_put_att_text(fileID, ncvarid, "coordinates", len, coordinates);
}

static
int cdfDefineDimsAndChunks(const stream_t *streamptr, int varID, int xid, int yid, int zid, size_t gridsize, const int dimorder[3], int dims[4], bool lchunk, size_t chunks[4], char axis[5], size_t *piax)
{
  const int fileID  = streamptr->fileID;
  const int vlistID = streamptr->vlistID;

  size_t iax = *piax;
  int ndims = 0;

  for (int i = 0; i < 4; ++i) chunks[i] = 0;

  size_t xsize = 0, ysize = 0;
  if ( xid != CDI_UNDEFID ) cdf_inq_dimlen(fileID, xid, &xsize);
  if ( yid != CDI_UNDEFID ) cdf_inq_dimlen(fileID, yid, &ysize);

  const int timetype = vlistInqVarTimetype(vlistID, varID);
  if ( vlistHasTime(vlistID) && timetype != TIME_CONSTANT )
    {
      const int tid = streamptr->basetime.ncdimid;
      if (tid == CDI_UNDEFID) Error("Internal problem, time undefined!");
      axis[iax++] = 'T';
      chunks[ndims] = 1;
      dims[ndims] = tid;
      ndims++;
    }

  int chunktype = vlistInqVarChunkType(vlistID, varID);
  size_t chunk_size_max = 65536;
  if ( chunktype != CDI_CHUNK_LINES && gridsize > INT32_MAX )
    {
      if ( CDI_Debug ) fprintf(stderr, "gridsize > %d, changed chunktype to CDI_CHUNK_LINES!\n", INT32_MAX);
      chunktype = CDI_CHUNK_LINES;
    }

  for ( int id = 0; id < 3; ++id )
    {
      if ( dimorder[id] == 3 && zid != CDI_UNDEFID )
        {
          axis[iax++] = 'Z';
          chunks[ndims] = 1;
          dims[ndims] = zid;
          ndims++;
        }
      else if ( dimorder[id] == 2 && yid != CDI_UNDEFID )
        {
          if ( chunktype == CDI_CHUNK_AUTO )
            chunks[ndims] = (chunk_size_max > gridsize) ? ysize : chunk_size_max/xsize;
          else
            chunks[ndims] = (chunktype == CDI_CHUNK_LINES) ? 1 : ysize;
          dims[ndims] = yid;
          ndims++;
        }
      else if ( dimorder[id] == 1 && xid != CDI_UNDEFID )
        {
          if ( chunktype == CDI_CHUNK_AUTO && yid == CDI_UNDEFID )
            chunks[ndims] = (chunk_size_max > xsize) ? xsize : chunk_size_max;
          else
            chunks[ndims] = (xsize > INT32_MAX) ? INT32_MAX : xsize;
          dims[ndims] = xid;
          ndims++;
        }
    }

  if ( CDI_Debug )
    fprintf(stderr, "lchunk %d chunktype %d  chunks %zu %zu %zu %zu\n", lchunk, chunktype, chunks[0], chunks[1], chunks[2], chunks[3]);

  iax = *piax;
  return ndims;
}

static
void cdfDefineAttrLeveltype(int fileID, int ncvarid, int zaxisID, int zaxistype)
{
  if ( zaxistype == ZAXIS_CLOUD_BASE          ||
       zaxistype == ZAXIS_CLOUD_TOP           ||
       zaxistype == ZAXIS_ISOTHERM_ZERO       ||
       zaxistype == ZAXIS_TOA                 ||
       zaxistype == ZAXIS_SEA_BOTTOM          ||
       zaxistype == ZAXIS_LAKE_BOTTOM         ||
       zaxistype == ZAXIS_SEDIMENT_BOTTOM     ||
       zaxistype == ZAXIS_SEDIMENT_BOTTOM_TA  ||
       zaxistype == ZAXIS_SEDIMENT_BOTTOM_TW  ||
       zaxistype == ZAXIS_MIX_LAYER           ||
       zaxistype == ZAXIS_ATMOSPHERE )
    {
      char varname[CDI_MAX_NAME];
      zaxisInqName(zaxisID, varname);
      cdf_put_att_text(fileID, ncvarid, "level_type", strlen(varname), varname);
    }
}

static
void cdfDefineAttrEnsemble(int fileID, int ncvarid, int vlistID, int varID)
{
  int perturbationNumber, numberOfForecastsInEnsemble, typeOfEnsembleForecast;
  if ( cdiInqKeyInt(vlistID, varID, CDI_KEY_PERTURBATIONNUMBER, &perturbationNumber) == 0 )
    cdf_put_att_int(fileID, ncvarid, "realization", NC_INT, 1, &perturbationNumber);
  if ( cdiInqKeyInt(vlistID, varID, CDI_KEY_NUMBEROFFORECASTSINENSEMBLE, &numberOfForecastsInEnsemble) == 0 )
    cdf_put_att_int(fileID, ncvarid, "ensemble_members", NC_INT, 1, &numberOfForecastsInEnsemble);
  if ( cdiInqKeyInt(vlistID, varID, CDI_KEY_TYPEOFENSEMBLEFORECAST, &typeOfEnsembleForecast) == 0 )
    cdf_put_att_int(fileID, ncvarid, "forecast_init_type", NC_INT, 1, &typeOfEnsembleForecast);
}

static
void cdfCheckVarname(int fileID, char name[CDI_MAX_NAME])
{
  if ( name[0] )
    {
      int ncvarid;
      char varname[CDI_MAX_NAME];
      sprintf(varname, "%s", name);
      char *varname2 = varname+strlen(varname);
      unsigned iz = 0;

      do
        {
          if ( iz ) sprintf(varname2, "_%u", iz+1);

          if ( nc_inq_varid(fileID, varname, &ncvarid) != NC_NOERR ) break;

          ++iz;
        }
      while ( iz <= 99 );

      if (iz > 99) Error("Variable name %s already exsist!", name);

      if ( strcmp(name, varname) != 0 )
        Warning("Changed %s entry of variable name '%s' to '%s'!", (iz==1)?"double":"multiple", name, varname);

      strcpy(name, varname);
    }
}

static
void cdfGenVarname(int fileID, char name[CDI_MAX_NAME], int pnum, int pcat, int *pdis, int *pcode)
{
  char varname[CDI_MAX_NAME];

  int code = *pcode;
  if ( code < 0 ) code = -code;
  if ( pnum < 0 ) pnum = -pnum;

  if ( *pdis == 255 )
    sprintf(varname, "var%d", code);
  else
    sprintf(varname, "param%d.%d.%d", pnum, pcat, *pdis);

  char *varname2 = varname+strlen(varname);
  int ncvarid;
  unsigned iz = 0;

  do
    {
      if ( iz ) sprintf(varname2, "_%u", iz+1);

      if ( nc_inq_varid(fileID, varname, &ncvarid) != NC_NOERR ) break;

      ++iz;
    }
  while ( iz <= 99 );

  if (iz > 99) Error("Variable name %s already exsist!", name);

  strcpy(name, varname);
  *pcode = 0;
  *pdis = 255;
}

static
int cdfDefVar(stream_t *streamptr, int varID)
{
  if (streamptr->vars[varID].ncvarid != CDI_UNDEFID)
    return streamptr->vars[varID].ncvarid;

  const int fileID  = streamptr->fileID;
  if (CDI_Debug) Message("streamID = %d, fileID = %d, varID = %d", streamptr->self, fileID, varID);

  const int vlistID = streamptr->vlistID;
  const int param = vlistInqVarParam(vlistID, varID);
  int code = vlistInqVarCode(vlistID, varID);
  int pnum, pcat, pdis;
  cdiDecodeParam(param, &pnum, &pcat, &pdis);

  const int gridID = vlistInqVarGrid(vlistID, varID);
  const size_t gridsize = gridInqSize(gridID);
  const int gridtype = gridInqType(gridID);
  const int gridindex = nc_grid_index(streamptr, gridID);
  const int xid = (gridtype != GRID_TRAJECTORY) ? streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_X] : CDI_UNDEFID;
  const int yid = (gridtype != GRID_TRAJECTORY) ? streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_Y] : CDI_UNDEFID;

  const int zaxisID = vlistInqVarZaxis(vlistID, varID);
  const int zaxistype = zaxisInqType(zaxisID);
  const int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  const int zid = streamptr->zaxisID[zaxisindex];

  int dimorder[3]; // ZYX and ZXY
  vlistInqVarDimorder(vlistID, varID, &dimorder);
  const bool lchunk = (dimorder[0] == 3) ? (gridsize >= 16) : false;

  if ( ((dimorder[0]>0)+(dimorder[1]>0)+(dimorder[2]>0)) < ((xid!=CDI_UNDEFID)+(yid!=CDI_UNDEFID)+(zid!=CDI_UNDEFID)) )
    {
      printf("zid=%d  yid=%d  xid=%d\n", zid, yid, xid);
      Error("Internal problem, dimension order missing!");
    }

  size_t iax = 0;
  char axis[5];
  int dims[4];
  size_t chunks[4];
  int ndims = cdfDefineDimsAndChunks(streamptr, varID, xid, yid, zid, gridsize, dimorder, dims, lchunk, chunks, axis, &iax);

  char name[CDI_MAX_NAME]; name[0] = 0;
  if ( vlistInqVarNamePtr(vlistID, varID) ) vlistInqVarName(vlistID, varID, name);

  char longname[CDI_MAX_NAME]; longname[0] = 0;
  char stdname[CDI_MAX_NAME]; stdname[0] = 0;
  char units[CDI_MAX_NAME]; units[0] = 0;
  vlistInqVarLongname(vlistID, varID, longname);
  vlistInqVarStdname(vlistID, varID, stdname);
  vlistInqVarUnits(vlistID, varID, units);

  const int tableID  = vlistInqVarTable(vlistID, varID);
  if (!name[0]) tableInqEntry(tableID, code, -1, name, longname, units);
  if (name[0]) cdfCheckVarname(fileID, name);
  else         cdfGenVarname(fileID, name, pnum, pcat, &pdis, &code);

  const int dtype = vlistInqVarDatatype(vlistID, varID);
  const nc_type xtype = cdfDefDatatype(dtype, streamptr);

  int ncvarid = -1;
  cdf_def_var(fileID, name, xtype, ndims, dims, &ncvarid);

#if  defined  (HAVE_NETCDF4)
  if ( lchunk && (streamptr->filetype == CDI_FILETYPE_NC4 || streamptr->filetype == CDI_FILETYPE_NC4C) )
    cdf_def_var_chunking(fileID, ncvarid, NC_CHUNKED, chunks);
#endif

  cdfDefVarCompression(streamptr, ncvarid, lchunk);

  if ( *stdname )  cdf_put_att_text(fileID, ncvarid, "standard_name", strlen(stdname), stdname);
  if ( *longname ) cdf_put_att_text(fileID, ncvarid, "long_name", strlen(longname), longname);
  if ( *units )    cdf_put_att_text(fileID, ncvarid, "units", strlen(units), units);

  if ( code > 0 && pdis == 255 )
    cdf_put_att_int(fileID, ncvarid, "code", NC_INT, 1, &code);

  if ( pdis != 255 )
    {
      char paramstr[32];
      cdiParamToString(param, paramstr, sizeof(paramstr));
      cdf_put_att_text(fileID, ncvarid, "param", strlen(paramstr), paramstr);
    }

  if ( tableID != CDI_UNDEFID )
    {
      int tablenum = tableInqNum(tableID);
      if (tablenum > 0) cdf_put_att_int(fileID, ncvarid, "table", NC_INT, 1, &tablenum);
    }

  const bool zaxisIsScalar = (zid == CDI_UNDEFID) ? (zaxisInqScalar(zaxisID) > 0) : false;
  const int nczvarID = (zaxisIsScalar || zaxistype == ZAXIS_CHAR) ? streamptr->nczvarID[zaxisindex] : CDI_UNDEFID;

  cdfDefineCoordinates(streamptr, ncvarid, nczvarID, gridtype, gridID, gridindex, xid, yid, gridsize, axis, iax);

  cdfDefVarPacking(streamptr, ncvarid, xtype, vlistID, varID);

  if ( dtype == CDI_DATATYPE_UINT8 && xtype == NC_BYTE )
    {
      const int validrange[2] = {0, 255};
      cdf_put_att_int(fileID, ncvarid, "valid_range", NC_SHORT, 2, validrange);
      cdf_put_att_text(fileID, ncvarid, "_Unsigned", 4, "true");
    }

  streamptr->vars[varID].ncvarid = ncvarid;

  if ( vlistInqVarMissvalUsed(vlistID, varID) )
    cdfDefVarMissval(streamptr, varID, vlistInqVarDatatype(vlistID, varID), 0);

  if (zid == CDI_UNDEFID) cdfDefineAttrLeveltype(fileID, ncvarid, zaxisID, zaxistype);

  cdfDefineAttrEnsemble(fileID, ncvarid, vlistID, varID);

  // Attributes
  cdfDefineAttributes(vlistID, varID, fileID, ncvarid);

  return ncvarid;
}


void cdfEndDef(stream_t *streamptr)
{
  cdfDefGlobalAtts(streamptr);
  cdfDefLocalAtts(streamptr);

  if ( streamptr->accessmode == 0 )
    {
      const int fileID = streamptr->fileID;
      if ( streamptr->ncmode == 2 ) cdf_redef(fileID);

      const int nvars = streamptr->nvars;
      for ( int varID = 0; varID < nvars; varID++ )
	cdfDefVar(streamptr, varID);

      if ( streamptr->ncmode == 2 )
        {
          if ( CDI_netcdf_hdr_pad == 0UL )
            cdf_enddef(fileID);
          else
            cdf__enddef(fileID, CDI_netcdf_hdr_pad);
        }

      streamptr->accessmode = 1;
    }
}

static
void cdfWriteGridTraj(stream_t *streamptr, int gridID)
{
  const int gridindex = nc_grid_index(streamptr, gridID);
  const int lonID = streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_X];
  const int latID = streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_Y];
  const int tsID = streamptr->curTsID;
  size_t index = (size_t)tsID;

  double xlon = gridInqXval(gridID, 0);
  double xlat = gridInqYval(gridID, 0);

  cdf_put_var1_double(streamptr->fileID, lonID, &index, &xlon);
  cdf_put_var1_double(streamptr->fileID, latID, &index, &xlat);
}

static
void cdf_write_var_data(int fileID, int vlistID, int varID, int ncvarid, int dtype, size_t nvals, size_t xsize, size_t ysize,
                        bool swapxy, size_t *start, size_t *count, int memtype, const void *data, size_t nmiss)
{
  const double *pdata_dp = (const double *) data;
  double *mdata_dp = NULL;
  double *sdata_dp = NULL;
  const float *pdata_sp = (const float *) data;
  float *mdata_sp = NULL;
  float *sdata_sp = NULL;

  /*  if ( dtype == CDI_DATATYPE_INT8 || dtype == CDI_DATATYPE_INT16 || dtype == CDI_DATATYPE_INT32 ) */
    {
      const double missval      = vlistInqVarMissval(vlistID, varID);
      const double addoffset    = vlistInqVarAddoffset(vlistID, varID);
      const double scalefactor  = vlistInqVarScalefactor(vlistID, varID);
      const bool laddoffset     = IS_NOT_EQUAL(addoffset, 0);
      const bool lscalefactor   = IS_NOT_EQUAL(scalefactor, 1);

      if ( laddoffset || lscalefactor )
        {
          if ( memtype == MEMTYPE_FLOAT )
            {
              mdata_sp = (float *) Malloc(nvals*sizeof(float));
              memcpy(mdata_sp, pdata_sp, nvals*sizeof(float));
              pdata_sp = mdata_sp;

              if ( nmiss > 0 )
                {
                  for ( size_t i = 0; i < nvals; i++ )
                    {
                      double temp = mdata_sp[i];
                      if ( !DBL_IS_EQUAL(temp, missval) )
                        {
                          if ( laddoffset )   temp -= addoffset;
                          if ( lscalefactor ) temp /= scalefactor;
                          mdata_sp[i] = (float)temp;
                        }
                    }
                }
              else
                {
                  for ( size_t i = 0; i < nvals; i++ )
                    {
                      double temp = mdata_sp[i];
                      if ( laddoffset )   temp -= addoffset;
                      if ( lscalefactor ) temp /= scalefactor;
                      mdata_sp[i] = (float)temp;
                    }
                }
            }
          else
            {
              mdata_dp = (double *) Malloc(nvals*sizeof(double));
              memcpy(mdata_dp, pdata_dp, nvals*sizeof(double));
              pdata_dp = mdata_dp;

              if ( nmiss > 0 )
                {
                  for ( size_t i = 0; i < nvals; i++ )
                    {
                      if ( !DBL_IS_EQUAL(mdata_dp[i], missval) )
                        {
                          if ( laddoffset )   mdata_dp[i] -= addoffset;
                          if ( lscalefactor ) mdata_dp[i] /= scalefactor;
                        }
                    }
                }
              else
                {
                  for ( size_t i = 0; i < nvals; i++ )
                    {
                      if ( laddoffset )   mdata_dp[i] -= addoffset;
                      if ( lscalefactor ) mdata_dp[i] /= scalefactor;
                    }
                }
            }
        }

      if ( dtype == CDI_DATATYPE_UINT8 || dtype == CDI_DATATYPE_INT8 ||
           dtype == CDI_DATATYPE_INT16 || dtype == CDI_DATATYPE_INT32 )
        {
          if ( memtype == MEMTYPE_FLOAT )
            {
              if ( mdata_sp == NULL )
                {
                  mdata_sp = (float *) Malloc(nvals*sizeof(float));
                  memcpy(mdata_sp, pdata_sp, nvals*sizeof(float));
                  pdata_sp = mdata_sp;
                }

              for ( size_t i = 0; i < nvals; i++ ) mdata_sp[i] = roundf(mdata_sp[i]);

              if ( dtype == CDI_DATATYPE_UINT8 )
                {
                  nc_type xtype;
                  cdf_inq_vartype(fileID, ncvarid, &xtype);
                  if ( xtype == NC_BYTE )
                    {
                      for ( size_t i = 0; i < nvals; ++i )
                        if ( mdata_sp[i] > 127 ) mdata_sp[i] -= 256;
                    }
                }
            }
          else
            {
              if ( mdata_dp == NULL )
                {
                  mdata_dp = (double *) Malloc(nvals*sizeof(double));
                  memcpy(mdata_dp, pdata_dp, nvals*sizeof(double));
                  pdata_dp = mdata_dp;
                }

              for ( size_t i = 0; i < nvals; i++ ) mdata_dp[i] = round(mdata_dp[i]);

              if ( dtype == CDI_DATATYPE_UINT8 )
                {
                  nc_type xtype;
                  cdf_inq_vartype(fileID, ncvarid, &xtype);
                  if ( xtype == NC_BYTE )
                    {
                      for ( size_t i = 0; i < nvals; ++i )
                        if ( mdata_dp[i] > 127 ) mdata_dp[i] -= 256;
                    }
                }
            }
        }

      if ( CDF_Debug && memtype != MEMTYPE_FLOAT )
        {
          double fmin =  1.0e200;
          double fmax = -1.0e200;
          for ( size_t i = 0; i < nvals; ++i )
            {
              if ( !DBL_IS_EQUAL(pdata_dp[i], missval) )
                {
                  if ( pdata_dp[i] < fmin ) fmin = pdata_dp[i];
                  if ( pdata_dp[i] > fmax ) fmax = pdata_dp[i];
                }
            }
          Message("nvals = %zu, nmiss = %d, missval = %g, minval = %g, maxval = %g",
                  nvals, nmiss, missval, fmin, fmax);
        }
    }

  if ( swapxy ) // implemented only for cdf_write_var_slice()
    {
      size_t gridsize = xsize*ysize;
      if ( memtype == MEMTYPE_FLOAT )
        {
          sdata_sp = (float *) Malloc(gridsize*sizeof(float));
          for ( size_t j = 0; j < ysize; ++j )
            for ( size_t i = 0; i < xsize; ++i )
              sdata_sp[i*ysize+j] = pdata_sp[j*xsize+i];
          pdata_sp = sdata_sp;
        }
      else
        {
          sdata_dp = (double *) Malloc(gridsize*sizeof (double));
          for ( size_t j = 0; j < ysize; ++j )
            for ( size_t i = 0; i < xsize; ++i )
              sdata_dp[i*ysize+j] = pdata_dp[j*xsize+i];
          pdata_dp = sdata_dp;
        }
    }

  if ( memtype == MEMTYPE_FLOAT )
    cdf_put_vara_float(fileID, ncvarid, start, count, pdata_sp);
  else
    cdf_put_vara_double(fileID, ncvarid, start, count, pdata_dp);

  if ( mdata_dp ) Free(mdata_dp);
  if ( sdata_dp ) Free(sdata_dp);
  if ( mdata_sp ) Free(mdata_sp);
  if ( sdata_sp ) Free(sdata_sp);
}

static
void cdfGetXYZid(stream_t *streamptr, int gridID, int zaxisID, int *xid, int *yid, int *zid)
{
  *xid = CDI_UNDEFID;
  *yid = CDI_UNDEFID;

  if (gridInqType(gridID) == GRID_TRAJECTORY)
    {
      cdfWriteGridTraj(streamptr, gridID);
    }
  else
    {
      const int gridindex = nc_grid_index(streamptr, gridID);
      *xid = streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_X];
      *yid = streamptr->ncgrid[gridindex].ncIDs[CDF_DIMID_Y];
    }

  const int vlistID = streamptr->vlistID;
  const int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
  *zid = streamptr->zaxisID[zaxisindex];
}

static
void cdfDefineStartAndCount(stream_t *streamptr, int varID, int xid, int yid, int zid, size_t start[5], size_t count[5], size_t *xsize, size_t *ysize)
{
  size_t ndims = 0;
  *xsize = 0;
  *ysize = 0;

  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;

  const long ntsteps = streamptr->ntsteps;
  if ( CDI_Debug ) Message("ntsteps = %ld", ntsteps);

  const int timetype = vlistInqVarTimetype(vlistID, varID);

  if ( vlistHasTime(vlistID) && timetype != TIME_CONSTANT )
    {
      start[ndims] = (size_t)ntsteps - 1;
      count[ndims] = 1;
      ndims++;
    }

  if ( zid != CDI_UNDEFID )
    {
      const int zaxisID = vlistInqVarZaxis(vlistID, varID);
      start[ndims] = 0;
      count[ndims] = (size_t)zaxisInqSize(zaxisID);
      ndims++;
    }

  if ( yid != CDI_UNDEFID )
    {
      start[ndims] = 0;
      size_t size;
      cdf_inq_dimlen(fileID, yid, &size);
      /*      count[ndims] = gridInqYsize(gridID); */
      count[ndims] = size;
      ndims++;
    }

  if ( xid != CDI_UNDEFID )
    {
      start[ndims] = 0;
      size_t size;
      cdf_inq_dimlen(fileID, xid, &size);
      /*      count[ndims] = gridInqXsize(gridID); */
      count[ndims] = size;
      ndims++;
    }

  if ( CDI_Debug )
    for (size_t idim = 0; idim < ndims; idim++)
      Message("dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);
}

void cdf_write_var(stream_t *streamptr, int varID, int memtype, const void *data, size_t nmiss)
{
  if ( streamptr->accessmode == 0 ) cdfEndDef(streamptr);

  if ( CDI_Debug ) Message("streamID = %d  varID = %d", streamptr->self, varID);

  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;

  const int ncvarid = cdfDefVar(streamptr, varID);

  const int gridID   = vlistInqVarGrid(vlistID, varID);
  const int zaxisID  = vlistInqVarZaxis(vlistID, varID);

  int xid, yid, zid;
  cdfGetXYZid(streamptr, gridID, zaxisID, &xid, &yid, &zid);

  size_t xsize, ysize;
  size_t start[5], count[5];
  cdfDefineStartAndCount(streamptr, varID, xid, yid, zid, start, count, &xsize, &ysize);

  if ( streamptr->ncmode == 1 )
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  const int dtype = vlistInqVarDatatype(vlistID, varID);

  if ( nmiss > 0 ) cdfDefVarMissval(streamptr, varID, dtype, 1);

  const size_t nvals = gridInqSize(gridID) * (size_t)(zaxisInqSize(zaxisID));

  bool swapxy = false;
  cdf_write_var_data(fileID, vlistID, varID, ncvarid, dtype, nvals, xsize, ysize, swapxy, start, count, memtype, data, nmiss);
}

static
void cdfDefineStartAndCountChunk(stream_t *streamptr, const int rect[][2], int varID, int xid, int yid, int zid, size_t start[5], size_t count[5], size_t *xsize, size_t *ysize)
{
  size_t ndims = 0;
  *xsize = 0;
  *ysize = 0;

  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;

  const long ntsteps = streamptr->ntsteps;
  if ( CDI_Debug ) Message("ntsteps = %ld", ntsteps);

  const int timetype = vlistInqVarTimetype(vlistID, varID);

  if ( vlistHasTime(vlistID) && timetype != TIME_CONSTANT )
    {
      start[ndims] = (size_t)ntsteps - 1;
      count[ndims] = 1;
      ndims++;
    }

  if ( zid != CDI_UNDEFID )
    {
      const int zaxisID  = vlistInqVarZaxis(vlistID, varID);
      int size = zaxisInqSize(zaxisID);
      xassert(rect[2][0] >= 0 && rect[2][0] <= rect[2][1] && rect[2][1] <= size);
      start[ndims] = (size_t)rect[2][0];
      count[ndims] = (size_t)rect[2][1] - (size_t)rect[2][0] + 1;
      ndims++;
    }

  if ( yid != CDI_UNDEFID )
    {
      size_t size;
      cdf_inq_dimlen(fileID, yid, &size);
      xassert(rect[1][0] >= 0 && rect[1][0] <= rect[1][1] && (size_t)rect[1][1] <= size);
      start[ndims] = (size_t)rect[1][0];
      count[ndims] = (size_t)rect[1][1] - (size_t)rect[1][0] + 1;
      ndims++;
    }

  if ( xid != CDI_UNDEFID )
    {
      size_t size;
      cdf_inq_dimlen(fileID, xid, &size);
      xassert(rect[0][0] >= 0 && rect[0][0] <= rect[0][1] && (size_t)rect[0][1] <= size);
      start[ndims] = (size_t)rect[0][0];
      count[ndims] = (size_t)rect[0][1] - (size_t)rect[0][0] + 1;
      ndims++;
    }

  if ( CDI_Debug )
    for (size_t idim = 0; idim < ndims; idim++)
      Message("dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);
}

void cdf_write_var_chunk(stream_t *streamptr, int varID, int memtype,
                         const int rect[][2], const void *data, size_t nmiss)
{
  if ( streamptr->accessmode == 0 ) cdfEndDef(streamptr);

  const int streamID = streamptr->self;

  if ( CDI_Debug )
    Message("streamID = %d  varID = %d", streamID, varID);

  const int vlistID = streamInqVlist(streamID);
  const int fileID  = streamInqFileID(streamID);

  const int ncvarid = cdfDefVar(streamptr, varID);

  const int gridID   = vlistInqVarGrid(vlistID, varID);
  const int zaxisID  = vlistInqVarZaxis(vlistID, varID);

  int xid, yid, zid;
  cdfGetXYZid(streamptr, gridID, zaxisID, &xid, &yid, &zid);

  size_t xsize, ysize;
  size_t start[5], count[5];
  cdfDefineStartAndCountChunk(streamptr, rect, varID, xid, yid, zid, start, count, &xsize, &ysize);

  if ( streamptr->ncmode == 1 )
    {
      cdf_enddef(fileID);
      streamptr->ncmode = 2;
    }

  const int dtype = vlistInqVarDatatype(vlistID, varID);

  if ( nmiss > 0 ) cdfDefVarMissval(streamptr, varID, dtype, 1);

  const size_t nvals = gridInqSize(gridID) * (size_t)(zaxisInqSize(zaxisID));

  bool swapxy = false;
  cdf_write_var_data(fileID, vlistID, varID, ncvarid, dtype, nvals,
                     xsize, ysize, swapxy, start, count, memtype, data, nmiss);
}

static
void cdfDefineStartAndCountSlice(stream_t *streamptr, int varID, int levelID, int dimorder[3], int xid, int yid, int zid, size_t start[5], size_t count[5], size_t *xsize, size_t *ysize)
{
  size_t ndims = 0;
  *xsize = 0;
  *ysize = 0;

  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;

  const long ntsteps = streamptr->ntsteps;
  if ( CDI_Debug ) Message("ntsteps = %ld", ntsteps);

  const int timetype = vlistInqVarTimetype(vlistID, varID);

  if ( vlistHasTime(vlistID) && timetype != TIME_CONSTANT )
    {
      start[ndims] = (size_t)ntsteps - 1;
      count[ndims] = 1;
      ndims++;
    }

  for ( int id = 0; id < 3; ++id )
    {
      if ( dimorder[id] == 3 && zid != CDI_UNDEFID )
        {
          start[ndims] = (size_t)levelID;
          count[ndims] = 1;
          ndims++;
        }
      else if ( dimorder[id] == 2 && yid != CDI_UNDEFID )
        {
          start[ndims] = 0;
          cdf_inq_dimlen(fileID, yid, ysize);
          count[ndims] = *ysize;
          ndims++;
        }
      else if ( dimorder[id] == 1 && xid != CDI_UNDEFID )
        {
          start[ndims] = 0;
          cdf_inq_dimlen(fileID, xid, xsize);
          count[ndims] = *xsize;
          ndims++;
        }
    }

  if ( CDI_Debug )
    for (size_t idim = 0; idim < ndims; idim++)
      Message("dim = %d  start = %d  count = %d", idim, start[idim], count[idim]);
}

void cdf_write_var_slice(stream_t *streamptr, int varID, int levelID, int memtype, const void *data, size_t nmiss)
{
  if ( streamptr->accessmode == 0 ) cdfEndDef(streamptr);

  if ( CDI_Debug ) Message("streamID = %d  varID = %d", streamptr->self, varID);

  const int vlistID = streamptr->vlistID;
  const int fileID  = streamptr->fileID;

  const int ncvarid = cdfDefVar(streamptr, varID);

  const int gridID   = vlistInqVarGrid(vlistID, varID);
  const int zaxisID  = vlistInqVarZaxis(vlistID, varID);

  int xid, yid, zid;
  cdfGetXYZid(streamptr, gridID, zaxisID, &xid, &yid, &zid);

  int dimorder[3];
  vlistInqVarDimorder(vlistID, varID, &dimorder);
  const bool swapxy = (dimorder[2] == 2 || dimorder[0] == 1) && xid != CDI_UNDEFID && yid != CDI_UNDEFID;

  size_t xsize, ysize;
  size_t start[5], count[5];
  cdfDefineStartAndCountSlice(streamptr, varID, levelID, dimorder, xid, yid, zid, start, count, &xsize, &ysize);

  const int dtype = vlistInqVarDatatype(vlistID, varID);

  if ( nmiss > 0 ) cdfDefVarMissval(streamptr, varID, dtype, 1);

  const size_t nvals = gridInqSize(gridID);

  cdf_write_var_data(fileID, vlistID, varID, ncvarid, dtype, nvals, xsize, ysize, swapxy, start, count, memtype, data, nmiss);
}


void cdf_write_record(stream_t *streamptr, int memtype, const void *data, size_t nmiss)
{
  const int varID   = streamptr->record->varID;
  const int levelID = streamptr->record->levelID;
  cdf_write_var_slice(streamptr, varID, levelID, memtype, data, nmiss);
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
