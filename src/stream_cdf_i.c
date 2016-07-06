#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

//#define TEST_GROUPS 1

#include <ctype.h>

#include "dmemory.h"
#include "gaussgrid.h"
#include "cdi_int.h"
#include "cdi_uuid.h"
#include "stream_cdf.h"
#include "cdf_int.h"
#include "varscan.h"
#include "vlist.h"
#include "cdf_util.h"
#include "cdf_lazy_grid.h"

//#define PROJECTION_TEST

#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID

#define  X_AXIS  1
#define  Y_AXIS  2
#define  Z_AXIS  3
#define  T_AXIS  4

#define  POSITIVE_UP    1
#define  POSITIVE_DOWN  2

typedef struct {
  int     ncvarid;
  int     dimtype;
  size_t  len;
  char    name[CDI_MAX_NAME];
}
ncdim_t;
#define  MAX_COORDVARS  4
#define  MAX_AUXVARS    4

typedef struct {
  int      ncid;
  int      isvar;
  bool     ignore;
  bool     islon;
  bool     islat;
  bool     islev;
  bool     istime;
  bool     warn;
  bool     climatology;
  int      tsteptype;
  int      param;
  int      code;
  int      tabnum;
  int      bounds;
  int      lformula;
  int      lformulaterms;
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
  int      psvarid;
  int      p0varid;
  int      ncoordvars;
  int      coordvarids[MAX_COORDVARS];
  int      nauxvars;
  int      auxvarids[MAX_AUXVARS];
  int      cellarea;
  int      calendar;
  int      tableID;
  int      truncation;
  int      position;
  bool     defmissval;
  bool     deffillval;
  int      xtype;
  int      ndims;
  int      gmapid;
  int      positive;
  int      dimids[8];
  int      dimtype[8];
  int      chunks[8];
  int      chunked;
  int      chunktype;
  int      natts;
  int      deflate;
  bool     lunsigned;
  bool     lvalidrange;
  int     *atts;
  size_t   vctsize;
  double  *vct;
  double   missval;
  double   fillval;
  double   addoffset;
  double   scalefactor;
  double   validrange[2];
  char     name[CDI_MAX_NAME];
  char     longname[CDI_MAX_NAME];
  char     stdname[CDI_MAX_NAME];
  char     units[CDI_MAX_NAME];
  char     extra[CDI_MAX_NAME];
  ensinfo_t   *ensdata;    /* Ensemble information */
}
ncvar_t;


static
void scanTimeString(const char *ptu, int *rdate, int *rtime)
{
  int year = 1, month = 1, day = 1;
  int hour = 0, minute = 0, second = 0;
  int v1 = 1, v2 = 1, v3 = 1;

  *rdate = 0;
  *rtime = 0;

  if ( *ptu )
    {
      v1 = atoi(ptu);
      if ( v1 < 0 ) ptu++;
      while ( isdigit((int) *ptu) ) ptu++;
      if ( *ptu )
        {
          v2 = atoi(++ptu);
          while ( isdigit((int) *ptu) ) ptu++;
          if ( *ptu )
            {
              v3 = atoi(++ptu);
              while ( isdigit((int) *ptu) ) ptu++;
            }
        }
    }

  if ( v3 > 999 && v1 < 32 )
    { year = v3; month = v2; day = v1; }
  else
    { year = v1; month = v2; day = v3; }

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
            }
        }
    }

  *rdate = cdiEncodeDate(year, month, day);
  *rtime = cdiEncodeTime(hour, minute, second);
}

static
int scanTimeUnit(const char *unitstr)
{
  size_t len = strlen(unitstr);
  int timeunit = get_timeunit(len, unitstr);
  if ( timeunit == -1 )
    Message("Unsupported TIMEUNIT: %s!", unitstr);

  return timeunit;
}

static
void setForecastTime(const char *timestr, taxis_t *taxis)
{
  (*taxis).fdate = 0;
  (*taxis).ftime = 0;

  int len = (int) strlen(timestr);
  if ( len == 0 ) return;

  int fdate = 0, ftime = 0;
  scanTimeString(timestr, &fdate, &ftime);

  (*taxis).fdate = fdate;
  (*taxis).ftime = ftime;
}

static
int setBaseTime(const char *timeunits, taxis_t *taxis)
{
  int timetype = TAXIS_ABSOLUTE;
  int rdate = -1, rtime = -1;

  size_t len = strlen(timeunits);
  char *tu = (char *) Malloc((len+1) * sizeof (char));
  memcpy(tu, timeunits, (len+1) * sizeof (char));
  char *ptu = tu;

  for ( size_t i = 0; i < len; i++ ) ptu[i] = (char)tolower((int) ptu[i]);

  int timeunit = get_timeunit(len, ptu);
  if ( timeunit == -1 )
    {
      Message("Unsupported TIMEUNIT: %s!", timeunits);
      return 1;
    }

  while ( ! isspace(*ptu) && *ptu != 0 ) ptu++;
  if ( *ptu )
    {
      while ( isspace(*ptu) ) ptu++;

      if ( memcmp(ptu, "as", 2) == 0 )
        timetype = TAXIS_ABSOLUTE;
      else if ( memcmp(ptu, "since", 5) == 0 )
        timetype = TAXIS_RELATIVE;

      while ( ! isspace(*ptu) && *ptu != 0 ) ptu++;
      if ( *ptu )
        {
          while ( isspace(*ptu) ) ptu++;

          if ( timetype == TAXIS_ABSOLUTE )
            {
              if ( memcmp(ptu, "%y%m%d.%f", 9) != 0 && timeunit == TUNIT_DAY )
                {
                  Message("Unsupported format %s for TIMEUNIT day!", ptu);
                  timeunit = -1;
                }
              else if ( memcmp(ptu, "%y%m.%f", 7) != 0 && timeunit == TUNIT_MONTH )
                {
                  Message("Unsupported format %s for TIMEUNIT month!", ptu);
                  timeunit = -1;
                }
            }
          else if ( timetype == TAXIS_RELATIVE )
            {
              scanTimeString(ptu, &rdate, &rtime);

              (*taxis).rdate = rdate;
              (*taxis).rtime = rtime;

              if ( CDI_Debug )
                Message("rdate = %d  rtime = %d", rdate, rtime);
            }
        }
    }

  (*taxis).type = timetype;
  (*taxis).unit = timeunit;

  Free(tu);

  if ( CDI_Debug )
    Message("timetype = %d  unit = %d", timetype, timeunit);

  return 0;
}

static
void cdfGetAttInt(int fileID, int ncvarid, const char *attname, int attlen, int *attint)
{
  nc_type atttype;
  size_t nc_attlen;

  *attint = 0;

  cdf_inq_atttype(fileID, ncvarid, attname, &atttype);
  cdf_inq_attlen(fileID, ncvarid, attname, &nc_attlen);

  if ( atttype != NC_CHAR )
    {
      int *pintatt = (int)nc_attlen > attlen
        ? (int *)(Malloc(nc_attlen * sizeof (int))) : attint;

      cdf_get_att_int(fileID, ncvarid, attname, pintatt);

      if ( (int)nc_attlen > attlen )
        {
          memcpy(attint, pintatt, (size_t)attlen * sizeof (int));
          Free(pintatt);
        }
    }
}

static
void cdfGetAttDouble(int fileID, int ncvarid, char *attname, int attlen, double *attdouble)
{
  nc_type atttype;
  size_t nc_attlen;

  *attdouble = 0;

  cdf_inq_atttype(fileID, ncvarid, attname, &atttype);
  cdf_inq_attlen(fileID, ncvarid, attname, &nc_attlen);

  if ( atttype != NC_CHAR )
    {
      double *pdoubleatt = NULL;

      if ( (int)nc_attlen > attlen )
        pdoubleatt = (double *) Malloc(nc_attlen * sizeof (double));
      else
        pdoubleatt = attdouble;

      cdf_get_att_double(fileID, ncvarid, attname, pdoubleatt);

      if ( (int)nc_attlen > attlen )
        {
          memcpy(attdouble, pdoubleatt, (size_t)attlen * sizeof (double));
          Free(pdoubleatt);
        }
    }
}

static
void cdfGetAttText(int fileID, int ncvarid,const char *attname, int attlen, char *atttext)
{
  nc_type atttype;
  size_t nc_attlen;

  cdf_inq_atttype(fileID, ncvarid, attname, &atttype);
  cdf_inq_attlen(fileID, ncvarid, attname, &nc_attlen);

  if ( atttype == NC_CHAR )
    {
      char attbuf[65636];
      if ( nc_attlen < sizeof(attbuf) )
        {
          cdf_get_att_text(fileID, ncvarid, attname, attbuf);

          if ( (int) nc_attlen > (attlen-1) ) nc_attlen = (size_t)(attlen-1);

          attbuf[nc_attlen++] = 0;
          memcpy(atttext, attbuf, nc_attlen);
        }
      else
        {
          atttext[0] = 0;
        }
    }
#if  defined  (HAVE_NETCDF4)
  else if ( atttype == NC_STRING )
    {
      if ( nc_attlen == 1 )
        {
          char *attbuf = NULL;
          cdf_get_att_string(fileID, ncvarid, attname, &attbuf);

          size_t ssize = strlen(attbuf) + 1;

          if ( ssize > (size_t)attlen ) ssize = (size_t)attlen;
          memcpy(atttext, attbuf, ssize);
          atttext[ssize - 1] = 0;
          Free(attbuf);
        }
      else
        {
          atttext[0] = 0;
        }
    }
#endif
}

static
bool xtypeIsText(nc_type xtype)
{
  bool isText = false;

  if      ( xtype == NC_CHAR ) isText = true;
#if  defined  (HAVE_NETCDF4)
  else if ( xtype == NC_STRING ) isText = true;
#endif

  return isText;
}

static
bool xtypeIsFloat(nc_type xtype)
{
  bool isFloat = xtype == NC_FLOAT || xtype == NC_DOUBLE;

  return isFloat;
}

static
bool xtypeIsInt(nc_type xtype)
{
  bool isInt = xtype == NC_SHORT || xtype == NC_INT
            || xtype == NC_BYTE
#if  defined  (HAVE_NETCDF4)
            || xtype == NC_USHORT || xtype == NC_UINT
            || xtype == NC_UBYTE
#endif
             ;

  return isInt;
}

static
int cdfInqDatatype(int xtype, bool lunsigned)
{
  int datatype = -1;

#if  defined  (HAVE_NETCDF4)
  if ( xtype == NC_BYTE && lunsigned ) xtype = NC_UBYTE;
#endif

  if      ( xtype == NC_BYTE   )  datatype = DATATYPE_INT8;
  /* else if ( xtype == NC_CHAR   )  datatype = DATATYPE_UINT8; */
  else if ( xtype == NC_SHORT  )  datatype = DATATYPE_INT16;
  else if ( xtype == NC_INT    )  datatype = DATATYPE_INT32;
  else if ( xtype == NC_FLOAT  )  datatype = DATATYPE_FLT32;
  else if ( xtype == NC_DOUBLE )  datatype = DATATYPE_FLT64;
#if  defined  (HAVE_NETCDF4)
  else if ( xtype == NC_UBYTE  )  datatype = DATATYPE_UINT8;
  else if ( xtype == NC_LONG   )  datatype = DATATYPE_INT32;
  else if ( xtype == NC_USHORT )  datatype = DATATYPE_UINT16;
  else if ( xtype == NC_UINT   )  datatype = DATATYPE_UINT32;
  else if ( xtype == NC_INT64  )  datatype = DATATYPE_FLT64;
  else if ( xtype == NC_UINT64 )  datatype = DATATYPE_FLT64;
#endif

  return datatype;
}


/* not used
int cdfInqRecord(stream_t *streamptr, int *varID, int *levelID)
{
  int tsID, recID;

  recID = streamptr->tsteps[0].curRecID++;
  printf("cdfInqRecord recID %d %d\n", recID, streamptr->tsteps[0].curRecID);
  printf("cdfInqRecord tsID %d\n", streamptr->curTsID);

  if ( streamptr->tsteps[0].curRecID >= streamptr->tsteps[0].nrecs )
    {
      streamptr->tsteps[0].curRecID = 0;
    }

  *varID   = streamptr->tsteps[0].records[recID].varID;
  *levelID = streamptr->tsteps[0].records[recID].levelID;

  streamptr->record->varID   = *varID;
  streamptr->record->levelID = *levelID;

  if ( CDI_Debug )
    Message("recID = %d  varID = %d  levelID = %d", recID, *varID, *levelID);

  return (recID+1);
}
*/

void cdf_scale_add(size_t size, double *data, double addoffset, double scalefactor)
{
  int laddoffset   = IS_NOT_EQUAL(addoffset, 0);
  int lscalefactor = IS_NOT_EQUAL(scalefactor, 1);

  if ( laddoffset || lscalefactor )
    {
      for ( size_t i = 0; i < size; ++i )
        {
          if ( lscalefactor ) data[i] *= scalefactor;
          if ( laddoffset )   data[i] += addoffset;
        }
    }
}

static
void cdfCreateRecords(stream_t *streamptr, int tsID)
{
  if ( tsID < 0 || (tsID >= streamptr->ntsteps && tsID > 0) ) return;

  if ( streamptr->tsteps[tsID].nallrecs > 0 ) return;

  int vlistID  = streamptr->vlistID;

  tsteps_t* sourceTstep = streamptr->tsteps;
  tsteps_t* destTstep = sourceTstep + tsID;

  int nvars = vlistNvars(vlistID);
  int nrecs = vlistNrecs(vlistID);

  if ( nrecs <= 0 ) return;

  if ( tsID == 0 )
    {
      int nvrecs = nrecs; /* use all records at first timestep */

      streamptr->nrecs += nrecs;

      destTstep->records    = (record_t *) Malloc((size_t)nrecs*sizeof(record_t));
      destTstep->nrecs      = nrecs;
      destTstep->nallrecs   = nrecs;
      destTstep->recordSize = nrecs;
      destTstep->curRecID   = UNDEFID;
      destTstep->recIDs     = (int *) Malloc((size_t)nvrecs*sizeof (int));;
      for ( int recID = 0; recID < nvrecs; recID++ ) destTstep->recIDs[recID] = recID;

      record_t *records = destTstep->records;

      for ( int varID = 0, recID = 0; varID < nvars; varID++ )
        {
          int zaxisID = vlistInqVarZaxis(vlistID, varID);
          int nlev    = zaxisInqSize(zaxisID);
          for ( int levelID = 0; levelID < nlev; levelID++ )
            {
              recordInitEntry(&records[recID]);
              records[recID].varID   = (short)varID;
              records[recID].levelID = (short)levelID;
              recID++;
            }
        }
    }
  else if ( tsID == 1 )
    {
      int nvrecs = 0;
      for ( int varID = 0; varID < nvars; varID++ )
        {
          if ( vlistInqVarTsteptype(vlistID, varID) != TSTEP_CONSTANT )
            {
              int zaxisID = vlistInqVarZaxis(vlistID, varID);
              nvrecs += zaxisInqSize(zaxisID);
            }
        }

      streamptr->nrecs += nvrecs;

      destTstep->records    = (record_t *) Malloc((size_t)nrecs*sizeof(record_t));
      destTstep->nrecs      = nvrecs;
      destTstep->nallrecs   = nrecs;
      destTstep->recordSize = nrecs;
      destTstep->curRecID   = UNDEFID;

      memcpy(destTstep->records, sourceTstep->records, (size_t)nrecs*sizeof(record_t));

      if ( nvrecs )
        {
          destTstep->recIDs = (int *) Malloc((size_t)nvrecs * sizeof (int));
          for ( int recID = 0, vrecID = 0; recID < nrecs; recID++ )
            {
              int varID = destTstep->records[recID].varID;
              if ( vlistInqVarTsteptype(vlistID, varID) != TSTEP_CONSTANT )
                {
                  destTstep->recIDs[vrecID++] = recID;
                }
            }
        }
    }
  else
    {
      if ( streamptr->tsteps[1].records == 0 ) cdfCreateRecords(streamptr, 1);

      int nvrecs = streamptr->tsteps[1].nrecs;

      streamptr->nrecs += nvrecs;

      destTstep->records    = (record_t *) Malloc((size_t)nrecs*sizeof(record_t));
      destTstep->nrecs      = nvrecs;
      destTstep->nallrecs   = nrecs;
      destTstep->recordSize = nrecs;
      destTstep->curRecID   = UNDEFID;

      memcpy(destTstep->records, sourceTstep->records, (size_t)nrecs*sizeof(record_t));

      destTstep->recIDs     = (int *) Malloc((size_t)nvrecs * sizeof(int));

      memcpy(destTstep->recIDs, streamptr->tsteps[1].recIDs, (size_t)nvrecs*sizeof(int));
    }
}

static
int cdfTimeDimID(int fileID, int ndims, int nvars)
{
  char dimname[80];
  for ( int dimid = 0; dimid < ndims; ++dimid )
    {
      dimname[0] = 0;
      cdf_inq_dimname(fileID, dimid, dimname);
      size_t len = strlen(dimname);
      if ( len >= 4 && memcmp(dimname, "time", 4) == 0 ) return dimid;
    }

  for ( int varid = 0; varid < nvars; ++varid )
    {
      nc_type xtype;
      int nvdims, nvatts, dimids[9];
      cdf_inq_var(fileID, varid, NULL, &xtype, &nvdims, dimids, &nvatts);
      if ( nvdims == 1 )
        {
          char sbuf[CDI_MAX_NAME];
          for ( int iatt = 0; iatt < nvatts; ++iatt )
            {
              sbuf[0] = 0;
              cdf_inq_attname(fileID, varid, iatt, sbuf);
              if ( strncmp(sbuf, "units", 5) == 0 )
                {
                  cdfGetAttText(fileID, varid, "units", sizeof(sbuf), sbuf);
                  strtolower(sbuf);

                  if ( is_time_units(sbuf) ) return dimids[0];
                }
            }
        }
    }

  return UNDEFID;
}

static
void init_ncdims(long ndims, ncdim_t *ncdims)
{
  for ( long ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      ncdims[ncdimid].ncvarid      = UNDEFID;
      ncdims[ncdimid].dimtype      = UNDEFID;
      ncdims[ncdimid].len          = 0;
      ncdims[ncdimid].name[0]      = 0;
    }
}

