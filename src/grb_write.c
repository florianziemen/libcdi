#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBGRIB

#ifdef HAVE_LIBFDB5
#include <fdb5/api/fdb_c.h>
#endif

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "gribapi.h"
#include "stream_cgribex.h"
#include "stream_grb.h"
#include "stream_gribapi.h"
#include "file.h"
#include "cgribex.h"  /* gribZip gribGetZip gribGinfo */
#include "namespace.h"


static
size_t grbEncode(int filetype, int memtype, int varID, int levelID, int vlistID, int gridID, int zaxisID,
		 int date, int time, int tsteptype, int numavg,
		 size_t datasize, const void *data, size_t nmiss, void **gribbuffer,
		 int comptype, void *gribContainer)
{
  size_t nbytes = 0;

#ifdef HAVE_LIBCGRIBEX
  if ( filetype == CDI_FILETYPE_GRB && !CDI_gribapi_grib1 )
    {
      size_t gribbuffersize = datasize*4+3000;
      *gribbuffer = Malloc(gribbuffersize);

      nbytes = cgribexEncode(memtype, varID, levelID, vlistID, gridID, zaxisID,
			     date, time, tsteptype, numavg,
			     datasize, data, nmiss, *gribbuffer, gribbuffersize);
    }
  else
#endif
#ifdef HAVE_LIBGRIB_API
    {
      const void *datap = data;
      if ( memtype == MEMTYPE_FLOAT )
        {
          const float *dataf = (const float*) data;
          double *datad = (double*) Malloc(datasize*sizeof(double));
          for ( size_t i = 0; i < datasize; ++i ) datad[i] = (double) dataf[i];
          datap = (const void*) datad;
        }

      size_t gribbuffersize;
      nbytes = gribapiEncode(varID, levelID, vlistID, gridID, zaxisID,
			     date, time, tsteptype, numavg,
			     (long) datasize, datap, nmiss, gribbuffer, &gribbuffersize,
			     comptype, gribContainer);
      
      if ( memtype == MEMTYPE_FLOAT ) Free((void*)datap);
    }
#else
    {
      Error("ecCodes support not compiled in!");
      (void)gribContainer;
      (void)comptype;
    }
#endif

  return nbytes;
}

static
size_t grbSzip(int filetype, void *gribbuffer, size_t gribbuffersize)
{
  size_t buffersize = gribbuffersize + 1000; /* compressed record can be greater than source record */
  void *buffer = Malloc(buffersize);

  /*  memcpy(buffer, gribbuffer, gribbuffersize); */

  size_t nbytes = 0;
  if ( filetype == CDI_FILETYPE_GRB )
    {
      nbytes = (size_t)gribZip((unsigned char *)gribbuffer, (long) gribbuffersize, (unsigned char *)buffer, (long) buffersize);
    }
  else
    {
      static int lszip_warn = 1;
      if ( lszip_warn ) Warning("Szip compression of GRIB2 records not implemented!");
      lszip_warn = 0;
      nbytes = gribbuffersize;
    }

  Free(buffer);

  return nbytes;
}


void cdi_fdb_store(void *gribbuffer, size_t nbytes)
{
#ifdef HAVE_LIBFDB5
  Message("cdi_fdb_store()");
  fdb_handle_t *fdb;
  fdb_new_handle(&fdb);

  fdb_key_t *key;
  fdb_new_key(&key);
  fdb_key_add(key, "class", "ea");
  fdb_key_add(key, "expver", "0001");
  fdb_key_add(key, "stream", "oper");
  fdb_key_add(key, "date", "20180603");
  fdb_key_add(key, "time", "0000");
  fdb_key_add(key, "type", "an");
  fdb_key_add(key, "levtype", "ml");
  fdb_key_add(key, "step", "0");
  fdb_key_add(key, "param", "8");
  fdb_key_add(key, "domain", "g");
  fdb_key_add(key, "levelist", "1");

  // Message("fdb_archive()");
  // fdb_archive(fdb, key, gribbuffer, nbytes);

  fdb_delete_key(key);
  fdb_delete_handle(fdb);
#endif
}