static
void init_ncvars(long nvars, ncvar_t *ncvars)
{
  for ( long ncvarid = 0; ncvarid < nvars; ++ncvarid )
    {
      ncvars[ncvarid].ncid            = UNDEFID;
      ncvars[ncvarid].isvar           = UNDEFID;
      ncvars[ncvarid].ignore          = false;
      ncvars[ncvarid].islon           = false;
      ncvars[ncvarid].islat           = false;
      ncvars[ncvarid].islev           = false;
      ncvars[ncvarid].istime          = false;
      ncvars[ncvarid].warn            = false;
      ncvars[ncvarid].climatology     = false;
      ncvars[ncvarid].tsteptype       = TSTEP_CONSTANT;
      ncvars[ncvarid].param           = UNDEFID;
      ncvars[ncvarid].code            = UNDEFID;
      ncvars[ncvarid].tabnum          = 0;
      ncvars[ncvarid].calendar        = FALSE;
      ncvars[ncvarid].bounds          = UNDEFID;
      ncvars[ncvarid].lformula        = FALSE;
      ncvars[ncvarid].lformulaterms   = FALSE;
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
      ncvars[ncvarid].psvarid         = UNDEFID;
      ncvars[ncvarid].p0varid         = UNDEFID;
      ncvars[ncvarid].ncoordvars      = 0;
      for ( int i = 0; i < MAX_COORDVARS; ++i )
        ncvars[ncvarid].coordvarids[i]  = UNDEFID;
      ncvars[ncvarid].nauxvars      = 0;
      for ( int i = 0; i < MAX_AUXVARS; ++i )
        ncvars[ncvarid].auxvarids[i]  = UNDEFID;
      ncvars[ncvarid].cellarea        = UNDEFID;
      ncvars[ncvarid].tableID         = UNDEFID;
      ncvars[ncvarid].xtype           = 0;
      ncvars[ncvarid].ndims           = 0;
      ncvars[ncvarid].gmapid          = UNDEFID;
      ncvars[ncvarid].vctsize         = 0;
      ncvars[ncvarid].vct             = NULL;
      ncvars[ncvarid].truncation      = 0;
      ncvars[ncvarid].position        = 0;
      ncvars[ncvarid].positive        = 0;
      ncvars[ncvarid].chunked         = 0;
      ncvars[ncvarid].chunktype       = UNDEFID;
      ncvars[ncvarid].defmissval      = false;
      ncvars[ncvarid].deffillval      = false;
      ncvars[ncvarid].missval         = 0;
      ncvars[ncvarid].fillval         = 0;
      ncvars[ncvarid].addoffset       = 0;
      ncvars[ncvarid].scalefactor     = 1;
      ncvars[ncvarid].name[0]         = 0;
      ncvars[ncvarid].longname[0]     = 0;
      ncvars[ncvarid].stdname[0]      = 0;
      ncvars[ncvarid].units[0]        = 0;
      ncvars[ncvarid].extra[0]        = 0;
      ncvars[ncvarid].natts           = 0;
      ncvars[ncvarid].atts            = NULL;
      ncvars[ncvarid].deflate         = 0;
      ncvars[ncvarid].lunsigned       = false;
      ncvars[ncvarid].lvalidrange     = false;
      ncvars[ncvarid].validrange[0]   = VALIDMISS;
      ncvars[ncvarid].validrange[1]   = VALIDMISS;
      ncvars[ncvarid].ensdata         = NULL;
    }
}

static
void cdfSetVar(ncvar_t *ncvars, int ncvarid, short isvar)
{
  if ( ncvars[ncvarid].isvar != UNDEFID &&
       ncvars[ncvarid].isvar != isvar   &&
       ncvars[ncvarid].warn  == false )
    {
      if ( ! ncvars[ncvarid].ignore )
        Warning("Inconsistent variable definition for %s!", ncvars[ncvarid].name);

      ncvars[ncvarid].warn = true;
      isvar = FALSE;
    }

  ncvars[ncvarid].isvar = isvar;
}

static
void cdfSetDim(ncvar_t *ncvars, int ncvarid, int dimid, int dimtype)
{
  if ( ncvars[ncvarid].dimtype[dimid] != UNDEFID &&
       ncvars[ncvarid].dimtype[dimid] != dimtype )
    {
      Warning("Inconsistent dimension definition for %s! dimid = %d;  type = %d;  newtype = %d",
              ncvars[ncvarid].name, dimid, ncvars[ncvarid].dimtype[dimid], dimtype);
    }

  ncvars[ncvarid].dimtype[dimid] = dimtype;
}

static
int scan_hybrid_formula(int ncid, int ncfvarid, int *apvarid, int *bvarid, int *psvarid, int *avarid, int *p0varid)
{
  int status = 0;
  *apvarid = -1;
  *bvarid  = -1;
  *psvarid = -1;
  *avarid  = -1;
  *p0varid = -1;
  enum { attstringlen = 8192 }; char attstring[attstringlen];
  cdfGetAttText(ncid, ncfvarid, "formula", attstringlen, attstring);
  if ( strcmp(attstring, "p = ap + b*ps") == 0 )
    {
      status = 1;
      int lstop = FALSE;
      int dimvarid;
      cdfGetAttText(ncid, ncfvarid, "formula_terms", attstringlen, attstring);
      char *pstring = attstring;

      for ( int i = 0; i < 3; i++ )
        {
          while ( isspace((int) *pstring) ) pstring++;
          if ( *pstring == 0 ) break;
          char *tagname = pstring;
          while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
          if ( *pstring == 0 ) lstop = TRUE;
          *pstring++ = 0;

          while ( isspace((int) *pstring) ) pstring++;
          if ( *pstring == 0 ) break;
          char *varname = pstring;
          while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
          if ( *pstring == 0 ) lstop = TRUE;
          *pstring++ = 0;

          int status_nc = nc_inq_varid(ncid, varname, &dimvarid);
          if ( status_nc == NC_NOERR )
            {
              if      ( strcmp(tagname, "ap:") == 0 ) *apvarid = dimvarid;
              else if ( strcmp(tagname, "b:")  == 0 ) *bvarid  = dimvarid;
              else if ( strcmp(tagname, "ps:") == 0 ) *psvarid = dimvarid;
            }
          else if ( strcmp(tagname, "ps:") != 0 )
            {
              Warning("%s - %s", nc_strerror(status_nc), varname);
            }

          if ( lstop ) break;
        }
    }
  else if ( strcmp(attstring, "xxxp = a*p0 + b*ps") == 0 )
    {
      status = 2;
      int lstop = FALSE;
      int dimvarid;
      cdfGetAttText(ncid, ncfvarid, "formula_terms", attstringlen, attstring);
      char *pstring = attstring;

      for ( int i = 0; i < 4; i++ )
        {
          while ( isspace((int) *pstring) ) pstring++;
          if ( *pstring == 0 ) break;
          char *tagname = pstring;
          while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
          if ( *pstring == 0 ) lstop = TRUE;
          *pstring++ = 0;

          while ( isspace((int) *pstring) ) pstring++;
          if ( *pstring == 0 ) break;
          char *varname = pstring;
          while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
          if ( *pstring == 0 ) lstop = TRUE;
          *pstring++ = 0;

          int status_nc = nc_inq_varid(ncid, varname, &dimvarid);
          if ( status_nc == NC_NOERR )
            {
              if      ( strcmp(tagname, "a:")  == 0 ) *avarid  = dimvarid;
              else if ( strcmp(tagname, "b:")  == 0 ) *bvarid  = dimvarid;
              else if ( strcmp(tagname, "ps:") == 0 ) *psvarid = dimvarid;
              else if ( strcmp(tagname, "p0:") == 0 ) *p0varid = dimvarid;
            }
          else if ( strcmp(tagname, "ps:") != 0 )
            {
              Warning("%s - %s", nc_strerror(status_nc), varname);
            }

          if ( lstop ) break;
        }
    }

  return status;
}

static
bool isHybridSigmaPressureCoordinate(int ncid, int ncvarid, ncvar_t *ncvars, const ncdim_t *ncdims)
{
  bool status = false;
  int ncfvarid = ncvarid;
  ncvar_t *ncvar = &ncvars[ncvarid];

  if ( strcmp(ncvar->stdname, "atmosphere_hybrid_sigma_pressure_coordinate") == 0 )
    {
      cdiConvention = CDI_CONVENTION_CF;

      status = true;
      ncvar->zaxistype = ZAXIS_HYBRID;
      int dimid = ncvar->dimids[0];
      size_t dimlen = ncdims[dimid].len;

      int ret;
      int apvarid1 = -1, bvarid1 = -1, psvarid1 = -1, avarid1 = -1, p0varid1 = -1;
      if ( ncvars[ncfvarid].lformula && ncvars[ncfvarid].lformulaterms )
        ret = scan_hybrid_formula(ncid, ncfvarid, &apvarid1, &bvarid1, &psvarid1, &avarid1, &p0varid1);
      if ( apvarid1 != -1 ) ncvars[apvarid1].isvar = FALSE;
      if ( bvarid1  != -1 ) ncvars[bvarid1].isvar  = FALSE;
      if ( psvarid1 != -1 ) ncvar->psvarid = psvarid1;
      if ( avarid1  != -1 ) ncvars[avarid1].isvar = FALSE;
      if ( p0varid1 != -1 ) ncvar->p0varid = p0varid1;

      if ( ncvar->bounds != UNDEFID && ncvars[ncvar->bounds].lformula && ncvars[ncvar->bounds].lformulaterms )
        {
          ncfvarid = ncvar->bounds;
          int apvarid2 = -1, bvarid2 = -1, psvarid2 = -1, avarid2 = -1, p0varid2 = -1;
          ret = 0;
          if ( ncvars[ncfvarid].lformula && ncvars[ncfvarid].lformulaterms )
            ret = scan_hybrid_formula(ncid, ncfvarid, &apvarid2, &bvarid2, &psvarid2, &avarid2, &p0varid2);
          if ( ret == 1 ) avarid2 = apvarid2;
          if ( avarid2 != -1 && bvarid2 != -1 )
            {
              ncvars[avarid2].isvar = FALSE;
              ncvars[bvarid2].isvar  = FALSE;

              if ( dimid == ncvars[avarid2].dimids[0] && ncdims[ncvars[avarid2].dimids[1]].len == 2 )
                {
                  double px = 1;
                  if ( ret == 2 && p0varid1 == p0varid2 )
                    cdf_get_var_double(ncid, p0varid2, &px);

                  double abuf[dimlen*2], bbuf[dimlen*2];
                  cdf_get_var_double(ncid, avarid2, abuf);
                  cdf_get_var_double(ncid, bvarid2, bbuf);
                  /*
                  for ( int i = 0; i < dimlen; ++i )
                    printf("%d  %g %g    %g %g\n", i, abuf[i*2], abuf[i*2+1], bbuf[i*2], bbuf[i*2+1]);
                  */
                  size_t vctsize = (dimlen+1)*2;
                  double *vct = (double *) Malloc(vctsize * sizeof(double));
                  for ( size_t i = 0; i < dimlen; ++i )
                    {
                      vct[i] = abuf[i*2];
                      vct[i+dimlen+1] = bbuf[i*2];
                    }
                  vct[dimlen]     = abuf[dimlen*2-1];
                  vct[dimlen*2+1] = bbuf[dimlen*2-1];

                  if ( ret == 2 && IS_NOT_EQUAL(px, 1) )
                    for ( size_t i = 0; i < dimlen+1; ++i ) vct[i] *= px;

                  ncvar->vct = vct;
                  ncvar->vctsize = vctsize;
                }
            }
        }
    }

  return status;
}

static
void printNCvars(const ncvar_t *ncvars, int nvars, const char *oname)
{
  char axis[7];
  int ncvarid, i;
  int ndim;
  static const char iaxis[] = {'t', 'z', 'y', 'x'};

  fprintf(stderr, "%s:\n", oname);

  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      ndim = 0;
      if ( ncvars[ncvarid].isvar )
        {
          axis[ndim++] = 'v';
          axis[ndim++] = ':';
          for ( i = 0; i < ncvars[ncvarid].ndims; i++ )
            {/*
              if      ( ncvars[ncvarid].tvarid != -1 ) axis[ndim++] = iaxis[0];
              else if ( ncvars[ncvarid].zvarid != -1 ) axis[ndim++] = iaxis[1];
              else if ( ncvars[ncvarid].yvarid != -1 ) axis[ndim++] = iaxis[2];
              else if ( ncvars[ncvarid].xvarid != -1 ) axis[ndim++] = iaxis[3];
              else
             */
              if      ( ncvars[ncvarid].dimtype[i] == T_AXIS ) axis[ndim++] = iaxis[0];
              else if ( ncvars[ncvarid].dimtype[i] == Z_AXIS ) axis[ndim++] = iaxis[1];
              else if ( ncvars[ncvarid].dimtype[i] == Y_AXIS ) axis[ndim++] = iaxis[2];
              else if ( ncvars[ncvarid].dimtype[i] == X_AXIS ) axis[ndim++] = iaxis[3];
              else                                             axis[ndim++] = '?';
            }
        }
      else
        {
          axis[ndim++] = 'c';
          axis[ndim++] = ':';
          if      ( ncvars[ncvarid].istime ) axis[ndim++] = iaxis[0];
          else if ( ncvars[ncvarid].islev  ) axis[ndim++] = iaxis[1];
          else if ( ncvars[ncvarid].islat  ) axis[ndim++] = iaxis[2];
          else if ( ncvars[ncvarid].islon  ) axis[ndim++] = iaxis[3];
          else                               axis[ndim++] = '?';
        }

      axis[ndim++] = 0;

      fprintf(stderr, "%3d %3d  %-6s %s\n", ncvarid, ndim-3, axis, ncvars[ncvarid].name);
    }
}

static
void cdf_scan_attr_axis(ncvar_t *ncvars, ncdim_t *ncdims, int ncvarid, const char *attstring, size_t attlen,
                        int nvdims, int *dimidsp, const char *name)
{
  int i;
  for ( i = 0; i < (int)attlen; ++i )
    {
      if ( attstring[i] != '-' && attstring[i] != 't' && attstring[i] != 'z' &&
           attstring[i] != 'y' && attstring[i] != 'x' )
        {
          Warning("Unexpected character in axis attribute for %s, ignored!", name);
          break;
        }
    }

  if ( i == (int) attlen && (int) attlen == nvdims )
    {
      while ( attlen-- )
        {
          if ( (int) attstring[attlen] == 't' )
            {
              if ( attlen != 0 ) Warning("axis attribute 't' not on first position");
              cdfSetDim(ncvars, ncvarid, (int)attlen, T_AXIS);
            }
          else if ( (int) attstring[attlen] == 'z' )
            {
              ncvars[ncvarid].zdim = dimidsp[attlen];
              cdfSetDim(ncvars, ncvarid, (int)attlen, Z_AXIS);

              if ( ncvars[ncvarid].ndims == 1 )
                {
                  cdfSetVar(ncvars, ncvarid, FALSE);
                  ncdims[ncvars[ncvarid].dimids[0]].dimtype = Z_AXIS;
                }
            }
          else if ( (int) attstring[attlen] == 'y' )
            {
              ncvars[ncvarid].ydim = dimidsp[attlen];
              cdfSetDim(ncvars, ncvarid, (int)attlen, Y_AXIS);

              if ( ncvars[ncvarid].ndims == 1 )
                {
                  cdfSetVar(ncvars, ncvarid, FALSE);
                  ncdims[ncvars[ncvarid].dimids[0]].dimtype = Y_AXIS;
                }
            }
          else if ( (int) attstring[attlen] == 'x' )
            {
              ncvars[ncvarid].xdim = dimidsp[attlen];
              cdfSetDim(ncvars, ncvarid, (int)attlen, X_AXIS);

              if ( ncvars[ncvarid].ndims == 1 )
                {
                  cdfSetVar(ncvars, ncvarid, FALSE);
                  ncdims[ncvars[ncvarid].dimids[0]].dimtype = X_AXIS;
                }
            }
        }
    }
}

static
void cdf_scan_var_attributes(int nvars, ncvar_t *ncvars, ncdim_t *ncdims,
                             int timedimid, int modelID, int format)
{
  int ncdimid;
  int nvdims, nvatts;
  int iatt;
  nc_type xtype, atttype;
  size_t attlen;
  char name[CDI_MAX_NAME];
  char attname[CDI_MAX_NAME];
  const int attstringlen = 8192; char attstring[8192];

  int nchecked_vars = 0;
  enum { max_check_vars = 9 };
  char *checked_vars[max_check_vars];
  for ( int i = 0; i < max_check_vars; ++i ) checked_vars[i] = NULL;

  for ( int ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      int ncid    = ncvars[ncvarid].ncid;
      int *dimidsp = ncvars[ncvarid].dimids;

      cdf_inq_var(ncid, ncvarid, name, &xtype, &nvdims, dimidsp, &nvatts);
      strcpy(ncvars[ncvarid].name, name);

      for ( ncdimid = 0; ncdimid < nvdims; ncdimid++ )
        ncvars[ncvarid].dimtype[ncdimid] = -1;

      ncvars[ncvarid].xtype = xtype;
      ncvars[ncvarid].ndims = nvdims;

#if  defined  (HAVE_NETCDF4)
      if ( format == NC_FORMAT_NETCDF4_CLASSIC || format == NC_FORMAT_NETCDF4 )
        {
          int shuffle, deflate, deflate_level;
          size_t chunks[nvdims];
          int storage_in;
          nc_inq_var_deflate(ncid, ncvarid, &shuffle, &deflate, &deflate_level);
          if ( deflate > 0 ) ncvars[ncvarid].deflate = 1;

          if ( nc_inq_var_chunking(ncid, ncvarid, &storage_in, chunks) == NC_NOERR )
            {
              if ( storage_in == NC_CHUNKED )
                {
                  ncvars[ncvarid].chunked = 1;
                  for ( int i = 0; i < nvdims; ++i ) ncvars[ncvarid].chunks[i] = (int)chunks[i];
                  if ( CDI_Debug )
                    {
                      fprintf(stderr, "%s: chunking %d %d %d  chunks ", name, storage_in, NC_CONTIGUOUS, NC_CHUNKED);
                      for ( int i = 0; i < nvdims; ++i ) fprintf(stderr, "%ld ", chunks[i]);
                      fprintf(stderr, "\n");
                    }
                  {
                    char *buf = ncvars[ncvarid].extra;
                    size_t pos = strlen(buf);
                    static const char prefix[] = "chunks=";
                    memcpy(buf + pos, prefix, sizeof (prefix));
                    pos += sizeof (prefix) - 1;
                    for ( int i = nvdims-1; i >= 0; --i )
                      {
                        pos += (size_t)(sprintf(buf + pos, "%zu%s", chunks[i],
                                                i > 0 ? "x" : ""));
                      }
                    buf[pos] = ' '; buf[pos + 1] = 0;
                  }
                }
            }
        }
#endif

      if ( nvdims > 0 )
        {
          if ( timedimid == dimidsp[0] )
            {
              ncvars[ncvarid].tsteptype = TSTEP_INSTANT;
              cdfSetDim(ncvars, ncvarid, 0, T_AXIS);
            }
          else
            {
              for ( ncdimid = 1; ncdimid < nvdims; ncdimid++ )
                {
                  if ( timedimid == dimidsp[ncdimid] )
                    {
                      Warning("Time must be the first dimension! Unsupported array structure, skipped variable %s!", ncvars[ncvarid].name);
                      ncvars[ncvarid].isvar = FALSE;
                    }
                }
            }
        }

      for ( iatt = 0; iatt < nvatts; iatt++ )
        {
          cdf_inq_attname(ncid, ncvarid, iatt, attname);
          cdf_inq_atttype(ncid, ncvarid, attname, &atttype);
          cdf_inq_attlen(ncid, ncvarid, attname, &attlen);

          if ( strcmp(attname, "long_name") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, CDI_MAX_NAME, ncvars[ncvarid].longname);
            }
          else if ( strcmp(attname, "standard_name") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, CDI_MAX_NAME, ncvars[ncvarid].stdname);
            }
          else if ( strcmp(attname, "units") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, CDI_MAX_NAME, ncvars[ncvarid].units);
            }
          else if ( strcmp(attname, "calendar") == 0 )
            {
              ncvars[ncvarid].calendar = TRUE;
            }
          else if ( strcmp(attname, "param") == 0 && xtypeIsText(atttype) )
            {
	      char paramstr[32];
	      int pnum = 0, pcat = 255, pdis = 255;
              cdfGetAttText(ncid, ncvarid, attname, sizeof(paramstr), paramstr);
	      sscanf(paramstr, "%d.%d.%d", &pnum, &pcat, &pdis);
	      ncvars[ncvarid].param = cdiEncodeParam(pnum, pcat, pdis);
              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( strcmp(attname, "code") == 0 && !xtypeIsText(atttype) )
            {
              cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvars[ncvarid].code);
              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( strcmp(attname, "table") == 0 && !xtypeIsText(atttype) )
            {
              int tablenum;
              cdfGetAttInt(ncid, ncvarid, attname, 1, &tablenum);
              if ( tablenum > 0 )
                {
                  ncvars[ncvarid].tabnum = tablenum;
                  ncvars[ncvarid].tableID = tableInq(modelID, tablenum, NULL);
                  if ( ncvars[ncvarid].tableID == CDI_UNDEFID )
                    ncvars[ncvarid].tableID = tableDef(modelID, tablenum, NULL);
                }
              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( strcmp(attname, "trunc_type") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              if ( memcmp(attstring, "Triangular", attlen) == 0 )
                ncvars[ncvarid].gridtype = GRID_SPECTRAL;
            }
          else if ( strcmp(attname, "grid_type") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              strtolower(attstring);

              set_gridtype(attstring, &ncvars[ncvarid].gridtype);

              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( strcmp(attname, "level_type") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              strtolower(attstring);

              set_zaxistype(attstring, &ncvars[ncvarid].zaxistype);

              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( strcmp(attname, "trunc_count") == 0 && !xtypeIsText(atttype) )
            {
              cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvars[ncvarid].truncation);
            }
          else if ( strcmp(attname, "truncation") == 0 && !xtypeIsText(atttype) )
            {
              cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvars[ncvarid].truncation);
            }
          else if ( strcmp(attname, "number_of_grid_in_reference") == 0 && !xtypeIsText(atttype) )
            {
              cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvars[ncvarid].position);
            }
          else if ( strcmp(attname, "add_offset") == 0 && !xtypeIsText(atttype) )
            {
	      cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvars[ncvarid].addoffset);
	      /*
		if ( atttype != NC_BYTE && atttype != NC_SHORT && atttype != NC_INT )
		if ( ncvars[ncvarid].addoffset != 0 )
		Warning("attribute add_offset not supported for atttype %d", atttype);
	      */
	      /* (also used for lon/lat) cdfSetVar(ncvars, ncvarid, TRUE); */
            }
          else if ( strcmp(attname, "scale_factor") == 0 && !xtypeIsText(atttype) )
            {
	      cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvars[ncvarid].scalefactor);
	      /*
		if ( atttype != NC_BYTE && atttype != NC_SHORT && atttype != NC_INT )
		if ( ncvars[ncvarid].scalefactor != 1 )
		Warning("attribute scale_factor not supported for atttype %d", atttype);
	      */
	      /* (also used for lon/lat) cdfSetVar(ncvars, ncvarid, TRUE); */
            }
          else if ( strcmp(attname, "climatology") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              int ncboundsid;
              int status = nc_inq_varid(ncid, attstring, &ncboundsid);
              if ( status == NC_NOERR )
                {
                  ncvars[ncvarid].climatology = true;
                  ncvars[ncvarid].bounds = ncboundsid;
                  cdfSetVar(ncvars, ncvars[ncvarid].bounds, FALSE);
                  cdfSetVar(ncvars, ncvarid, FALSE);
                }
              else
                Warning("%s - %s", nc_strerror(status), attstring);
            }
          else if ( xtypeIsText(atttype) && strcmp(attname, "bounds") == 0 )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              int ncboundsid;
              int status = nc_inq_varid(ncid, attstring, &ncboundsid);
              if ( status == NC_NOERR )
                {
                  ncvars[ncvarid].bounds = ncboundsid;
                  cdfSetVar(ncvars, ncvars[ncvarid].bounds, FALSE);
                  cdfSetVar(ncvars, ncvarid, FALSE);
                }
              else
                Warning("%s - %s", nc_strerror(status), attstring);
            }
          else if ( xtypeIsText(atttype) && strcmp(attname, "formula_terms") == 0 )
            {
              ncvars[ncvarid].lformulaterms = TRUE;
            }
          else if ( xtypeIsText(atttype) && strcmp(attname, "formula") == 0 )
            {
              ncvars[ncvarid].lformula = TRUE;
            }
          else if ( strcmp(attname, "cell_measures") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              char *pstring = attstring;

              while ( isspace((int) *pstring) ) pstring++;
              char *cell_measures = pstring;
              while ( isalnum((int) *pstring) ) pstring++;
              *pstring++ = 0;
              while ( isspace((int) *pstring) ) pstring++;
              char *cell_var = pstring;
              while ( ! isspace((int) *pstring) && *pstring != 0 ) pstring++;
              *pstring++ = 0;
              /*
              printf("cell_measures >%s<\n", cell_measures);
              printf("cell_var >%s<\n", cell_var);
              */
              if ( memcmp(cell_measures, "area", 4) == 0 )
                {
                  int nc_cell_id;
                  int status = nc_inq_varid(ncid, cell_var, &nc_cell_id);
                  if ( status == NC_NOERR )
                    {
                      ncvars[ncvarid].cellarea = nc_cell_id;
                      /* ncvars[nc_cell_id].isvar = UNDEFID; */
                      cdfSetVar(ncvars, nc_cell_id, FALSE);
                    }
                  else
                    Warning("%s - %s", nc_strerror(status), cell_var);
                }
              else
                {
                  Warning("%s has an unexpected contents: %s", attname, cell_measures);
                }
              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          /*
          else if ( strcmp(attname, "coordinates") == 0 )
            {
              char *pstring, *xvarname = NULL, *yvarname = NULL;

              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              pstring = attstring;

              while ( isspace((int) *pstring) ) pstring++;
              xvarname = pstring;
              while ( isgraph((int) *pstring) ) pstring++;
              *pstring++ = 0;
              while ( isspace((int) *pstring) ) pstring++;
              yvarname = pstring;
              while ( isgraph((int) *pstring) ) pstring++;
              *pstring++ = 0;

              cdf_inq_varid(ncid, xvarname, &ncvars[ncvarid].xvarid);
              cdf_inq_varid(ncid, yvarname, &ncvars[ncvarid].yvarid);

              cdfSetVar(ncvars, ncvars[ncvarid].xvarid, FALSE);
              cdfSetVar(ncvars, ncvars[ncvarid].yvarid, FALSE);
              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          */
          else if ( (strcmp(attname, "associate")  == 0 || strcmp(attname, "coordinates") == 0) && xtypeIsText(atttype) )
            {
              int lstop = FALSE;
              int dimvarid;

              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              char *pstring = attstring;

              for ( int i = 0; i < MAX_COORDVARS; i++ )
                {
                  while ( isspace((int) *pstring) ) pstring++;
                  if ( *pstring == 0 ) break;
                  char *varname = pstring;
                  while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
                  if ( *pstring == 0 ) lstop = TRUE;
                  *pstring++ = 0;

                  int status = nc_inq_varid(ncid, varname, &dimvarid);
                  if ( status == NC_NOERR )
                    {
                      cdfSetVar(ncvars, dimvarid, FALSE);
                      if ( cdiIgnoreAttCoordinates == false )
                        {
                          ncvars[ncvarid].coordvarids[i] = dimvarid;
                          ncvars[ncvarid].ncoordvars++;
                        }
                    }
                  else
                    {
                      int k;
                      for ( k = 0; k < nchecked_vars; ++k )
                        if ( strcmp(checked_vars[k], varname) == 0 ) break;

                      if ( k == nchecked_vars )
                        {
                          if ( nchecked_vars < max_check_vars ) checked_vars[nchecked_vars++] = strdup(varname);
                          Warning("%s - %s", nc_strerror(status), varname);
                        }
                    }

                  if ( lstop ) break;
                }

              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( (strcmp(attname, "auxiliary_variable") == 0) && xtypeIsText(atttype) )
            {
              int lstop = FALSE;
              int dimvarid;

              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              char *pstring = attstring;

              for ( int i = 0; i < MAX_AUXVARS; i++ )
                {
                  while ( isspace((int) *pstring) ) pstring++;
                  if ( *pstring == 0 ) break;
                  char *varname = pstring;
                  while ( !isspace((int) *pstring) && *pstring != 0 ) pstring++;
                  if ( *pstring == 0 ) lstop = TRUE;
                  *pstring++ = 0;

                  int status = nc_inq_varid(ncid, varname, &dimvarid);
                  if ( status == NC_NOERR )
                    {
                      cdfSetVar(ncvars, dimvarid, FALSE);
                      //  if ( cdiIgnoreAttCoordinates == FALSE )
                        {
                          ncvars[ncvarid].auxvarids[i] = dimvarid;
                          ncvars[ncvarid].nauxvars++;
                        }
                    }
                  else
                    Warning("%s - %s", nc_strerror(status), varname);

                  if ( lstop ) break;
                }

              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( strcmp(attname, "grid_mapping") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              int nc_gmap_id;
              int status = nc_inq_varid(ncid, attstring, &nc_gmap_id);
              if ( status == NC_NOERR )
                {
                  ncvars[ncvarid].gmapid = nc_gmap_id;
                  cdfSetVar(ncvars, ncvars[ncvarid].gmapid, FALSE);
                }
              else
                Warning("%s - %s", nc_strerror(status), attstring);

              cdfSetVar(ncvars, ncvarid, TRUE);
            }
          else if ( strcmp(attname, "positive") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              strtolower(attstring);

              if    ( memcmp(attstring, "down", 4) == 0 ) ncvars[ncvarid].positive = POSITIVE_DOWN;
              else if ( memcmp(attstring, "up", 2) == 0 ) ncvars[ncvarid].positive = POSITIVE_UP;

              if ( ncvars[ncvarid].ndims == 1 )
                {
                  cdfSetVar(ncvars, ncvarid, FALSE);
                  cdfSetDim(ncvars, ncvarid, 0, Z_AXIS);
                  ncdims[ncvars[ncvarid].dimids[0]].dimtype = Z_AXIS;
                }
            }
          else if ( strcmp(attname, "_FillValue") == 0 && !xtypeIsText(atttype) )
            {
	      cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvars[ncvarid].fillval);
	      ncvars[ncvarid].deffillval = true;
	      /* cdfSetVar(ncvars, ncvarid, TRUE); */
            }
          else if ( strcmp(attname, "missing_value") == 0 && !xtypeIsText(atttype) )
            {
	      cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvars[ncvarid].missval);
	      ncvars[ncvarid].defmissval = true;
	      /* cdfSetVar(ncvars, ncvarid, TRUE); */
            }
          else if ( strcmp(attname, "valid_range") == 0 && attlen == 2 )
            {
              if ( ncvars[ncvarid].lvalidrange == false )
                {
                  extern int cdiIgnoreValidRange;
                  bool lignore = xtypeIsFloat(atttype) != xtypeIsFloat(xtype);
                  if ( cdiIgnoreValidRange == FALSE && lignore == false )
                    {
                      cdfGetAttDouble(ncid, ncvarid, attname, 2, ncvars[ncvarid].validrange);
                      ncvars[ncvarid].lvalidrange = true;
                      if ( ((int)ncvars[ncvarid].validrange[0]) == 0 && ((int)ncvars[ncvarid].validrange[1]) == 255 )
                        ncvars[ncvarid].lunsigned = true;
                      /* cdfSetVar(ncvars, ncvarid, TRUE); */
                    }
                  else if ( lignore )
                    {
                      Warning("Inconsistent data type for attribute %s:valid_range, ignored!", name);
                    }
                }
            }
          else if ( strcmp(attname, "valid_min") == 0 && attlen == 1 )
            {
              if ( ncvars[ncvarid].lvalidrange == false )
                {
                  extern int cdiIgnoreValidRange;
                  bool lignore = xtypeIsFloat(atttype) != xtypeIsFloat(xtype);
                  if ( cdiIgnoreValidRange == FALSE && lignore == false )
                    {
                      cdfGetAttDouble(ncid, ncvarid, attname, 1, &(ncvars[ncvarid].validrange)[0]);
                      ncvars[ncvarid].lvalidrange = true;
                    }
                  else if ( lignore )
                    {
                      Warning("Inconsistent data type for attribute %s:valid_min, ignored!", name);
                    }
                }
            }
          else if ( strcmp(attname, "valid_max") == 0 && attlen == 1 )
            {
              if ( ncvars[ncvarid].lvalidrange == false )
                {
                  extern int cdiIgnoreValidRange;
                  bool lignore = xtypeIsFloat(atttype) != xtypeIsFloat(xtype);
                  if ( cdiIgnoreValidRange == FALSE && lignore == false )
                    {
                      cdfGetAttDouble(ncid, ncvarid, attname, 1, &(ncvars[ncvarid].validrange)[1]);
                      ncvars[ncvarid].lvalidrange = true;
                    }
                  else if ( lignore )
                    {
                      Warning("Inconsistent data type for attribute %s:valid_max, ignored!", name);
                    }
                }
            }
          else if ( strcmp(attname, "_Unsigned") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
              strtolower(attstring);

              if ( memcmp(attstring, "true", 4) == 0 )
                {
                  ncvars[ncvarid].lunsigned = true;
                  /*
                  ncvars[ncvarid].lvalidrange = true;
                  ncvars[ncvarid].validrange[0] = 0;
                  ncvars[ncvarid].validrange[1] = 255;
                  */
                }
	      /* cdfSetVar(ncvars, ncvarid, TRUE); */
            }
          else if ( strcmp(attname, "cdi") == 0 && xtypeIsText(atttype) )
            {
	      cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
	      strtolower(attstring);

	      if ( memcmp(attstring, "ignore", 6) == 0 )
		{
		  ncvars[ncvarid].ignore = true;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		}
            }
          else if ( strcmp(attname, "axis") == 0 && xtypeIsText(atttype) )
            {
              cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
	      attlen = strlen(attstring);

	      if ( (int) attlen > nvdims && nvdims > 0 && attlen > 1 )
		{
		    Warning("Unexpected axis attribute length for %s, ignored!", name);
		}
              else if ( nvdims == 0 && attlen == 1 )
                {
                  if ( attstring[0] == 'z' || attstring[0] == 'Z' )
                    {
                      cdfSetVar(ncvars, ncvarid, FALSE);
                      ncvars[ncvarid].islev = true;
                    }
                }
	      else
		{
		  strtolower(attstring);
                  cdf_scan_attr_axis(ncvars, ncdims, ncvarid, attstring, attlen, nvdims, dimidsp, name);
		}
	    }
	  else if ( ( strcmp(attname, "realization") == 0 )         ||
	            ( strcmp(attname, "ensemble_members") == 0 )    ||
	            ( strcmp(attname, "forecast_init_type") == 0 )    )
	    {
	      int temp;

	      if( ncvars[ncvarid].ensdata == NULL )
		ncvars[ncvarid].ensdata = (ensinfo_t *) Malloc( sizeof( ensinfo_t ) );

	      cdfGetAttInt(ncid, ncvarid, attname, 1, &temp);

	      if( strcmp(attname, "realization") == 0 )
		ncvars[ncvarid].ensdata->ens_index = temp;
	      else if( strcmp(attname, "ensemble_members") == 0 )
		ncvars[ncvarid].ensdata->ens_count = temp;
	      else if( strcmp(attname, "forecast_init_type") == 0 )
		ncvars[ncvarid].ensdata->forecast_init_type = temp;

	      cdfSetVar(ncvars, ncvarid, TRUE);
	    }
	  else
	    {
	      if ( ncvars[ncvarid].natts == 0 )
		ncvars[ncvarid].atts
                  = (int *) Malloc((size_t)nvatts * sizeof (int));

	      ncvars[ncvarid].atts[ncvars[ncvarid].natts++] = iatt;
	      /*
	      int attrint;
	      double attrflt;
	      nc_type attrtype;
	      cdf_inq_attlen(ncid, ncvarid, attname, &attlen);
	      cdf_inq_atttype(ncid, ncvarid, attname, &attrtype);
	      if ( attlen == 1 && (attrtype == NC_INT || attrtype == NC_SHORT) )
		{
		  cdfGetAttInt(ncid, ncvarid, attname, 1, &attrint);
		  printf("int: %s.%s = %d\n", ncvars[ncvarid].name, attname, attrint);
		}
	      else if ( attlen == 1 && (attrtype == NC_FLOAT || attrtype == NC_DOUBLE) )
		{
		  cdfGetAttDouble(ncid, ncvarid, attname, 1, &attrflt);
		  printf("flt: %s.%s = %g\n", ncvars[ncvarid].name, attname, attrflt);
		}
	      else if ( attrtype == NC_CHAR )
		{
		  cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
		  attstring[attlen] = 0;
		  printf("txt: %s.%s = %s\n", ncvars[ncvarid].name, attname, attstring);
		}
	      else
		printf("att: %s.%s = unknown\n", ncvars[ncvarid].name, attname);
	      */
	    }
	}
    }

  for ( int i = 0; i < max_check_vars; ++i ) if ( checked_vars[i] ) Free(checked_vars[i]);
}