void grbCopyRecord(stream_t * streamptr2, stream_t * streamptr1)
{
  const int filetype = streamptr1->filetype;
  const int fileID1 = streamptr1->fileID;
  const int fileID2 = streamptr2->fileID;
  const int tsID    = streamptr1->curTsID;
  const int vrecID  = streamptr1->tsteps[tsID].curRecID;
  const int recID   = streamptr1->tsteps[tsID].recIDs[vrecID];
  const off_t recpos  = streamptr1->tsteps[tsID].records[recID].position;
  const size_t recsize = streamptr1->tsteps[tsID].records[recID].size;

  fileSetPos(fileID1, recpos, SEEK_SET);

  // round up recsize to next multiple of 8
  const size_t gribbuffersize = ((recsize + 7U) & ~7U);

  unsigned char *gribbuffer = (unsigned char *) Malloc(gribbuffersize);

  if (fileRead(fileID1, gribbuffer, recsize) != recsize)
    Error("Could not read GRIB record for copying!");

  size_t nbytes = recsize;

#ifdef HAVE_LIBCGRIBEX
  if ( filetype == CDI_FILETYPE_GRB && !CDI_gribapi_grib1 )
    {
      if ( cdiGribChangeParameterID.active )
        {
          // Even if you are stream-copy records you might need to change a bit of grib-header !
          void *gh = cgribex_handle_new_from_meassage((void*) gribbuffer, recsize);
          cgribexChangeParameterIdentification(gh, cdiGribChangeParameterID.code, cdiGribChangeParameterID.ltype, cdiGribChangeParameterID.lev);
          cgribex_handle_delete(gh);
          cdiGribChangeParameterID.active = false; // after grib attributes have been changed turn it off again
        }
    }
  else
#endif
#ifdef HAVE_LIBGRIB_API
    {
      if ( cdiGribChangeParameterID.active )
        {
          // Even if you are stream-copy records you might need to change a bit of grib-header !
          grib_handle *gh = grib_handle_new_from_message(NULL, (void*) gribbuffer, recsize);
          gribapiChangeParameterIdentification(gh, cdiGribChangeParameterID.code, cdiGribChangeParameterID.ltype, cdiGribChangeParameterID.lev);
          grib_handle_delete(gh);
          cdiGribChangeParameterID.active = false; // after grib attributes have been changed turn it off again
        }
    }
#else
    {
      Error("ecCodes support not compiled in!");
    }
#endif

#ifdef HAVE_LIBGRIB_API
#ifdef HIRLAM_EXTENSIONS
  // Even if you are stream-copy records you might need to change a bit of grib-header !

  if ( cdiGribDataScanningMode.active )
    // allowed modes: <0, 64, 96>; Default is 64
    // This will overrule the old scanning mode of the given grid
    {
      grib_handle *gh = NULL;
      gh = grib_handle_new_from_message(NULL, (void *) gribbuffer, recsize);

      const int scanModeIN = gribapiGetScanningMode(gh);

      grib_handle_delete(gh);

      if (cdiDebugExt>=20) Message("Change GribDataScanningMode => %d (scanModeIN = %d)", cdiGribDataScanningMode.value, scanModeIN);

      if (scanModeIN != cdiGribDataScanningMode.value)
        {
          size_t nmiss = 0;

          const int vlistID = streamptr1->vlistID;
          const int varID   = streamptr1->tsteps[tsID].records[recID].varID;
          const int levelID = streamptr1->tsteps[tsID].records[recID].levelID;
          const int gridID  = vlistInqVarGrid(vlistID, varID);

          size_t gridsize = gridInqSize(gridID);
          if ( vlistNumber(vlistID) != CDI_REAL ) gridsize *= 2;
          double *data = (double *) malloc(gridsize*sizeof(double));

          if (cdiDebugExt>=20) Message(" processing varID %d; levelID %d",varID,levelID);

          grb_write_var_slice(streamptr2, varID, levelID, MEMTYPE_DOUBLE, (const void *) data, nmiss);

          free(data);
        }
    }
#endif // HIRLAM_EXTENSIONS
#endif

  if (filetype == CDI_FILETYPE_GRB)
    {
      size_t unzipsize;
      const int izip = gribGetZip(recsize, gribbuffer, &unzipsize);

      if (izip == 0 && (streamptr2->comptype == CDI_COMPRESS_SZIP || streamptr2->comptype == CDI_COMPRESS_AEC))
        nbytes = grbSzip(filetype, gribbuffer, nbytes);
    }

  while (nbytes & 7) gribbuffer[nbytes++] = 0;

  if (streamptr2->fdbStore)
    {
      cdi_fdb_store(gribbuffer, nbytes);
    }
  else
    {
      const size_t nwrite = fileWrite(fileID2, gribbuffer, nbytes);
      if (nwrite != nbytes)
        {
          perror(__func__);
          Error("Could not write record for copying!");
        }
    }

  Free(gribbuffer);
}