static
void setDimType(int nvars, ncvar_t *ncvars, ncdim_t *ncdims)
{
  for ( int ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE )
	{
	  bool lxdim = false, lydim = false, lzdim = false/* , ltdim = false */;
	  int ndims = ncvars[ncvarid].ndims;
	  for ( int i = 0; i < ndims; i++ )
	    {
	      int ncdimid = ncvars[ncvarid].dimids[i];
	      if      ( ncdims[ncdimid].dimtype == X_AXIS ) cdfSetDim(ncvars, ncvarid, i, X_AXIS);
	      else if ( ncdims[ncdimid].dimtype == Y_AXIS ) cdfSetDim(ncvars, ncvarid, i, Y_AXIS);
	      else if ( ncdims[ncdimid].dimtype == Z_AXIS ) cdfSetDim(ncvars, ncvarid, i, Z_AXIS);
	      else if ( ncdims[ncdimid].dimtype == T_AXIS ) cdfSetDim(ncvars, ncvarid, i, T_AXIS);
	    }

	  if ( CDI_Debug )
	    {
	      Message("var %d %s", ncvarid, ncvars[ncvarid].name);
	      for ( int i = 0; i < ndims; i++ )
		printf("  dim%d type=%d  ", i, ncvars[ncvarid].dimtype[i]);
	      printf("\n");
	    }

	  for ( int i = 0; i < ndims; i++ )
	    {
	      if      ( ncvars[ncvarid].dimtype[i] == X_AXIS ) lxdim = true;
	      else if ( ncvars[ncvarid].dimtype[i] == Y_AXIS ) lydim = true;
	      else if ( ncvars[ncvarid].dimtype[i] == Z_AXIS ) lzdim = true;
	      /* else if ( ncvars[ncvarid].dimtype[i] == T_AXIS ) ltdim = true; */
	    }

          if ( !lxdim && ncvars[ncvarid].xvarid != UNDEFID )
            {
              if (  ncvars[ncvars[ncvarid].xvarid].ndims == 0 ) lxdim = true;
            }

          if ( !lydim && ncvars[ncvarid].yvarid != UNDEFID )
            {
              if (  ncvars[ncvars[ncvarid].yvarid].ndims == 0 ) lydim = true;
            }

          //   if ( ndims > 1 )
            for ( int i = ndims-1; i >= 0; i-- )
              {
                if ( ncvars[ncvarid].dimtype[i] == -1 )
                  {
                    if ( !lxdim )
                      {
                        cdfSetDim(ncvars, ncvarid, i, X_AXIS);
                        lxdim = true;
                      }
                    else if ( !lydim && ncvars[ncvarid].gridtype != GRID_UNSTRUCTURED )
                      {
                        cdfSetDim(ncvars, ncvarid, i, Y_AXIS);
                        lydim = true;
                      }
                    else if ( !lzdim )
                      {
                        cdfSetDim(ncvars, ncvarid, i, Z_AXIS);
                        lzdim = true;
                      }
                  }
              }
	}
    }
}

/* verify coordinate vars - first scan (dimname == varname) */
static
void verify_coordinate_vars_1(int ncid, int ndims, ncdim_t *ncdims, ncvar_t *ncvars, int timedimid)
{
  for ( int ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      int ncvarid = ncdims[ncdimid].ncvarid;
      if ( ncvarid != -1 )
	{
	  if ( ncvars[ncvarid].dimids[0] == timedimid )
	    {
              ncvars[ncvarid].istime = true;
	      ncdims[ncdimid].dimtype = T_AXIS;
	      continue;
	    }

          if ( isHybridSigmaPressureCoordinate(ncid, ncvarid, ncvars, ncdims) ) continue;

	  if ( ncvars[ncvarid].units[0] != 0 )
	    {
	      if ( is_lon_axis(ncvars[ncvarid].units, ncvars[ncvarid].stdname) )
		{
		  ncvars[ncvarid].islon = true;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, X_AXIS);
		  ncdims[ncdimid].dimtype = X_AXIS;
		}
	      else if ( is_lat_axis(ncvars[ncvarid].units, ncvars[ncvarid].stdname) )
		{
		  ncvars[ncvarid].islat = true;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, Y_AXIS);
		  ncdims[ncdimid].dimtype = Y_AXIS;
		}
	      else if ( is_pressure_units(ncvars[ncvarid].units) )
		{
		  ncvars[ncvarid].zaxistype = ZAXIS_PRESSURE;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "level") == 0 || strcmp(ncvars[ncvarid].units, "1") == 0 )
		{
		  if      ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer midpoints") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strncmp(ncvars[ncvarid].longname, "hybrid level at midpoints", 25) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer interfaces") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else if ( strncmp(ncvars[ncvarid].longname, "hybrid level at interfaces", 26) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else if ( strcmp(ncvars[ncvarid].units, "level") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_GENERIC;
		}
	      else if ( is_DBL_axis(ncvars[ncvarid].longname) )
                {
                  ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_LAND;
		}
	      else if ( is_height_units(ncvars[ncvarid].units) )
		{
		  if ( is_depth_axis(ncvars[ncvarid].stdname, ncvars[ncvarid].longname) )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_SEA;
		  else if ( is_height_axis(ncvars[ncvarid].stdname, ncvars[ncvarid].longname) )
		    ncvars[ncvarid].zaxistype = ZAXIS_HEIGHT;
		}
	    }
          else
            {
              if ( (strcmp(ncvars[ncvarid].longname, "generalized_height") == 0 ||
                    strcmp(ncvars[ncvarid].longname, "generalized height") == 0) &&
                   strcmp(ncvars[ncvarid].stdname, "height") == 0 )
                  ncvars[ncvarid].zaxistype = ZAXIS_REFERENCE;
            }

	  if ( ncvars[ncvarid].islon == false && ncvars[ncvarid].longname[0] != 0 &&
               ncvars[ncvarid].islat == false && ncvars[ncvarid].longname[1] != 0 )
	    {
	      if ( memcmp(ncvars[ncvarid].longname+1, "ongitude", 8) == 0 )
		{
		  ncvars[ncvarid].islon = true;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, X_AXIS);
		  ncdims[ncdimid].dimtype = X_AXIS;
		  continue;
		}
	      else if ( memcmp(ncvars[ncvarid].longname+1, "atitude", 7) == 0 )
		{
		  ncvars[ncvarid].islat = true;
		  cdfSetVar(ncvars, ncvarid, FALSE);
		  cdfSetDim(ncvars, ncvarid, 0, Y_AXIS);
		  ncdims[ncdimid].dimtype = Y_AXIS;
		  continue;
		}
	    }

	  if ( ncvars[ncvarid].zaxistype != UNDEFID )
	    {
              ncvars[ncvarid].islev = true;
	      cdfSetVar(ncvars, ncvarid, FALSE);
	      cdfSetDim(ncvars, ncvarid, 0, Z_AXIS);
	      ncdims[ncdimid].dimtype = Z_AXIS;
	    }
	}
    }
}

/* verify coordinate vars - second scan (all other variables) */
static
void verify_coordinate_vars_2(int nvars, ncvar_t *ncvars)
{
  for ( int ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == 0 )
	{
	  if ( ncvars[ncvarid].units[0] != 0 )
	    {
	      if ( is_lon_axis(ncvars[ncvarid].units, ncvars[ncvarid].stdname) )
		{
		  ncvars[ncvarid].islon = true;
		  continue;
		}
	      else if ( is_lat_axis(ncvars[ncvarid].units, ncvars[ncvarid].stdname) )
		{
		  ncvars[ncvarid].islat = true;
		  continue;
		}
	      else if ( is_pressure_units(ncvars[ncvarid].units) )
		{
		  ncvars[ncvarid].zaxistype = ZAXIS_PRESSURE;
		  continue;
		}
	      else if ( strcmp(ncvars[ncvarid].units, "level") == 0 || strcmp(ncvars[ncvarid].units, "1") == 0 )
		{
		  if      ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer midpoints") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strncmp(ncvars[ncvarid].longname, "hybrid level at midpoints", 25) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID;
		  else if ( strcmp(ncvars[ncvarid].longname, "hybrid level at layer interfaces") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else if ( strncmp(ncvars[ncvarid].longname, "hybrid level at interfaces", 26) == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_HYBRID_HALF;
		  else if ( strcmp(ncvars[ncvarid].units, "level") == 0 )
		    ncvars[ncvarid].zaxistype = ZAXIS_GENERIC;
		  continue;
		}
	      else if ( is_DBL_axis(ncvars[ncvarid].longname) )
		{
                  ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_LAND;
		  continue;
		}
	      else if ( is_height_units(ncvars[ncvarid].units) )
		{
		  if ( is_depth_axis(ncvars[ncvarid].stdname, ncvars[ncvarid].longname) )
		    ncvars[ncvarid].zaxistype = ZAXIS_DEPTH_BELOW_SEA;
		  else if ( is_height_axis(ncvars[ncvarid].stdname, ncvars[ncvarid].longname) )
		    ncvars[ncvarid].zaxistype = ZAXIS_HEIGHT;
		  continue;
		}
            }

	  /* not needed anymore for rotated grids */
	  if ( ncvars[ncvarid].islon == false && ncvars[ncvarid].longname[0] != 0 &&
               ncvars[ncvarid].islat == false && ncvars[ncvarid].longname[1] != 0 )
	    {
	      if ( memcmp(ncvars[ncvarid].longname+1, "ongitude", 8) == 0 )
		{
		  ncvars[ncvarid].islon = true;
		  continue;
		}
	      else if ( memcmp(ncvars[ncvarid].longname+1, "atitude", 7) == 0 )
		{
		  ncvars[ncvarid].islat = true;
		  continue;
		}
	    }
	}
    }
}

#if defined (PROJECTION_TEST)
static
void copy_numeric_projatts(int gridID, int ncvarID, int ncfileID)
{
  int nvatts;
  size_t attlen;
  char attname[CDI_MAX_NAME];
  nc_type xtype;

  cdf_inq_varnatts(ncfileID, ncvarID, &nvatts);

  for ( int iatt = 0; iatt < nvatts; iatt++ )
    {
      cdf_inq_attname(ncfileID, ncvarID, iatt, attname);
      cdf_inq_atttype(ncfileID, ncvarID, attname, &xtype);
      cdf_inq_attlen(ncfileID, ncvarID, attname, &attlen);

      //  printf("%s %d\n", attname, (int)attlen);
    }

}
#endif

static
void grid_set_chunktype(grid_t *grid, ncvar_t *ncvar)
{
  if ( ncvar->chunked )
    {
      int ndims = ncvar->ndims;

      if ( grid->type == GRID_UNSTRUCTURED )
        {
          if ( ncvar->chunks[ndims-1] == grid->size )
            ncvar->chunktype = CHUNK_GRID;
          else
            ncvar->chunktype = CHUNK_AUTO;
        }
      else
        {
          if ( grid->x.size > 1 && grid->y.size > 1 && ndims > 1 &&
               grid->x.size == ncvar->chunks[ndims-1] &&
               grid->y.size == ncvar->chunks[ndims-2] )
            ncvar->chunktype = CHUNK_GRID;
          else if ( grid->x.size > 1 && grid->x.size == ncvar->chunks[ndims-1] )
            ncvar->chunktype = CHUNK_LINES;
          else
            ncvar->chunktype = CHUNK_AUTO;
        }
    }
}

/* define all input grids */
static
void cdf_load_vals(size_t size, int ndims, int varid, ncvar_t *ncvar, double **gridvals, struct xyValGet *valsGet,
                   int ntdims, size_t *start, size_t *count)
{
  if ( CDI_netcdf_lazy_grid_load )
    {
      *valsGet = (struct xyValGet){
        .scalefactor = ncvar->scalefactor,
        .addoffset = ncvar->addoffset,
        .start = { start[0], start[1], start[2] },
        .count = { count[0], count[1], count[2] },
        .size = size,
        .datasetNCId = ncvar->ncid,
        .varNCId = varid,
        .ndims = (short)ndims,
      };
      *gridvals = cdfPendingLoad;
    }
  else
    {
      *gridvals = (double*) Malloc(size*sizeof(double));
      if ( ntdims == 1 )
        cdf_get_vara_double(ncvar->ncid, varid, start, count, *gridvals);
      else
        cdf_get_var_double(ncvar->ncid, varid, *gridvals);
      cdf_scale_add(size, *gridvals, ncvar->addoffset, ncvar->scalefactor);
    }
}

static
void cdf_load_bounds(size_t size, ncvar_t *ncvar, double **gridbounds, struct cdfLazyGridIds *cellBoundsGet)
{
  if ( CDI_netcdf_lazy_grid_load )
    {
      cellBoundsGet->datasetNCId = ncvar->ncid;
      cellBoundsGet->varNCId  = ncvar->bounds;
      *gridbounds = cdfPendingLoad;
    }
  else
    {
      *gridbounds = (double*) Malloc(size*sizeof(double));
      cdf_get_var_double(ncvar->ncid, ncvar->bounds, *gridbounds);
    }
}

static
void cdf_load_cellarea(size_t size, ncvar_t *ncvar, double **gridarea, struct cdfLazyGridIds *cellAreaGet)
{
  if ( CDI_netcdf_lazy_grid_load )
    {
      cellAreaGet->datasetNCId = ncvar->ncid;
      cellAreaGet->varNCId = ncvar->cellarea;
      *gridarea = cdfPendingLoad;
    }
  else
    {
      *gridarea = (double*) Malloc(size*sizeof(double));
      cdf_get_var_double(ncvar->ncid, ncvar->cellarea, *gridarea);
    }
}

static
void cdf_define_all_grids(stream_t *streamptr, int vlistID, ncdim_t *ncdims, int nvars, ncvar_t *ncvars,
                          int timedimid, unsigned char *uuidOfHGrid, char *gridfile, int number_of_grid_used)
{
  bool ltwarn = true;
  struct cdfLazyGrid *restrict lazyGrid = NULL, *restrict lazyProj = NULL;
#define grid (&lazyGrid->base)
#define proj (&lazyProj->base)

  for ( int ncvarid = 0; ncvarid < nvars; ++ncvarid )
    {
      if ( ncvars[ncvarid].isvar && ncvars[ncvarid].gridID == UNDEFID )
	{
          int xdimids[2] = {-1,-1}, ydimids[2] = {-1,-1};
	  int xdimid = -1, ydimid = -1;
          int vdimid = -1;
	  bool islon = false, islat = false;
	  int nxdims = 0, nydims = 0;
          size_t size = 0;
          size_t xsize = 0, ysize = 0;
          struct addIffNewRes projAdded = { .Id = CDI_UNDEFID, .isNew = 0 },
            gridAdded  = { .Id = CDI_UNDEFID, .isNew = 0 };

	  int ndims = ncvars[ncvarid].ndims;
	  for ( int i = 0; i < ndims; i++ )
	    {
	      if ( ncvars[ncvarid].dimtype[i] == X_AXIS && nxdims < 2 )
		{
		  xdimids[nxdims] = ncvars[ncvarid].dimids[i];
		  nxdims++;
		}
	      else if ( ncvars[ncvarid].dimtype[i] == Y_AXIS && nydims < 2 )
		{
		  ydimids[nydims] = ncvars[ncvarid].dimids[i];
		  nydims++;
		}
	    }

	  if ( nxdims == 2 )
	    {
	      xdimid = xdimids[1];
	      ydimid = xdimids[0];
	    }
	  else if ( nydims == 2 )
	    {
	      xdimid = ydimids[1];
	      ydimid = ydimids[0];
	    }
	  else
	    {
	      xdimid = xdimids[0];
	      ydimid = ydimids[0];
	    }

	  int xvarid = ncvars[ncvarid].xvarid != UNDEFID
	    ? ncvars[ncvarid].xvarid
            : (xdimid != UNDEFID ? ncdims[xdimid].ncvarid : -1);
          int yvarid = ncvars[ncvarid].yvarid != UNDEFID
            ? ncvars[ncvarid].yvarid
            : (ydimid != UNDEFID ? ncdims[ydimid].ncvarid : -1);

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

	  if ( xdimid != UNDEFID ) xsize = ncdims[xdimid].len;
	  if ( ydimid != UNDEFID ) ysize = ncdims[ydimid].len;

	  if ( ydimid == UNDEFID && yvarid != UNDEFID )
	    {
	      if ( ncvars[yvarid].ndims == 1 )
		{
		  ydimid = ncvars[yvarid].dimids[0];
		  ysize  = ncdims[ydimid].len;
		}
	    }

	  if ( ncvars[ncvarid].gridtype == UNDEFID || ncvars[ncvarid].gridtype == GRID_GENERIC )
	    if ( xdimid != UNDEFID && xdimid == ydimid && nydims == 0 ) ncvars[ncvarid].gridtype = GRID_UNSTRUCTURED;

          if ( CDI_netcdf_lazy_grid_load )
            {
              cdfLazyGridRenew(&lazyGrid, ncvars[ncvarid].gridtype);
              cdfLazyGridRenew(&lazyProj, GRID_PROJECTION);
            }
          else
            {
              cdfBaseGridRenew(&lazyGrid, ncvars[ncvarid].gridtype);
              cdfBaseGridRenew(&lazyProj, GRID_PROJECTION);
            }

	  grid->prec  = DATATYPE_FLT64;
	  grid->trunc = ncvars[ncvarid].truncation;

	  if ( ncvars[ncvarid].gridtype == GRID_TRAJECTORY )
	    {
	      if ( ncvars[ncvarid].xvarid == UNDEFID )
		Error("Longitude coordinate undefined for %s!", ncvars[ncvarid].name);
	      if ( ncvars[ncvarid].yvarid == UNDEFID )
		Error("Latitude coordinate undefined for %s!", ncvars[ncvarid].name);
	    }
	  else
	    {
	      size_t start[3], count[3];
	      int ntdims = 0;

	      if ( xvarid != UNDEFID && yvarid != UNDEFID )
		{
                  int ndims = ncvars[xvarid].ndims;
		  if ( ndims != ncvars[yvarid].ndims )
		    {
		      Warning("Inconsistent grid structure for variable %s!", ncvars[ncvarid].name);
		      ncvars[ncvarid].xvarid = xvarid = UNDEFID;
		      ncvars[ncvarid].yvarid = yvarid = UNDEFID;
		    }
		  if ( ndims > 1 )
		    {
		      if ( ndims <= 3 )
                        {
                          if ( ncvars[xvarid].dimids[0] == timedimid && ncvars[yvarid].dimids[0] == timedimid )
                            {
                              if ( ltwarn )
                                Warning("Time varying grids unsupported, using grid at time step 1!");
                              ntdims = 1;
                              ltwarn = false;
                              start[0] = start[1] = start[2] = 0;
                              count[0] = 1; count[1] = ysize; count[ndims-1] = xsize;
                            }
			}
		      else
			{
			  Warning("Unsupported grid structure for variable %s (grid dims > 2)!", ncvars[ncvarid].name);
                          ncvars[ncvarid].xvarid = xvarid = UNDEFID;
                          ncvars[ncvarid].yvarid = yvarid = UNDEFID;
			}
		    }
		}

              if ( xvarid != UNDEFID )
                {
                  if ( (ncvars[xvarid].ndims - ntdims) > 2 )
                    {
                      Warning("Coordinate variable %s has to many dimensions (%d), skipped!", ncvars[xvarid].name, ncvars[xvarid].ndims);
                      //ncvars[ncvarid].xvarid = UNDEFID;
                      xvarid = UNDEFID;
                    }
                }

              if ( yvarid != UNDEFID )
                {
                  if ( (ncvars[yvarid].ndims - ntdims) > 2 )
                    {
                      Warning("Coordinate variable %s has to many dimensions (%d), skipped!", ncvars[yvarid].name, ncvars[yvarid].ndims);
                      //ncvars[ncvarid].yvarid = UNDEFID;
                      yvarid = UNDEFID;
                    }
                }

              if ( xvarid != UNDEFID )
		{
                  bool skipvar = true;
		  islon = ncvars[xvarid].islon;
		  ndims = ncvars[xvarid].ndims;
		  if ( (ndims - ntdims) == 2 )
		    {
		      ncvars[ncvarid].gridtype = GRID_CURVILINEAR;
		      size = xsize*ysize;
		      /* Check size of 2 dimensional coordinate variables */
                      int dimid = ncvars[xvarid].dimids[ndims-2];
                      size_t dimsize1 = ncdims[dimid].len;
                      dimid = ncvars[xvarid].dimids[ndims-1];
                      size_t dimsize2 = ncdims[dimid].len;
                      skipvar = dimsize1*dimsize2 != size;
		    }
		  else if ( (ndims - ntdims) == 1 )
		    {
		      size = xsize;
		      /* Check size of 1 dimensional coordinate variables */
                      int dimid = ncvars[xvarid].dimids[ndims-1];
                      size_t dimsize = ncdims[dimid].len;
                      skipvar = dimsize != size;
		    }
		  else if ( ndims == 0 && xsize == 0 )
		    {
                      size = xsize = 1;
                      skipvar = false;
		    }

                  if ( skipvar )
                    {
                      Warning("Unsupported array structure, skipped variable %s!", ncvars[ncvarid].name);
                      ncvars[ncvarid].isvar = -1;
                      continue;
                    }

		  if ( ncvars[xvarid].xtype == NC_FLOAT ) grid->prec = DATATYPE_FLT32;

                  cdf_load_vals(size, ndims, xvarid, &ncvars[xvarid], &grid->x.vals, &lazyGrid->xValsGet,
                                ntdims, start, count);

		  strcpy(grid->x.name, ncvars[xvarid].name);
		  strcpy(grid->x.longname, ncvars[xvarid].longname);
		  strcpy(grid->x.units, ncvars[xvarid].units);
		}

	      if ( yvarid != UNDEFID )
		{
                  bool skipvar = true;
		  islat = ncvars[yvarid].islat;
		  ndims = ncvars[yvarid].ndims;
		  if ( (ndims - ntdims) == 2 )
		    {
		      ncvars[ncvarid].gridtype = GRID_CURVILINEAR;
		      size = xsize*ysize;
		      /* Check size of 2 dimensional coordinate variables */
                      int dimid = ncvars[yvarid].dimids[ndims-2];
                      size_t dimsize1 = ncdims[dimid].len;
                      dimid = ncvars[yvarid].dimids[ndims-1];
                      size_t dimsize2 = ncdims[dimid].len;
                      skipvar = dimsize1*dimsize2 != size;
		    }
		  else if ( (ndims - ntdims) == 1 )
		    {
		      if ( (int) ysize == 0 ) size = xsize;
		      else                    size = ysize;

                      int dimid = ncvars[yvarid].dimids[ndims-1];
                      size_t dimsize = ncdims[dimid].len;
                      skipvar = dimsize != size;
		    }
		  else if ( ndims == 0 && ysize == 0 )
		    {
                      size = ysize = 1;
                      skipvar = false;
		    }

                  if ( skipvar )
                    {
                      Warning("Unsupported array structure, skipped variable %s!", ncvars[ncvarid].name);
                      ncvars[ncvarid].isvar = -1;
                      continue;
                    }

		  if ( ncvars[yvarid].xtype == NC_FLOAT ) grid->prec = DATATYPE_FLT32;

                  cdf_load_vals(size, ndims, yvarid, &ncvars[yvarid], &grid->y.vals, &lazyGrid->yValsGet,
                                ntdims, start, count);

                  strcpy(grid->y.name, ncvars[yvarid].name);
                  strcpy(grid->y.longname, ncvars[yvarid].longname);
                  strcpy(grid->y.units, ncvars[yvarid].units);
		}

	      if      ( (int) ysize == 0 ) size = xsize;
	      else if ( (int) xsize == 0 ) size = ysize;
	      else if ( ncvars[ncvarid].gridtype == GRID_UNSTRUCTURED ) size = xsize;
	      else                         size = xsize*ysize;
	    }

	  if ( ncvars[ncvarid].gridtype == UNDEFID ||
	       ncvars[ncvarid].gridtype == GRID_GENERIC )
	    {
	      if ( islat && (islon || xsize == 0) )
		{
                  double yinc = 0;
                  if ( islon && (int) ysize > 1 )
                    {
                      yinc = fabs(grid->y.vals[0] - grid->y.vals[1]);
                      for ( size_t i = 2; i < ysize; i++ )
                        if ( (fabs(grid->y.vals[i-1] - grid->y.vals[i]) - yinc) > (yinc/1000) )
                          {
                            yinc = 0;
                            break;
                          }
                    }
		  if ( isGaussGrid(ysize, yinc, grid->y.vals) )
                    {
                      ncvars[ncvarid].gridtype = GRID_GAUSSIAN;
                      grid->np = (int)(ysize/2);
                    }
                  else
		    ncvars[ncvarid].gridtype = GRID_LONLAT;
		}
	      else if ( islon && !islat && ysize == 0 )
		{
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
	    case GRID_UNSTRUCTURED:
	    case GRID_CURVILINEAR:
	      {
		grid->size  = (int)size;
		grid->x.size = (int)xsize;
		grid->y.size = (int)ysize;
		if ( xvarid != UNDEFID )
		  {
		    grid->xdef  = 1;
		    if ( ncvars[xvarid].bounds != UNDEFID )
		      {
			int nbdims = ncvars[ncvars[xvarid].bounds].ndims;
			if ( nbdims == 2 || nbdims == 3 )
			  {
                            vdimid = ncvars[ncvars[xvarid].bounds].dimids[nbdims-1];
			    size_t nvertex = ncdims[vdimid].len;
			    grid->nvertex = (int)nvertex;
                            cdf_load_bounds(size*grid->nvertex, &ncvars[xvarid], &grid->x.bounds, &lazyGrid->xBoundsGet);
			  }
		      }
		  }
		if ( yvarid != UNDEFID )
		  {
		    grid->ydef  = 1;
		    if ( ncvars[yvarid].bounds != UNDEFID )
		      {
			int nbdims = ncvars[ncvars[yvarid].bounds].ndims;
			if ( nbdims == 2 || nbdims == 3 )
			  {
                            if ( vdimid == -1 )
                              {
                                vdimid = ncvars[ncvars[xvarid].bounds].dimids[nbdims-1];
                                size_t nvertex = ncdims[vdimid].len;
                                grid->nvertex = (int)nvertex;
                              }
                            cdf_load_bounds(size*grid->nvertex, &ncvars[yvarid], &grid->y.bounds, &lazyGrid->yBoundsGet);
			  }
		      }
		  }

		if ( ncvars[ncvarid].cellarea != UNDEFID )
                  cdf_load_cellarea(size, &ncvars[ncvarid], &grid->area, &lazyGrid->cellAreaGet);

		break;
	      }
	    case GRID_SPECTRAL:
	      {
		grid->size = (int)size;
		grid->lcomplex = 1;
		break;
	      }
	    case GRID_FOURIER:
	      {
		grid->size = (int)size;
		break;
	      }
	    case GRID_TRAJECTORY:
	      {
		grid->size = 1;
		break;
	      }
	    }

          if ( grid->type != ncvars[ncvarid].gridtype )
            {
              int gridtype = ncvars[ncvarid].gridtype;
              grid->type = gridtype;
              cdiGridTypeInit(grid, gridtype, grid->size);
            }

	  if ( grid->size == 0 )
	    {
	      if ( (ncvars[ncvarid].ndims == 1 && ncvars[ncvarid].dimtype[0] == T_AXIS) ||
		   (ncvars[ncvarid].ndims == 1 && ncvars[ncvarid].dimtype[0] == Z_AXIS) ||
		   (ncvars[ncvarid].ndims == 2 && ncvars[ncvarid].dimtype[0] == T_AXIS && ncvars[ncvarid].dimtype[1] == Z_AXIS) )
		{
		  grid->type  = GRID_GENERIC;
		  grid->size  = 1;
		  grid->x.size = 0;
		  grid->y.size = 0;
		}
	      else
		{
		  Warning("Variable %s has an unsupported grid, skipped!", ncvars[ncvarid].name);
		  ncvars[ncvarid].isvar = -1;
		  continue;
		}
	    }

	  if ( number_of_grid_used != UNDEFID && (grid->type == UNDEFID || grid->type == GRID_GENERIC) )
            grid->type   = GRID_UNSTRUCTURED;

	  if ( number_of_grid_used != UNDEFID && grid->type == GRID_UNSTRUCTURED )
            grid->number = number_of_grid_used;

	  if ( ncvars[ncvarid].gmapid >= 0 && ncvars[ncvarid].gridtype != GRID_CURVILINEAR )
	    {
              int nvatts;
	      cdf_inq_varnatts(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, &nvatts);

	      for ( int iatt = 0; iatt < nvatts; iatt++ )
		{
                  size_t attlen;
                  char attname[CDI_MAX_NAME];
		  cdf_inq_attname(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, iatt, attname);
		  cdf_inq_attlen(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, &attlen);

		  if ( strcmp(attname, "grid_mapping_name") == 0 )
		    {
                      enum {
                        attstringlen = 8192,
                      };
                      char attstring[attstringlen];

		      cdfGetAttText(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, attstringlen, attstring);
		      strtolower(attstring);

		      if ( strcmp(attstring, "rotated_latitude_longitude") == 0 )
			grid->isRotated = TRUE;
		      else if ( strcmp(attstring, "sinusoidal") == 0 )
			grid->type = GRID_SINUSOIDAL;
		      else if ( strcmp(attstring, "lambert_azimuthal_equal_area") == 0 )
			grid->type = GRID_LAEA;
		      else if ( strcmp(attstring, "lambert_conformal_conic") == 0 )
			grid->type = GRID_LCC2;
		      else if ( strcmp(attstring, "lambert_cylindrical_equal_area") == 0 )
			{
			  proj->type = GRID_PROJECTION;
			  proj->name = strdup(attstring);
			}
		    }
		  else if ( strcmp(attname, "earth_radius") == 0 )
		    {
                      double datt;
		      cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &datt);
		      grid->laea.a = datt;
		      grid->lcc2.a = datt;
		    }
		  else if ( strcmp(attname, "longitude_of_projection_origin") == 0 )
		    {
		      cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &grid->laea.lon_0);
		    }
		  else if ( strcmp(attname, "longitude_of_central_meridian") == 0 )
		    {
		      cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &grid->lcc2.lon_0);
		    }
		  else if ( strcmp(attname, "latitude_of_projection_origin") == 0 )
		    {
                      double datt;
		      cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &datt);
		      grid->laea.lat_0 = datt;
		      grid->lcc2.lat_0 = datt;
		    }
		  else if ( strcmp(attname, "standard_parallel") == 0 )
		    {
		      if ( attlen == 1 )
			{
                          double datt;
			  cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &datt);
			  grid->lcc2.lat_1 = datt;
			  grid->lcc2.lat_2 = datt;
			}
		      else
			{
			  double datt2[2];
			  cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 2, datt2);
			  grid->lcc2.lat_1 = datt2[0];
			  grid->lcc2.lat_2 = datt2[1];
			}
		    }
		  else if ( strcmp(attname, "grid_north_pole_latitude") == 0 )
		    {
		      cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &grid->ypole);
		    }
		  else if ( strcmp(attname, "grid_north_pole_longitude") == 0 )
		    {
		      cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &grid->xpole);
		    }
		  else if ( strcmp(attname, "north_pole_grid_longitude") == 0 )
		    {
		      cdfGetAttDouble(ncvars[ncvarid].ncid, ncvars[ncvarid].gmapid, attname, 1, &grid->angle);
		    }
		}
	    }

          if ( grid->type == GRID_UNSTRUCTURED )
            {
              int zdimid = UNDEFID;
              int xdimidx = -1, ydimidx = -1;

              for ( int i = 0; i < ndims; i++ )
                {
                  if      ( ncvars[ncvarid].dimtype[i] == X_AXIS ) xdimidx = i;
                  else if ( ncvars[ncvarid].dimtype[i] == Y_AXIS ) ydimidx = i;
                  else if ( ncvars[ncvarid].dimtype[i] == Z_AXIS ) zdimid = ncvars[ncvarid].dimids[i];
                }

              if ( xdimid != UNDEFID && ydimid != UNDEFID && zdimid == UNDEFID )
                {
                  if ( grid->x.size > grid->y.size && grid->y.size < 1000 )
                    {
                      ncvars[ncvarid].dimtype[ydimidx] = Z_AXIS;
                      ydimid = UNDEFID;
                      grid->size  = grid->x.size;
                      grid->y.size = 0;
                    }
                  else if ( grid->y.size > grid->x.size && grid->x.size < 1000 )
                    {
                      ncvars[ncvarid].dimtype[xdimidx] = Z_AXIS;
                      xdimid = ydimid;
                      ydimid = UNDEFID;
                      grid->size  = grid->y.size;
                      grid->x.size = grid->y.size;
                      grid->y.size = 0;
                    }
                }

              if ( grid->size != grid->x.size )
                {
                  Warning("Unsupported array structure, skipped variable %s!", ncvars[ncvarid].name);
                  ncvars[ncvarid].isvar = -1;
                  continue;
                }

              if ( ncvars[ncvarid].position > 0 ) grid->position = ncvars[ncvarid].position;
              if ( uuidOfHGrid[0] != 0 ) memcpy(grid->uuid, uuidOfHGrid, 16);
            }

#if defined (PROJECTION_TEST)
	  if ( proj->type == GRID_PROJECTION )
	    {
	      if ( grid->type == GRID_GENERIC )
		{
		  grid->type = GRID_CURVILINEAR;
		}

	      if ( grid->type == GRID_CURVILINEAR )
		{
                  proj->size  = grid->size;
                  proj->x.size = grid->x.size;
                  proj->y.size = grid->y.size;
		}

	      //  grid->proj = gridGenerate(proj);
	    }
#endif

	  if ( CDI_Debug )
	    {
	      Message("grid: type = %d, size = %d, nx = %d, ny %d",
		      grid->type, grid->size, grid->x.size, grid->y.size);
	      Message("proj: type = %d, size = %d, nx = %d, ny %d",
		      proj->type, proj->size, proj->x.size, proj->y.size);
	    }

#if defined (PROJECTION_TEST)
	  if ( proj->type == GRID_PROJECTION )
	    {
              projAdded = cdiVlistAddGridIfNew(vlistID, proj, 1);
              ncvars[ncvarid].gridID = projAdded.Id;
	      copy_numeric_projatts(ncvars[ncvarid].gridID, ncvars[ncvarid].gmapid, ncvars[ncvarid].ncid);
	    }
	  else