void grb_write_var_slice(stream_t *streamptr, int varID, int levelID, int memtype, const void *data, size_t nmiss)
{
  void *gribbuffer = NULL;
  void *gc = NULL;

  int filetype  = streamptr->filetype;
  int fileID    = streamptr->fileID;
  int vlistID   = streamptr->vlistID;
  int gridID    = vlistInqVarGrid(vlistID, varID);
  int zaxisID   = vlistInqVarZaxis(vlistID, varID);
  int tsteptype = vlistInqVarTsteptype(vlistID, varID);
  int comptype  = streamptr->comptype;
  int tsID      = streamptr->curTsID;
  int date      = streamptr->tsteps[tsID].taxis.vdate;
  int time      = streamptr->tsteps[tsID].taxis.vtime;
  int numavg    = (tsteptype == TSTEP_AVG) ? streamptr->tsteps[tsID].taxis.numavg : 0;

  if (CDI_Debug) Message("gridID = %d zaxisID = %d", gridID, zaxisID);

  size_t datasize = gridInqSize(gridID);

#ifdef HAVE_LIBCGRIBEX
  if ( filetype == CDI_FILETYPE_GRB && !CDI_gribapi_grib1 )
    {
    }
  else
#endif
    {
#ifdef GRIBCONTAINER2D
      gribContainer_t **gribContainers =  (gribContainer_t **) streamptr->gribContainers;
      gc = (void *) &gribContainers[varID][levelID];
#else
      gribContainer_t *gribContainers =  (gribContainer_t *) streamptr->gribContainers;
      gc = (void *) &gribContainers[varID];
#endif
    }

  if ( comptype != CDI_COMPRESS_JPEG && comptype != CDI_COMPRESS_SZIP && comptype != CDI_COMPRESS_AEC ) comptype = CDI_COMPRESS_NONE;

  if ( filetype == CDI_FILETYPE_GRB && !CDI_gribapi_grib1 && comptype == CDI_COMPRESS_JPEG )
    {
      static int ljpeg_warn = 1;
      if ( ljpeg_warn ) Warning("JPEG compression of GRIB1 records not available!");
      ljpeg_warn = 0;
    }

  size_t nbytes = grbEncode(filetype, memtype, varID, levelID, vlistID, gridID, zaxisID, date, time, tsteptype, numavg,
                            datasize, data, nmiss, &gribbuffer, comptype, gc);

  if ( filetype == CDI_FILETYPE_GRB && (streamptr->comptype == CDI_COMPRESS_SZIP || streamptr->comptype == CDI_COMPRESS_AEC) )
    nbytes = grbSzip(filetype, gribbuffer, nbytes);

  size_t (*myFileWrite)(int fileID, const void *restrict buffer, size_t len)
    = (size_t (*)(int, const void *restrict, size_t))
    namespaceSwitchGet(NSSWITCH_FILE_WRITE).func;

  if (streamptr->fdbStore)
    {
      cdi_fdb_store(gribbuffer, nbytes);
    }
  else
    {
      const size_t nwrite = myFileWrite(fileID, gribbuffer, nbytes);
      if (nwrite != nbytes)
        {
          perror(__func__);
          Error("Failed to write GRIB slice!");
        }
    }

  if (gribbuffer) Free(gribbuffer);
}


void grb_write_var(stream_t *streamptr, int varID, int memtype, const void *data, size_t nmiss)
{
  int vlistID  = streamptr->vlistID,
    gridID   = vlistInqVarGrid(vlistID, varID),
    zaxisID  = vlistInqVarZaxis(vlistID, varID),
    nlevs    = zaxisInqSize(zaxisID);
  size_t gridsize = gridInqSize(gridID);
  double missval = vlistInqVarMissval(vlistID, varID);

  size_t chunkLen = gridsize;
  if ( memtype == MEMTYPE_FLOAT )
    for ( int levelID = 0; levelID < nlevs; levelID++ )
      {
        const float *restrict fdata = ((const float *)data)+levelID*gridsize;
        
        size_t nmiss_slice = 0;
        if ( nmiss )
          for ( size_t i = 0; i < chunkLen; ++i )
            nmiss_slice += DBL_IS_EQUAL(fdata[i], missval);

        grb_write_var_slice(streamptr, varID, levelID, memtype, fdata, nmiss_slice);
      }
  else
    for ( int levelID = 0; levelID < nlevs; levelID++ )
      {
        const double *restrict ddata = ((const double *)data)+levelID*gridsize;
        
        size_t nmiss_slice = 0;
        if ( nmiss )
          for ( size_t i = 0; i < chunkLen; ++i )
            nmiss_slice += DBL_IS_EQUAL(ddata[i], missval);

        grb_write_var_slice(streamptr, varID, levelID, memtype, ddata, nmiss_slice);
      }
}


void grb_write_record(stream_t *streamptr, int memtype, const void *data, size_t nmiss)
{
  int varID   = streamptr->record->varID;
  int levelID = streamptr->record->levelID;

  grb_write_var_slice(streamptr, varID, levelID, memtype, data, nmiss);
}


#endif