#endif
            {
              gridAdded = cdiVlistAddGridIfNew(vlistID, grid, 1);
              ncvars[ncvarid].gridID = gridAdded.Id;
            }

          if ( grid->type == GRID_UNSTRUCTURED )
            {
              if ( gridfile[0] != 0 ) gridDefReference(ncvars[ncvarid].gridID, gridfile);
            }

          if ( ncvars[ncvarid].chunked ) grid_set_chunktype(grid, &ncvars[ncvarid]);

	  int gridindex = vlistGridIndex(vlistID, ncvars[ncvarid].gridID);
	  streamptr->xdimID[gridindex] = xdimid;
	  streamptr->ydimID[gridindex] = ydimid;
          if ( xdimid == -1 && ydimid == -1 && grid->size == 1 )
            gridDefHasDims(ncvars[ncvarid].gridID, FALSE);

          if ( xdimid != -1 )
            cdiGridDefKeyStr(ncvars[ncvarid].gridID, CDI_KEY_XDIMNAME, (int)(strlen(ncdims[xdimid].name)+1), ncdims[xdimid].name);
          if ( ydimid != -1 )
            cdiGridDefKeyStr(ncvars[ncvarid].gridID, CDI_KEY_YDIMNAME, (int)(strlen(ncdims[ydimid].name)+1), ncdims[ydimid].name);
          if ( vdimid != -1 )
            cdiGridDefKeyStr(ncvars[ncvarid].gridID, CDI_KEY_VDIMNAME, (int)(strlen(ncdims[vdimid].name)+1), ncdims[vdimid].name);

	  if ( CDI_Debug )
	    Message("gridID %d %d %s", ncvars[ncvarid].gridID, ncvarid, ncvars[ncvarid].name);

	  for ( int ncvarid2 = ncvarid+1; ncvarid2 < nvars; ncvarid2++ )
	    if ( ncvars[ncvarid2].isvar == TRUE && ncvars[ncvarid2].gridID == UNDEFID )
	      {
		int xdimid2 = UNDEFID, ydimid2 = UNDEFID, zdimid2 = UNDEFID;
                int xdimidx = -1, ydimidx = -1;
		int ndims2 = ncvars[ncvarid2].ndims;

		for ( int i = 0; i < ndims2; i++ )
		  {
		    if ( ncvars[ncvarid2].dimtype[i] == X_AXIS )
		      { xdimid2 = ncvars[ncvarid2].dimids[i]; xdimidx = i; }
		    else if ( ncvars[ncvarid2].dimtype[i] == Y_AXIS )
		      { ydimid2 = ncvars[ncvarid2].dimids[i]; ydimidx = i; }
		    else if ( ncvars[ncvarid2].dimtype[i] == Z_AXIS )
		      { zdimid2 = ncvars[ncvarid2].dimids[i]; }
		  }

                if ( ncvars[ncvarid2].gridtype == UNDEFID && grid->type == GRID_UNSTRUCTURED )
                  {
                    if ( xdimid == xdimid2 && ydimid2 != UNDEFID && zdimid2 == UNDEFID )
                      {
                        ncvars[ncvarid2].dimtype[ydimidx] = Z_AXIS;
                        ydimid2 = UNDEFID;
                      }

                    if ( xdimid == ydimid2 && xdimid2 != UNDEFID && zdimid2 == UNDEFID )
                      {
                        ncvars[ncvarid2].dimtype[xdimidx] = Z_AXIS;
                        xdimid2 = ydimid2;
                        ydimid2 = UNDEFID;
                      }
                  }

                if ( xdimid == xdimid2 &&
		    (ydimid == ydimid2 || (xdimid == ydimid && ydimid2 == UNDEFID)) )
		  {
		    bool same_grid = ncvars[ncvarid].xvarid == ncvars[ncvarid2].xvarid
                      && ncvars[ncvarid].yvarid == ncvars[ncvarid2].yvarid
                      && ncvars[ncvarid].position == ncvars[ncvarid2].position;
                    /*
		    if ( xvarid != -1 && ncvars[ncvarid2].xvarid != UNDEFID &&
			 xvarid != ncvars[ncvarid2].xvarid ) same_grid = false;

		    if ( yvarid != -1 && ncvars[ncvarid2].yvarid != UNDEFID &&
			 yvarid != ncvars[ncvarid2].yvarid ) same_grid = false;
                    */

		    if ( same_grid )
		      {
			if ( CDI_Debug )
			  Message("Same gridID %d %d %s", ncvars[ncvarid].gridID, ncvarid2, ncvars[ncvarid2].name);
			ncvars[ncvarid2].gridID = ncvars[ncvarid].gridID;
			ncvars[ncvarid2].chunktype = ncvars[ncvarid].chunktype;
		      }
		  }
	      }

          if ( gridAdded.isNew ) lazyGrid = NULL;
          if ( projAdded.isNew ) lazyProj = NULL;
	}
    }

  if ( lazyGrid )
    {
      if ( CDI_netcdf_lazy_grid_load ) cdfLazyGridDestroy(lazyGrid);
      grid_free(grid);
      Free(grid);
    }

  if ( lazyProj )
    {
      if ( CDI_netcdf_lazy_grid_load ) cdfLazyGridDestroy(lazyProj);
      grid_free(proj);
      Free(proj);
    }
#undef proj
#undef grid
}

/* define all input zaxes */
static
void cdf_define_all_zaxes(stream_t *streamptr, int vlistID, ncdim_t *ncdims, int nvars, ncvar_t *ncvars,
                          size_t vctsize_echam, double *vct_echam, unsigned char *uuidOfVGrid)
{
  int ncvarid, ncvarid2;
  int i, ilev;
  int zaxisindex;
  int nbdims, nvertex, nlevel;
  int psvarid = -1;
  char *pname, *plongname, *punits;
  size_t vctsize = vctsize_echam;
  double *vct = vct_echam;

  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE && ncvars[ncvarid].zaxisID == UNDEFID )
	{
          int is_scalar = FALSE;
	  int with_bounds = FALSE;
	  int zdimid = UNDEFID;
	  int zvarid = UNDEFID;
	  int zsize = 1;
	  double *lbounds = NULL;
	  double *ubounds = NULL;

          int positive = 0;
	  int ndims = ncvars[ncvarid].ndims;

          if ( ncvars[ncvarid].zvarid != -1 && ncvars[ncvars[ncvarid].zvarid].ndims == 0 )
            {
              zvarid = ncvars[ncvarid].zvarid;
              is_scalar = TRUE;
            }
          else
            {
              for ( i = 0; i < ndims; i++ )
                {
                  if ( ncvars[ncvarid].dimtype[i] == Z_AXIS )
                    zdimid = ncvars[ncvarid].dimids[i];
                }

              if ( zdimid != UNDEFID )
                {
                  zvarid = ncdims[zdimid].ncvarid;
                  zsize  = (int)ncdims[zdimid].len;
                }
            }

	  if ( CDI_Debug ) Message("nlevs = %d", zsize);

	  double *zvar = (double *) Malloc((size_t)zsize * sizeof (double));

	  int zaxisType = UNDEFID;
	  if ( zvarid != UNDEFID ) zaxisType = ncvars[zvarid].zaxistype;
	  if ( zaxisType == UNDEFID )  zaxisType = ZAXIS_GENERIC;

	  int zprec = DATATYPE_FLT64;

	  if ( zvarid != UNDEFID )
	    {
	      positive  = ncvars[zvarid].positive;
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
              psvarid = -1;
              if ( zaxisType == ZAXIS_HYBRID && ncvars[zvarid].vct )
                {
                  vct = ncvars[zvarid].vct;
                  vctsize = ncvars[zvarid].vctsize;

                  if ( ncvars[zvarid].psvarid != -1 ) psvarid = ncvars[zvarid].psvarid;
                }

	      cdf_get_var_double(ncvars[zvarid].ncid, zvarid, zvar);

	      if ( ncvars[zvarid].bounds != UNDEFID )
		{
		  nbdims = ncvars[ncvars[zvarid].bounds].ndims;
		  if ( nbdims == 2 )
		    {
		      nlevel  = (int)ncdims[ncvars[ncvars[zvarid].bounds].dimids[0]].len;
		      nvertex = (int)ncdims[ncvars[ncvars[zvarid].bounds].dimids[1]].len;
		      if ( nlevel == zsize && nvertex == 2 )
			{
			  with_bounds = TRUE;
			  lbounds = (double *) Malloc((size_t)nlevel*sizeof(double));
			  ubounds = (double *) Malloc((size_t)nlevel*sizeof(double));
			  double zbounds[2*nlevel];
			  cdf_get_var_double(ncvars[zvarid].ncid, ncvars[zvarid].bounds, zbounds);
			  for ( i = 0; i < nlevel; ++i )
			    {
			      lbounds[i] = zbounds[i*2];
			      ubounds[i] = zbounds[i*2+1];
			    }
			}
		    }
		}
	    }
	  else
	    {
	      pname     = NULL;
	      plongname = NULL;
	      punits    = NULL;

	      if ( zsize == 1 )
		{
                  if ( ncvars[ncvarid].zaxistype != UNDEFID )
                    zaxisType = ncvars[ncvarid].zaxistype;
                  else
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
		  for ( ilev = 0; ilev < zsize; ilev++ ) zvar[ilev] = ilev + 1;
		}
	    }

      	  ncvars[ncvarid].zaxisID = varDefZaxis(vlistID, zaxisType, (int) zsize, zvar, with_bounds, lbounds, ubounds,
						(int)vctsize, vct, pname, plongname, punits, zprec, 1, 0);

	  if ( uuidOfVGrid[0] != 0 )
            {
              // printf("uuidOfVGrid: defined\n");
              zaxisDefUUID(ncvars[ncvarid].zaxisID, uuidOfVGrid);
            }

          if ( zaxisType == ZAXIS_HYBRID && psvarid != -1 ) zaxisDefPsName(ncvars[ncvarid].zaxisID, ncvars[psvarid].name);

          if ( positive > 0 ) zaxisDefPositive(ncvars[ncvarid].zaxisID, positive);
          if ( is_scalar ) zaxisDefScalar(ncvars[ncvarid].zaxisID);

          if ( zdimid != -1 )
            cdiZaxisDefString(ncvars[ncvarid].zaxisID, CDI_ZAXIS_DIMNAME, (int)(strlen(ncdims[zdimid].name)+1), ncdims[zdimid].name);
          /*
          if ( vdimid != -1 )
            cdiZaxisDefString(ncvars[ncvarid].zaxisID, CDI_ZAXIS_VDIMNAME, strlen(ncdims[vdimid].name)+1, ncdims[vdimid].name);
          */
	  Free(zvar);
	  Free(lbounds);
	  Free(ubounds);

	  zaxisindex = vlistZaxisIndex(vlistID, ncvars[ncvarid].zaxisID);
	  streamptr->zaxisID[zaxisindex]  = zdimid;

	  if ( CDI_Debug )
	    Message("zaxisID %d %d %s", ncvars[ncvarid].zaxisID, ncvarid, ncvars[ncvarid].name);

	  for ( ncvarid2 = ncvarid+1; ncvarid2 < nvars; ncvarid2++ )
	    if ( ncvars[ncvarid2].isvar == TRUE && ncvars[ncvarid2].zaxisID == UNDEFID /*&& ncvars[ncvarid2].zaxistype == UNDEFID*/ )
	      {
                int zvarid2 = UNDEFID;
                if ( ncvars[ncvarid2].zvarid != UNDEFID && ncvars[ncvars[ncvarid2].zvarid].ndims == 0 )
                  zvarid2 = ncvars[ncvarid2].zvarid;

		int zdimid2 = UNDEFID;
		ndims = ncvars[ncvarid2].ndims;
		for ( i = 0; i < ndims; i++ )
		  {
		    if ( ncvars[ncvarid2].dimtype[i] == Z_AXIS )
		      zdimid2 = ncvars[ncvarid2].dimids[i];
		  }

		if ( zdimid == zdimid2 /* && zvarid == zvarid2 */)
		  {
                    if ( (zdimid != UNDEFID && ncvars[ncvarid2].zaxistype == UNDEFID) ||
                         (zdimid == UNDEFID && zvarid != UNDEFID && zvarid == zvarid2) ||
                         (zdimid == UNDEFID && zaxisType == ncvars[ncvarid2].zaxistype) ||
                         (zdimid == UNDEFID && zvarid2 == UNDEFID && ncvars[ncvarid2].zaxistype == UNDEFID) )
                      {
                        if ( CDI_Debug )
                          Message("zaxisID %d %d %s", ncvars[ncvarid].zaxisID, ncvarid2, ncvars[ncvarid2].name);
                        ncvars[ncvarid2].zaxisID = ncvars[ncvarid].zaxisID;
                      }
                  }
	      }
	}
    }
}

struct varinfo
{
  int      ncvarid;
  const char *name;
};

static
int cmpvarname(const void *s1, const void *s2)
{
  const struct varinfo *x = (const struct varinfo *)s1,
    *y = (const struct varinfo *)s2;
  return strcmp(x->name, y->name);
}

/* define all input data variables */
static
void define_all_vars(stream_t *streamptr, int vlistID, int instID, int modelID, int *varids, int nvars, int num_ncvars, ncvar_t *ncvars)
{
  if ( CDI_Debug )
    for(int i = 0; i < nvars; i++) Message("varids[%d] = %d", i, varids[i]);

  if ( streamptr->sortname )
    {
      struct varinfo *varInfo
        = (struct varinfo *) Malloc((size_t)nvars * sizeof (struct varinfo));

      for ( int varID = 0; varID < nvars; varID++ )
	{
	  int ncvarid = varids[varID];
	  varInfo[varID].ncvarid = ncvarid;
	  varInfo[varID].name = ncvars[ncvarid].name;
	}
      qsort(varInfo, (size_t)nvars, sizeof(varInfo[0]), cmpvarname);
      for ( int varID = 0; varID < nvars; varID++ )
	{
	  varids[varID] = varInfo[varID].ncvarid;
	}
      Free(varInfo);
      if ( CDI_Debug )
        {
          for(int i = 0; i < nvars; i++) Message("sorted varids[%d] = %d", i, varids[i]);
        }
    }

  for ( int varID1 = 0; varID1 < nvars; varID1++ )
    {
      int ncvarid = varids[varID1];
      int gridID  = ncvars[ncvarid].gridID;
      int zaxisID = ncvars[ncvarid].zaxisID;

      stream_new_var(streamptr, gridID, zaxisID, CDI_UNDEFID);
      int varID = vlistDefVar(vlistID, gridID, zaxisID, ncvars[ncvarid].tsteptype);

#if  defined  (HAVE_NETCDF4)
      if ( ncvars[ncvarid].deflate )
	vlistDefVarCompType(vlistID, varID, COMPRESS_ZIP);

      if ( ncvars[ncvarid].chunked && ncvars[ncvarid].chunktype != UNDEFID )
        vlistDefVarChunkType(vlistID, varID, ncvars[ncvarid].chunktype);
#endif

      streamptr->vars[varID1].defmiss = 0;
      streamptr->vars[varID1].ncvarid = ncvarid;

      vlistDefVarName(vlistID, varID, ncvars[ncvarid].name);
      if ( ncvars[ncvarid].param != UNDEFID ) vlistDefVarParam(vlistID, varID, ncvars[ncvarid].param);
      if ( ncvars[ncvarid].code != UNDEFID )  vlistDefVarCode(vlistID, varID, ncvars[ncvarid].code);
      if ( ncvars[ncvarid].code != UNDEFID )
	{
	  int param = cdiEncodeParam(ncvars[ncvarid].code, ncvars[ncvarid].tabnum, 255);
	  vlistDefVarParam(vlistID, varID, param);
	}
      if ( ncvars[ncvarid].longname[0] )  vlistDefVarLongname(vlistID, varID, ncvars[ncvarid].longname);
      if ( ncvars[ncvarid].stdname[0] )   vlistDefVarStdname(vlistID, varID, ncvars[ncvarid].stdname);
      if ( ncvars[ncvarid].units[0] )     vlistDefVarUnits(vlistID, varID, ncvars[ncvarid].units);

      if ( ncvars[ncvarid].lvalidrange )
        vlistDefVarValidrange(vlistID, varID, ncvars[ncvarid].validrange);

      if ( IS_NOT_EQUAL(ncvars[ncvarid].addoffset, 0) )
	vlistDefVarAddoffset(vlistID, varID, ncvars[ncvarid].addoffset);
      if ( IS_NOT_EQUAL(ncvars[ncvarid].scalefactor, 1) )
	vlistDefVarScalefactor(vlistID, varID, ncvars[ncvarid].scalefactor);

      vlistDefVarDatatype(vlistID, varID, cdfInqDatatype(ncvars[ncvarid].xtype, ncvars[ncvarid].lunsigned));

      vlistDefVarInstitut(vlistID, varID, instID);
      vlistDefVarModel(vlistID, varID, modelID);
      if ( ncvars[ncvarid].tableID != UNDEFID )
	vlistDefVarTable(vlistID, varID, ncvars[ncvarid].tableID);

      if ( ncvars[ncvarid].deffillval == false && ncvars[ncvarid].defmissval )
        {
          ncvars[ncvarid].deffillval = true;
          ncvars[ncvarid].fillval    = ncvars[ncvarid].missval;
        }

      if ( ncvars[ncvarid].deffillval )
        vlistDefVarMissval(vlistID, varID, ncvars[ncvarid].fillval);

      if ( CDI_Debug )
	Message("varID = %d  gridID = %d  zaxisID = %d", varID,
		vlistInqVarGrid(vlistID, varID), vlistInqVarZaxis(vlistID, varID));

      int gridindex = vlistGridIndex(vlistID, gridID);
      int xdimid = streamptr->xdimID[gridindex];
      int ydimid = streamptr->ydimID[gridindex];

      int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
      int zdimid = streamptr->zaxisID[zaxisindex];

      int ndims = ncvars[ncvarid].ndims;
      int iodim = 0;
      int ixyz = 0;
      int ipow10[4] = {1, 10, 100, 1000};

      if ( ncvars[ncvarid].tsteptype != TSTEP_CONSTANT ) iodim++;

      if ( gridInqType(gridID) == GRID_UNSTRUCTURED && ndims-iodim <= 2 && ydimid == xdimid )
        {
          if ( xdimid == ncvars[ncvarid].dimids[ndims-1] )
            {
              ixyz = 321;
            }
          else
            {
              ixyz = 213;
            }
        }
      else
        {
          for ( int idim = iodim; idim < ndims; idim++ )
            {
              if      ( xdimid == ncvars[ncvarid].dimids[idim] )
                ixyz += 1*ipow10[ndims-idim-1];
              else if ( ydimid == ncvars[ncvarid].dimids[idim] )
                ixyz += 2*ipow10[ndims-idim-1];
              else if ( zdimid == ncvars[ncvarid].dimids[idim] )
                ixyz += 3*ipow10[ndims-idim-1];
            }
        }

      vlistDefVarXYZ(vlistID, varID, ixyz);
      /*
      printf("ixyz %d\n", ixyz);
      printf("ndims %d\n", ncvars[ncvarid].ndims);
      for ( int i = 0; i < ncvars[ncvarid].ndims; ++i )
        printf("dimids: %d %d\n", i, ncvars[ncvarid].dimids[i]);
      printf("xdimid, ydimid %d %d\n", xdimid, ydimid);
      */
      if ( ncvars[ncvarid].ensdata != NULL )
        {
          vlistDefVarEnsemble( vlistID, varID, ncvars[ncvarid].ensdata->ens_index,
                               ncvars[ncvarid].ensdata->ens_count,
                               ncvars[ncvarid].ensdata->forecast_init_type );
          Free(ncvars[ncvarid].ensdata);
          ncvars[ncvarid].ensdata = NULL;
        }

      if ( ncvars[ncvarid].extra[0] != 0 )
        {
          vlistDefVarExtra(vlistID, varID, ncvars[ncvarid].extra);
        }
    }

  for ( int varID = 0; varID < nvars; varID++ )
    {
      int ncvarid = varids[varID];
      int ncid = ncvars[ncvarid].ncid;

      if ( ncvars[ncvarid].natts )
	{
	  nc_type attrtype;
	  size_t attlen;
	  char attname[CDI_MAX_NAME];
	  const int attstringlen = 8192; char attstring[8192];
	  int nvatts = ncvars[ncvarid].natts;

	  for ( int iatt = 0; iatt < nvatts; ++iatt )
	    {
	      int attnum = ncvars[ncvarid].atts[iatt];
	      cdf_inq_attname(ncid, ncvarid, attnum, attname);
	      cdf_inq_attlen(ncid, ncvarid, attname, &attlen);
	      cdf_inq_atttype(ncid, ncvarid, attname, &attrtype);

	      if ( xtypeIsInt(attrtype) )
		{
		  int attint[attlen];
		  cdfGetAttInt(ncid, ncvarid, attname, (int)attlen, attint);
                  int datatype = (attrtype == NC_SHORT) ? DATATYPE_INT16 :
                                 (attrtype == NC_BYTE)  ? DATATYPE_INT8 :
#if  defined  (HAVE_NETCDF4)
                                 (attrtype == NC_UBYTE)  ? DATATYPE_UINT8 :
                                 (attrtype == NC_USHORT) ? DATATYPE_UINT16 :
                                 (attrtype == NC_UINT) ? DATATYPE_UINT32 :
#endif
                                 DATATYPE_INT32;
                  vlistDefAttInt(vlistID, varID, attname, datatype, (int)attlen, attint);
		}
	      else if ( xtypeIsFloat(attrtype) )
		{
		  double attflt[attlen];
		  cdfGetAttDouble(ncid, ncvarid, attname, (int)attlen, attflt);
                  int datatype = (attrtype == NC_FLOAT) ? DATATYPE_FLT32 : DATATYPE_FLT64;
                  vlistDefAttFlt(vlistID, varID, attname, datatype, (int)attlen, attflt);
		}
	      else if ( xtypeIsText(attrtype) )
		{
		  cdfGetAttText(ncid, ncvarid, attname, attstringlen, attstring);
		  vlistDefAttTxt(vlistID, varID, attname, (int)attlen, attstring);
		}
	      else
		{
		  if ( CDI_Debug ) printf("att: %s.%s = unknown\n", ncvars[ncvarid].name, attname);
		}
	    }

	  if (ncvars[ncvarid].vct) Free(ncvars[ncvarid].vct);
	  if (ncvars[ncvarid].atts) Free(ncvars[ncvarid].atts);
          ncvars[ncvarid].vct = NULL;
          ncvars[ncvarid].atts = NULL;
	}
    }

  /* release mem of not freed attributes */
  for ( int ncvarid = 0; ncvarid < num_ncvars; ncvarid++ )
    if ( ncvars[ncvarid].atts ) Free(ncvars[ncvarid].atts);

  if ( varids ) Free(varids);

  for ( int varID = 0; varID < nvars; varID++ )
    {
      if ( vlistInqVarCode(vlistID, varID) == -varID-1 )
	{
	  const char *pname = vlistInqVarNamePtr(vlistID, varID);
	  size_t len = strlen(pname);
	  if ( len > 3 && isdigit((int) pname[3]) )
	    {
	      if ( memcmp("var", pname, 3) == 0 )
		{
		  vlistDefVarCode(vlistID, varID, atoi(pname+3));
                  // vlistDestroyVarName(vlistID, varID);
		}
	    }
	  else if ( len > 4 && isdigit((int) pname[4]) )
	    {
	      if ( memcmp("code", pname, 4) == 0 )
		{
		  vlistDefVarCode(vlistID, varID, atoi(pname+4));
		  // vlistDestroyVarName(vlistID, varID);
		}
	    }
	  else if ( len > 5 && isdigit((int) pname[5]) )
	    {
	      if ( memcmp("param", pname, 5) == 0 )
		{
		  int pnum = -1, pcat = 255, pdis = 255;
		  sscanf(pname+5, "%d.%d.%d", &pnum, &pcat, &pdis);
		  vlistDefVarParam(vlistID, varID, cdiEncodeParam(pnum, pcat, pdis));
                  // vlistDestroyVarName(vlistID, varID);
		}
	    }
	}
    }

  for ( int varID = 0; varID < nvars; varID++ )
    {
      int varInstID  = vlistInqVarInstitut(vlistID, varID);
      int varModelID = vlistInqVarModel(vlistID, varID);
      int varTableID = vlistInqVarTable(vlistID, varID);
      int code = vlistInqVarCode(vlistID, varID);
      if ( cdiDefaultTableID != UNDEFID )
	{
	  if ( tableInqParNamePtr(cdiDefaultTableID, code) )
	    {
	      vlistDestroyVarName(vlistID, varID);
	      vlistDestroyVarLongname(vlistID, varID);
	      vlistDestroyVarUnits(vlistID, varID);

	      if ( varTableID != UNDEFID )
		{
		  vlistDefVarName(vlistID, varID, tableInqParNamePtr(cdiDefaultTableID, code));
		  if ( tableInqParLongnamePtr(cdiDefaultTableID, code) )
		    vlistDefVarLongname(vlistID, varID, tableInqParLongnamePtr(cdiDefaultTableID, code));
		  if ( tableInqParUnitsPtr(cdiDefaultTableID, code) )
		    vlistDefVarUnits(vlistID, varID, tableInqParUnitsPtr(cdiDefaultTableID, code));
		}
	      else
		{
		  varTableID = cdiDefaultTableID;
		}
	    }

	  if ( cdiDefaultModelID != UNDEFID ) varModelID = cdiDefaultModelID;
	  if ( cdiDefaultInstID  != UNDEFID ) varInstID  = cdiDefaultInstID;
	}
      if ( varInstID  != UNDEFID ) vlistDefVarInstitut(vlistID, varID, varInstID);
      if ( varModelID != UNDEFID ) vlistDefVarModel(vlistID, varID, varModelID);
      if ( varTableID != UNDEFID ) vlistDefVarTable(vlistID, varID, varTableID);
    }
}

static
void scan_global_attributes(int fileID, int vlistID, stream_t *streamptr, int ngatts, int *instID, int *modelID, int *ucla_les, unsigned char *uuidOfHGrid, unsigned char *uuidOfVGrid, char *gridfile, int *number_of_grid_used)
{
  nc_type xtype;
  size_t attlen;
  char attname[CDI_MAX_NAME];
  enum { attstringlen = 65636 };
  char attstring[attstringlen];

  for ( int iatt = 0; iatt < ngatts; iatt++ )
    {
      cdf_inq_attname(fileID, NC_GLOBAL, iatt, attname);
      cdf_inq_atttype(fileID, NC_GLOBAL, attname, &xtype);
      cdf_inq_attlen(fileID, NC_GLOBAL, attname, &attlen);

      if ( xtypeIsText(xtype) )
	{
	  cdfGetAttText(fileID, NC_GLOBAL, attname, attstringlen, attstring);

          size_t attstrlen = strlen(attstring);

	  if ( attlen > 0 && attstring[0] != 0 )
	    {
	      if ( strcmp(attname, "history") == 0 )
		{
		  streamptr->historyID = iatt;
		}
	      else if ( strcmp(attname, "institution") == 0 )
		{
		  *instID = institutInq(0, 0, NULL, attstring);
		  if ( *instID == UNDEFID )
		    *instID = institutDef(0, 0, NULL, attstring);
		}
	      else if ( strcmp(attname, "source") == 0 )
		{
		  *modelID = modelInq(-1, 0, attstring);
		  if ( *modelID == UNDEFID )
		    *modelID = modelDef(-1, 0, attstring);
		}
	      else if ( strcmp(attname, "Source") == 0 )
		{
		  if ( strncmp(attstring, "UCLA-LES", 8) == 0 )
		    *ucla_les = TRUE;
		}
	      /*
	      else if ( strcmp(attname, "Conventions") == 0 )
		{
		}
	      */
	      else if ( strcmp(attname, "CDI") == 0 )
		{
		}
	      else if ( strcmp(attname, "CDO") == 0 )
		{
		}
              /*
	      else if ( strcmp(attname, "forecast_reference_time") == 0 )
		{
                  memcpy(fcreftime, attstring, attstrlen+1);
		}
              */
	      else if ( strcmp(attname, "grid_file_uri") == 0 )
		{
                  memcpy(gridfile, attstring, attstrlen+1);
		}
	      else if ( strcmp(attname, "uuidOfHGrid") == 0 && attstrlen == 36 )
		{
                  attstring[36] = 0;
                  cdiStr2UUID(attstring, uuidOfHGrid);
                  //   printf("uuid: %d %s\n", attlen, attstring);
		}
	      else if ( strcmp(attname, "uuidOfVGrid") == 0 && attstrlen == 36 )
		{
                  attstring[36] = 0;
                  cdiStr2UUID(attstring, uuidOfVGrid);
		}
	      else
		{
                  if ( strcmp(attname, "ICON_grid_file_uri") == 0 && gridfile[0] == 0 )
                    {
                      memcpy(gridfile, attstring, attstrlen+1);
                    }

		  vlistDefAttTxt(vlistID, CDI_GLOBAL, attname, (int)attstrlen, attstring);
		}
	    }
	}
      else if ( xtype == NC_SHORT || xtype == NC_INT )
	{
	  if ( strcmp(attname, "number_of_grid_used") == 0 )
	    {
	      (*number_of_grid_used) = UNDEFID;
	      cdfGetAttInt(fileID, NC_GLOBAL, attname, 1, number_of_grid_used);
	    }
 	  else
            {
              int attint[attlen];
              cdfGetAttInt(fileID, NC_GLOBAL, attname, (int)attlen, attint);
              int datatype = (xtype == NC_SHORT) ? DATATYPE_INT16 : DATATYPE_INT32;
              vlistDefAttInt(vlistID, CDI_GLOBAL, attname, datatype, (int)attlen, attint);
            }
        }
      else if ( xtype == NC_FLOAT || xtype == NC_DOUBLE )
	{
	  double attflt[attlen];
	  cdfGetAttDouble(fileID, NC_GLOBAL, attname, (int)attlen, attflt);
          int datatype = (xtype == NC_FLOAT) ? DATATYPE_FLT32 : DATATYPE_FLT64;
          vlistDefAttFlt(vlistID, CDI_GLOBAL, attname, datatype, (int)attlen, attflt);
	}
    }
}

static
int find_leadtime(int nvars, ncvar_t *ncvars)
{
  int leadtime_id = UNDEFID;

  for ( int ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].stdname[0] )
        {
          if ( strcmp(ncvars[ncvarid].stdname, "forecast_period") == 0 )
            {
              leadtime_id = ncvarid;
              break;
            }
        }
    }

  return leadtime_id;
}

static
void find_time_vars(int nvars, ncvar_t *ncvars, ncdim_t *ncdims, int timedimid, stream_t *streamptr,
                    bool *time_has_units, bool *time_has_bounds, bool *time_climatology)
{
  int ncvarid;

  if ( timedimid == UNDEFID )
    {
      char timeunits[CDI_MAX_NAME];

      for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
        {
          if ( ncvars[ncvarid].ndims == 0 && strcmp(ncvars[ncvarid].name, "time") == 0 )
            {
              if ( ncvars[ncvarid].units[0] )
                {
                  strcpy(timeunits, ncvars[ncvarid].units);
                  strtolower(timeunits);

                  if ( is_time_units(timeunits) )
                    {
                      streamptr->basetime.ncvarid = ncvarid;
                      break;
                    }
                }
            }
        }
    }
  else
    {
      bool ltimevar = false;

      if ( ncdims[timedimid].ncvarid != UNDEFID )
        {
          streamptr->basetime.ncvarid = ncdims[timedimid].ncvarid;
          ltimevar = true;
        }

      for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
        if ( ncvarid != streamptr->basetime.ncvarid &&
             ncvars[ncvarid].ndims == 1 &&
             timedimid == ncvars[ncvarid].dimids[0] &&
             !xtypeIsText(ncvars[ncvarid].xtype) &&
             is_timeaxis_units(ncvars[ncvarid].units) )
          {
            ncvars[ncvarid].isvar = FALSE;

            if ( !ltimevar )
              {
                streamptr->basetime.ncvarid = ncvarid;
                ltimevar = true;
                if ( CDI_Debug )
                  fprintf(stderr, "timevar %s\n", ncvars[ncvarid].name);
              }
            else
              {
                Warning("Found more than one time variable, skipped variable %s!", ncvars[ncvarid].name);
              }
          }

      if ( ltimevar == false ) /* search for WRF time description */
        {
          for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
            if ( ncvarid != streamptr->basetime.ncvarid &&
                 ncvars[ncvarid].ndims == 2 &&
                 timedimid == ncvars[ncvarid].dimids[0] &&
                 xtypeIsText(ncvars[ncvarid].xtype) &&
                 ncdims[ncvars[ncvarid].dimids[1]].len == 19 )
              {
                streamptr->basetime.ncvarid = ncvarid;
                streamptr->basetime.lwrf    = true;
                break;
              }
        }

      /* time varID */
      ncvarid = streamptr->basetime.ncvarid;

      if ( ncvarid == UNDEFID )
        {
          Warning("Time variable >%s< not found!", ncdims[timedimid].name);
        }
    }

  /* time varID */
  ncvarid = streamptr->basetime.ncvarid;

  if ( ncvarid != UNDEFID && streamptr->basetime.lwrf == false )
    {
      if ( ncvars[ncvarid].units[0] != 0 ) *time_has_units = true;

      if ( ncvars[ncvarid].bounds != UNDEFID )
        {
          int nbdims = ncvars[ncvars[ncvarid].bounds].ndims;
          if ( nbdims == 2 )
            {
              int len = (int) ncdims[ncvars[ncvars[ncvarid].bounds].dimids[nbdims-1]].len;
              if ( len == 2 && timedimid == ncvars[ncvars[ncvarid].bounds].dimids[0] )
                {
                  *time_has_bounds = true;
                  streamptr->basetime.ncvarboundsid = ncvars[ncvarid].bounds;
                  if ( ncvars[ncvarid].climatology ) *time_climatology = true;
                }
            }
        }
    }
}

static
void read_vct_echam(int fileID, int nvars, ncvar_t *ncvars, ncdim_t *ncdims, double **vct, size_t *pvctsize)
{
  /* find ECHAM VCT */
  int nvcth_id = UNDEFID, vcta_id = UNDEFID, vctb_id = UNDEFID;

  for ( int ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].ndims == 1 )
        {
          size_t len = strlen(ncvars[ncvarid].name);
          if ( len == 4 && ncvars[ncvarid].name[0] == 'h' && ncvars[ncvarid].name[1] == 'y' )
            {
              if ( ncvars[ncvarid].name[2] == 'a' && ncvars[ncvarid].name[3] == 'i' ) // hyai
                {
                  vcta_id = ncvarid;
                  nvcth_id = ncvars[ncvarid].dimids[0];
                  ncvars[ncvarid].isvar = FALSE;
                }
              else if ( ncvars[ncvarid].name[2] == 'b' && ncvars[ncvarid].name[3] == 'i' ) //hybi
                {
                  vctb_id = ncvarid;
                  nvcth_id = ncvars[ncvarid].dimids[0];
                  ncvars[ncvarid].isvar = FALSE;
                }
              else if ( (ncvars[ncvarid].name[2] == 'a' || ncvars[ncvarid].name[2] == 'b') && ncvars[ncvarid].name[3] == 'm' )
                {
                  ncvars[ncvarid].isvar = FALSE; // hyam or hybm
                }
            }
	}
    }

  /* read VCT */
  if ( nvcth_id != UNDEFID && vcta_id != UNDEFID && vctb_id != UNDEFID )
    {
      size_t vctsize = ncdims[nvcth_id].len;
      vctsize *= 2;
      *vct = (double *) Malloc(vctsize*sizeof(double));
      cdf_get_var_double(fileID, vcta_id, *vct);
      cdf_get_var_double(fileID, vctb_id, *vct+vctsize/2);
      *pvctsize = vctsize;
    }
}


int cdfInqContents(stream_t *streamptr)
{
  int ndims, nvars, ngatts, unlimdimid;
  int ncvarid;
  int ncdimid;
  size_t ntsteps;
  int timedimid = -1;
  int *varids;
  int nvarids;
  bool time_has_units = false;
  bool time_has_bounds = false;
  bool time_climatology = false;
  int leadtime_id = UNDEFID;
  int nvars_data;
  int instID  = UNDEFID;
  int modelID = UNDEFID;
  int taxisID;
  int i;
  int calendar = UNDEFID;
  ncdim_t *ncdims;
  ncvar_t *ncvars = NULL;
  int format = 0;
  int ucla_les = FALSE;
  unsigned char uuidOfHGrid[CDI_UUID_SIZE];
  unsigned char uuidOfVGrid[CDI_UUID_SIZE];
  char gridfile[8912];
  char fcreftime[CDI_MAX_NAME];
  int number_of_grid_used = UNDEFID;

  memset(uuidOfHGrid, 0, CDI_UUID_SIZE);
  memset(uuidOfVGrid, 0, CDI_UUID_SIZE);
  gridfile[0] = 0;
  fcreftime[0] = 0;

  int vlistID = streamptr->vlistID;
  int fileID  = streamptr->fileID;

  if ( CDI_Debug ) Message("streamID = %d, fileID = %d", streamptr->self, fileID);

#if  defined  (HAVE_NETCDF4)
  nc_inq_format(fileID, &format);
#endif

  cdf_inq(fileID, &ndims , &nvars, &ngatts, &unlimdimid);

  if ( CDI_Debug )
    Message("root: ndims %d, nvars %d, ngatts %d", ndims, nvars, ngatts);

  if ( ndims == 0 )
    {
      Warning("No dimensions found!");
      return CDI_EUFSTRUCT;
    }

  /* alloc ncdims */
  ncdims = (ncdim_t *) Malloc((size_t)ndims * sizeof (ncdim_t));
  init_ncdims(ndims, ncdims);

  if ( nvars > 0 )
    {
      /* alloc ncvars */
      ncvars = (ncvar_t *) Malloc((size_t)nvars * sizeof (ncvar_t));
      init_ncvars(nvars, ncvars);

      for ( ncvarid = 0; ncvarid < nvars; ++ncvarid )
        ncvars[ncvarid].ncid = fileID;
    }

#if  defined  (TEST_GROUPS)
#if  defined  (HAVE_NETCDF4)
  if ( format == NC_FORMAT_NETCDF4 )
    {
      int ncid;
      int numgrps;
      int ncids[NC_MAX_VARS];
      char name1[CDI_MAX_NAME];
      int gndims, gnvars, gngatts, gunlimdimid;
      nc_inq_grps(fileID, &numgrps, ncids);
      for ( int i = 0; i < numgrps; ++i )
        {
          ncid = ncids[i];
          nc_inq_grpname(ncid, name1);
          cdf_inq(ncid, &gndims , &gnvars, &gngatts, &gunlimdimid);

          if ( CDI_Debug )
            Message("%s: ndims %d, nvars %d, ngatts %d", name1, gndims, gnvars, gngatts);

          if ( gndims == 0 )
            {
            }
        }
    }
#endif
#endif

  if ( nvars == 0 )
    {
      Warning("No arrays found!");
      return CDI_EUFSTRUCT;
    }

  /* scan global attributes */
  scan_global_attributes(fileID, vlistID, streamptr, ngatts, &instID, &modelID, &ucla_les,
                         uuidOfHGrid, uuidOfVGrid, gridfile, &number_of_grid_used);

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

  if ( CDI_Debug ) Message("Number of timesteps = %d", ntsteps);
  if ( CDI_Debug ) Message("Time dimid = %d", streamptr->basetime.ncdimid);

  /* read ncdims */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      cdf_inq_dimlen(fileID, ncdimid, &ncdims[ncdimid].len);
      cdf_inq_dimname(fileID, ncdimid, ncdims[ncdimid].name);
      if ( timedimid == ncdimid )
	ncdims[ncdimid].dimtype = T_AXIS;
    }

  if ( CDI_Debug ) printNCvars(ncvars, nvars, "cdf_scan_var_attributes");

  /* scan attributes of all variables */
  cdf_scan_var_attributes(nvars, ncvars, ncdims, timedimid, modelID, format);


  if ( CDI_Debug ) printNCvars(ncvars, nvars, "find coordinate vars");

  /* find coordinate vars */
  for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
    {
      for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
	{
	  if ( ncvars[ncvarid].ndims == 1 )
	    {
	      if ( timedimid != UNDEFID && timedimid == ncvars[ncvarid].dimids[0] )
		{
		  if ( ncvars[ncvarid].isvar != FALSE ) cdfSetVar(ncvars, ncvarid, TRUE);
		}
	      else
		{
                  //  if ( ncvars[ncvarid].isvar != TRUE ) cdfSetVar(ncvars, ncvarid, FALSE);
		}
	      // if ( ncvars[ncvarid].isvar != TRUE ) cdfSetVar(ncvars, ncvarid, FALSE);

	      if ( ncdimid == ncvars[ncvarid].dimids[0] && ncdims[ncdimid].ncvarid == UNDEFID )
		if ( strcmp(ncvars[ncvarid].name, ncdims[ncdimid].name) == 0 )
		  {
		    ncdims[ncdimid].ncvarid = ncvarid;
		    ncvars[ncvarid].isvar = FALSE;
		  }
	    }
	}
    }

  /* find time vars */
  find_time_vars(nvars, ncvars, ncdims, timedimid, streamptr, &time_has_units, &time_has_bounds, &time_climatology);

  leadtime_id = find_leadtime(nvars, ncvars);
  if ( leadtime_id != UNDEFID ) ncvars[leadtime_id].isvar = FALSE;

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

      //if ( ncvars[ncvarid].isvar == -1 && ncvars[ncvarid].ndims > 1 )
      if ( ncvars[ncvarid].isvar == -1 && ncvars[ncvarid].ndims >= 1 )
	cdfSetVar(ncvars, ncvarid, TRUE);

      if ( ncvars[ncvarid].isvar == -1 )
	{
	  ncvars[ncvarid].isvar = 0;
	  Warning("Variable %s has an unknown type, skipped!", ncvars[ncvarid].name);
	  continue;
	}

      if ( ncvars[ncvarid].ndims > 4 )
	{
	  ncvars[ncvarid].isvar = 0;
	  Warning("%d dimensional variables are not supported, skipped variable %s!",
		ncvars[ncvarid].ndims, ncvars[ncvarid].name);
	  continue;
	}

      if ( ncvars[ncvarid].ndims == 4 && timedimid == UNDEFID )
	{
	  ncvars[ncvarid].isvar = 0;
	  Warning("%d dimensional variables without time dimension are not supported, skipped variable %s!",
		ncvars[ncvarid].ndims, ncvars[ncvarid].name);
	  continue;
	}

      if ( xtypeIsText(ncvars[ncvarid].xtype) )
	{
	  ncvars[ncvarid].isvar = 0;
	  continue;
	}

      if ( cdfInqDatatype(ncvars[ncvarid].xtype, ncvars[ncvarid].lunsigned) == -1 )
	{
	  ncvars[ncvarid].isvar = 0;
	  Warning("Variable %s has an unsupported data type, skipped!", ncvars[ncvarid].name);
	  continue;
	}

      if ( timedimid != UNDEFID && ntsteps == 0 && ncvars[ncvarid].ndims > 0 )
	{
	  if ( timedimid == ncvars[ncvarid].dimids[0] )
	    {
	      ncvars[ncvarid].isvar = 0;
	      Warning("Number of time steps undefined, skipped variable %s!", ncvars[ncvarid].name);
	      continue;
	    }
	}
    }

  /* verify coordinate vars - first scan (dimname == varname) */
  verify_coordinate_vars_1(fileID, ndims, ncdims, ncvars, timedimid);

  /* verify coordinate vars - second scan (all other variables) */
  verify_coordinate_vars_2(nvars, ncvars);

  if ( CDI_Debug ) printNCvars(ncvars, nvars, "verify_coordinate_vars");

  if ( ucla_les == TRUE )
    {
      for ( ncdimid = 0; ncdimid < ndims; ncdimid++ )
	{
	  ncvarid = ncdims[ncdimid].ncvarid;
	  if ( ncvarid != -1 )
	    {
	      if ( ncdims[ncdimid].dimtype == UNDEFID && ncvars[ncvarid].units[0] == 'm' )
		{
		  if      ( ncvars[ncvarid].name[0] == 'x' ) ncdims[ncdimid].dimtype = X_AXIS;
		  else if ( ncvars[ncvarid].name[0] == 'y' ) ncdims[ncdimid].dimtype = Y_AXIS;
		  else if ( ncvars[ncvarid].name[0] == 'z' ) ncdims[ncdimid].dimtype = Z_AXIS;
		}
	    }
	}
    }
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

  /* Set coordinate varids (att: associate)  */
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    {
      if ( ncvars[ncvarid].isvar == TRUE && ncvars[ncvarid].ncoordvars )
	{
	  /* ndims = ncvars[ncvarid].ndims; */
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

  /* set dim type */
  setDimType(nvars, ncvars, ncdims);

  /* read ECHAM VCT if present */
  size_t vctsize = 0;
  double *vct = NULL;
  read_vct_echam(fileID, nvars, ncvars, ncdims, &vct, &vctsize);


  if ( CDI_Debug ) printNCvars(ncvars, nvars, "define_all_grids");

  /* define all grids */
  cdf_define_all_grids(streamptr, vlistID, ncdims, nvars, ncvars, timedimid, uuidOfHGrid, gridfile, number_of_grid_used);


  /* define all zaxes */
  cdf_define_all_zaxes(streamptr, vlistID, ncdims, nvars, ncvars, vctsize, vct, uuidOfVGrid);
  if ( vct ) Free(vct);


  /* select vars */
  varids = (int *) Malloc((size_t)nvars * sizeof (int));
  nvarids = 0;
  for ( ncvarid = 0; ncvarid < nvars; ncvarid++ )
    if ( ncvars[ncvarid].isvar == TRUE ) varids[nvarids++] = ncvarid;

  nvars_data = nvarids;

  if ( CDI_Debug ) Message("time varid = %d", streamptr->basetime.ncvarid);
  if ( CDI_Debug ) Message("ntsteps = %d", ntsteps);
  if ( CDI_Debug ) Message("nvars_data = %d", nvars_data);


  if ( nvars_data == 0 )
    {
      streamptr->ntsteps = 0;
      return CDI_EUFSTRUCT;
    }

  if ( ntsteps == 0 && streamptr->basetime.ncdimid == UNDEFID && streamptr->basetime.ncvarid != UNDEFID )
    ntsteps = 1;

  streamptr->ntsteps = (long)ntsteps;

  /* define all data variables */
  define_all_vars(streamptr, vlistID, instID, modelID, varids, nvars_data, nvars, ncvars);


  cdiCreateTimesteps(streamptr);

  /* time varID */
  int nctimevarid = streamptr->basetime.ncvarid;

  if ( time_has_units )
    {
      taxis_t *taxis = &streamptr->tsteps[0].taxis;

      if ( setBaseTime(ncvars[nctimevarid].units, taxis) == 1 )
        {
          nctimevarid = UNDEFID;
          streamptr->basetime.ncvarid = UNDEFID;
        }

      if ( leadtime_id != UNDEFID && taxis->type == TAXIS_RELATIVE )
        {
          streamptr->basetime.leadtimeid = leadtime_id;
          taxis->type = TAXIS_FORECAST;

          int timeunit = -1;
          if ( ncvars[leadtime_id].units[0] != 0 ) timeunit = scanTimeUnit(ncvars[leadtime_id].units);
          if ( timeunit == -1 ) timeunit = taxis->unit;
          taxis->fc_unit = timeunit;

          setForecastTime(fcreftime, taxis);
        }
    }

  if ( time_has_bounds )
    {
      streamptr->tsteps[0].taxis.has_bounds = true;
      if ( time_climatology ) streamptr->tsteps[0].taxis.climatology = true;
    }

  if ( nctimevarid != UNDEFID )
    {
      taxis_t *taxis = &streamptr->tsteps[0].taxis;
      ptaxisDefName(taxis, ncvars[nctimevarid].name);
      if ( ncvars[nctimevarid].longname[0] )
        ptaxisDefLongname(taxis, ncvars[nctimevarid].longname);
    }

  if ( nctimevarid != UNDEFID )
    if ( ncvars[nctimevarid].calendar == TRUE )
      {
        enum {attstringlen = 8192};
        char attstring[attstringlen];

	cdfGetAttText(fileID, nctimevarid, "calendar", attstringlen, attstring);
	strtolower(attstring);

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
	  Warning("calendar >%s< unsupported!", attstring);
      }

  if ( streamptr->tsteps[0].taxis.type == TAXIS_FORECAST )
    {
      taxisID = taxisCreate(TAXIS_FORECAST);
    }
  else if ( streamptr->tsteps[0].taxis.type == TAXIS_RELATIVE )
    {
      taxisID = taxisCreate(TAXIS_RELATIVE);
    }
  else
    {
      taxisID = taxisCreate(TAXIS_ABSOLUTE);
      if ( !time_has_units )
	{
	  taxisDefTunit(taxisID, TUNIT_DAY);
	  streamptr->tsteps[0].taxis.unit = TUNIT_DAY;
	}
    }


  if ( calendar == UNDEFID && streamptr->tsteps[0].taxis.type != TAXIS_ABSOLUTE )
    {
      calendar = CALENDAR_STANDARD;
    }

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5)
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wstrict-overflow"
#endif
  if ( calendar != UNDEFID )
    {
      taxis_t *taxis = &streamptr->tsteps[0].taxis;
      taxis->calendar = calendar;
      taxisDefCalendar(taxisID, calendar);
    }
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5)
#pragma GCC diagnostic pop
#endif

  vlistDefTaxis(vlistID, taxisID);

  streamptr->curTsID = 0;
  streamptr->rtsteps = 1;

  (void) cdfInqTimestep(streamptr, 0);

  cdfCreateRecords(streamptr, 0);

  /* free ncdims */
  Free(ncdims);

  /* free ncvars */
  Free(ncvars);

  return 0;
}

static
void wrf_read_timestep(int fileID, int nctimevarid, int tsID, taxis_t *taxis)
{
  size_t start[2], count[2];
  char stvalue[32];
  start[0] = (size_t) tsID; start[1] = 0;
  count[0] = 1; count[1] = 19;
  stvalue[0] = 0;
  cdf_get_vara_text(fileID, nctimevarid, start, count, stvalue);
  stvalue[19] = 0;
  {
    int year = 1, month = 1, day = 1 , hour = 0, minute = 0, second = 0;
    if ( strlen(stvalue) == 19 )
      sscanf(stvalue, "%d-%d-%d_%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    taxis->vdate = cdiEncodeDate(year, month, day);
    taxis->vtime = cdiEncodeTime(hour, minute, second);
    taxis->type = TAXIS_ABSOLUTE;
  }
}

static
double get_timevalue(int fileID, int nctimevarid, int tsID, timecache_t *tcache)
{
  double timevalue = 0;

  if ( tcache )
    {
      if ( tcache->size == 0 || (tsID < tcache->startid || tsID > (tcache->startid+tcache->size-1)) )
        {
          int maxvals = MAX_TIMECACHE_SIZE;
          tcache->startid = (tsID/MAX_TIMECACHE_SIZE)*MAX_TIMECACHE_SIZE;
          if ( (tcache->startid + maxvals) > tcache->maxvals ) maxvals = (tcache->maxvals)%MAX_TIMECACHE_SIZE;
          tcache->size = maxvals;
          size_t index = (size_t) tcache->startid;
          // fprintf(stderr, "fill time cache: %d %d %d %d %d\n", tcache->maxvals, tsID, tcache->startid, tcache->startid+maxvals-1, maxvals);
          for ( int ival = 0; ival < maxvals; ++ival )
            {
              cdf_get_var1_double(fileID, nctimevarid, &index, &timevalue);
              if ( timevalue >= NC_FILL_DOUBLE || timevalue < -NC_FILL_DOUBLE ) timevalue = 0;
              tcache->cache[ival] = timevalue;
              index++;
            }
        }

      timevalue = tcache->cache[tsID%MAX_TIMECACHE_SIZE];
    }
  else
    {
      size_t index = (size_t) tsID;
      cdf_get_var1_double(fileID, nctimevarid, &index, &timevalue);
      if ( timevalue >= NC_FILL_DOUBLE || timevalue < -NC_FILL_DOUBLE ) timevalue = 0;
    }

  return timevalue;
}


int cdfInqTimestep(stream_t * streamptr, int tsID)
{
  if ( CDI_Debug ) Message("streamID = %d  tsID = %d", streamptr->self, tsID);

  if ( tsID < 0 ) Error("unexpected tsID = %d", tsID);

  if ( tsID < streamptr->ntsteps && streamptr->ntsteps > 0 )
    {
      cdfCreateRecords(streamptr, tsID);

      taxis_t *taxis = &streamptr->tsteps[tsID].taxis;
      if ( tsID > 0 )
	ptaxisCopy(taxis, &streamptr->tsteps[0].taxis);

      double timevalue = tsID;

      int nctimevarid = streamptr->basetime.ncvarid;
      if ( nctimevarid != UNDEFID )
	{
	  int fileID = streamptr->fileID;
	  size_t index  = (size_t)tsID;

	  if ( streamptr->basetime.lwrf )
	    {
              wrf_read_timestep(fileID, nctimevarid, tsID, taxis);
	    }
	  else
	    {
#if defined (USE_TIMECACHE)
              if ( streamptr->basetime.timevar_cache == NULL )
                {
                  streamptr->basetime.timevar_cache = (timecache_t *) Malloc(MAX_TIMECACHE_SIZE*sizeof(timecache_t));
                  streamptr->basetime.timevar_cache->size = 0;
                  streamptr->basetime.timevar_cache->maxvals = streamptr->ntsteps;
                }
#endif
              timevalue = get_timevalue(fileID, nctimevarid, tsID, streamptr->basetime.timevar_cache);
	      cdiDecodeTimeval(timevalue, taxis, &taxis->vdate, &taxis->vtime);
	    }

	  int nctimeboundsid = streamptr->basetime.ncvarboundsid;
	  if ( nctimeboundsid != UNDEFID )
	    {
	      size_t start[2], count[2];
              start[0] = index; count[0] = 1; start[1] = 0; count[1] = 1;
	      cdf_get_vara_double(fileID, nctimeboundsid, start, count, &timevalue);
              if ( timevalue >= NC_FILL_DOUBLE || timevalue < -NC_FILL_DOUBLE ) timevalue = 0;

	      cdiDecodeTimeval(timevalue, taxis, &taxis->vdate_lb, &taxis->vtime_lb);

              start[0] = index; count[0] = 1; start[1] = 1; count[1] = 1;
	      cdf_get_vara_double(fileID, nctimeboundsid, start, count, &timevalue);
              if ( timevalue >= NC_FILL_DOUBLE || timevalue < -NC_FILL_DOUBLE ) timevalue = 0;

	      cdiDecodeTimeval(timevalue, taxis, &taxis->vdate_ub, &taxis->vtime_ub);
	    }

          int leadtimeid = streamptr->basetime.leadtimeid;
          if ( leadtimeid != UNDEFID )
            {
              timevalue = get_timevalue(fileID, leadtimeid, tsID, NULL);
              cdiSetForecastPeriod(timevalue, taxis);
            }
	}
    }

  streamptr->curTsID = tsID;
  long nrecs = streamptr->tsteps[tsID].nrecs;

  return (int) nrecs;
}


int cdfInqHistorySize(stream_t *streamptr)
{
  size_t size = 0;
  int ncid = streamptr->fileID;
  if ( streamptr->historyID != UNDEFID )
    cdf_inq_attlen(ncid, NC_GLOBAL, "history", &size);

  return (int) size;
}


void cdfInqHistoryString(stream_t *streamptr, char *history)
{
  int ncid = streamptr->fileID;
  if ( streamptr->historyID != UNDEFID )
    {
      nc_type atttype;
      cdf_inq_atttype(ncid, NC_GLOBAL, "history", &atttype);

      if ( atttype == NC_CHAR )
        {
          cdf_get_att_text(ncid, NC_GLOBAL, "history", history);
        }
#if  defined  (HAVE_NETCDF4)
      else if ( atttype == NC_STRING )
        {
          // ToDo
          Warning("History attribute with type NC_STRING unsupported!");
        }
#endif
    }
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
