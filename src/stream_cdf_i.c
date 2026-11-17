#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cdi.h"
#include "cdi_utils.h"
#include "cdi_query.h"
#include "cdi_limits.h"

#ifdef HAVE_LIBNETCDF

#include <ctype.h>
#include <limits.h>

#include "dmemory.h"
#include "cdi_int.h"
#include "cdi_utils.h"
#include "cdi_uuid.h"
#include "stream_cdf.h"
#include "cdf_int.h"
#include "varscan.h"
#include "vlist.h"
#include "cdf_util.h"
#include "cdf_lazy_grid.h"
#include "cdf_filter.h"

// On Windows, define strcasecmp manually
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

typedef struct
{
  char buffer[128];
  char *mem;
  char *data;
  size_t size;
} String;

void
string_init(String *str)
{
  str->mem = NULL;
  str->data = str->buffer;
  str->size = sizeof(str->buffer);
}

void
string_reserve(String *str, size_t requiredSize)
{
  if (requiredSize > str->size)
  {
    str->mem = realloc(str->mem, requiredSize);
    str->data = str->mem;
    str->size = requiredSize;
  }
}

void
string_free(String *str)
{
  if (str->mem != NULL)
  {
    free(str->mem);
    string_init(str);
  }
}

enum VarStatus
{
  UndefVar = -1,
  CoordVar = 0,
  DataVar = 1,
};

enum AxisType
{
  X_AXIS = 1,
  Y_AXIS = 2,
  Z_AXIS = 3,
  E_AXIS = 4,
  T_AXIS = 5,
};

static int axisTypeChar[] = { '?', 'X', 'Y', 'Z', 'E', 'T' };

typedef struct
{
  size_t len;   // Dimension size
  int dimid;    // NetCDF dim ID
  int ncvarid;  // NetCDF var ID
  int dimtype;  // AxisType
  char name[CDI_MAX_NAME];
} ncdim_t;

#define MAX_COORDVARS 5
#define MAX_AUXVARS 4
#define MAX_DIMS_CDF 8

typedef struct
{
  int cdiVarID;
  int ncid;
  int varStatus;
  int timetype;
  int param;
  int code;
  int tabnum;
  int bounds;
  int gridID;
  int zaxisID;
  int gridtype;
  int zaxistype;
  int xdim;
  int ydim;
  int zdim;
  int xvarid;
  int yvarid;
  int rpvarid;
  int zvarid;
  int tvarid;
  int ivarid;
  int psvarid;
  int p0varid;
  int ncoordvars;
  int cvarids[MAX_COORDVARS];
  int coordvarids[MAX_COORDVARS];
  int auxvarids[MAX_AUXVARS];
  int nauxvars;
  int cellarea;
  int tableID;
  int truncation;
  int position;
  int numLPE;
  int xtype;
  int gmapid;
  int positive;
  int ndims;
  int dimids[MAX_DIMS_CDF];    // Netcdf dimension IDs
  int dimtypes[MAX_DIMS_CDF];  // AxisType
  size_t chunks[MAX_DIMS_CDF];
  size_t xstart;
  size_t xcount;
  size_t zstart;
  size_t zcount;
  size_t tstart;
  size_t tcount;
  int chunkType;
  int chunkSize;
  size_t chunkCacheSize;
  size_t chunkCacheNelems;
  float chunkCachePreemption;
  size_t gridSize;
  size_t xSize;
  size_t ySize;
  size_t zSize;
  int nattsNC;
  int natts;
  int *atts;
  size_t vctsize;
  double *vct;
  double missval;
  double fillval;
  double addoffset;
  double scalefactor;
  double validrange[2];
  int typeOfEnsembleForecast;
  int numberOfForecastsInEnsemble;
  int perturbationNumber;
  int unitsLen;
  char name[CDI_MAX_NAME];
  char longname[CDI_MAX_NAME];
  char stdname[CDI_MAX_NAME];
  char units[CDI_MAX_NAME];
  char filterSpec[CDI_MAX_NAME];
  bool ignoreVar;
  bool isLonLatMapping;
  bool isHealpixMapping;
  bool isCubeSphere;
  bool isCharAxis;
  bool isIndexAxis;
  bool isXaxis;
  bool isYaxis;
  bool isZaxis;
  bool isTaxis;
  bool isLon;
  bool isLat;
  bool isClimatology;
  bool hasCalendar;
  bool hasFormulaterms;
  bool printWarning;
  bool missvalDefined;
  bool fillvalDefined;
  bool isChunked;
  bool hasFilter;
  bool hasDeflate;
  bool hasSzip;
  bool isUnsigned;
  bool validrangeDefined;
} ncvar_t;

typedef struct
{
  char gridfile[8192];
  unsigned char uuid[CDI_UUID_SIZE];
  int number_of_grid_used;
  int timedimid;
} GridInfo;

static CdiDateTime
scan_time_string(const char *ptu)
{
  int year = 0, month = 0, day = 0;
  int hour = 0, minute = 0;
  double fseconds = 0.0;
  char ch = ' ';
  if (*ptu) sscanf(ptu, "%d-%d-%d%c%d:%d:%lf", &year, &month, &day, &ch, &hour, &minute, &fseconds);

  if (day > 999 && year < 32)
  {
    int tmp = year;
    year = day;
    day = tmp;
  }

  int second = (int) fseconds;
  double aseconds;
  double ms = modf(fseconds, &aseconds) * 1000;
  assert((int) aseconds == second);
  CdiDateTime datetime;
  datetime.date.year = year;
  datetime.date.month = (short) month;
  datetime.date.day = (short) day;
  datetime.time.hour = (short) hour;
  datetime.time.minute = (short) minute;
  datetime.time.second = (short) second;
  datetime.time.ms = (short) ms;

  return datetime;
}

static int
scan_time_units(const char *unitstr)
{
  int timeunit = get_time_units(strlen(unitstr), unitstr);
  if (timeunit == -1) Warning("Unsupported TIMEUNIT: %s!", unitstr);
  return timeunit;
}

static int
set_base_time(const char *timeUnitsStr, taxis_t *taxis)
{
  int taxisType = TAXIS_ABSOLUTE;

  size_t len = strlen(timeUnitsStr);
  while (isspace(*timeUnitsStr) && len)
  {
    timeUnitsStr++;
    len--;
  }
  char tmp[32], *tu = tmp;
  if (len + 1 > sizeof(tmp)) { tu = (char *) Malloc(len + 1); }

  for (size_t i = 0; i < len; i++) { tu[i] = (char) tolower((int) timeUnitsStr[i]); }
  tu[len] = 0;

  int timeUnits = get_time_units(len, tu);
  if (timeUnits == -1)
  {
    Warning("Unsupported TIMEUNIT: %s!", timeUnitsStr);
    if (tu != tmp) Free(tu);
    return 1;
  }

  size_t pos = 0;
  while (pos < len && !isspace(tu[pos])) ++pos;
  if (tu[pos])
  {
    while (isspace(tu[pos])) ++pos;

    if (strStartsWith(tu + pos, "since")) taxisType = TAXIS_RELATIVE;

    while (pos < len && !isspace(tu[pos])) ++pos;
    if (tu[pos])
    {
      while (isspace(tu[pos])) ++pos;

      if (taxisType == TAXIS_ABSOLUTE)
      {
        if (timeUnits == TUNIT_DAY)
        {
          if (!strStartsWith(tu + pos, "%y%m%d.%f"))
          {
            Warning("Unsupported format %s for TIMEUNIT day!", tu + pos);
            timeUnits = -1;
          }
        }
        else if (timeUnits == TUNIT_MONTH)
        {
          if (!strStartsWith(tu + pos, "%y%m.%f"))
          {
            Warning("Unsupported format %s for TIMEUNIT month!", tu + pos);
            timeUnits = -1;
          }
        }
        else if (timeUnits == TUNIT_YEAR)
        {
          if (!strStartsWith(tu + pos, "%y.%f"))
          {
            Warning("Unsupported format %s for TIMEUNIT year!", tu + pos);
            timeUnits = -1;
          }
        }
        else
        {
          Warning("Unsupported format for time units: %s!", tu);
        }
      }
      else if (taxisType == TAXIS_RELATIVE)
      {
        taxis->rDateTime = scan_time_string(tu + pos);
        if (CDI_Debug)
          Message("rdate = %d  rtime = %d", (int) cdiDate_get(taxis->rDateTime.date), cdiTime_get(taxis->rDateTime.time));
      }
    }
  }

  taxis->type = taxisType;
  taxis->unitsValue = timeUnits;

  if (tu != tmp) Free(tu);

  if (CDI_Debug) Message("taxisType = %d  timeUnits = %d", taxisType, timeUnits);

  return 0;
}

bool
xtypeIsText(int xtype)
{
  return (xtype == NC_CHAR) || (xtype == NC_STRING);
}

static inline bool
xtypeIsFloat(nc_type xtype)
{
  return (xtype == NC_FLOAT || xtype == NC_DOUBLE);
}

static inline bool
xtypeIsInt(nc_type xtype)
{
  return (xtype == NC_SHORT || xtype == NC_INT || xtype == NC_BYTE || xtype == NC_USHORT || xtype == NC_UINT || xtype == NC_UBYTE);
}

static inline bool
xtypeIsInt64(nc_type xtype)
{
  return (xtype == NC_INT64 || xtype == NC_UINT64);
}

static int
cdfInqDatatype(stream_t *streamP, int xtype, bool isUnsigned)
{
  if (xtype == NC_BYTE && isUnsigned) { xtype = NC_UBYTE; }

  // clang-format off
  switch (xtype)
  {
    case NC_BYTE:   return CDI_DATATYPE_INT8;
    case NC_CHAR:   return CDI_DATATYPE_UINT8;
    case NC_SHORT:  return CDI_DATATYPE_INT16;
    case NC_INT:    return CDI_DATATYPE_INT32;
    case NC_FLOAT:  return CDI_DATATYPE_FLT32;
    case NC_DOUBLE: return CDI_DATATYPE_FLT64;
    case NC_UBYTE:  return CDI_DATATYPE_UINT8;
    case NC_USHORT: return CDI_DATATYPE_UINT16;
    case NC_UINT:   return CDI_DATATYPE_UINT32;
    case NC_INT64:  return CDI_DATATYPE_FLT64;
    case NC_UINT64: return CDI_DATATYPE_FLT64;
  }
  // clang-format on

  CdfInfo *cdfInfo = &(streamP->cdfInfo);
  if (xtype != cdfInfo->complexFloatId && xtype != cdfInfo->complexDoubleId)
  {
    bool isUserDefinedType = false;
#ifdef NC_FIRSTUSERTYPEID
    isUserDefinedType = (xtype >= NC_FIRSTUSERTYPEID);
#endif
    if (isUserDefinedType)
    {
      int fileID = streamP->groupID;
      size_t nfields = 0, compoundsize = 0;
      int status = nc_inq_compound(fileID, xtype, NULL, &compoundsize, &nfields);
      if (status == NC_NOERR && nfields == 2 && (compoundsize == 8 || compoundsize == 16))
      {
        nc_type field_type1 = -1, field_type2 = -1;
        int field_dims1 = 0, field_dims2 = 0;
        nc_inq_compound_field(fileID, xtype, 0, NULL, NULL, &field_type1, &field_dims1, NULL);
        nc_inq_compound_field(fileID, xtype, 1, NULL, NULL, &field_type2, &field_dims2, NULL);
        if (field_type1 == field_type2 && field_dims1 == 0 && field_dims2 == 0)
        {
          if (field_type1 == NC_FLOAT)
            cdfInfo->complexFloatId = xtype;
          else if (field_type1 == NC_DOUBLE)
            cdfInfo->complexDoubleId = xtype;
        }
      }
    }
  }

  if (xtype == cdfInfo->complexFloatId) return CDI_DATATYPE_CPX32;
  if (xtype == cdfInfo->complexDoubleId) return CDI_DATATYPE_CPX64;

  return -1;
}

static void
cdfGetAttInt(int fileID, int ncvarid, const char *attname, size_t attlen, int *attint)
{
  *attint = 0;

  nc_type atttype;
  size_t nc_attlen;
  cdf_inq_att(fileID, ncvarid, attname, &atttype, &nc_attlen);

  if (xtypeIsFloat(atttype) || xtypeIsInt(atttype))
  {
    bool needAlloc = (nc_attlen > attlen);
    int *pintatt = needAlloc ? (int *) Malloc(nc_attlen * sizeof(int)) : attint;
    cdf_get_att_int(fileID, ncvarid, attname, pintatt);
    if (needAlloc)
    {
      memcpy(attint, pintatt, attlen * sizeof(int));
      Free(pintatt);
    }
  }
}

static void
cdfGetAttInt64(int fileID, int ncvarid, const char *attname, size_t attlen, int64_t *attint)
{
  *attint = 0;

  nc_type atttype;
  size_t nc_attlen;
  cdf_inq_att(fileID, ncvarid, attname, &atttype, &nc_attlen);

  if (xtypeIsFloat(atttype) || xtypeIsInt(atttype) || xtypeIsInt64(atttype))
  {
    long long attLong = 0;
    long long *pattLong = (attlen > 1) ? (long long *) Malloc(nc_attlen * sizeof(long long)) : &attLong;
    cdf_get_att_longlong(fileID, ncvarid, attname, pattLong);
    for (size_t i = 0; i < attlen; ++i) { attint[i] = pattLong[i]; }
    if (attlen > 1) Free(pattLong);
  }
}

static void
cdfGetAttDouble(int fileID, int ncvarid, const char *attname, size_t attlen, double *attdouble)
{
  *attdouble = 0.0;

  nc_type atttype;
  size_t nc_attlen;
  cdf_inq_att(fileID, ncvarid, attname, &atttype, &nc_attlen);

  if (xtypeIsFloat(atttype) || xtypeIsInt(atttype))
  {
    bool needAlloc = (nc_attlen > attlen);
    double *pdoubleatt = needAlloc ? (double *) Malloc(nc_attlen * sizeof(double)) : attdouble;
    cdf_get_att_double(fileID, ncvarid, attname, pdoubleatt);
    if (needAlloc)
    {
      memcpy(attdouble, pdoubleatt, attlen * sizeof(double));
      Free(pdoubleatt);
    }
  }
}

static bool
cdfCheckAttInt(int fileID, int ncvarid, const char *attname)
{
  nc_type atttype;
  int status_nc = nc_inq_atttype(fileID, ncvarid, attname, &atttype);
  return (status_nc == NC_NOERR && (xtypeIsInt(atttype) || xtypeIsInt64(atttype)));
}

static bool
cdfCheckAttText(int fileID, int ncvarid, const char *attname)
{
  nc_type atttype;
  int status_nc = nc_inq_atttype(fileID, ncvarid, attname, &atttype);
  return (status_nc == NC_NOERR && (atttype == NC_CHAR || atttype == NC_STRING));
}

static void
cdfGetAttString(int fileID, int ncvarid, const char *attname, String *attString)
{
  attString->data[0] = 0;

  nc_type atttype;
  size_t attlen;
  cdf_inq_att(fileID, ncvarid, attname, &atttype, &attlen);

  if (atttype == NC_CHAR)
  {
    string_reserve(attString, attlen + 1);
    cdf_get_att_text(fileID, ncvarid, attname, attString->data);
    attString->data[attlen] = 0;
  }
  else if (atttype == NC_STRING)
  {
    if (attlen == 1)
    {
      char *attbuf = NULL;
      cdf_get_att_string(fileID, ncvarid, attname, &attbuf);
      size_t stringLen = strlen(attbuf);
      string_reserve(attString, stringLen + 1);
      memcpy(attString->data, attbuf, stringLen);
      attString->data[stringLen] = 0;
      nc_free_string(1, &attbuf);
    }
  }
}

void
cdf_scale_add(size_t size, double *restrict data, double addoffset, double scalefactor)
{
  bool haveAddoffset = is_not_equal(addoffset, 0.0);
  bool haveScalefactor = is_not_equal(scalefactor, 1.0);
  if (haveAddoffset && haveScalefactor)
  {
    for (size_t i = 0; i < size; ++i) { data[i] = data[i] * scalefactor + addoffset; }
  }
  else if (haveScalefactor)
  {
    for (size_t i = 0; i < size; ++i) { data[i] *= scalefactor; }
  }
  else if (haveAddoffset)
  {
    for (size_t i = 0; i < size; ++i) { data[i] += addoffset; }
  }
}

static int
cdf_time_dimid(int fileID, int ndims, ncdim_t *ncdims, int nvars, ncvar_t *ncvars)
{
  for (int dimIdx = 0; dimIdx < ndims; ++dimIdx)
  {
    const char *name = ncdims[dimIdx].name;
    if ((name[0] == 't' || name[0] == 'T') && strcasecmp(name, "time") == 0) return ncdims[dimIdx].dimid;
  }

  bool check_dimids[MAX_DIMS_CDF * 4];  // *4, because ndims could be greater than MAX_DIMS_CDF
  for (int i = 0; i < MAX_DIMS_CDF * 4; ++i) { check_dimids[i] = false; }

  String attString;
  string_init(&attString);
  for (int varid = 0; varid < nvars; ++varid)
  {
    ncvar_t *ncvar = &ncvars[varid];
    if (ncvars[varid].ndims == 1)
    {
      int dimid0 = ncvar->dimids[0];
      if (dimid0 == CDI_UNDEFID || check_dimids[dimid0] == true) { continue; }
      if (ncdims[dimid0].ncvarid != CDI_UNDEFID && ncdims[dimid0].ncvarid != varid) { continue; }
      check_dimids[dimid0] = true;

      int attid = -1;
      if (nc_inq_attid(fileID, varid, "units", &attid) == NC_NOERR)
      {
        cdfGetAttString(fileID, varid, "units", &attString);
        if (is_timeaxis_units(attString.data))
        {
          string_free(&attString);
          return ncdims[dimid0].dimid;
        }
      }
    }
  }

  string_free(&attString);

  return CDI_UNDEFID;
}

static void
init_ncdims(int ndims, ncdim_t *ncdims)
{
  for (int i = 0; i < ndims; i++)
  {
    ncdim_t *ncdim = &ncdims[i];
    ncdim->dimid = CDI_UNDEFID;
    ncdim->ncvarid = CDI_UNDEFID;
    ncdim->dimtype = CDI_UNDEFID;
    ncdim->len = 0;
    ncdim->name[0] = 0;
  }
}

static void
init_ncvars(int nvars, ncvar_t *ncvars, int ncid)
{
  for (int varid = 0; varid < nvars; varid++)
  {
    ncvar_t *ncvar = &ncvars[varid];
    memset(ncvar, 0, sizeof(ncvar_t));

    ncvar->cdiVarID = CDI_UNDEFID;
    ncvar->ncid = ncid;
    ncvar->varStatus = UndefVar;
    ncvar->printWarning = true;
    ncvar->timetype = TIME_CONSTANT;
    ncvar->param = CDI_UNDEFID;
    ncvar->code = CDI_UNDEFID;
    ncvar->bounds = CDI_UNDEFID;
    ncvar->gridID = CDI_UNDEFID;
    ncvar->zaxisID = CDI_UNDEFID;
    ncvar->gridtype = CDI_UNDEFID;
    ncvar->zaxistype = CDI_UNDEFID;
    ncvar->xdim = CDI_UNDEFID;
    ncvar->ydim = CDI_UNDEFID;
    ncvar->zdim = CDI_UNDEFID;
    ncvar->xvarid = CDI_UNDEFID;
    ncvar->yvarid = CDI_UNDEFID;
    ncvar->rpvarid = CDI_UNDEFID;
    ncvar->zvarid = CDI_UNDEFID;
    ncvar->tvarid = CDI_UNDEFID;
    ncvar->ivarid = CDI_UNDEFID;
    ncvar->psvarid = CDI_UNDEFID;
    ncvar->p0varid = CDI_UNDEFID;
    for (int i = 0; i < MAX_COORDVARS; ++i) { ncvar->cvarids[i] = CDI_UNDEFID; }
    for (int i = 0; i < MAX_COORDVARS; ++i) { ncvar->coordvarids[i] = CDI_UNDEFID; }
    for (int i = 0; i < MAX_AUXVARS; ++i) { ncvar->auxvarids[i] = CDI_UNDEFID; }
    ncvar->cellarea = CDI_UNDEFID;
    ncvar->tableID = CDI_UNDEFID;
    ncvar->gmapid = CDI_UNDEFID;
    for (int i = 0; i < MAX_DIMS_CDF; ++i) { ncvar->dimids[i] = CDI_UNDEFID; }
    for (int i = 0; i < MAX_DIMS_CDF; ++i) { ncvar->dimtypes[i] = CDI_UNDEFID; }
    ncvar->chunkType = CDI_UNDEFID;
    ncvar->chunkSize = CDI_UNDEFID;
    ncvar->addoffset = 0.0;
    ncvar->scalefactor = 1.0;
    ncvar->validrange[0] = VALIDMISS;
    ncvar->validrange[1] = VALIDMISS;
    ncvar->typeOfEnsembleForecast = -1;
    ncvar->numberOfForecastsInEnsemble = -1;
    ncvar->perturbationNumber = -1;
  }
}

static void
cdf_set_var(ncvar_t *ncvar, int varStatus)
{
  if (ncvar->varStatus != UndefVar && ncvar->varStatus != varStatus && ncvar->printWarning)
  {
    if (!ncvar->ignoreVar) Warning("Inconsistent variable definition for %s!", ncvar->name);

    ncvar->printWarning = false;
    varStatus = CoordVar;
  }

  ncvar->varStatus = varStatus;
}

static void
cdf_set_dim(ncvar_t *ncvar, int dimid, int dimtype)
{
  if (ncvar->dimtypes[dimid] != CDI_UNDEFID && ncvar->dimtypes[dimid] != dimtype)
  {
    Warning("Inconsistent dimension definition for %s! dimid=%d  type=%c  newtype=%c", ncvar->name, dimid,
            axisTypeChar[ncvar->dimtypes[dimid]], axisTypeChar[dimtype]);
  }

  ncvar->dimtypes[dimid] = dimtype;
}

static void
scan_hybrid_formulaterms(int ncid, int ncfvarid, int *avarid, int *bvarid, int *psvarid, int *p0varid)
{
  *avarid = -1;
  *bvarid = -1;
  *psvarid = -1;
  *p0varid = -1;

  String attString;
  string_init(&attString);
  cdfGetAttString(ncid, ncfvarid, "formula_terms", &attString);
  char *pstring = attString.data;

  bool lstop = false;
  for (int i = 0; i < 4; i++)
  {
    while (isspace((int) *pstring)) pstring++;
    if (*pstring == 0) break;
    char *tagname = pstring;
    while (!isspace((int) *pstring) && *pstring != 0) pstring++;
    if (*pstring == 0) lstop = true;
    *(pstring++) = 0;

    while (isspace((int) *pstring)) pstring++;
    if (*pstring == 0) break;
    char *varname = pstring;
    while (!isspace((int) *pstring) && *pstring != 0) pstring++;
    if (*pstring == 0) lstop = true;
    *(pstring++) = 0;

    int dimvarid;
    int status_nc = nc_inq_varid(ncid, varname, &dimvarid);
    if (status_nc == NC_NOERR)
    {
      // clang-format off
      if      (str_is_equal(tagname, "ap:")) *avarid  = dimvarid;
      else if (str_is_equal(tagname, "a:") ) *avarid  = dimvarid;
      else if (str_is_equal(tagname, "b:") ) *bvarid  = dimvarid;
      else if (str_is_equal(tagname, "ps:")) *psvarid = dimvarid;
      else if (str_is_equal(tagname, "p0:")) *p0varid = dimvarid;
      // clang-format on
    }
    else if (!str_is_equal(tagname, "ps:")) { Warning("%s - %s", nc_strerror(status_nc), varname); }

    if (lstop) break;
  }

  string_free(&attString);
}

static void
readVCT(int ncid, int ndims2, size_t dimlen, size_t dimlen2, int avarid2, int bvarid2, double *vct)
{
  double *abuf = (double *) Malloc(dimlen * 2 * sizeof(double));
  double *bbuf = (double *) Malloc(dimlen * 2 * sizeof(double));
  cdf_get_var_double(ncid, avarid2, abuf);
  cdf_get_var_double(ncid, bvarid2, bbuf);

  if (ndims2 == 2)
  {
    for (size_t i = 0; i < dimlen; ++i)
    {
      vct[i] = abuf[i * 2];
      vct[i + dimlen + 1] = bbuf[i * 2];
    }
    vct[dimlen] = abuf[dimlen * 2 - 1];
    vct[dimlen * 2 + 1] = bbuf[dimlen * 2 - 1];
  }
  else
  {
    for (size_t i = 0; i < dimlen2; ++i)
    {
      vct[i] = abuf[i];
      vct[i + dimlen + 1] = bbuf[i];
    }
  }

  Free(abuf);
  Free(bbuf);
}

static bool
isHybridSigmaPressureCoordinate(int ncid, int ncvarid, ncvar_t *ncvars, const ncdim_t *ncdims)
{
  bool status = false;
  ncvar_t *ncvar = &ncvars[ncvarid];

  if (str_is_equal(ncvar->stdname, "atmosphere_hybrid_sigma_pressure_coordinate"))
  {
    CDI_Convention = CDI_CONVENTION_CF;

    status = true;
    ncvar->zaxistype = ZAXIS_HYBRID;
    // int ndims = ncvar->ndims;
    int dimid = ncvar->dimids[0];
    size_t dimlen = ncdims[dimid].len;
    int avarid1 = -1, bvarid1 = -1, psvarid1 = -1, p0varid1 = -1;
    int ncfvarid = ncvarid;
    if (ncvars[ncfvarid].hasFormulaterms) scan_hybrid_formulaterms(ncid, ncfvarid, &avarid1, &bvarid1, &psvarid1, &p0varid1);
    // printf("avarid1, bvarid1, psvarid1, p0varid1 %d %d %d %d\n", avarid1, bvarid1, psvarid1, p0varid1);
    if (avarid1 != -1) ncvars[avarid1].varStatus = CoordVar;
    if (bvarid1 != -1) ncvars[bvarid1].varStatus = CoordVar;
    if (psvarid1 != -1) ncvar->psvarid = psvarid1;
    if (p0varid1 != -1) ncvar->p0varid = p0varid1;

    if (ncvar->bounds != CDI_UNDEFID && ncvars[ncvar->bounds].hasFormulaterms)
    {
      ncfvarid = ncvar->bounds;
      int avarid2 = -1, bvarid2 = -1, psvarid2 = -1, p0varid2 = -1;
      if (ncvars[ncfvarid].hasFormulaterms) scan_hybrid_formulaterms(ncid, ncfvarid, &avarid2, &bvarid2, &psvarid2, &p0varid2);
      // printf("avarid2, bvarid2, psvarid2, p0varid2 %d %d %d %d\n", avarid2, bvarid2, psvarid2, p0varid2);
      if (avarid2 != -1 && bvarid2 != -1)
      {
        ncvars[avarid2].varStatus = CoordVar;
        ncvars[bvarid2].varStatus = CoordVar;

        int ndims2 = ncvars[avarid2].ndims;
        int dimid2 = ncvars[avarid2].dimids[0];
        size_t dimlen2 = ncdims[dimid2].len;

        if ((ndims2 == 2 && dimid == dimid2) || (ndims2 == 1 && dimlen == dimlen2 - 1))
        {
          double px = 1;
          if (p0varid1 != -1 && p0varid1 == p0varid2) cdf_get_var_double(ncid, p0varid2, &px);

          size_t vctsize = (dimlen + 1) * 2;
          double *vct = (double *) Malloc(vctsize * sizeof(double));

          readVCT(ncid, ndims2, dimlen, dimlen2, avarid2, bvarid2, vct);

          if (p0varid1 != -1 && is_not_equal(px, 1))
            for (size_t i = 0; i < dimlen + 1; ++i) vct[i] *= px;

          ncvar->vct = vct;
          ncvar->vctsize = vctsize;
        }
      }
    }
  }

  return status;
}

static int
atttypeInt_to_datatype(nc_type atttype)
{
  // clang-format off
  return (atttype == NC_SHORT)  ? CDI_DATATYPE_INT16 :
         (atttype == NC_BYTE)   ? CDI_DATATYPE_INT8 :
         (atttype == NC_UBYTE)  ? CDI_DATATYPE_UINT8 :
         (atttype == NC_USHORT) ? CDI_DATATYPE_UINT16 :
         (atttype == NC_UINT)   ? CDI_DATATYPE_UINT32 :
                                  CDI_DATATYPE_INT32;
  // clang-format on
}

static void
cdf_set_cdi_attr(int ncid, int ncvarid, int attnum, int cdiID, int varID, bool removeFillValue)
{
  nc_type atttype;
  size_t attlen;
  char attname[CDI_MAX_NAME];

  cdf_inq_attname(ncid, ncvarid, attnum, attname);
  cdf_inq_att(ncid, ncvarid, attname, &atttype, &attlen);

  if (removeFillValue && str_is_equal("_FillValue", attname)) return;

  if (xtypeIsInt(atttype))
  {
    int attint[8];
    int *pattint = (attlen > 8) ? (int *) Malloc(attlen * sizeof(int)) : attint;
    cdfGetAttInt(ncid, ncvarid, attname, attlen, pattint);
    cdiDefAttInt(cdiID, varID, attname, atttypeInt_to_datatype(atttype), (int) attlen, pattint);
    if (attlen > 8) Free(pattint);
  }
  else if (xtypeIsInt64(atttype))
  {
    int64_t attInt64[8];
    int64_t *pattInt64 = (attlen > 8) ? (int64_t *) Malloc(attlen * sizeof(int64_t)) : attInt64;
    cdfGetAttInt64(ncid, ncvarid, attname, attlen, pattInt64);
    bool defineAtts = true;
    for (size_t i = 0; i < attlen; ++i)
      if (pattInt64[i] > INT_MAX) { defineAtts = false; }
    if (defineAtts)
    {
      int attInt[8];
      int *pattInt = (attlen > 1) ? (int *) Malloc(attlen * sizeof(int)) : attInt;
      for (size_t i = 0; i < attlen; ++i) { pattInt[i] = (int) pattInt64[i]; }
      cdiDefAttInt(cdiID, varID, attname, CDI_DATATYPE_INT32, (int) attlen, pattInt);
      if (attlen > 8) Free(pattInt);
    }
    if (attlen > 8) Free(pattInt64);
  }
  else if (xtypeIsFloat(atttype))
  {
    double attflt[8];
    double *pattflt = (attlen > 8) ? (double *) Malloc(attlen * sizeof(double)) : attflt;
    cdfGetAttDouble(ncid, ncvarid, attname, attlen, pattflt);
    int datatype = (atttype == NC_FLOAT) ? CDI_DATATYPE_FLT32 : CDI_DATATYPE_FLT64;
    cdiDefAttFlt(cdiID, varID, attname, datatype, (int) attlen, pattflt);
    if (attlen > 8) Free(pattflt);
  }
  else if (xtypeIsText(atttype))
  {
    String attString;
    string_init(&attString);
    cdfGetAttString(ncid, ncvarid, attname, &attString);
    cdiDefAttTxt(cdiID, varID, attname, (int) strlen(attString.data), attString.data);
    string_free(&attString);
  }
}

static void
cdf_print_vars(const ncvar_t *ncvars, int nvars, const char *oname)
{
  // clang-format off
  char axis[7];
  enum { TAXIS = 't', ZAXIS = 'z', EAXIS = 'e', YAXIS = 'y', XAXIS = 'x' };

  fprintf(stderr, "%s:\n", oname);

  for (int varid = 0; varid < nvars; varid++)
  {
    const ncvar_t *ncvar = &ncvars[varid];
    int ndim = 0;
    if (ncvar->varStatus == DataVar || ncvar->varStatus == UndefVar)
    {
      axis[ndim++] = (ncvar->varStatus == DataVar) ? 'v' : 'u';
      axis[ndim++] = ':';
      for (int i = 0; i < ncvar->ndims; i++)
      {
        int dimType = ncvar->dimtypes[i];
        if      (dimType == T_AXIS) axis[ndim++] = TAXIS;
        else if (dimType == Z_AXIS) axis[ndim++] = ZAXIS;
        else if (dimType == E_AXIS) axis[ndim++] = EAXIS;
        else if (dimType == Y_AXIS) axis[ndim++] = YAXIS;
        else if (dimType == X_AXIS) axis[ndim++] = XAXIS;
        else                        axis[ndim++] = '?';
      }
    }
    else
    {
      axis[ndim++] = 'c';
      axis[ndim++] = ':';
      if      (ncvar->isTaxis) axis[ndim++] = TAXIS;
      else if (ncvar->isZaxis) axis[ndim++] = ZAXIS;
      else if (ncvar->isLat  ) axis[ndim++] = YAXIS;
      else if (ncvar->isYaxis) axis[ndim++] = YAXIS;
      else if (ncvar->isLon  ) axis[ndim++] = XAXIS;
      else if (ncvar->isXaxis) axis[ndim++] = XAXIS;
      else                     axis[ndim++] = '?';
    }

    axis[ndim++] = 0;

    fprintf(stderr, "%3d %3d  %-6s %s\n", varid, ndim-3, axis, ncvar->name);
  }
  // clang-format on
}

static void
cdf_scan_attr_axis(ncvar_t *ncvars, ncdim_t *ncdims, int ncvarid, const char *attstring, int nvdims, const int *dimidsp)
{
  ncvar_t *ncvar = &ncvars[ncvarid];
  int attlen = (int) strlen(attstring);

  if (nvdims == 0 && attlen == 1 && attstring[0] == 'z')
  {
    cdf_set_var(ncvar, CoordVar);
    ncvar->isZaxis = true;
    return;
  }

  if (attlen != nvdims) return;

  static const char accept[] = "-tTzZyYxX";
  if ((int) strspn(attstring, accept) != attlen) return;

  while (attlen--)
  {
    int dimtype;
    bool setVar = false;
    int attchar = toupper(attstring[attlen]);
    switch (attchar)
    {
      case 'T':
        if (attlen != 0) Warning("axis attribute 't' not on first position");
        dimtype = T_AXIS;
        break;
      case 'Z':
        ncvar->zdim = dimidsp[attlen];
        dimtype = Z_AXIS;
        setVar = (ncvar->ndims == 1);
        break;
      case 'Y':
        ncvar->ydim = dimidsp[attlen];
        dimtype = Y_AXIS;
        setVar = (ncvar->ndims == 1);
        break;
      case 'X':
        ncvar->xdim = dimidsp[attlen];
        dimtype = X_AXIS;
        setVar = (ncvar->ndims == 1);
        break;
      default: continue;
    }
    cdf_set_dim(ncvar, attlen, dimtype);

    if (setVar)
    {
      cdf_set_var(ncvar, CoordVar);
      int dimid0 = ncvar->dimids[0];
      ncdims[dimid0].dimtype = (ncdims[dimid0].dimtype == CDI_UNDEFID) ? dimtype : CDI_UNDEFID;
    }
  }
}

static int
cdf_get_cell_varid(char *attstring, int ncid)
{
  int nc_cell_id = CDI_UNDEFID;

  char *pstring = attstring;
  while (isspace((int) *pstring)) pstring++;
  char *cell_measures = pstring;
  while (isalnum((int) *pstring)) pstring++;
  *(pstring++) = 0;
  while (isspace((int) *pstring)) pstring++;
  char *cell_var = pstring;
  while (!isspace((int) *pstring) && *pstring != 0) pstring++;
  *(pstring++) = 0;
  /*
    printf("cell_measures >%s<\n", cell_measures);
    printf("cell_var >%s<\n", cell_var);
  */
  if (strStartsWith(cell_measures, "area"))
  {
    int nc_var_id;
    int status = nc_inq_varid(ncid, cell_var, &nc_var_id);
    if (status == NC_NOERR) nc_cell_id = nc_var_id;
    /*
    else
      Warning("%s - %s", nc_strerror(status), cell_var);
    */
  }

  return nc_cell_id;
}

static bool
is_valid_coordinate(ncvar_t *ncvar)
{
  bool status = true;
  if (ncvar->ndims > 1 && (str_is_equal(ncvar->name, "zg") || str_is_equal(ncvar->name, "zghalf"))) status = false;
  return status;
}

static void
read_coordinates_vars(int ncid, char *attstring, ncvar_t *ncvar, ncvar_t *ncvars, int *nchecked_vars, char *checked_vars[],
                      int max_check_vars)
{
  bool lstop = false;
  for (int i = 0; i < MAX_COORDVARS && !lstop; i++)
  {
    while (isspace((int) *attstring)) attstring++;
    if (*attstring == 0) break;
    char *varname = attstring;
    while (!isspace((int) *attstring) && *attstring != 0) attstring++;
    if (*attstring == 0) lstop = true;
    if (*(attstring - 1) == ',') *(attstring - 1) = 0;
    *(attstring++) = 0;

    int dimvarid;
    int status = nc_inq_varid(ncid, varname, &dimvarid);
    if (status == NC_NOERR)
    {
      if (is_valid_coordinate(&ncvars[dimvarid]))
      {
        cdf_set_var(&ncvars[dimvarid], CoordVar);
        if (!CDI_Ignore_Att_Coordinates)
        {
          ncvar->coordvarids[i] = dimvarid;
          ncvar->ncoordvars++;
        }
      }
    }
    else
    {
      if (!CDI_Ignore_Att_Coordinates) ncvar->ncoordvars++;

      int k;
      for (k = 0; k < *nchecked_vars; ++k)
        if (str_is_equal(checked_vars[k], varname)) break;

      if (k == *nchecked_vars)
      {
        if (*nchecked_vars < max_check_vars) checked_vars[(*nchecked_vars)++] = strdup(varname);
        Warning("%s - >%s<", nc_strerror(status), varname);
      }
    }
  }
}

static void
read_auxiliary_vars(int ncid, char *attstring, ncvar_t *ncvar, ncvar_t *ncvars)
{
  bool lstop = false;
  for (int i = 0; i < MAX_AUXVARS && !lstop; i++)
  {
    while (isspace((int) *attstring)) attstring++;
    if (*attstring == 0) break;
    char *varname = attstring;
    while (!isspace((int) *attstring) && *attstring != 0) attstring++;
    if (*attstring == 0) lstop = true;
    *(attstring++) = 0;

    int dimvarid;
    int status = nc_inq_varid(ncid, varname, &dimvarid);
    if (status == NC_NOERR)
    {
      cdf_set_var(&ncvars[dimvarid], CoordVar);
      //  if ( !CDI_Ignore_Att_Coordinates )
      {
        ncvar->auxvarids[i] = dimvarid;
        ncvar->nauxvars++;
      }
    }
    else
      Warning("%s - %s", nc_strerror(status), varname);
  }
}

static void
read_grid_mapping(int ncid, char *attstring, ncvar_t *ncvar, ncvar_t *ncvars)
{
  int nc_gmap_id;
  int status = nc_inq_varid(ncid, attstring, &nc_gmap_id);
  if (status == NC_NOERR)
  {
    ncvar->gmapid = nc_gmap_id;
    cdf_set_var(&ncvars[ncvar->gmapid], CoordVar);
    int nc_gmap_varid = ncvars[ncvar->gmapid].ncid;
    if (cdfCheckAttText(nc_gmap_varid, nc_gmap_id, "grid_mapping_name"))
    {
      String attString;
      string_init(&attString);
      cdfGetAttString(nc_gmap_varid, nc_gmap_id, "grid_mapping_name", &attString);
      if (str_is_equal(attString.data, "healpix")) { ncvar->isHealpixMapping = true; }
      else if (str_is_equal(attString.data, "latitude_longitude")) { ncvar->isLonLatMapping = true; }
      string_free(&attString);
    }
  }
  else
    Warning("%s - %s", nc_strerror(status), attstring);
}

static void
set_vars_chunks(int ncid, int ncvarid, int nvdims, ncvar_t *ncvar)
{
  int shuffle = 0, deflate = 0, deflateLevel = 0;
  nc_inq_var_deflate(ncid, ncvarid, &shuffle, &deflate, &deflateLevel);
  if (deflate > 0) ncvar->hasDeflate = true;

#ifdef HAVE_NC_DEF_VAR_SZIP
  int options_mask = 0, pixels_per_block = 0;
  nc_inq_var_szip(ncid, ncvarid, &options_mask, &pixels_per_block);
  if (options_mask && pixels_per_block) ncvar->hasSzip = true;
#endif
  ncvar->hasFilter = (cdf_get_var_filter(ncid, ncvarid, ncvar->filterSpec, CDI_MAX_NAME) > 0);
  // if (ncvar->hasFilter) printf("filterSpec: %s=%s\n", ncvar->name, ncvar->filterSpec);

  size_t chunks[MAX_DIMS_CDF];
  int storageIn;
  if (nc_inq_var_chunking(ncid, ncvarid, &storageIn, chunks) == NC_NOERR)
  {
    if (storageIn == NC_CHUNKED)
    {
      ncvar->isChunked = true;
      for (int i = 0; i < nvdims; ++i) ncvar->chunks[i] = chunks[i];
      if (CDI_Debug)
      {
        fprintf(stderr, "%s: chunking %d %d %d  chunks ", ncvar->name, storageIn, NC_CONTIGUOUS, NC_CHUNKED);
        for (int i = 0; i < nvdims; ++i) fprintf(stderr, "%zu ", chunks[i]);
        fprintf(stderr, "\n");
      }
    }
  }

  size_t size;
  size_t nelems;
  float preemption;
  if (nc_get_var_chunk_cache(ncid, ncvarid, &size, &nelems, &preemption) == NC_NOERR)
  {
    ncvar->chunkCacheSize = size;
    ncvar->chunkCacheNelems = nelems;
    ncvar->chunkCachePreemption = preemption;
    if (CDI_Debug) fprintf(stderr, "%s: chunkCacheSize=%zu nelems=%zu preemption=%g\n", ncvar->name, size, nelems, preemption);
  }
}

#if defined __GNUC__ || (__GNUC__ == 4 && __GNUC_MINOR__ == 9)
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wstrict-overflow"
#endif

static void
change_dimension_ids(int nvars, ncvar_t *ncvars, int ndims, ncdim_t *ncdims)
{
  for (int varid = 0; varid < nvars; varid++)
  {
    ncvar_t *ncvar = &ncvars[varid];
    for (int vdimid = 0; vdimid < ncvar->ndims; ++vdimid)
      for (int gdimid = 0; gdimid < ndims; ++gdimid)
      {
        if (ncdims[gdimid].dimid == ncvar->dimids[vdimid])
        {
          ncvar->dimids[vdimid] = gdimid;
          break;
        }
      }
  }
}

static void
add_dimid_ifnew(int dimid, int ndims, ncdim_t *ncdims)
{
  for (int i = 0; i < ndims; ++i)
  {
    ncdim_t *ncdim = &ncdims[i];
    if (ncdim->dimid != -1 && ncdim->dimid == dimid) { return; }
    if (ncdim->dimid == -1)
    {
      ncdim->dimid = dimid;
      return;
    }
  }
}

static void
set_dimension_ids(int nvars, ncvar_t *ncvars, int ndims, ncdim_t *ncdims)
{
  for (int varid = 0; varid < nvars; varid++)
  {
    ncvar_t *ncvar = &ncvars[varid];
    for (int i = 0; i < ncvar->ndims; ++i) add_dimid_ifnew(ncvar->dimids[i], ndims, ncdims);
  }
}

static void
read_vars_info(int nvars, ncvar_t *ncvars, int format)
{
  for (int varid = 0; varid < nvars; varid++)
  {
    ncvar_t *ncvar = &ncvars[varid];
    cdf_inq_var(ncvar->ncid, varid, ncvar->name, &ncvar->xtype, &ncvar->ndims, ncvar->dimids, &ncvar->nattsNC);

    for (int i = 0; i < ncvar->ndims; i++) { ncvar->dimtypes[i] = -1; }

    if (format == NC_FORMAT_NETCDF4_CLASSIC || format == NC_FORMAT_NETCDF4)
    {
      set_vars_chunks(ncvar->ncid, varid, ncvar->ndims, ncvar);
    }
  }
}

static void
set_vars_timetype(int nvars, ncvar_t *ncvars, int timedimid)
{
  for (int varid = 0; varid < nvars; varid++)
  {
    ncvar_t *ncvar = &ncvars[varid];
    if (ncvar->ndims > 0)
    {
      if (timedimid == ncvar->dimids[0])
      {
        ncvar->timetype = TIME_VARYING;
        cdf_set_dim(ncvar, 0, T_AXIS);
      }
      else
      {
        for (int i = 1, n = ncvar->ndims; i < n; i++)
        {
          if (timedimid == ncvar->dimids[i])
          {
            Warning("Time must be the first dimension! Unsupported array structure, skipped variable %s!", ncvar->name);
            ncvar->varStatus = CoordVar;
          }
        }
      }
    }
  }
}

static size_t
copy_str_bounded(char *dest, size_t destSize, const char *src, size_t srcLen)
{
  if (srcLen > destSize - 1) { srcLen = destSize - 1; }
  memcpy(dest, src, srcLen);
  dest[srcLen] = 0;
  return srcLen;
}

static void
scan_units_attr(ncvar_t *ncvar, const char *attstring, size_t attstringsize)
{
  ncvar->unitsLen = (int) copy_str_bounded(ncvar->units, sizeof(ncvar->units), attstring, attstringsize);
}

static void
scan_param_attr(ncvar_t *ncvar, char attstring[8192])
{
  int pnum = 0, pcat = 255, pdis = 255;
  sscanf(attstring, "%d.%d.%d", &pnum, &pcat, &pdis);
  ncvar->param = cdiEncodeParam(pnum, pcat, pdis);
  cdf_set_var(ncvar, DataVar);
}

static void
scan_trunctype_attr(ncvar_t *ncvar, char attstring[8192])
{
  if (str_is_equal(attstring, "Triangular")) { ncvar->gridtype = GRID_SPECTRAL; }
}

static void
scan_gridtype_attr(ncvar_t *ncvar, char attstring[8192])
{
  str_to_lower(attstring);
  cdf_set_gridtype(attstring, &ncvar->gridtype);
  cdf_set_var(ncvar, DataVar);
}

static void
scan_gridlatitudes_attr(ncvar_t *ncvar, char attstring[8192], ncvar_t *ncvars)
{
  int ncid = ncvar->ncid;
  int nc_yvar_id;
  int status = nc_inq_varid(ncid, attstring, &nc_yvar_id);
  if (status == NC_NOERR)
  {
    ncvar->yvarid = nc_yvar_id;
    cdf_set_var(&ncvars[ncvar->yvarid], CoordVar);
  }
  else
    Warning("%s - %s", nc_strerror(status), attstring);

  cdf_set_var(ncvar, DataVar);
}

static void
scan_gridreduced_attr(ncvar_t *ncvar, char attstring[8192], ncvar_t *ncvars)
{
  int ncid = ncvar->ncid;
  int nc_rpvar_id;
  int status = nc_inq_varid(ncid, attstring, &nc_rpvar_id);
  if (status == NC_NOERR)
  {
    ncvar->rpvarid = nc_rpvar_id;
    cdf_set_var(&ncvars[ncvar->rpvarid], CoordVar);
  }
  else
    Warning("%s - %s", nc_strerror(status), attstring);

  cdf_set_var(ncvar, DataVar);
}

static void
scan_leveltype_attr(ncvar_t *ncvar, char attstring[8192])
{
  str_to_lower(attstring);
  cdf_set_zaxistype(attstring, &ncvar->zaxistype);
  cdf_set_var(ncvar, DataVar);
}

static void
scan_climatology_attr(ncvar_t *ncvar, char attstring[8192], ncvar_t *ncvars)
{
  int ncid = ncvar->ncid;
  int ncboundsid;
  int status = nc_inq_varid(ncid, attstring, &ncboundsid);
  if (status == NC_NOERR)
  {
    ncvar->isClimatology = true;
    ncvar->bounds = ncboundsid;
    cdf_set_var(&ncvars[ncvar->bounds], CoordVar);
    cdf_set_var(ncvar, CoordVar);
  }
  else
    Warning("%s - %s", nc_strerror(status), attstring);
}

static void
scan_bounds_attr(ncvar_t *ncvar, char attstring[8192], ncvar_t *ncvars)
{
  int ncid = ncvar->ncid;
  int ncboundsid;
  int status = nc_inq_varid(ncid, attstring, &ncboundsid);
  if (status == NC_NOERR)
  {
    ncvar->bounds = ncboundsid;
    cdf_set_var(&ncvars[ncvar->bounds], CoordVar);
    cdf_set_var(ncvar, CoordVar);
  }
  else
  {
    static bool once = true;
    if (once) { once = false, Warning("%s - %s", nc_strerror(status), attstring); }
  }
}

static bool
scan_valid_range_attr(ncvar_t *ncvar, int fileID, int ncvarid, const char *attname, nc_type atttype, nc_type xtype, size_t attlen)
{
  // attlen: 2 für valid_range, 1 für valid_min/valid_max
  bool ignoreDatatype = (xtypeIsFloat(atttype) != xtypeIsFloat(xtype));
  if (CDI_Ignore_Valid_Range || ignoreDatatype)
  {
    if (ignoreDatatype) Warning("Inconsistent data type for attribute %s:%s, ignored!", ncvar->name, attname);
    return true;
  }

  if (ncvar->validrangeDefined && attlen == 1) return true;

  size_t idx = (attlen == 1 && str_is_equal(attname, "valid_max")) ? 1 : 0;
  size_t count = (attlen == 2) ? 2 : 1;

  cdfGetAttDouble(fileID, ncvarid, attname, count, &ncvar->validrange[idx]);
  ncvar->validrangeDefined = true;

  if (attlen == 2)
  {
    ncvar->validrangeDefined = (ncvar->validrange[0] <= ncvar->validrange[1]);
    if (((int) ncvar->validrange[0]) == 0 && ((int) ncvar->validrange[1]) == 255) ncvar->isUnsigned = true;
  }

  return true;
}

static void
scan_vars_attr(int nvars, ncvar_t *ncvars, int ndims, ncdim_t *ncdims, int modelID)
{
  int nchecked_vars = 0;
  enum
  {
    max_check_vars = 9
  };
  char *checked_vars[max_check_vars];
  for (int i = 0; i < max_check_vars; ++i) { checked_vars[i] = NULL; }

  char attname[CDI_MAX_NAME];
  String attString;
  string_init(&attString);

  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    int ncid = ncvar->ncid;
    int nvdims = ncvar->ndims;
    nc_type xtype = ncvar->xtype;
    int nvatts = ncvar->nattsNC;

    if (ncvar->natts == 0 && nvatts > 0) { ncvar->atts = (int *) Malloc((size_t) nvatts * sizeof(int)); }

    for (int iatt = 0; iatt < nvatts; ++iatt)
    {
      int nc_cell_id = CDI_UNDEFID;

      nc_type atttype;
      size_t attlen;
      cdf_inq_attname(ncid, ncvarid, iatt, attname);
      cdf_inq_att(ncid, ncvarid, attname, &atttype, &attlen);

      bool isText = xtypeIsText(atttype);
      bool isNumber = xtypeIsFloat(atttype) || xtypeIsInt(atttype);
      bool isRealization = false, isEnsembleMembers = false, isForecastInitType = false;

      if (isText)
      {
        cdfGetAttString(ncid, ncvarid, attname, &attString);
        size_t attstringLen = strlen(attString.data) + 1;
        char *attstring = attString.data;

        if (str_is_equal(attname, "long_name"))
        {
          copy_str_bounded(ncvar->longname, sizeof(ncvar->longname), attstring, attstringLen);
        }
        else if (str_is_equal(attname, "standard_name"))
        {
          copy_str_bounded(ncvar->stdname, sizeof(ncvar->stdname), attstring, attstringLen);
        }
        else if (str_is_equal(attname, "units")) { scan_units_attr(ncvar, attstring, attstringLen); }
        else if (str_is_equal(attname, "calendar")) { ncvar->hasCalendar = true; }
        else if (str_is_equal(attname, "param")) { scan_param_attr(ncvar, attstring); }
        else if (str_is_equal(attname, "trunc_type")) { scan_trunctype_attr(ncvar, attstring); }
        else if (str_is_equal(attname, "grid_type")) { scan_gridtype_attr(ncvar, attstring); }
        else if (str_is_equal(attname, "CDI_grid_type")) { scan_gridtype_attr(ncvar, attstring); }
        else if (str_is_equal(attname, "CDI_grid_latitudes")) { scan_gridlatitudes_attr(ncvar, attstring, ncvars); }
        else if (str_is_equal(attname, "CDI_grid_reduced_points")) { scan_gridreduced_attr(ncvar, attstring, ncvars); }
        else if (str_is_equal(attname, "level_type")) { scan_leveltype_attr(ncvar, attstring); }
        else if (str_is_equal(attname, "climatology")) { scan_climatology_attr(ncvar, attstring, ncvars); }
        else if (str_is_equal(attname, "bounds")) { scan_bounds_attr(ncvar, attstring, ncvars); }
        else if (str_is_equal(attname, "formula_terms")) { ncvar->hasFormulaterms = true; }
        else if (str_is_equal(attname, "cell_measures") && (nc_cell_id = cdf_get_cell_varid(attstring, ncid)) != CDI_UNDEFID)
        {
          ncvar->cellarea = nc_cell_id;
          ncvars[nc_cell_id].varStatus = CoordVar;
          cdf_set_var(ncvar, DataVar);
        }
        else if (str_is_equal(attname, "associate") || str_is_equal(attname, "coordinates"))
        {
          read_coordinates_vars(ncid, attstring, ncvar, ncvars, &nchecked_vars, checked_vars, max_check_vars);
          cdf_set_var(ncvar, DataVar);
        }
        else if (str_is_equal(attname, "auxiliary_variable"))
        {
          read_auxiliary_vars(ncid, attstring, ncvar, ncvars);
          cdf_set_var(ncvar, DataVar);
        }
        else if (str_is_equal(attname, "grid_mapping"))
        {
          read_grid_mapping(ncid, attstring, ncvar, ncvars);
          cdf_set_var(ncvar, DataVar);
        }
        else if (str_is_equal(attname, "positive"))
        {
          str_to_lower(attstring);
          if (str_is_equal(attstring, "down")) { ncvar->positive = POSITIVE_DOWN; }
          else if (str_is_equal(attstring, "up")) { ncvar->positive = POSITIVE_UP; }

          int dimid0 = ncvar->dimids[0];
          if (ncvar->varStatus == UndefVar
              && (nvdims == 0 || (nvdims == 1 && ncvar->dimtypes[0] == CDI_UNDEFID && ncdims[dimid0].ncvarid == CDI_UNDEFID)))
          {
            if (nvdims == 1)
            {
              cdf_set_var(ncvar, CoordVar);
              cdf_set_dim(ncvar, 0, Z_AXIS);
              if (dimid0 < ndims) ncdims[dimid0].dimtype = Z_AXIS;
            }
            else if (nvdims == 0)
            {
              cdf_set_var(ncvar, CoordVar);
              ncvar->isZaxis = true;
            }
          }
          else
          {
            ncvar->atts[ncvar->natts++] = iatt;
          }
        }
        else if (str_is_equal(attname, "cdi"))
        {
          if (!strcasecmp(attstring, "ignore"))
          {
            ncvar->ignoreVar = true;
            cdf_set_var(ncvar, CoordVar);
          }
        }
        else if (str_is_equal(attname, "_Unsigned"))
        {
          if (!strcasecmp(attstring, "true"))
          {
            ncvar->isUnsigned = true;
            /*
            ncvar->validrangeDefined = true;
            ncvar->validrange[0] = 0;
            ncvar->validrange[1] = 255;
            */
          }
          // cdf_set_var(ncvar, DataVar);
        }
        else
        {
          ncvar->atts[ncvar->natts++] = iatt;
        }
      }
      else if (isNumber)
      {
        if (str_is_equal(attname, "code"))
        {
          cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvar->code);
          cdf_set_var(ncvar, DataVar);
        }
        else if (str_is_equal(attname, "table"))
        {
          int tablenum;
          cdfGetAttInt(ncid, ncvarid, attname, 1, &tablenum);
          if (tablenum > 0)
          {
            ncvar->tabnum = tablenum;
            ncvar->tableID = tableInq(modelID, tablenum, NULL);
            if (ncvar->tableID == CDI_UNDEFID) ncvar->tableID = tableDef(modelID, tablenum, NULL);
          }
          cdf_set_var(ncvar, DataVar);
        }
        else if (str_is_equal(attname, "CDI_grid_num_LPE")) { cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvar->numLPE); }
        else if (str_is_equal(attname, "trunc_count")) { cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvar->truncation); }
        else if (str_is_equal(attname, "truncation")) { cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvar->truncation); }
        else if (str_is_equal(attname, "number_of_grid_in_reference"))
        {
          cdfGetAttInt(ncid, ncvarid, attname, 1, &ncvar->position);
        }
        else if (str_is_equal(attname, "add_offset")) { cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvar->addoffset); }
        else if (str_is_equal(attname, "scale_factor")) { cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvar->scalefactor); }
        else if (str_is_equal(attname, "_FillValue"))
        {
          cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvar->fillval);
          if (xtype == NC_FLOAT) { ncvar->fillval = (float) ncvar->fillval; }
          ncvar->fillvalDefined = true;
          // cdf_set_var(ncvar, DataVar);
        }
        else if (str_is_equal(attname, "missing_value"))
        {
          cdfGetAttDouble(ncid, ncvarid, attname, 1, &ncvar->missval);
          if (xtype == NC_FLOAT) { ncvar->missval = (float) ncvar->missval; }
          ncvar->missvalDefined = true;
          // cdf_set_var(ncvar, DataVar);
        }
        else if ((str_is_equal(attname, "valid_range") && attlen == 2) || (str_is_equal(attname, "valid_min") && attlen == 1)
                 || (str_is_equal(attname, "valid_max") && attlen == 1))
        {
          scan_valid_range_attr(ncvar, ncid, ncvarid, attname, atttype, xtype, attlen);
        }
        else if ((isRealization = str_is_equal(attname, "realization"))
                 || (isEnsembleMembers = str_is_equal(attname, "ensemble_members"))
                 || (isForecastInitType = str_is_equal(attname, "forecast_init_type")))
        {
          int temp;
          cdfGetAttInt(ncid, ncvarid, attname, 1, &temp);
          // clang-format off
          if      (isRealization)      ncvar->perturbationNumber = temp;
          else if (isEnsembleMembers)  ncvar->numberOfForecastsInEnsemble = temp;
          else if (isForecastInitType) ncvar->typeOfEnsembleForecast = temp;
          // clang-format on
          cdf_set_var(ncvar, DataVar);
        }
        else
        {
          ncvar->atts[ncvar->natts++] = iatt;
        }
      }
      else
      {
        ncvar->atts[ncvar->natts++] = iatt;
      }
    }
  }

  string_free(&attString);
  for (int i = 0; i < max_check_vars; ++i) Free(checked_vars[i]);
}

#if defined __GNUC__ || (__GNUC__ == 4 && __GNUC_MINOR__ == 9)
#pragma GCC diagnostic pop
#endif

static void
cdf_set_chunk_info(stream_t *streamP, int nvars, ncvar_t *ncvars)
{
  int vlistID = streamP->vlistID;
  CdfInfo *cdfInfo = &(streamP->cdfInfo);
  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    int chunkSizeDimT = 0;
    int chunkSizeDimZ = 0;
    int chunkSizeDimY = 0;
    int chunkSizeDimX = 0;
    ncvar_t *ncvar = &ncvars[ncvarid];
    int varID = ncvar->cdiVarID;
    if (ncvar->varStatus != DataVar || !ncvar->isChunked || varID == CDI_UNDEFID) { continue; }

    for (int i = 0, n = ncvar->ndims; i < n; ++i)
    {
      size_t chunkSize = ncvar->chunks[i];
      if (chunkSize > 1)
      {
        int dimType = ncvar->dimtypes[i];
        // clang-format off
          if      (dimType == T_AXIS && chunkSize > cdfInfo->chunkSizeDimT) { cdfInfo->chunkSizeDimT = chunkSize; }
          else if (dimType == Z_AXIS && chunkSize > cdfInfo->chunkSizeDimZ) { cdfInfo->chunkSizeDimZ = chunkSize; }

          if      (dimType == T_AXIS) { chunkSizeDimT = chunkSize; }
          else if (dimType == Z_AXIS) { chunkSizeDimZ = chunkSize; }
          else if (dimType == Y_AXIS) { chunkSizeDimY = chunkSize; }
          else if (dimType == X_AXIS) { chunkSizeDimX = chunkSize; }
        // clang-format on
      }
    }
    if ((CDI_Copy_ChunkSpec || chunkSizeDimT == 0) && CDI_Remove_ChunkSpec == false)
    {
      if (chunkSizeDimT > 0) cdiDefKeyInt(vlistID, varID, CDI_KEY_CHUNKSIZE_DIMT, chunkSizeDimT);
      if (chunkSizeDimZ > 0) cdiDefKeyInt(vlistID, varID, CDI_KEY_CHUNKSIZE_DIMZ, chunkSizeDimZ);
      if (chunkSizeDimY > 0) cdiDefKeyInt(vlistID, varID, CDI_KEY_CHUNKSIZE_DIMY, chunkSizeDimY);
      if (chunkSizeDimX > 0) cdiDefKeyInt(vlistID, varID, CDI_KEY_CHUNKSIZE_DIMX, chunkSizeDimX);
    }
  }
}

static void
verify_vars_attr(int nvars, ncvar_t *ncvars, ncdim_t *ncdims)
{
  nc_type atttype;
  size_t attlen;
  char attname[CDI_MAX_NAME];
  String attString;
  string_init(&attString);

  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    int ncid = ncvar->ncid;
    const int *dimidsp = ncvar->dimids;
    int nvdims = ncvar->ndims;
    int nvatts = ncvar->natts;

    for (int i = 0; i < nvatts; i++)
    {
      int attnum = ncvar->atts[i];
      cdf_inq_attname(ncid, ncvarid, attnum, attname);
      cdf_inq_att(ncid, ncvarid, attname, &atttype, &attlen);

      // bool isNumber = (xtypeIsFloat(atttype) || xtypeIsInt(atttype));
      bool isText = xtypeIsText(atttype);
      if (isText)
      {
        if (str_is_equal(attname, "axis"))
        {
          cdfGetAttString(ncid, ncvarid, attname, &attString);
          cdf_scan_attr_axis(ncvars, ncdims, ncvarid, attString.data, nvdims, dimidsp);
        }
      }
    }
  }

  string_free(&attString);
}

static void
find_dimtypes(ncvar_t *ncvars, ncvar_t *ncvar, bool *plxdim, bool *plydim, bool *plzdim, int *plcdim)
{
  bool lxdim = false, lydim = false, lzdim = false /*, ltdim = false */;
  int lcdim = 0;
  for (int i = 0, n = ncvar->ndims; i < n; i++)
  {
    int dimtype = ncvar->dimtypes[i];
    lxdim |= (dimtype == X_AXIS);
    lydim |= (dimtype == Y_AXIS);
    lzdim |= (dimtype == Z_AXIS);
    if (ncvar->cvarids[i] != CDI_UNDEFID) lcdim++;
    // ltdim |= (dimtype == T_AXIS);
  }

  if (!lxdim && ncvar->xvarid != CDI_UNDEFID && ncvars[ncvar->xvarid].ndims == 0) lxdim = true;
  if (!lydim && ncvar->yvarid != CDI_UNDEFID && ncvars[ncvar->yvarid].ndims == 0) lydim = true;

  *plxdim = lxdim;
  *plydim = lydim;
  *plzdim = lzdim;
  *plcdim = lcdim;
}

static void
cdf_set_dimtype(int numVars, ncvar_t *ncvars, ncdim_t *ncdims)
{
  for (int varId = 0; varId < numVars; ++varId)
  {
    ncvar_t *ncvar = &ncvars[varId];
    if (ncvar->varStatus == DataVar)
    {
      for (int i = 0, n = ncvar->ndims; i < n; ++i)
      {
        int dimid = ncvar->dimids[i];
        int dimtype = ncdims[dimid].dimtype;
        if (dimtype >= X_AXIS && dimtype <= T_AXIS) cdf_set_dim(ncvar, i, dimtype);
      }

      if (CDI_Debug)
      {
        Message("var %d %s", varId, ncvar->name);
        for (int i = 0, n = ncvar->ndims; i < n; i++) printf("  dim%d type=%d  ", i, ncvar->dimtypes[i]);
        printf("\n");
      }
    }
  }

  for (int varId = 0; varId < numVars; ++varId)
  {
    ncvar_t *ncvar = &ncvars[varId];
    if (ncvar->varStatus == DataVar)
    {
      bool lxdim = false, lydim = false, lzdim = false /* , ltdim = false */;
      int lcdim;
      find_dimtypes(ncvars, ncvar, &lxdim, &lydim, &lzdim, &lcdim);
      int allcdims = lcdim;
      int ndims = ncvar->ndims;

      if (lxdim && (lydim || ncvar->gridtype == GRID_UNSTRUCTURED))
        for (int i = ndims - 1; i >= 0; i--)
        {
          if (ncvar->dimtypes[i] == -1 && !lzdim)
          {
            if (lcdim)
            {
              int cdimvar = ncvar->cvarids[allcdims - lcdim];
              ncvar->zvarid = cdimvar;
              lcdim--;
              ncvars[cdimvar].zaxistype = ZAXIS_CHAR;
            }
            cdf_set_dim(ncvar, i, Z_AXIS);
            lzdim = true;
            int dimid = ncvar->dimids[i];
            if (ncdims[dimid].dimtype == CDI_UNDEFID) ncdims[dimid].dimtype = Z_AXIS;
          }
        }
    }
  }

  for (int varId = 0; varId < numVars; ++varId)
  {
    ncvar_t *ncvar = &ncvars[varId];
    for (int i = 0, n = ncvar->ndims; i < n; ++i)
    {
      if (ncvar->dimtypes[i] == CDI_UNDEFID)
      {
        int dimid = ncvar->dimids[i];
        if (ncdims[dimid].dimtype == Z_AXIS)
        {
          ncvar->isZaxis = true;
          cdf_set_dim(ncvar, i, Z_AXIS);
        }
      }
    }
  }

  // find variables with coordinates attribute on unstructured grids
  for (int varId = 0; varId < numVars; ++varId)
  {
    ncvar_t *ncvar = &ncvars[varId];
    if (ncvar->varStatus == DataVar && ncvar->gridtype == CDI_UNDEFID && ncvar->ncoordvars > 0)
    {
      int xDimId = -1, yDimId = -1;
      for (int i = 0; i < ncvar->ncoordvars; ++i)
      {
        int cvarId = ncvar->coordvarids[i];
        ncvar_t *cvar = &ncvars[cvarId];
        if (cvar->ndims == 1 && cvar->isLon) { xDimId = cvar->dimids[0]; }
        if (cvar->ndims == 1 && cvar->isLat) { yDimId = cvar->dimids[0]; }
      }
      if (xDimId != -1 && xDimId == yDimId) { ncvar->gridtype = GRID_UNSTRUCTURED; }
    }
  }

  for (int varId = 0; varId < numVars; ++varId)
  {
    ncvar_t *ncvar = &ncvars[varId];
    if (ncvar->varStatus == DataVar)
    {
      bool lxdim = false, lydim = false, lzdim = false /*, ltdim = false */;
      int lcdim = 0;
      find_dimtypes(ncvars, ncvar, &lxdim, &lydim, &lzdim, &lcdim);
      int allcdims = lcdim;
      int ndims = ncvar->ndims;

      //   if ( ndims > 1 )
      for (int i = ndims - 1; i >= 0; i--)
      {
        if (ncvar->dimtypes[i] == -1)
        {
          int dimtype;
          if (!lxdim)
          {
            if (lcdim && ncvar->xvarid == CDI_UNDEFID)
            {
              int cdimvar = ncvar->cvarids[allcdims - lcdim];
              ncvar->xvarid = cdimvar;
              lcdim--;
            }
            dimtype = X_AXIS;
            lxdim = true;
          }
          else if (!lydim && ncvar->gridtype != GRID_UNSTRUCTURED && ncvar->isHealpixMapping == false)
          // else if ( !lydim && ! (ncvars[ncvar->xvarid].dimids[0] == ncvars[ncvar->yvarid].dimids[0] &&
          //                        ncvars[ncvar->xvarid].ndims == 1 && ncvars[ncvar->yvarid].ndims == 1))
          {
            if (lcdim && ncvar->yvarid == CDI_UNDEFID)
            {
              int cdimvar = ncvar->cvarids[allcdims - lcdim];
              ncvar->yvarid = cdimvar;
              lcdim--;
            }
            dimtype = Y_AXIS;
            lydim = true;
          }
          else if (!lzdim)
          {
            if (lcdim > 0)
            {
              int cdimvar = ncvar->cvarids[allcdims - lcdim];
              ncvar->zvarid = cdimvar;
              lcdim--;
              ncvars[cdimvar].zaxistype = ZAXIS_CHAR;
            }
            dimtype = Z_AXIS;
            lzdim = true;
          }
          else
          {
            continue;
          }
          cdf_set_dim(ncvar, i, dimtype);
        }
      }
      // if (lcdim > 0) Warning("Could not assign all character coordinates to data variable!");
    }
  }
}

static void
set_vardim_coord(ncvar_t *ncvar, ncdim_t *ncdim, int axisType)
{
  cdf_set_var(ncvar, CoordVar);
  cdf_set_dim(ncvar, 0, axisType);
  ncdim->dimtype = axisType;
}

static int
get_hybrid_zaxistype(const char *longname, const char *units)
{
  if (strStartsWith(longname, "hybrid"))
  {
    if (str_is_equal(longname, "hybrid level at layer midpoints")) return ZAXIS_HYBRID;
    if (str_is_equal(longname, "hybrid model level at layer midpoints")) return ZAXIS_HYBRID;
    if (strStartsWith(longname, "hybrid level at midpoints")) return ZAXIS_HYBRID;
    if (str_is_equal(longname, "hybrid level at layer interfaces")) return ZAXIS_HYBRID_HALF;
    if (str_is_equal(longname, "hybrid model level at layer interfaces")) return ZAXIS_HYBRID_HALF;
    if (strStartsWith(longname, "hybrid level at interfaces")) return ZAXIS_HYBRID_HALF;
  }
  if (str_is_equal(units, "level")) return ZAXIS_GENERIC;
  return CDI_UNDEFID;
}

// verify coordinates vars - first scan (dimname == varname)
static void
verify_coordinates_vars_1(int ncid, int ndims, ncdim_t *ncdims, ncvar_t *ncvars, int timedimid, bool *isHybridCF)
{
  for (int ncdimid = 0; ncdimid < ndims; ncdimid++)
  {
    ncdim_t *ncdim = &ncdims[ncdimid];
    int ncvarid = ncdim->ncvarid;
    if (ncvarid != -1)
    {
      ncvar_t *ncvar = &ncvars[ncvarid];
      if (ncvar->dimids[0] == timedimid)
      {
        ncvar->isTaxis = true;
        ncdim->dimtype = T_AXIS;
        continue;
      }

      if (isHybridSigmaPressureCoordinate(ncid, ncvarid, ncvars, ncdims))
      {
        *isHybridCF = true;
        continue;
      }

      if (str_is_equal(ncvar->stdname, "healpix_index"))
      {
        ncvar->isIndexAxis = true;
        continue;
      }

      if (ncvar->units[0] != 0)
      {
        if (is_lon_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isLon = true;
          set_vardim_coord(ncvar, ncdim, X_AXIS);
        }
        else if (is_lat_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isLat = true;
          set_vardim_coord(ncvar, ncdim, Y_AXIS);
        }
        else if (is_x_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isXaxis = true;
          set_vardim_coord(ncvar, ncdim, X_AXIS);
        }
        else if (is_y_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isYaxis = true;
          set_vardim_coord(ncvar, ncdim, Y_AXIS);
        }
        else if (is_pressure_units(ncvar->units)) { ncvar->zaxistype = ZAXIS_PRESSURE; }
        else if (str_is_equal(ncvar->units, "level") || str_is_equal(ncvar->units, "1"))
        {
          int zaxistype = get_hybrid_zaxistype(ncvar->longname, ncvar->units);
          if (zaxistype != CDI_UNDEFID) ncvar->zaxistype = zaxistype;
        }
        else if (is_DBL_axis(ncvar->longname)) { ncvar->zaxistype = ZAXIS_DEPTH_BELOW_LAND; }
        else if (is_height_units(ncvar->units))
        {
          // clang-format off
          if      (is_depth_axis(ncvar->stdname, ncvar->longname))    ncvar->zaxistype = ZAXIS_DEPTH_BELOW_SEA;
          else if (is_height_axis(ncvar->stdname, ncvar->longname))   ncvar->zaxistype = ZAXIS_HEIGHT;
          else if (is_altitude_axis(ncvar->stdname, ncvar->longname)) ncvar->zaxistype = ZAXIS_ALTITUDE;
          // clang-format on
        }
      }
      else
      {
        // clang-format off
        if      (is_reference_axis(ncvar->stdname, ncvar->longname)) ncvar->zaxistype = ZAXIS_REFERENCE;
        else if (str_is_equal(ncvar->stdname, "air_pressure"))       ncvar->zaxistype = ZAXIS_PRESSURE;
        // clang-format on
      }

      if (!ncvar->isLon && (ncvar->longname[0] != 0) && !ncvar->isLat && (ncvar->longname[1] != 0))
      {
        if (strStartsWith(ncvar->longname + 1, "ongitude"))
        {
          ncvar->isLon = true;
          set_vardim_coord(ncvar, ncdim, X_AXIS);
          continue;
        }
        else if (strStartsWith(ncvar->longname + 1, "atitude"))
        {
          ncvar->isLat = true;
          set_vardim_coord(ncvar, ncdim, Y_AXIS);
          continue;
        }
      }

      if (ncvar->zaxistype != CDI_UNDEFID)
      {
        ncvar->isZaxis = true;
        set_vardim_coord(ncvar, ncdim, Z_AXIS);
      }
    }
  }
}

// verify coordinates vars - second scan (all other variables)
static void
verify_coordinates_vars_2(stream_t *streamP, int nvars, ncvar_t *ncvars)
{
  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->varStatus == CoordVar)
    {
      if (str_is_equal(ncvar->stdname, "healpix_index"))
      {
        ncvar->isIndexAxis = true;
        continue;
      }
      else if (ncvar->units[0] != 0)
      {
        if (is_lon_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isLon = true;
          continue;
        }
        else if (is_lat_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isLat = true;
          continue;
        }
        else if (is_x_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isXaxis = true;
          continue;
        }
        else if (is_y_axis(ncvar->units, ncvar->stdname))
        {
          ncvar->isYaxis = true;
          continue;
        }
        else if (ncvar->zaxistype == CDI_UNDEFID && (str_is_equal(ncvar->units, "level") || str_is_equal(ncvar->units, "1")))
        {
          int zaxistype = get_hybrid_zaxistype(ncvar->longname, ncvar->units);
          if (zaxistype != CDI_UNDEFID) ncvar->zaxistype = zaxistype;
          continue;
        }
        else if (ncvar->zaxistype == CDI_UNDEFID && is_pressure_units(ncvar->units))
        {
          ncvar->zaxistype = ZAXIS_PRESSURE;
          continue;
        }
        else if (is_DBL_axis(ncvar->longname))
        {
          ncvar->zaxistype = ZAXIS_DEPTH_BELOW_LAND;
          continue;
        }
        else if (is_height_units(ncvar->units))
        {
          // clang-format off
          if      (is_depth_axis(ncvar->stdname, ncvar->longname))  ncvar->zaxistype = ZAXIS_DEPTH_BELOW_SEA;
          else if (is_height_axis(ncvar->stdname, ncvar->longname)) ncvar->zaxistype = ZAXIS_HEIGHT;
          // clang-format on
          continue;
        }
      }
      else if (str_is_equal(ncvar->stdname, "region") || str_is_equal(ncvar->stdname, "area_type")
               || cdfInqDatatype(streamP, ncvar->xtype, ncvar->isUnsigned) == CDI_DATATYPE_UINT8)
      {
        ncvar->isCharAxis = true;
      }
      else if (str_is_equal(ncvar->stdname, "air_pressure")) { ncvar->zaxistype = ZAXIS_PRESSURE; }

      // not needed anymore for rotated grids
      if (!ncvar->isLon && (ncvar->longname[0] != 0) && !ncvar->isLat && (ncvar->longname[1] != 0))
      {
        if (strStartsWith(ncvar->longname + 1, "ongitude"))
        {
          ncvar->isLon = true;
          continue;
        }
        else if (strStartsWith(ncvar->longname + 1, "atitude"))
        {
          ncvar->isLat = true;
          continue;
        }
      }
    }
  }
}

static void
grid_set_chunktype(grid_t *grid, ncvar_t *ncvar)
{
  if (ncvar->isChunked)
  {
    int ndims = ncvar->ndims;
    size_t chunkSizeAllDims = 1;
    for (int i = 0; i < ndims; ++i) chunkSizeAllDims *= ncvar->chunks[i];

    size_t dimN = ncvar->chunks[ndims - 1];
    if (grid->type == GRID_UNSTRUCTURED)
    {
      size_t chunkSize = (chunkSizeAllDims == dimN) ? dimN : 0;
      ncvar->chunkType = (chunkSize == grid->size) ? CDI_CHUNK_GRID : CDI_CHUNK_AUTO;
      if (ncvar->chunkType == CDI_CHUNK_AUTO && chunkSize > 1) ncvar->chunkSize = (int) chunkSize;
    }
    else
    {
      if (grid->x.size > 1 && grid->y.size > 1 && ndims > 1 && grid->x.size == dimN && grid->y.size == ncvar->chunks[ndims - 2])
        ncvar->chunkType = CDI_CHUNK_GRID;
      else if (grid->x.size > 1 && grid->x.size == dimN && chunkSizeAllDims == dimN)
        ncvar->chunkType = CDI_CHUNK_LINES;
      else
        ncvar->chunkType = CDI_CHUNK_AUTO;
    }
  }
}

// define all input grids
static void
cdf_load_vals(size_t size, int ndims, int varid, ncvar_t *ncvar, double **gridvals, struct xyValGet *valsGet, bool hasTimeDim,
              bool readPart, size_t *start, size_t *count)
{
  if (CDI_Netcdf_Lazy_Grid_Load)
  {
    *valsGet = (struct xyValGet) {
      .scalefactor = ncvar->scalefactor,
      .addoffset = ncvar->addoffset,
      .start = { start[0], start[1], start[2] },
      .count = { count[0], count[1], count[2] },
      .size = size,
      .datasetNCId = ncvar->ncid,
      .varNCId = varid,
      .ndims = (short) ndims,
    };
    *gridvals = cdfPendingLoad;
  }
  else
  {
    *gridvals = (double *) Malloc(size * sizeof(double));
    if (hasTimeDim || readPart)
      cdf_get_vara_double(ncvar->ncid, varid, start, count, *gridvals);
    else
      cdf_get_var_double(ncvar->ncid, varid, *gridvals);
    cdf_scale_add(size, *gridvals, ncvar->addoffset, ncvar->scalefactor);
  }
}

#ifndef USE_MPI
static void
cdf_load_cvals(size_t size, int varid, ncvar_t *ncvar, char ***gridvals, size_t dimlength)
{
  size_t startc[] = { 0, 0 };
  size_t countc[] = { 1, size / dimlength };
  *gridvals = (char **) Malloc(dimlength * sizeof(char *));
  for (size_t i = 0; i < dimlength; i++)
  {
    (*gridvals)[i] = (char *) Malloc((size / dimlength) * sizeof(char));
    cdf_get_vara_text(ncvar->ncid, varid, startc, countc, (*gridvals)[i]);
    startc[0] = i + 1;
  }
}
#endif

static void
cdf_load_bounds(size_t size, ncvar_t *ncvar, double **gridbounds, struct cdfLazyGridIds *cellBoundsGet, bool readPart,
                size_t *start, size_t *count)
{
  if (CDI_Netcdf_Lazy_Grid_Load)
  {
    cellBoundsGet->datasetNCId = ncvar->ncid;
    cellBoundsGet->varNCId = ncvar->bounds;
    *gridbounds = cdfPendingLoad;
  }
  else
  {
    *gridbounds = (double *) Malloc(size * sizeof(double));
    if (readPart)
      cdf_get_vara_double(ncvar->ncid, ncvar->bounds, start, count, *gridbounds);
    else
      cdf_get_var_double(ncvar->ncid, ncvar->bounds, *gridbounds);
  }
}

static void
cdf_load_bounds_cube_sphere(size_t bxsize, size_t bysize, size_t size, ncvar_t *ncvar, double **gridbounds,
                            struct cdfLazyGridIds *cellBoundsGet)
{
  if (CDI_Netcdf_Lazy_Grid_Load)
  {
    cellBoundsGet->datasetNCId = ncvar->ncid;
    cellBoundsGet->varNCId = ncvar->bounds;
    *gridbounds = cdfPendingLoad;
  }
  else
  {
    float *bounds = (float *) Malloc(6 * bxsize * bysize * sizeof(float));
    cdf_get_var_float(ncvar->ncid, ncvar->bounds, bounds);

    *gridbounds = (double *) Malloc(size * sizeof(double));
    double *pbounds = *gridbounds;

    size_t m = 0;
    for (size_t k = 0; k < 6; ++k)
    {
      size_t offset = k * bysize * bxsize;
      for (size_t j = 0; j < (bysize - 1); ++j)
        for (size_t i = 0; i < (bxsize - 1); ++i)
        {
          pbounds[m + 0] = bounds[offset + (j + 1) * bxsize + i];
          pbounds[m + 1] = bounds[offset + j * bxsize + i];
          pbounds[m + 2] = bounds[offset + j * bxsize + (i + 1)];
          pbounds[m + 3] = bounds[offset + (j + 1) * bxsize + (i + 1)];
          m += 4;
        }
    }

    Free(bounds);
  }
}

static void
cdf_load_cellarea(size_t size, ncvar_t *ncvar, double **gridarea, struct cdfLazyGridIds *cellAreaGet)
{
  if (CDI_Netcdf_Lazy_Grid_Load)
  {
    cellAreaGet->datasetNCId = ncvar->ncid;
    cellAreaGet->varNCId = ncvar->cellarea;
    *gridarea = cdfPendingLoad;
  }
  else
  {
    *gridarea = (double *) Malloc(size * sizeof(double));
    cdf_get_var_double(ncvar->ncid, ncvar->cellarea, *gridarea);
  }
}

static void
cdf_load_cellindices(size_t size, ncvar_t *ncvar, int64_t **cellIndices, bool readPart, size_t start, size_t count)
{
  *cellIndices = (int64_t *) Malloc(size * sizeof(int64_t));
  if (readPart)
    cdf_get_vara_int64(ncvar->ncid, ncvar->ivarid, &start, &count, *cellIndices);
  else
    cdf_get_var_int64(ncvar->ncid, ncvar->ivarid, *cellIndices);
}

static void
set_cellindices(int64_t *cellIndices, size_t start, size_t count)
{
  for (size_t i = 0; i < count; ++i) { cellIndices[i] = start + i; }
}

static void
cdf_copy_grid_axis_attr(ncvar_t *ncvar, struct gridaxis_t *gridaxis)
{
  cdiDefVarKeyBytes(&gridaxis->keys, CDI_KEY_NAME, (const unsigned char *) ncvar->name, (int) strlen(ncvar->name) + 1);
  if (ncvar->longname[0])
    cdiDefVarKeyBytes(&gridaxis->keys, CDI_KEY_LONGNAME, (const unsigned char *) ncvar->longname,
                      (int) strlen(ncvar->longname) + 1);
  if (ncvar->units[0])
    cdiDefVarKeyBytes(&gridaxis->keys, CDI_KEY_UNITS, (const unsigned char *) ncvar->units, (int) strlen(ncvar->units) + 1);
#ifndef USE_MPI
  if (gridaxis->cvals)
    if (ncvar->stdname[0])
      cdiDefVarKeyBytes(&gridaxis->keys, CDI_KEY_STDNAME, (const unsigned char *) ncvar->stdname, (int) strlen(ncvar->stdname) + 1);
#endif
}

static int
cdf_get_xydimid(int ndims, int *dimids, int *dimtypes, int *xdimid, int *ydimid)
{
  int nxdims = 0, nydims = 0;
  int xdimids[2] = { -1, -1 }, ydimids[2] = { -1, -1 };

  for (int i = 0; i < ndims; i++)
  {
    if (dimtypes[i] == X_AXIS && nxdims < 2)
    {
      xdimids[nxdims] = dimids[i];
      nxdims++;
    }
    else if (dimtypes[i] == Y_AXIS && nydims < 2)
    {
      ydimids[nydims] = dimids[i];
      nydims++;
    }
  }

  if (nxdims == 2)
  {
    *xdimid = xdimids[1];
    *ydimid = xdimids[0];
  }
  else if (nydims == 2)
  {
    *xdimid = ydimids[1];
    *ydimid = ydimids[0];
  }
  else
  {
    *xdimid = xdimids[0];
    *ydimid = ydimids[0];
  }

  return nydims;
}

static void
cdf_check_gridtype(int *gridtype, bool isLon, bool isLat, size_t xsize, size_t ysize, grid_t *grid)
{
  if (grid->y.vals == NULL)
  {
    *gridtype = GRID_GENERIC;
    return;
  }

  if (isLat && (isLon || xsize == 0))
  {
    double yinc = 0.0;
    if (isLon && ysize > 1)
    {
      yinc = fabs(grid->y.vals[0] - grid->y.vals[1]);
      for (size_t i = 2; i < ysize; i++)
        if ((fabs(grid->y.vals[i - 1] - grid->y.vals[i]) - yinc) > (yinc / 1000))
        {
          yinc = 0;
          break;
        }
    }
    if (ysize < 10000 && is_equal(yinc, 0.0) && latitudes_are_gaussian(ysize, grid->y.vals))
    {
      *gridtype = GRID_GAUSSIAN;
      grid->np = (int) (ysize / 2);
    }
    else
    {
      *gridtype = GRID_LONLAT;
    }
  }
  else
  {
    *gridtype = (isLon && !isLat && ysize == 0) ? GRID_LONLAT : GRID_GENERIC;
  }
}

// isX=true  → Lese X-Koordinate (grid->x, lazyGrid->xValsGet)
// isX=false → Lese Y-Koordinate (grid->y, lazyGrid->yValsGet)
// primarySize: Pointer auf die primäre Achsengröße (xsize für X, ysize für Y)
// otherSize:   Wert der anderen Achsengröße  (ysize für X, xsize für Y)
static bool
cdf_read_coord(stream_t *streamP, struct cdfLazyGrid *lazyGrid, ncdim_t *ncdims, ncvar_t *ncvar, int varid, ncvar_t *axisvar,
               size_t *primarySize, size_t otherSize, bool isX, bool hasTimeDim, bool readPart, size_t *start, size_t *count,
               bool *isAxis)
{
  grid_t *grid = &lazyGrid->base;
  bool skipvar = true;
  *isAxis = isX ? axisvar->isLon : axisvar->isLat;
  int ndims = axisvar->ndims;
  size_t size = 0;

  if (ndims == 1 && xtypeIsText(axisvar->xtype))
  {
    ncvar->varStatus = CoordVar;
    Warning("Unsupported %c-coordinate type (char/string), skipped variable %s!", isX ? 'x' : 'y', ncvar->name);
    return true;
  }

  int datatype = cdfInqDatatype(streamP, axisvar->xtype, axisvar->isUnsigned);

  if ((ndims - hasTimeDim) == 2)
  {
    // Check size of 2 dimensional coordinates variables
    int dimid = axisvar->dimids[ndims - 2];
    size_t dimsize1 = ncdims[dimid].len;
    dimid = axisvar->dimids[ndims - 1];
    size_t dimsize2 = ncdims[dimid].len;

    if (datatype == CDI_DATATYPE_UINT8)
    {
      ncvar->gridtype = GRID_CHARXY;
      size = dimsize1 * dimsize2;
      skipvar = (dimsize1 != *primarySize);
    }
    else
    {
      ncvar->gridtype = GRID_CURVILINEAR;
      size = (*primarySize) * otherSize;
      skipvar = (dimsize1 * dimsize2 != size);
    }
  }
  else if ((ndims - hasTimeDim) == 1)
  {
    size = isX ? *primarySize : ((*primarySize) ? *primarySize : otherSize);
    // Check size of 1 dimensional coordinates variables
    int dimid = axisvar->dimids[ndims - 1];
    size_t dimsize = ncdims[dimid].len;
    skipvar = readPart ? false : (dimsize != size);
  }
  else if (ndims == 0 && *primarySize == 0)
  {
    size = *primarySize = 1;
    skipvar = false;
  }
  else if (ncvar->isCubeSphere)
  {
    size = *primarySize;
    skipvar = false;
  }

  if (skipvar)
  {
    Warning("Unsupported array structure, skipped variable %s!", ncvar->name);
    ncvar->varStatus = UndefVar;
    return true;
  }

  if (datatype != -1) grid->datatype = datatype;

  struct gridaxis_t *gaxis = isX ? &grid->x : &grid->y;

  if (datatype == CDI_DATATYPE_UINT8 && !CDI_Netcdf_Lazy_Grid_Load)
  {
#ifndef USE_MPI
    cdf_load_cvals(size, varid, axisvar, &gaxis->cvals, *primarySize);
    gaxis->clength = size / (*primarySize);
#endif
  }
  else if (CDI_Read_Cell_Center)
  {
    if (isX)
      cdf_load_vals(size, ndims, varid, axisvar, &grid->x.vals, &lazyGrid->xValsGet, hasTimeDim, readPart, start, count);
    else
      cdf_load_vals(size, ndims, varid, axisvar, &grid->y.vals, &lazyGrid->yValsGet, hasTimeDim, readPart, start, count);
  }

  cdf_copy_grid_axis_attr(axisvar, gaxis);

  return false;
}

typedef struct
{
  long start;
  long count;
  bool readPart;
} GridPart;

static void
gridpart_init(GridPart *gridPart)
{
  gridPart->start = -1;
  gridPart->count = -1;
  gridPart->readPart = false;
}

static void
cdf_load_bounds_impl(struct cdfLazyGrid *lazyGrid, ncvar_t *ncvar, ncvar_t *ncvars, ncdim_t *ncdims, int timedimid, int axisVarId,
                     int *vdimid, bool readPart, size_t *start, size_t *count, bool isX)
{
  grid_t *grid = &lazyGrid->base;
  size_t size = grid->size;
  struct gridaxis_t *gaxis = isX ? &grid->x : &grid->y;
  gaxis->flag = 1;

  const char axisChar = isX ? 'x' : 'y';
  int bvarid = ncvars[axisVarId].bounds;
  if (bvarid != CDI_UNDEFID)
  {
    int ndims = ncvars[axisVarId].ndims;
    int nbdims = ncvars[bvarid].ndims;
    if (nbdims == 2 || nbdims == 3)
    {
      if (ncvars[bvarid].dimids[0] == timedimid)
      {
        static bool ltwarn = true;
        if (ltwarn) Warning("Time varying grid %c-bounds unsupported, skipped!", axisChar);
        ltwarn = false;
      }
      else if (ncvar->isCubeSphere)
      {
        grid->nvertex = 4;
        size_t bxsize = ncdims[ncvars[bvarid].dimids[2]].len;
        size_t bysize = ncdims[ncvars[bvarid].dimids[1]].len;
        struct cdfLazyGridIds *boundsGet = isX ? &lazyGrid->xBoundsGet : &lazyGrid->yBoundsGet;
        cdf_load_bounds_cube_sphere(bxsize, bysize, size * (size_t) grid->nvertex, &ncvars[axisVarId], &gaxis->bounds, boundsGet);
      }
      else if (nbdims == ndims + 1)
      {
        if (isX || *vdimid == CDI_UNDEFID)
        {
          *vdimid = ncvars[bvarid].dimids[nbdims - 1];
          grid->nvertex = (int) ncdims[*vdimid].len;
        }
        if (readPart)
        {
          start[1] = 0;
          count[1] = (size_t) grid->nvertex;
        }
        struct cdfLazyGridIds *boundsGet = isX ? &lazyGrid->xBoundsGet : &lazyGrid->yBoundsGet;
        cdf_load_bounds(size * (size_t) grid->nvertex, &ncvars[axisVarId], &gaxis->bounds, boundsGet, readPart, start, count);
      }
      else
      {
        CDI_WARN_ONCE("%c-bounds doesn't follow the CF-Convention, skipped!", axisChar);
      }
    }
  }
}

static void
cdf_load_ybounds_reduced(struct cdfLazyGrid *lazyGrid, ncvar_t *ncvar, ncvar_t *ncvars, ncdim_t *ncdims, int yvarid, int *vdimid)
{
  grid_t *grid = &lazyGrid->base;
  size_t size = grid->size;
  int dimid = ncvars[ncvar->rpvarid].dimids[0];
  size_t len = ncdims[dimid].len;
  grid->y.size = len;
  assert(len <= INT_MAX);
  grid->reducedPointsSize = (int) len;
  grid->reducedPoints = (int *) Malloc(len * sizeof(int));
  cdf_get_var_int(ncvar->ncid, ncvar->rpvarid, grid->reducedPoints);
  grid->np = ncvar->numLPE;

  int bvarid = (yvarid == CDI_UNDEFID) ? CDI_UNDEFID : ncvars[yvarid].bounds;
  if (bvarid != CDI_UNDEFID)
  {
    int nbdims = ncvars[bvarid].ndims;
    if (nbdims == 2 || nbdims == 3)
    {
      if (*vdimid == CDI_UNDEFID)
      {
        *vdimid = ncvars[bvarid].dimids[nbdims - 1];
        grid->nvertex = (int) ncdims[*vdimid].len;
      }
      cdf_load_bounds(size * (size_t) grid->nvertex, &ncvars[yvarid], &grid->y.bounds, &lazyGrid->yBoundsGet, false, NULL, NULL);
    }
  }
}

static bool
cdf_read_coordinates(stream_t *streamP, struct cdfLazyGrid *lazyGrid, ncvar_t *ncvar, ncvar_t *ncvars, ncdim_t *ncdims,
                     int timedimid, int xvarid, int yvarid, size_t xsize, size_t ysize, int *vdimid, const GridPart *gridPart)
{
  grid_t *grid = &lazyGrid->base;
  size_t size = 0;
  size_t start[3] = { 0, 0, 0 }, count[3] = { 1, 1, 1 };
  bool readPart = false;

  grid->datatype = CDI_DATATYPE_FLT64;

  if (ncvar->gridtype == GRID_TRAJECTORY)
  {
    if (ncvar->xvarid == CDI_UNDEFID) Error("Longitude coordinates undefined for %s!", ncvar->name);
    if (ncvar->yvarid == CDI_UNDEFID) Error("Latitude coordinates undefined for %s!", ncvar->name);
  }
  else
  {
    bool hasTimeDim = false;

    if (xvarid != CDI_UNDEFID && yvarid != CDI_UNDEFID)
    {
      int ndims = ncvars[xvarid].ndims;
      if (ndims != ncvars[yvarid].ndims && !ncvars[xvarid].isCharAxis && !ncvars[yvarid].isCharAxis)
      {
        Warning("Inconsistent grid structure for variable %s!", ncvar->name);
        ncvar->xvarid = xvarid = CDI_UNDEFID;
        ncvar->yvarid = yvarid = CDI_UNDEFID;
      }
      if (ndims > 1)
      {
        if (ndims <= 3)
        {
          if (ncvars[xvarid].dimids[0] == timedimid && ncvars[yvarid].dimids[0] == timedimid)
          {
            static bool ltwarn = true;
            size_t ntsteps = ncdims[timedimid].len;
            if (ltwarn && ntsteps > 1) Warning("Time varying grids unsupported, using grid at time step 1!");
            ltwarn = false;
            hasTimeDim = true;
            count[1] = ysize;
            count[2] = xsize;
          }
        }
        else
        {
          Warning("Unsupported grid structure for variable %s (grid dims > 2)!", ncvar->name);
          ncvar->xvarid = xvarid = CDI_UNDEFID;
          ncvar->yvarid = yvarid = CDI_UNDEFID;
        }
      }
      else if (gridPart && gridPart->readPart)
      {
        start[0] = (size_t) gridPart->start;
        count[0] = (size_t) gridPart->count;
        readPart = true;
      }
    }
    else if (grid->type == GRID_HEALPIX && gridPart && gridPart->readPart)
    {
      start[0] = (size_t) gridPart->start;
      count[0] = (size_t) gridPart->count;
      readPart = true;
    }

    if (xvarid != CDI_UNDEFID)
    {
      if (!ncvar->isCubeSphere && (ncvars[xvarid].ndims - hasTimeDim) > 2)
      {
        Warning("Coordinates variable %s has too many dimensions (%d), skipped!", ncvars[xvarid].name, ncvars[xvarid].ndims);
        // ncvar->xvarid = CDI_UNDEFID;
        xvarid = CDI_UNDEFID;
      }
    }

    if (yvarid != CDI_UNDEFID)
    {
      if (!ncvar->isCubeSphere && (ncvars[yvarid].ndims - hasTimeDim) > 2)
      {
        Warning("Coordinates variable %s has too many dimensions (%d), skipped!", ncvars[yvarid].name, ncvars[yvarid].ndims);
        // ncvar->yvarid = CDI_UNDEFID;
        yvarid = CDI_UNDEFID;
      }
    }

    bool isLon = false, isLat = false;

    if (xvarid != CDI_UNDEFID)
      if (cdf_read_coord(streamP, lazyGrid, ncdims, ncvar, xvarid, &ncvars[xvarid], &xsize, ysize, true, hasTimeDim, readPart,
                         start, count, &isLon))
        return true;

    if (yvarid != CDI_UNDEFID)
      if (cdf_read_coord(streamP, lazyGrid, ncdims, ncvar, yvarid, &ncvars[yvarid], &ysize, xsize, false, hasTimeDim, readPart,
                         start, count, &isLat))
        return true;

    // clang-format off
    if      (ncvar->gridtype == GRID_UNSTRUCTURED)     size = xsize;
    else if (ncvar->gridtype == GRID_HEALPIX)          size = xsize;
    else if (ncvar->gridtype == GRID_GAUSSIAN_REDUCED) size = xsize;
    else if (ysize == 0)                               size = xsize;
    else if (xsize == 0)                               size = ysize;
    else                                               size = xsize * ysize;
    // clang-format on

    if (ncvar->gridtype == CDI_UNDEFID || ncvar->gridtype == GRID_GENERIC)
      cdf_check_gridtype(&ncvar->gridtype, isLon, isLat, xsize, ysize, grid);
  }

  int gridType = grid->type;
  if (gridType != GRID_PROJECTION) { gridType = ncvar->gridtype; }
  else if (gridType == GRID_PROJECTION && ncvar->gridtype == GRID_LONLAT && ncvar->isLonLatMapping) { gridType = ncvar->gridtype; }

  switch (gridType)
  {
    case GRID_GENERIC:
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
    case GRID_UNSTRUCTURED:
    case GRID_CURVILINEAR:
    case GRID_PROJECTION:
    {
      grid->size = size;
      grid->x.size = xsize;
      grid->y.size = ysize;
      if (xvarid != CDI_UNDEFID && CDI_Read_Cell_Corners)
      {
        cdf_load_bounds_impl(lazyGrid, ncvar, ncvars, ncdims, timedimid, xvarid, vdimid, readPart, start, count, true);
      }
      if (yvarid != CDI_UNDEFID && CDI_Read_Cell_Corners)
      {
        cdf_load_bounds_impl(lazyGrid, ncvar, ncvars, ncdims, timedimid, yvarid, vdimid, readPart, start, count, false);
      }

      if (ncvar->cellarea != CDI_UNDEFID) cdf_load_cellarea(size, ncvar, &grid->area, &lazyGrid->cellAreaGet);

      if (gridType == GRID_GAUSSIAN && ncvar->numLPE > 0) grid->np = ncvar->numLPE;

      break;
    }
    case GRID_HEALPIX:
    {
      grid->size = size;
      grid->x.size = size;
      if (ncvar->ivarid != CDI_UNDEFID) { cdf_load_cellindices(size, ncvar, &grid->indices, readPart, start[0], count[0]); }
      else if (readPart)
      {
        grid->indices = (int64_t *) Malloc(count[0] * sizeof(int64_t));
        set_cellindices(grid->indices, start[0], count[0]);
      }
      break;
    }
    case GRID_GAUSSIAN_REDUCED:
    {
      if (ncvar->numLPE > 0 && ncvar->rpvarid != CDI_UNDEFID && ncvars[ncvar->rpvarid].ndims == 1)
      {
        grid->size = size;
        cdf_load_ybounds_reduced(lazyGrid, ncvar, ncvars, ncdims, yvarid, vdimid);
      }
      break;
    }
    case GRID_SPECTRAL:
    {
      grid->size = size;
      grid->lcomplex = 1;
      grid->trunc = ncvar->truncation;
      break;
    }
    case GRID_FOURIER:
    {
      grid->size = size;
      grid->trunc = ncvar->truncation;
      break;
    }
    case GRID_TRAJECTORY:
    {
      grid->size = 1;
      break;
    }
    case GRID_CHARXY:
    {
      grid->size = size;
      grid->x.size = xsize;
      grid->y.size = ysize;
      break;
    }
  }

  // if ( grid->type != GRID_PROJECTION && grid->type != ncvar->gridtype )
  if (grid->type != gridType)
  {
    // int gridtype = ncvar->gridtype;
    grid->type = gridType;
    cdiGridTypeInit(grid, gridType, grid->size);
  }

  if (grid->size == 0)
  {
    int ndims = ncvar->ndims;
    int *dimtypes = ncvar->dimtypes;
    if (ndims == 0 || (ndims == 1 && dimtypes[0] == T_AXIS) || (ndims == 1 && dimtypes[0] == Z_AXIS)
        || (ndims == 2 && dimtypes[0] == T_AXIS && dimtypes[1] == Z_AXIS))
    {
      grid->type = GRID_GENERIC;
      grid->size = 1;
      grid->x.size = 0;
      grid->y.size = 0;
    }
    else
    {
      Warning("Unsupported grid, skipped variable %s!", ncvar->name);
      ncvar->varStatus = UndefVar;
      return true;
    }
  }

  return false;
}

static bool
cdf_set_unstructured_par(ncvar_t *ncvar, grid_t *grid, int *xdimid, int *ydimid, GridInfo *gridInfo, bool readPart)
{
  int ndims = ncvar->ndims;
  int *dimtypes = ncvar->dimtypes;

  int zdimid = CDI_UNDEFID;
  int xdimidx = CDI_UNDEFID, ydimidx = CDI_UNDEFID;

  for (int i = 0; i < ndims; i++)
  {
    // clang-format off
    if      (dimtypes[i] == X_AXIS) xdimidx = i;
    else if (dimtypes[i] == Y_AXIS) ydimidx = i;
    else if (dimtypes[i] == Z_AXIS) zdimid = ncvar->dimids[i];
    // clang-format on
  }

  if (*xdimid != CDI_UNDEFID && *ydimid != CDI_UNDEFID && zdimid == CDI_UNDEFID)
  {
    if (grid->x.size > grid->y.size && grid->y.size < 1000)
    {
      dimtypes[ydimidx] = Z_AXIS;
      *ydimid = CDI_UNDEFID;
      grid->size = grid->x.size;
      grid->y.size = 0;
    }
    else if (grid->y.size > grid->x.size && grid->x.size < 1000)
    {
      dimtypes[xdimidx] = Z_AXIS;
      *xdimid = *ydimid;
      *ydimid = CDI_UNDEFID;
      grid->size = grid->y.size;
      grid->x.size = grid->y.size;
      grid->y.size = 0;
    }
  }

  if (grid->size != grid->x.size)
  {
    Warning("Unsupported array structure, skipped variable %s!", ncvar->name);
    ncvar->varStatus = UndefVar;
    return true;
  }

  if (!readPart)
  {
    if (gridInfo->number_of_grid_used != CDI_UNDEFID)
      cdiDefVarKeyInt(&grid->keys, CDI_KEY_NUMBEROFGRIDUSED, gridInfo->number_of_grid_used);
    if (ncvar->position > 0) cdiDefVarKeyInt(&grid->keys, CDI_KEY_NUMBEROFGRIDINREFERENCE, ncvar->position);
    if (!cdiUUIDIsNull(gridInfo->uuid)) cdiDefVarKeyBytes(&grid->keys, CDI_KEY_UUID, gridInfo->uuid, CDI_UUID_SIZE);
  }

  return false;
}

static void
cdf_read_mapping_atts(int ncid, int gmapvarid, int nvatts, int projID)
{
  if (cdfCheckAttText(ncid, gmapvarid, "grid_mapping_name"))
  {
    String attString;
    string_init(&attString);
    cdfGetAttString(ncid, gmapvarid, "grid_mapping_name", &attString);
    cdiDefKeyString(projID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, attString.data);
    string_free(&attString);
  }

  bool removeFillValue = true;
  for (int i = 0; i < nvatts; ++i) cdf_set_cdi_attr(ncid, gmapvarid, i, projID, CDI_GLOBAL, removeFillValue);
}

static void
cdf_set_grid_to_similar_vars(ncvar_t *ncvar1, ncvar_t *ncvar2, int gridtype, int xdimid, int ydimid)
{
  if (ncvar2->varStatus == DataVar && ncvar2->gridID == CDI_UNDEFID)
  {
    int xdimid2 = CDI_UNDEFID, ydimid2 = CDI_UNDEFID, zdimid2 = CDI_UNDEFID;
    int xdimidx = CDI_UNDEFID, ydimidx = CDI_UNDEFID;

    const int *dimtypes2 = ncvar2->dimtypes;
    const int *dimids2 = ncvar2->dimids;
    int ndims2 = ncvar2->ndims;
    for (int i = 0; i < ndims2; i++)
    {
      if (dimtypes2[i] == X_AXIS)
      {
        xdimid2 = dimids2[i];
        xdimidx = i;
      }
      else if (dimtypes2[i] == Y_AXIS)
      {
        ydimid2 = dimids2[i];
        ydimidx = i;
      }
      else if (dimtypes2[i] == Z_AXIS) { zdimid2 = dimids2[i]; }
    }

    if (!ncvar2->isCubeSphere)
    {
      if (ncvar2->gridtype == CDI_UNDEFID && gridtype == GRID_UNSTRUCTURED)
      {
        if (xdimid == xdimid2 && ydimid2 != CDI_UNDEFID && zdimid2 == CDI_UNDEFID)
        {
          ncvar2->dimtypes[ydimidx] = Z_AXIS;
          ydimid2 = CDI_UNDEFID;
        }

        if (xdimid == ydimid2 && xdimid2 != CDI_UNDEFID && zdimid2 == CDI_UNDEFID)
        {
          ncvar2->dimtypes[xdimidx] = Z_AXIS;
          xdimid2 = ydimid2;
          ydimid2 = CDI_UNDEFID;
        }
      }
      else if (ncvar2->gridtype == GRID_GAUSSIAN_REDUCED && gridtype == GRID_GAUSSIAN_REDUCED) { ydimid = CDI_UNDEFID; }
    }

    if (xdimid == xdimid2 && (ydimid == ydimid2 || (xdimid == ydimid && ydimid2 == CDI_UNDEFID)))
    {
      bool isSameGrid
          = (ncvar1->xvarid == ncvar2->xvarid && ncvar1->yvarid == ncvar2->yvarid && ncvar1->position == ncvar2->position);

      // if (xvarid != -1 && ncvar2->xvarid != CDI_UNDEFID && xvarid != ncvar2->xvarid) isSameGrid = false;
      // if (yvarid != -1 && ncvar2->yvarid != CDI_UNDEFID && yvarid != ncvar2->yvarid) isSameGrid = false;

      if (isSameGrid)
      {
        if (CDI_Debug) Message("Same gridID %d %s", ncvar1->gridID, ncvar2->name);
        ncvar2->gridID = ncvar1->gridID;
        ncvar2->chunkType = ncvar1->chunkType;
        ncvar2->chunkSize = ncvar1->chunkSize;
        ncvar2->gridSize = ncvar1->gridSize;
        ncvar2->xSize = ncvar1->xSize;
        ncvar2->ySize = ncvar1->ySize;
        ncvar2->xstart = ncvar1->xstart;
        ncvar2->xcount = ncvar1->xcount;
      }
    }
  }
}

static void
destroy_grid(struct cdfLazyGrid *lazyGrid, grid_t *grid)
{
  if (lazyGrid)
  {
    if (CDI_Netcdf_Lazy_Grid_Load) cdfLazyGridDestroy(lazyGrid);
    if (grid)
    {
      grid_free(grid);
      Free(grid);
    }
  }
}

static bool
is_healpix_grid(int ncid, int gmapvarid)
{
  if (gmapvarid == CDI_UNDEFID) return false;
  return cdfCheckAttInt(ncid, gmapvarid, "refinement_level");
}

static int
process_layer_query(struct CdiQuery *query, ncvar_t *ncvar, size_t *zsize, GridPart *layerPart)
{
  // process layer query information if available
  if (query)
  {
    int numLayers = cdiQueryNumLayers(query);
    if (numLayers > 0)
    {
      const int64_t *layerRange = cdiQueryGetLayers(query);
      int64_t startLayer = adjust_negative_index((int64_t) *zsize, layerRange[0]);
      int64_t endLayer = adjust_negative_index((int64_t) *zsize, layerRange[1]);
      if ((size_t) endLayer > *zsize)
      {
        if (*zsize > 1)
          Warning("%s: layer %ld-%ld out of range (numLayers=%zu), skipped", ncvar->name, (long) startLayer, (long) endLayer,
                  *zsize);
        ncvar->varStatus = UndefVar;
        return -1;
      }

      int64_t count = endLayer - startLayer + 1;
      *zsize = (size_t) count;
      ncvar->zstart = (size_t) startLayer - 1;
      ncvar->zcount = count;
      layerPart->start = (long) startLayer - 1;
      layerPart->count = (long) count;
      layerPart->readPart = true;
    }
  }

  return 0;
}

static int
process_cell_query(struct CdiQuery *query, int xdimid, int ydimid, ncvar_t *ncvar, size_t *xsize, size_t *ysize, GridPart *gridPart)
{
  // process cell query information if available
  if (query)
  {
    int numCells = cdiQueryNumCells(query);
    if (numCells > 0)
    {
      // if (ncvar->gridtype != GRID_UNSTRUCTURED)
      if (xdimid != CDI_UNDEFID && ydimid != CDI_UNDEFID && xdimid != ydimid)
      {
        Warning("Query parameter cell is only available for 1D grids, skipped variable %s!", ncvar->name);
        ncvar->varStatus = UndefVar;
        return -1;
      }

      const int64_t *cellRange = cdiQueryGetCells(query);
      int64_t startCell = adjust_negative_index((int64_t) *xsize, cellRange[0]);
      int64_t endCell = adjust_negative_index((int64_t) *xsize, cellRange[1]);
      if ((size_t) endCell > *xsize)
      {
        Error("%s: cells %ld-%ld out of range (gridsize=%zu)", ncvar->name, (long) startCell, (long) endCell, *xsize);
      }

      int64_t count = endCell - startCell + 1;
      *xsize = (size_t) count;
      *ysize = 0;
      ncvar->xstart = (size_t) startCell - 1;
      ncvar->xcount = count;
      gridPart->start = (long) startCell - 1;
      gridPart->count = (long) count;
      gridPart->readPart = true;
    }
  }

  return 0;
}

static int
cdf_define_all_grids(stream_t *streamP, int vlistID, ncdim_t *ncdims, int nvars, ncvar_t *ncvars, GridInfo *gridInfo)
{
  for (int ncvarid = 0; ncvarid < nvars; ++ncvarid)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->varStatus != DataVar || ncvar->gridID != CDI_UNDEFID) { continue; }

    int ndims = ncvar->ndims;
    int *dimtypes = ncvar->dimtypes;
    int vdimid = CDI_UNDEFID;
    struct addIfNewRes projAdded = { .Id = CDI_UNDEFID, .isNew = 0 }, gridAdded = { .Id = CDI_UNDEFID, .isNew = 0 };
    int xdimid = CDI_UNDEFID, ydimid = CDI_UNDEFID;
    int nydims = cdf_get_xydimid(ndims, ncvar->dimids, dimtypes, &xdimid, &ydimid);

    int xaxisid = (xdimid != CDI_UNDEFID) ? ncdims[xdimid].ncvarid : CDI_UNDEFID;
    int yaxisid = (ydimid != CDI_UNDEFID) ? ncdims[ydimid].ncvarid : CDI_UNDEFID;
    int xvarid = (ncvar->xvarid != CDI_UNDEFID) ? ncvar->xvarid : xaxisid;
    int yvarid = (ncvar->yvarid != CDI_UNDEFID) ? ncvar->yvarid : yaxisid;

    size_t xsize = (xdimid != CDI_UNDEFID) ? ncdims[xdimid].len : 0;
    size_t ysize = (ydimid != CDI_UNDEFID) ? ncdims[ydimid].len : 0;

    if (ydimid == CDI_UNDEFID && yvarid != CDI_UNDEFID)
    {
      if (ncvars[yvarid].ndims == 1)
      {
        ydimid = ncvars[yvarid].dimids[0];
        ysize = ncdims[ydimid].len;
      }
    }

    int gmapvarid = ncvar->gmapid;
    bool lproj = (gmapvarid != CDI_UNDEFID);
    bool isHealpixGrid = (lproj && ncvar->isHealpixMapping) ? is_healpix_grid(ncvars[gmapvarid].ncid, gmapvarid) : false;
    if (isHealpixGrid) { ncvar->gridtype = GRID_HEALPIX; }
    if (isHealpixGrid && xvarid != CDI_UNDEFID && ncvar->ivarid == CDI_UNDEFID)
    {
      ncvar->ivarid = xvarid;
      xvarid = CDI_UNDEFID;
    }

    if (!lproj && xaxisid != CDI_UNDEFID && xaxisid != xvarid && yaxisid != CDI_UNDEFID && yaxisid != yvarid) lproj = true;

    if (ncvar->isCubeSphere && lproj && xvarid != CDI_UNDEFID && yvarid != CDI_UNDEFID && ncvars[xvarid].ndims == 3
        && ncvars[yvarid].ndims == 3)
    {
      lproj = false;
      ncvar->gridtype = GRID_UNSTRUCTURED;
      xsize = xsize * ysize * 6;
      ysize = xsize;
    }

    bool lgrid = !(lproj && ncvar->xvarid == CDI_UNDEFID);

    bool isUnstructured = (xdimid != CDI_UNDEFID && xdimid == ydimid && nydims == 0);
    if ((ncvar->gridtype == CDI_UNDEFID || ncvar->gridtype == GRID_GENERIC) && isUnstructured) ncvar->gridtype = GRID_UNSTRUCTURED;

    struct cdfLazyGrid *lazyGrid = NULL, *lazyProj = NULL;

    {
      int gridtype = (!lgrid && !isHealpixGrid) ? GRID_PROJECTION : ncvar->gridtype;
      if (CDI_Netcdf_Lazy_Grid_Load)
      {
        cdfLazyGridRenew(&lazyGrid, gridtype);
        if (lgrid && lproj) cdfLazyGridRenew(&lazyProj, GRID_PROJECTION);
      }
      else
      {
        cdfBaseGridRenew(&lazyGrid, gridtype);
        if (lgrid && lproj) cdfBaseGridRenew(&lazyProj, GRID_PROJECTION);
      }
    }
    grid_t *grid = &lazyGrid->base;
    grid_t *proj = (lgrid && lproj) ? &lazyProj->base : NULL;

    xaxisid = (xdimid != CDI_UNDEFID) ? ncdims[xdimid].ncvarid : CDI_UNDEFID;
    yaxisid = (ydimid != CDI_UNDEFID) ? ncdims[ydimid].ncvarid : CDI_UNDEFID;

    // process cell query information if available
    GridPart gridPart;
    gridpart_init(&gridPart);
    if (process_cell_query(streamP->query, xdimid, ydimid, ncvar, &xsize, &ysize, &gridPart) < 0) { continue; }

    if (cdf_read_coordinates(streamP, lazyGrid, ncvar, ncvars, ncdims, gridInfo->timedimid, xvarid, yvarid, xsize, ysize, &vdimid,
                             &gridPart))
    {
      continue;
    }

    if (gridInfo->number_of_grid_used != CDI_UNDEFID && (grid->type == CDI_UNDEFID || grid->type == GRID_GENERIC)
        && xdimid != CDI_UNDEFID && xsize > 999)
      grid->type = GRID_UNSTRUCTURED;

    if (!ncvar->isCubeSphere && grid->type == GRID_UNSTRUCTURED)
      if (cdf_set_unstructured_par(ncvar, grid, &xdimid, &ydimid, gridInfo, gridPart.readPart)) continue;

    if (lgrid && lproj)
    {
      int dimid;
      cdf_read_coordinates(streamP, lazyProj, ncvar, ncvars, ncdims, gridInfo->timedimid, xaxisid, yaxisid, xsize, ysize, &dimid,
                           NULL);
    }

    if (CDI_Debug)
    {
      Message("grid: type = %d, size = %zu, nx = %zu, ny = %zu", grid->type, grid->size, grid->x.size, grid->y.size);
      if (proj) Message("proj: type = %d, size = %zu, nx = %zu, ny = %zu", proj->type, proj->size, proj->x.size, proj->y.size);
    }

    if (lgrid && lproj)
    {
      projAdded = cdiVlistAddGridIfNew(vlistID, proj, 2);
      grid->proj = projAdded.Id;
    }

    gridAdded = cdiVlistAddGridIfNew(vlistID, grid, 1);
    ncvar->gridID = gridAdded.Id;
    ncvar->gridSize = grid->size;
    ncvar->xSize = grid->x.size;
    ncvar->ySize = grid->y.size;

    int gridID = ncvar->gridID;

    if (grid->type == GRID_HEALPIX && ncvar->ivarid != CDI_UNDEFID && !xtypeIsInt64(ncvars[ncvar->ivarid].xtype))
    {
      cdiDefKeyInt(gridID, CDI_GLOBAL, CDI_KEY_DATATYPE, CDI_DATATYPE_INT32);
    }

    if (lproj && gmapvarid != CDI_UNDEFID)
    {
      bool gridIsNew = lgrid ? projAdded.isNew : gridAdded.isNew;
      if (gridIsNew)
      {
        int projID = lgrid ? grid->proj : gridID;
        int ncid = ncvars[gmapvarid].ncid;
        int gmapvartype = ncvars[gmapvarid].xtype;
        int nvatts = ncvars[gmapvarid].nattsNC;
        cdiDefKeyInt(projID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARTYPE, gmapvartype);
        const char *gmapvarname = ncvars[gmapvarid].name;
        cdf_read_mapping_atts(ncid, gmapvarid, nvatts, projID);
        cdiDefKeyString(projID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARNAME, gmapvarname);
        gridVerifyProj(projID);
      }
    }

    if (grid->type == GRID_UNSTRUCTURED && gridInfo->gridfile[0] != 0 && !gridPart.readPart)
      gridDefReference(gridID, gridInfo->gridfile);

    if (ncvar->isChunked) grid_set_chunktype(grid, ncvar);

    int gridIndex = vlistGridIndex(vlistID, gridID);
    CdfGrid *cdfGrid = &streamP->cdfInfo.cdfGridList[gridIndex];
    if (gridPart.readPart)
    {
      cdfGrid->start = gridPart.start;
      cdfGrid->count = gridPart.count;
    }
    cdfGrid->gridID = gridID;
    if (grid->type == GRID_TRAJECTORY)
    {
      cdfGrid->ncIdList[CDF_VARID_X] = xvarid;
      cdfGrid->ncIdList[CDF_VARID_Y] = yvarid;
    }
    else
    {
      if (xdimid != CDI_UNDEFID) { cdfGrid->ncIdList[CDF_DIMID_X] = ncdims[xdimid].dimid; }
      if (ydimid != CDI_UNDEFID) { cdfGrid->ncIdList[CDF_DIMID_Y] = ncdims[ydimid].dimid; }
      if (ncvar->isCubeSphere) { cdfGrid->ncIdList[CDF_DIMID_E] = ncdims[ncvar->dimids[ndims - 3]].dimid; }
    }

    if (xdimid == CDI_UNDEFID && ydimid == CDI_UNDEFID && grid->size == 1) gridDefHasDims(gridID, CoordVar);

    int xvarID = (grid->type == GRID_HEALPIX && ydimid == CDI_UNDEFID) ? CDI_GLOBAL : CDI_XAXIS;
    if (xdimid != CDI_UNDEFID) cdiDefKeyString(gridID, xvarID, CDI_KEY_DIMNAME, ncdims[xdimid].name);
    if (ydimid != CDI_UNDEFID) cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_DIMNAME, ncdims[ydimid].name);
    if (vdimid != CDI_UNDEFID) cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_VDIMNAME, ncdims[vdimid].name);

    if (xvarid != CDI_UNDEFID && ncvars[xvarid].stdname[0])
      cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_STDNAME, ncvars[xvarid].stdname);
    if (yvarid != CDI_UNDEFID && ncvars[yvarid].stdname[0])
      cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_STDNAME, ncvars[yvarid].stdname);

    if (CDI_Debug) Message("gridID %d %d %s", gridID, ncvarid, ncvar->name);

    for (int ncvarid2 = ncvarid + 1; ncvarid2 < nvars; ncvarid2++)
      cdf_set_grid_to_similar_vars(ncvar, &ncvars[ncvarid2], grid->type, xdimid, ydimid);

    if (gridAdded.isNew) { lazyGrid = NULL; }
    if (projAdded.isNew) { lazyProj = NULL; }

    if (lazyGrid) destroy_grid(lazyGrid, grid);
    if (lazyProj) destroy_grid(lazyProj, proj);
  }

  return 0;
}

// define all input zaxes
static int
cdf_define_all_zaxes(stream_t *streamP, int vlistID, ncdim_t *ncdims, int nvars, ncvar_t *ncvars, size_t vctsize_echam,
                     double *vct_echam, unsigned char *uuidOfVGrid)
{
  size_t vctsize = vctsize_echam;
  double *vct = vct_echam;

  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->varStatus == DataVar && ncvar->zaxisID == CDI_UNDEFID)
    {
      bool isScalar = false;
      int zdimid = CDI_UNDEFID;
      int zvarid = CDI_UNDEFID;
      size_t zsize = 1;
      int psvarid = -1;
      int p0varid = -1;

      int positive = 0;
      int ndims = ncvar->ndims;

      if (ncvar->zvarid != -1 && ncvars[ncvar->zvarid].ndims == 0)
      {
        zvarid = ncvar->zvarid;
        isScalar = true;
      }
      else
      {
        for (int i = 0; i < ndims; i++)
        {
          if (ncvar->dimtypes[i] == Z_AXIS) zdimid = ncvar->dimids[i];
        }

        if (zdimid != CDI_UNDEFID)
        {
          // zvarid = ncdims[zdimid].ncvarid;
          zvarid = (ncvar->zvarid != CDI_UNDEFID) ? ncvar->zvarid : ncdims[zdimid].ncvarid;
          zsize = ncdims[zdimid].len;
        }
      }

      if (CDI_Debug) Message("nlevs = %zu", zsize);

      // process layer query information if available
      GridPart layerPart;
      gridpart_init(&layerPart);
      if (process_layer_query(streamP->query, ncvar, &zsize, &layerPart) < 0) { continue; }

      double *zvar = NULL;
      char **zcvals = NULL;
      size_t zclength = 0;

      int zaxisType = CDI_UNDEFID;
      if (zvarid != CDI_UNDEFID) zaxisType = ncvars[zvarid].zaxistype;
      if (zaxisType == CDI_UNDEFID) zaxisType = ZAXIS_GENERIC;

      int zdatatype = CDI_DATATYPE_FLT64;
      double *lbounds = NULL;
      double *ubounds = NULL;

      char *pname = NULL, *plongname = NULL, *punits = NULL, *pstdname = NULL;
      if (zvarid != CDI_UNDEFID)
      {
        ncvar_t *zncvar = &ncvars[zvarid];
        positive = zncvar->positive;
        pname = zncvar->name;
        plongname = zncvar->longname;
        punits = zncvar->units;
        pstdname = zncvar->stdname;
        // clang-format off
        if      (zncvar->xtype == NC_FLOAT) { zdatatype = CDI_DATATYPE_FLT32; }
        else if (zncvar->xtype == NC_INT)   { zdatatype = CDI_DATATYPE_INT32; }
        else if (zncvar->xtype == NC_SHORT) { zdatatype = CDI_DATATYPE_INT16; }
        // clang-format on

#ifndef USE_MPI
        if (zaxisType == ZAXIS_CHAR && ncvars[zvarid].ndims == 2)
        {
          zdatatype = CDI_DATATYPE_UINT8;
          zclength = ncdims[ncvars[zvarid].dimids[1]].len;
          cdf_load_cvals(zsize * zclength, zvarid, ncvar, &zcvals, zsize);
        }
#endif

        if ((zaxisType == ZAXIS_HYBRID || zaxisType == ZAXIS_HYBRID_HALF) && ncvars[zvarid].vct)
        {
          vct = ncvars[zvarid].vct;
          vctsize = ncvars[zvarid].vctsize;
          if (ncvars[zvarid].psvarid != -1) psvarid = ncvars[zvarid].psvarid;
          if (ncvars[zvarid].p0varid != -1) p0varid = ncvars[zvarid].p0varid;
        }

        if (zaxisType != ZAXIS_CHAR)
        {
          zvar = (double *) Malloc(zsize * sizeof(double));
          if (layerPart.count > 0 && layerPart.count == (long) zsize)
          {
            size_t start = layerPart.start;
            size_t count = layerPart.count;
            cdf_get_vara_double(ncvars[zvarid].ncid, zvarid, &start, &count, zvar);
          }
          else
          {
            cdf_get_var_double(ncvars[zvarid].ncid, zvarid, zvar);
          }
        }

        int boundsId = ncvars[zvarid].bounds;
        if (boundsId != CDI_UNDEFID)
        {
          int nbdims = ncvars[boundsId].ndims;
          if (nbdims == 2 || isScalar)
          {
            size_t nlevel = isScalar ? 1 : ncdims[ncvars[boundsId].dimids[0]].len;
            int nvertex = (int) ncdims[ncvars[boundsId].dimids[1 - isScalar]].len;
            if (nlevel == zsize && nvertex == 2)
            {
              lbounds = (double *) Malloc(4 * nlevel * sizeof(double));
              ubounds = lbounds + nlevel;
              double *zbounds = lbounds + 2 * nlevel;
              cdf_get_var_double(ncvars[zvarid].ncid, boundsId, zbounds);
              for (size_t i = 0; i < nlevel; ++i)
              {
                lbounds[i] = zbounds[i * 2];
                ubounds[i] = zbounds[i * 2 + 1];
              }
            }
          }
        }
      }
      else
      {
        pname = (zdimid != CDI_UNDEFID) ? ncdims[zdimid].name : NULL;

        if (zsize == 1 && zdimid == CDI_UNDEFID)
        {
          zaxisType = (ncvar->zaxistype != CDI_UNDEFID) ? ncvar->zaxistype : ZAXIS_SURFACE;
          // if ( pname )
          {
            zvar = (double *) Malloc(sizeof(double));
            zvar[0] = 0;
          }
        }
      }

      if (zsize > INT_MAX)
      {
        Warning("Size limit exceeded for z-axis dimension (limit=%d)!", INT_MAX);
        return CDI_EDIMSIZE;
      }

      ncvar->zaxisID = varDefZaxis(vlistID, zaxisType, (int) zsize, zvar, (const char **) zcvals, zclength, lbounds, ubounds,
                                   (int) vctsize, vct, pname, plongname, punits, zdatatype, 1, 0, -1);
      ncvar->zSize = zsize;

      int zaxisID = ncvar->zaxisID;

      int zaxisIndex = vlistZaxisIndex(vlistID, zaxisID);
      CdfZaxis *cdfZaxis = &streamP->cdfInfo.cdfZaxisList[zaxisIndex];
      if (layerPart.readPart)
      {
        cdfZaxis->start = layerPart.start;
        cdfZaxis->count = layerPart.count;
        cdfZaxis->zaxisID = zaxisID;
      }

      if (CDI_CMOR_Mode && zsize == 1 && zaxisType != ZAXIS_HYBRID) zaxisDefScalar(zaxisID);

      if (pstdname && *pstdname)
        cdiDefKeyBytes(zaxisID, CDI_GLOBAL, CDI_KEY_STDNAME, (const unsigned char *) pstdname, (int) strlen(pstdname) + 1);

      if (!cdiUUIDIsNull(uuidOfVGrid)) cdiDefKeyBytes(zaxisID, CDI_GLOBAL, CDI_KEY_UUID, uuidOfVGrid, CDI_UUID_SIZE);

      if (zaxisType == ZAXIS_HYBRID)
      {
        if (psvarid != -1) cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_PSNAME, ncvars[psvarid].name);
        if (p0varid != -1)
        {
          double px = 1;
          cdf_get_var_double(ncvars[p0varid].ncid, p0varid, &px);
          cdiDefKeyFloat(zaxisID, CDI_GLOBAL, CDI_KEY_P0VALUE, px);
          cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_P0NAME, ncvars[p0varid].name);
        }
      }

      if (positive > 0) zaxisDefPositive(zaxisID, positive);
      if (isScalar) zaxisDefScalar(zaxisID);

      if (zdimid != CDI_UNDEFID) cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_DIMNAME, ncdims[zdimid].name);

      Free(zvar);
      if (zcvals)
      {
        for (size_t i = 0; i < zsize; i++) Free(zcvals[i]);
        Free(zcvals);
      }
      Free(lbounds);

      if (zvarid != CDI_UNDEFID)
      {
        int ncid = ncvars[zvarid].ncid;
        int nvatts = ncvars[zvarid].natts;
        for (int iatt = 0; iatt < nvatts; ++iatt)
        {
          int attnum = ncvars[zvarid].atts[iatt];
          bool removeFillValue = true;
          cdf_set_cdi_attr(ncid, zvarid, attnum, zaxisID, CDI_GLOBAL, removeFillValue);
        }
      }

      streamP->cdfInfo.zaxisIdList[zaxisIndex] = (zdimid >= 0) ? ncdims[zdimid].dimid : zdimid;

      if (CDI_Debug) Message("zaxisID %d %d %s", zaxisID, ncvarid, ncvar->name);

      for (int ncvarid2 = ncvarid + 1; ncvarid2 < nvars; ncvarid2++)
        if (ncvars[ncvarid2].varStatus == DataVar
            && ncvars[ncvarid2].zaxisID == CDI_UNDEFID /*&& ncvars[ncvarid2].zaxistype == CDI_UNDEFID*/)
        {
          int zvarid2 = CDI_UNDEFID;
          if (ncvars[ncvarid2].zvarid != CDI_UNDEFID && ncvars[ncvars[ncvarid2].zvarid].ndims == 0)
            zvarid2 = ncvars[ncvarid2].zvarid;

          int zdimid2 = CDI_UNDEFID;
          int ndims2 = ncvars[ncvarid2].ndims;
          for (int i = 0; i < ndims2; i++)
          {
            if (ncvars[ncvarid2].dimtypes[i] == Z_AXIS) zdimid2 = ncvars[ncvarid2].dimids[i];
          }

          if (zdimid == zdimid2 /* && zvarid == zvarid2 */)
          {
            if ((zdimid != CDI_UNDEFID && ncvars[ncvarid2].zaxistype == CDI_UNDEFID)
                || (zdimid == CDI_UNDEFID && zvarid != CDI_UNDEFID && zvarid == zvarid2)
                || (zdimid == CDI_UNDEFID && zaxisType == ncvars[ncvarid2].zaxistype)
                || (zdimid == CDI_UNDEFID && zvarid2 == CDI_UNDEFID && ncvars[ncvarid2].zaxistype == CDI_UNDEFID))
            {
              if (CDI_Debug) Message("zaxisID %d %d %s", zaxisID, ncvarid2, ncvars[ncvarid2].name);
              ncvars[ncvarid2].zaxisID = zaxisID;
              ncvars[ncvarid2].zSize = zsize;
            }
          }
        }
    }
  }

  return 0;
}

struct cdf_varinfo
{
  int varid;
  const char *name;
};

static int
cdf_cmp_varname(const void *s1, const void *s2)
{
  const struct cdf_varinfo *x = (const struct cdf_varinfo *) s1;
  const struct cdf_varinfo *y = (const struct cdf_varinfo *) s2;
  return strcmp(x->name, y->name);
}

static void
cdf_sort_varnames(int *varids, int nvars, const ncvar_t *ncvars)
{
  struct cdf_varinfo *varInfo = (struct cdf_varinfo *) Malloc((size_t) nvars * sizeof(struct cdf_varinfo));

  for (int varID = 0; varID < nvars; varID++)
  {
    int ncvarid = varids[varID];
    varInfo[varID].varid = ncvarid;
    varInfo[varID].name = ncvars[ncvarid].name;
  }
  qsort(varInfo, (size_t) nvars, sizeof(varInfo[0]), cdf_cmp_varname);
  for (int varID = 0; varID < nvars; varID++) { varids[varID] = varInfo[varID].varid; }
  Free(varInfo);
  if (CDI_Debug)
    for (int i = 0; i < nvars; i++) Message("sorted varids[%d] = %d", i, varids[i]);
}

static void
cdf_define_code_and_param(int vlistID, int varID)
{
  if (vlistInqVarCode(vlistID, varID) == -varID - 1)
  {
    char name[CDI_MAX_NAME] = { 0 };
    vlistInqVarName(vlistID, varID, name);
    size_t len = strlen(name);
    if (len > 3 && isdigit((int) name[3]))
    {
      if (strStartsWith(name, "var")) vlistDefVarCode(vlistID, varID, atoi(name + 3));
    }
    else if (len > 4 && isdigit((int) name[4]))
    {
      if (strStartsWith(name, "code")) vlistDefVarCode(vlistID, varID, atoi(name + 4));
    }
    else if (len > 5 && isdigit((int) name[5]))
    {
      if (strStartsWith(name, "param"))
      {
        int pnum = -1, pcat = 255, pdis = 255;
        sscanf(name + 5, "%d.%d.%d", &pnum, &pcat, &pdis);
        vlistDefVarParam(vlistID, varID, cdiEncodeParam(pnum, pcat, pdis));
      }
    }
  }
}

static void
cdf_define_institut_and_model_id(int vlistID, int varID)
{
  int varInstID = vlistInqVarInstitut(vlistID, varID);
  int varModelID = vlistInqVarModel(vlistID, varID);
  int varTableID = vlistInqVarTable(vlistID, varID);
  int code = vlistInqVarCode(vlistID, varID);
  if (CDI_Default_TableID != CDI_UNDEFID)
  {
    char name[CDI_MAX_NAME] = { 0 };
    char longname[CDI_MAX_NAME] = { 0 };
    char units[CDI_MAX_NAME] = { 0 };
    tableInqEntry(CDI_Default_TableID, code, -1, name, longname, units);
    if (name[0])
    {
      cdiDeleteKey(vlistID, varID, CDI_KEY_NAME);
      cdiDeleteKey(vlistID, varID, CDI_KEY_LONGNAME);
      cdiDeleteKey(vlistID, varID, CDI_KEY_UNITS);

      if (varTableID != CDI_UNDEFID)
      {
        cdiDefKeyString(vlistID, varID, CDI_KEY_NAME, name);
        if (longname[0]) cdiDefKeyString(vlistID, varID, CDI_KEY_LONGNAME, longname);
        if (units[0]) cdiDefKeyString(vlistID, varID, CDI_KEY_UNITS, units);
      }
      else
      {
        varTableID = CDI_Default_TableID;
      }
    }

    if (CDI_Default_ModelID != CDI_UNDEFID) varModelID = CDI_Default_ModelID;
    if (CDI_Default_InstID != CDI_UNDEFID) varInstID = CDI_Default_InstID;
  }
  if (varInstID != CDI_UNDEFID) vlistDefVarInstitut(vlistID, varID, varInstID);
  if (varModelID != CDI_UNDEFID) vlistDefVarModel(vlistID, varID, varModelID);
  if (varTableID != CDI_UNDEFID) vlistDefVarTable(vlistID, varID, varTableID);
}

static inline size_t
size_of_dim_chunks(size_t n, size_t c)
{
  return (n / c + (n % c > 0)) * c;
}

typedef struct
{
  size_t cacheSize;
  size_t start[MAX_DIMENSIONS];
  size_t count[MAX_DIMENSIONS];
  int numDims;
} ChunkCacheValues;

static void
chunkCacheValues_init(ChunkCacheValues *chunkCacheValues)
{
  chunkCacheValues->cacheSize = 0;
  chunkCacheValues->numDims = 0;
  memset(chunkCacheValues->start, 0, sizeof(chunkCacheValues->start));
  memset(chunkCacheValues->count, 0, sizeof(chunkCacheValues->count));
}

static void
get_chunk_sizes_for_dims(ncvar_t *ncvar, size_t *cx, size_t *cy, size_t *cz)
{
  *cx = *cy = *cz = 0;
  for (int i = 0; i < ncvar->ndims; i++)
  {
    int dimtype = ncvar->dimtypes[i];
    // clang-format off
    if      (dimtype == Z_AXIS) *cz = ncvar->chunks[i];
    else if (dimtype == Y_AXIS) *cy = ncvar->chunks[i];
    else if (dimtype == X_AXIS) *cx = ncvar->chunks[i];
    // clang-format on
  }
}

static ChunkCacheValues
calc_chunk_cache_size(int timedimid, ncvar_t *ncvar)
{
  ChunkCacheValues chunkCacheValues;
  chunkCacheValues_init(&chunkCacheValues);
  int numDims = 0;
  size_t nx = ncvar->xSize, ny = ncvar->ySize, nz = ncvar->zSize;
  size_t cx = 0, cy = 0, cz = 0;
  get_chunk_sizes_for_dims(ncvar, &cx, &cy, &cz);

  size_t numSteps = (ncvar->dimids[0] == timedimid) ? ncvar->chunks[0] : 1;
  size_t chunkCacheSize = numSteps;
  size_t numLevels = 0;
  if (nz > 0 && cz > 0)
  {
    numLevels = (numSteps == 1) ? cz : size_of_dim_chunks(nz, cz);
    chunkCacheSize *= numLevels;
  }

  if (chunkCacheSize == 1) { chunkCacheSize = 0; }  // no chunk cache needed because the full field is read
  else
  {
    if (ny > 0 && cy > 0) { chunkCacheSize *= size_of_dim_chunks(ny, cy); }
    if (nx > 0 && cx > 0) { chunkCacheSize *= size_of_dim_chunks(nx, cx); }

    chunkCacheSize *= cdf_xtype_to_numbytes(ncvar->xtype);
    if (CDI_CacheSize_Max > 0 && chunkCacheSize > CDI_CacheSize_Max) { chunkCacheSize = CDI_CacheSize_Max; }
  }

  chunkCacheValues.cacheSize = chunkCacheSize;
  chunkCacheValues.numDims = numDims;
  return chunkCacheValues;
}

static ChunkCacheValues
get_chunk_cache_size(int timedimid, ncvar_t *ncvar)
{
  ChunkCacheValues chunkCacheValues;
  chunkCacheValues_init(&chunkCacheValues);
  int numDims = 0;
  size_t nxc = ncvar->xcount, nzc = ncvar->zcount;
  size_t nx = ncvar->xSize, ny = ncvar->ySize, nz = ncvar->zSize;
  size_t cx = 0, cy = 0, cz = 0;
  get_chunk_sizes_for_dims(ncvar, &cx, &cy, &cz);

  size_t numSteps = (ncvar->dimids[0] == timedimid) ? ncvar->chunks[0] : 0;
  size_t chunkCacheSize = (numSteps == 0) ? 1 : numSteps;
  size_t numLevels = 0;
  if (nz > 0 && cz > 0)
  {
    numLevels = (nzc > 0) ? nzc : ((numSteps == 0 || numSteps == 1) ? cz : nz);
    chunkCacheSize *= numLevels;
  }

  if (chunkCacheSize == 1) { chunkCacheSize = 0; }  // no chunk cache needed because the full field is read
  else
  {
    if (numSteps > 0) { chunkCacheValues.count[numDims++] = numSteps; }
    if (numLevels > 0)
    {
      if (nzc > 0) chunkCacheValues.start[numDims] = ncvar->zstart;
      chunkCacheValues.count[numDims++] = numLevels;
    }
    if (ny > 0 && cy > 0)
    {
      size_t size = ny;
      chunkCacheSize *= size;
      chunkCacheValues.count[numDims++] = size;
    }
    if (nx > 0 && cx > 0)
    {
      size_t size = (nxc > 0) ? nxc : nx;
      chunkCacheSize *= size;
      if (nxc > 0) chunkCacheValues.start[numDims] = ncvar->xstart;
      chunkCacheValues.count[numDims++] = size;
    }

    chunkCacheSize *= cdf_xtype_to_numbytes(ncvar->xtype);
    if (CDI_CacheSize_Max > 0 && chunkCacheSize > CDI_CacheSize_Max) { chunkCacheSize = CDI_CacheSize_Max; }
  }

  chunkCacheValues.cacheSize = chunkCacheSize;
  chunkCacheValues.numDims = numDims;
  return chunkCacheValues;
}

static void
cdf_set_var_chunk_cache(ncvar_t *ncvar, int ncvarid, size_t chunkCacheSize)
{
  if (CDI_Debug || CDI_Cache_Info) Message("%s: chunkCacheSize=%zu", ncvar->name, chunkCacheSize);
  nc_set_var_chunk_cache(ncvar->ncid, ncvarid, chunkCacheSize, ncvar->chunkCacheNelems, ncvar->chunkCachePreemption);
}

static void
cdf_init_cache(CdfCache *cache)
{
  cache->buffer = NULL;
  memset(cache->start, 0, sizeof(cache->start));
  memset(cache->count, 0, sizeof(cache->count));
  cache->bufferSize = 0;
  cache->cacheSize = 0;
  cache->numSteps = 0;
  cache->maxSteps = 0;
  cache->numDims = 0;
}

// define all input data variables
static void
cdf_define_all_vars(stream_t *streamP, int vlistID, int instID, int modelID, int nvarsData, int num_ncvars, ncvar_t *ncvars,
                    ncdim_t *ncdims, int timedimid)
{
  int varidsLocal[256];
  int *varids = (nvarsData <= (int) ARRAY_SIZE(varidsLocal)) ? varidsLocal : (int *) Malloc((size_t) nvarsData * sizeof(int));
  int n = 0;
  for (int ncvarid = 0; ncvarid < num_ncvars; ncvarid++)
    if (ncvars[ncvarid].varStatus == DataVar) { varids[n++] = ncvarid; }

  if (CDI_Debug)
    for (int i = 0; i < nvarsData; i++) Message("varids[%d] = %d", i, varids[i]);

  if (streamP->sortname) cdf_sort_varnames(varids, nvarsData, ncvars);

  for (int varID1 = 0; varID1 < nvarsData; varID1++)
  {
    int ncvarid = varids[varID1];
    ncvar_t *ncvar = &ncvars[ncvarid];
    int gridID = ncvar->gridID;
    int zaxisID = ncvar->zaxisID;

    stream_new_var(streamP, gridID, zaxisID, CDI_UNDEFID);
    int varID = vlistDefVar(vlistID, gridID, zaxisID, ncvar->timetype);
    ncvar->cdiVarID = varID;

    if (ncvar->hasFilter) cdiDefKeyString(vlistID, varID, CDI_KEY_FILTERSPEC_IN, ncvar->filterSpec);
    if (ncvar->hasFilter) vlistDefVarCompType(vlistID, varID, CDI_COMPRESS_FILTER);
    if (ncvar->hasDeflate) vlistDefVarCompType(vlistID, varID, CDI_COMPRESS_ZIP);
    if (ncvar->hasSzip) vlistDefVarCompType(vlistID, varID, CDI_COMPRESS_SZIP);
    if (ncvar->isChunked)
    {
      if (ncvar->chunkType != CDI_UNDEFID) cdiDefKeyInt(vlistID, varID, CDI_KEY_CHUNKTYPE, ncvar->chunkType);
      if (ncvar->chunkSize > 1) cdiDefKeyInt(vlistID, varID, CDI_KEY_CHUNKSIZE, ncvar->chunkSize);

      bool cacheGrid = (ncvar->gridtype == GRID_UNSTRUCTURED || ncvar->isHealpixMapping);
      bool cacheFormat = (streamP->filetype == CDI_FILETYPE_NCZARR && CDI_Cache_NCZ)
                         || ((streamP->filetype == CDI_FILETYPE_NC4 || streamP->filetype == CDI_FILETYPE_NC4C) && CDI_Cache_NC4);
      ChunkCacheValues chunkCacheValues
          = (cacheFormat && cacheGrid) ? get_chunk_cache_size(timedimid, ncvar) : calc_chunk_cache_size(timedimid, ncvar);
      size_t cacheSize = chunkCacheValues.cacheSize;
      if (cacheSize > 0 && cacheFormat && cacheGrid && chunkCacheValues.numDims <= 3)
      {
        CdfCache *cdfCache = (CdfCache *) Malloc(sizeof(CdfCache));
        cdf_init_cache(cdfCache);
        cdfCache->cacheSize = cacheSize;
        cdfCache->numDims = chunkCacheValues.numDims;
        cdfCache->maxSteps = (timedimid == CDI_UNDEFID) ? 0 : ncdims[timedimid].len;
        for (int i = 0; i < chunkCacheValues.numDims; ++i) { cdfCache->start[i] = chunkCacheValues.start[i]; }
        for (int i = 0; i < chunkCacheValues.numDims; ++i) { cdfCache->count[i] = chunkCacheValues.count[i]; }
        if (CDI_Debug || CDI_Cache_Info) Message("%s: chunkCacheSize=%zu", ncvar->name, cacheSize);
        streamP->vars[varID].cdfCache = cdfCache;
        cdf_set_var_chunk_cache(ncvar, ncvarid, 0);
      }
      else
      {
        if (CDI_CacheSize_In >= 0) { cacheSize = (size_t) CDI_CacheSize_In; }
        cdf_set_var_chunk_cache(ncvar, ncvarid, cacheSize);
      }
    }

    streamP->vars[varID1].defmiss = false;
    streamP->vars[varID1].ncvarid = ncvarid;

    cdiDefKeyString(vlistID, varID, CDI_KEY_NAME, ncvar->name);
    if (ncvar->param != CDI_UNDEFID) vlistDefVarParam(vlistID, varID, ncvar->param);
    if (ncvar->code != CDI_UNDEFID) vlistDefVarCode(vlistID, varID, ncvar->code);
    if (ncvar->code != CDI_UNDEFID) vlistDefVarParam(vlistID, varID, cdiEncodeParam(ncvar->code, ncvar->tabnum, 255));
    if (ncvar->longname[0]) cdiDefKeyString(vlistID, varID, CDI_KEY_LONGNAME, ncvar->longname);
    if (ncvar->stdname[0]) cdiDefKeyString(vlistID, varID, CDI_KEY_STDNAME, ncvar->stdname);
    if (ncvar->unitsLen > 0) cdiDefKeyString(vlistID, varID, CDI_KEY_UNITS, ncvar->units);

    if (ncvar->validrangeDefined) vlistDefVarValidrange(vlistID, varID, ncvar->validrange);

    if (is_not_equal(ncvar->addoffset, 0.0)) cdiDefKeyFloat(vlistID, varID, CDI_KEY_ADDOFFSET, ncvar->addoffset);
    if (is_not_equal(ncvar->scalefactor, 1.0)) cdiDefKeyFloat(vlistID, varID, CDI_KEY_SCALEFACTOR, ncvar->scalefactor);

    vlistDefVarDatatype(vlistID, varID, cdfInqDatatype(streamP, ncvar->xtype, ncvar->isUnsigned));

    vlistDefVarInstitut(vlistID, varID, instID);
    vlistDefVarModel(vlistID, varID, modelID);
    if (ncvar->tableID != CDI_UNDEFID) vlistDefVarTable(vlistID, varID, ncvar->tableID);

    if (ncvar->fillvalDefined == false && ncvar->missvalDefined)
    {
      ncvar->fillvalDefined = true;
      ncvar->fillval = ncvar->missval;
    }

    if (ncvar->fillvalDefined) vlistDefVarMissval(vlistID, varID, ncvar->fillval);

    if (CDI_Debug)
      Message("varID = %d  gridID = %d  zaxisID = %d", varID, vlistInqVarGrid(vlistID, varID), vlistInqVarZaxis(vlistID, varID));

    int gridIndex = vlistGridIndex(vlistID, gridID);
    const CdfGrid *ncGrid = &(streamP->cdfInfo.cdfGridList[gridIndex]);
    int xdimid = ncGrid->ncIdList[CDF_DIMID_X];
    int ydimid = ncGrid->ncIdList[CDF_DIMID_Y];

    int zaxisindex = vlistZaxisIndex(vlistID, zaxisID);
    int zdimid = streamP->cdfInfo.zaxisIdList[zaxisindex];

    int ndims = ncvar->ndims;
    static const int ipow10[4] = { 1, 10, 100, 1000 };

    int iodim = (ncvar->timetype != TIME_CONSTANT);

    const int *dimids = ncvar->dimids;

    int ixyz = 0;
    if (ndims > 0)
    {
      if ((ndims - iodim) <= 2 && (ydimid == xdimid || ydimid == CDI_UNDEFID))
      {
        ixyz = (xdimid == ncdims[dimids[ndims - 1]].dimid) ? 321 : 213;
      }
      else
      {
        for (int idim = iodim; idim < ndims; idim++)
        {
          int dimid = ncdims[dimids[idim]].dimid;
          // clang-format off
          if      (xdimid == dimid) { ixyz += 1 * ipow10[ndims - idim - 1]; }
          else if (ydimid == dimid) { ixyz += 2 * ipow10[ndims - idim - 1]; }
          else if (zdimid == dimid) { ixyz += 3 * ipow10[ndims - idim - 1]; }
          // clang-format on
        }
      }
    }

    if (ncvar->isCubeSphere) ixyz = 0;
    vlistDefVarXYZ(vlistID, varID, ixyz);
    /*
    printf("ixyz %d\n", ixyz);
    printf("ndims %d\n", ncvar->ndims);
    for (int i = 0; i < ncvar->ndims; ++i)
      printf("dimids: %d %d\n", i, dimids[i]);
    printf("xdimid, ydimid %d %d\n", xdimid, ydimid);
    */
    if (ncvar->numberOfForecastsInEnsemble != -1)
    {
      cdiDefKeyInt(vlistID, varID, CDI_KEY_NUMBEROFFORECASTSINENSEMBLE, ncvar->numberOfForecastsInEnsemble);
      cdiDefKeyInt(vlistID, varID, CDI_KEY_PERTURBATIONNUMBER, ncvar->perturbationNumber);
      if (ncvar->numberOfForecastsInEnsemble != -1)
        cdiDefKeyInt(vlistID, varID, CDI_KEY_TYPEOFENSEMBLEFORECAST, ncvar->typeOfEnsembleForecast);
    }
  }

  for (int varID = 0; varID < nvarsData; varID++)
  {
    int ncvarid = varids[varID];
    ncvar_t *ncvar = &ncvars[ncvarid];
    int ncid = ncvar->ncid;
    int nvatts = ncvar->natts;
    for (int iatt = 0; iatt < nvatts; ++iatt) cdf_set_cdi_attr(ncid, ncvarid, ncvar->atts[iatt], vlistID, varID, false);

    Free(ncvar->atts);
    Free(ncvar->vct);

    ncvar->atts = NULL;
    ncvar->vct = NULL;
  }

  // release mem of not freed attributes
  for (int ncvarid = 0; ncvarid < num_ncvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    Free(ncvar->atts);
    ncvar->atts = NULL;
  }

  if (varids && varids != varidsLocal) Free(varids);

  for (int varID = 0; varID < nvarsData; varID++) cdf_define_code_and_param(vlistID, varID);
  for (int varID = 0; varID < nvarsData; varID++) cdf_define_institut_and_model_id(vlistID, varID);
}

static void
cdf_copy_attint(int fileID, int vlistID, nc_type xtype, size_t attlen, char *attname)
{
  int attint[8];
  int *pattint = (attlen > 8) ? (int *) Malloc(attlen * sizeof(int)) : attint;
  cdfGetAttInt(fileID, NC_GLOBAL, attname, attlen, pattint);
  int datatype = (xtype == NC_SHORT) ? CDI_DATATYPE_INT16 : CDI_DATATYPE_INT32;
  cdiDefAttInt(vlistID, CDI_GLOBAL, attname, datatype, (int) attlen, pattint);
  if (attlen > 8) Free(pattint);
}

static void
cdf_copy_attflt(int fileID, int vlistID, nc_type xtype, size_t attlen, char *attname)
{
  double attflt[8];
  double *pattflt = (attlen > 8) ? (double *) Malloc(attlen * sizeof(double)) : attflt;
  cdfGetAttDouble(fileID, NC_GLOBAL, attname, attlen, pattflt);
  int datatype = (xtype == NC_FLOAT) ? CDI_DATATYPE_FLT32 : CDI_DATATYPE_FLT64;
  cdiDefAttFlt(vlistID, CDI_GLOBAL, attname, datatype, (int) attlen, pattflt);
  if (attlen > 8) Free(pattflt);
}

static void
check_cube_sphere(int vlistID, int nvars, ncvar_t *ncvars, ncdim_t *ncdims)
{
  bool isGeosData = false;
  int numNames = 4;
  const char *attnames[] = { "additional_vars", "file_format_version", "gridspec_file", "grid_mapping_name" };
  const char *grid_mapping = "gnomonic cubed-sphere";
  char attstring[256];
  int nf_dimid = -1, ncontact_dimid = -1;

  int numFound = 0;
  for (int i = 0; i < numNames; ++i)
    if (0 == cdiInqAttTxt(vlistID, CDI_GLOBAL, attnames[i], (int) sizeof(attstring), attstring)) numFound++;

  if (numFound == numNames && strStartsWith(attstring, grid_mapping))
  {
    for (int i = 0; i < numNames; ++i) cdiDelAtt(vlistID, CDI_GLOBAL, attnames[i]);

    const char *nf_name = "nf";
    const char *ncontact_name = "ncontact";
    for (int varid = 0; varid < nvars; ++varid)
    {
      ncvar_t *ncvar = &ncvars[varid];
      if (ncvar->ndims == 1)
      {
        int dimid = ncvar->dimids[0];
        if (ncdims[dimid].len == 6 && str_is_equal(nf_name, ncvar->name))
        {
          isGeosData = true;
          nf_dimid = ncvar->dimids[0];
        }
        if (ncdims[dimid].len == 4 && str_is_equal(ncontact_name, ncvar->name)) ncontact_dimid = ncvar->dimids[0];
      }
      if (isGeosData && ncontact_dimid != -1) break;
    }
  }

  if (isGeosData)
  {
    ncdims[nf_dimid].dimtype = E_AXIS;
    for (int varid = 0; varid < nvars; ++varid)
    {
      ncvar_t *ncvar = &ncvars[varid];
      if (str_is_equal("orientation", ncvar->name) || str_is_equal("anchor", ncvar->name) || str_is_equal("contacts", ncvar->name))
        cdf_set_var(ncvar, CoordVar);
    }

    for (int varid = 0; varid < nvars; ++varid)
    {
      ncvar_t *ncvar = &ncvars[varid];
      int ndims = ncvar->ndims;
      if (ndims >= 3 && ncvar->dimids[ndims - 3] == nf_dimid && ncvar->ncoordvars == 2 && ncvar->gmapid != -1)
        ncvar->isCubeSphere = true;
    }

    int xvarid = -1, yvarid = -1;
    int xboundsid = -1, yboundsid = -1;
    for (int varid = 0; varid < nvars; ++varid)
    {
      ncvar_t *ncvar = &ncvars[varid];
      int ndims = ncvar->ndims;
      if (ndims == 3)
      {
        // clang-format off
        if      (str_is_equal("lons", ncvar->name)) xvarid = varid;
        else if (str_is_equal("lats", ncvar->name)) yvarid = varid;
        else if (str_is_equal("corner_lons", ncvar->name)) xboundsid = varid;
        else if (str_is_equal("corner_lats", ncvar->name)) yboundsid = varid;
        // clang-format on
      }
      if (xvarid != -1 && xboundsid != -1 && yvarid != -1 && yboundsid != -1)
      {
        cdf_set_var(&ncvars[xboundsid], CoordVar);
        cdf_set_var(&ncvars[yboundsid], CoordVar);
        ncvars[xvarid].bounds = xboundsid;
        ncvars[yvarid].bounds = yboundsid;
        break;
      }
    }
  }

  if (CDI_Debug) Message("isGeosData %d", isGeosData);
}

static void
cdf_scan_global_attr(int fileID, int vlistID, int ngatts, int *instID, int *modelID, bool *ucla_les, unsigned char *uuidOfVGrid,
                     GridInfo *gridInfo)
{
  nc_type xtype = 0;
  size_t attlen = 0;
  char attname[CDI_MAX_NAME] = { 0 };
  String attString;
  string_init(&attString);

  for (int iatt = 0; iatt < ngatts; iatt++)
  {
    cdf_inq_attname(fileID, NC_GLOBAL, iatt, attname);
    cdf_inq_att(fileID, NC_GLOBAL, attname, &xtype, &attlen);

    if (xtypeIsText(xtype))
    {
      cdfGetAttString(fileID, NC_GLOBAL, attname, &attString);
      char *attstring = attString.data;
      size_t attstrlen = strlen(attstring);
      if (attlen > 0 && attstring[0] != 0)
      {
        if (str_is_equal(attname, "institution"))
        {
          *instID = institutInq(0, 0, NULL, attstring);
          if (*instID == CDI_UNDEFID) *instID = institutDef(0, 0, NULL, attstring);

          cdiDefAttTxt(vlistID, CDI_GLOBAL, attname, (int) attstrlen, attstring);
        }
        else if (str_is_equal(attname, "source"))
        {
          *modelID = modelInq(-1, 0, attstring);
          if (*modelID == CDI_UNDEFID) *modelID = modelDef(-1, 0, attstring);

          cdiDefAttTxt(vlistID, CDI_GLOBAL, attname, (int) attstrlen, attstring);
        }
        else if (str_is_equal(attname, "Source") && strStartsWith(attstring, "UCLA-LES"))
        {
          *ucla_les = true;
          cdiDefAttTxt(vlistID, CDI_GLOBAL, attname, (int) attstrlen, attstring);
        }
        /*
        else if ( str_is_equal(attname, "Conventions") )
          {
          }
        */
        else if (str_is_equal(attname, "_NCProperties")) {}
        else if (str_is_equal(attname, "CDI")) {}
        else if (str_is_equal(attname, "CDO")) {}
        else if (str_is_equal(attname, "grid_file_uri")) { memcpy(gridInfo->gridfile, attstring, attstrlen + 1); }
        else if (attstrlen == 36 && str_is_equal(attname, "uuidOfHGrid")) { cdiStr2UUID(attstring, gridInfo->uuid); }
        else if (attstrlen == 36 && str_is_equal(attname, "uuidOfVGrid")) { cdiStr2UUID(attstring, uuidOfVGrid); }
        else
        {
          if (str_is_equal(attname, "ICON_grid_file_uri") && gridInfo->gridfile[0] == 0)
            memcpy(gridInfo->gridfile, attstring, attstrlen + 1);

          cdiDefAttTxt(vlistID, CDI_GLOBAL, attname, (int) attstrlen, attstring);
        }
      }
      else
      {
        cdiDefAttTxt(vlistID, CDI_GLOBAL, attname, (int) attstrlen, attstring);
      }
    }
    else if (xtype == NC_SHORT || xtype == NC_INT)
    {
      if (str_is_equal(attname, "number_of_grid_used"))
      {
        gridInfo->number_of_grid_used = CDI_UNDEFID;
        cdfGetAttInt(fileID, NC_GLOBAL, attname, 1, &gridInfo->number_of_grid_used);
      }
      else
      {
        cdf_copy_attint(fileID, vlistID, xtype, attlen, attname);
      }
    }
    else if (xtype == NC_FLOAT || xtype == NC_DOUBLE) { cdf_copy_attflt(fileID, vlistID, xtype, attlen, attname); }
  }

  string_free(&attString);
}

static int
find_leadtime(int nvars, ncvar_t *ncvars, int timedimid)
{
  int leadtime_id = CDI_UNDEFID;

  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->ndims == 1 && timedimid == ncvar->dimids[0])
      if (ncvar->stdname[0] && str_is_equal(ncvar->stdname, "forecast_period"))
      {
        leadtime_id = ncvarid;
        break;
      }
  }

  return leadtime_id;
}

static void
find_time_vars(int nvars, ncvar_t *ncvars, ncdim_t *ncdims, int timedimid, stream_t *streamP, bool *timeHasUnits,
               bool *timeHasBounds, bool *timeClimatology)
{
  if (timedimid == CDI_UNDEFID)
  {
    for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
    {
      ncvar_t *ncvar = &ncvars[ncvarid];
      if (ncvar->ndims == 0 && ncvar->units[0] && str_is_equal(ncvar->name, "time"))
      {
        char timeUnitsStr[CDI_MAX_NAME];
        strcpy(timeUnitsStr, ncvar->units);
        str_to_lower(timeUnitsStr);
        if (is_time_units(timeUnitsStr))
        {
          streamP->basetime.ncvarid = ncvarid;
          break;
        }
      }
    }
  }
  else
  {
    bool hasTimeVar = false;

    if (ncdims[timedimid].ncvarid != CDI_UNDEFID)
    {
      streamP->basetime.ncvarid = ncdims[timedimid].ncvarid;
      hasTimeVar = true;
    }

    for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
    {
      ncvar_t *ncvar = &ncvars[ncvarid];
      if (ncvarid != streamP->basetime.ncvarid && ncvar->ndims == 1 && timedimid == ncvar->dimids[0] && !xtypeIsText(ncvar->xtype)
          && is_timeaxis_units(ncvar->units))
      {
        ncvar->varStatus = CoordVar;

        if (!hasTimeVar)
        {
          hasTimeVar = true;
          streamP->basetime.ncvarid = ncvarid;
        }
        else
        {
          Warning("Found more than one time variable, skipped variable %s!", ncvar->name);
        }
      }
    }

    if (hasTimeVar == false)  // search for WRF time description
    {
      for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
      {
        ncvar_t *ncvar = &ncvars[ncvarid];
        int dim1len = ncdims[ncvar->dimids[1]].len;
        if (ncvarid != streamP->basetime.ncvarid && ncvar->ndims == 2 && timedimid == ncvar->dimids[0] && xtypeIsText(ncvar->xtype)
            && (dim1len == 19 || dim1len == 64))
        {
          ncvar->isTaxis = true;
          streamP->basetime.ncvarid = ncvarid;
          streamP->basetime.isWRF = true;
          break;
        }
      }
    }

    // time varID
    int ncvarid = streamP->basetime.ncvarid;
    if (ncvarid == CDI_UNDEFID && ncdims[timedimid].len > 0) Warning("Time variable >%s< not found!", ncdims[timedimid].name);
  }

  // time varID
  int ncvarid = streamP->basetime.ncvarid;
  if (ncvarid != CDI_UNDEFID && streamP->basetime.isWRF == false)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->units[0] != 0) *timeHasUnits = true;

    if (ncvar->bounds != CDI_UNDEFID)
    {
      int nbdims = ncvars[ncvar->bounds].ndims;
      if (nbdims == 2)
      {
        int len = (int) ncdims[ncvars[ncvar->bounds].dimids[nbdims - 1]].len;
        if (len == 2 && timedimid == ncvars[ncvar->bounds].dimids[0])
        {
          *timeHasBounds = true;
          streamP->basetime.ncvarboundsid = ncvar->bounds;
          if (ncvar->isClimatology) *timeClimatology = true;
        }
      }
    }
  }
}

static void
read_vct_echam(int fileID, int nvars, ncvar_t *ncvars, ncdim_t *ncdims, double **vct, size_t *pvctsize)
{
  // find ECHAM VCT
  int nvcth_id = CDI_UNDEFID, vcta_id = CDI_UNDEFID, vctb_id = CDI_UNDEFID;
  // int p0_id = CDI_UNDEFID;

  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    const char *name = ncvar->name;
    if (ncvar->ndims == 1)
    {
      size_t len = strlen(name);
      if (len == 4 && name[0] == 'h' && name[1] == 'y')
      {
        if (name[2] == 'a' && name[3] == 'i')  // hyai
        {
          vcta_id = ncvarid;
          nvcth_id = ncvar->dimids[0];
          ncvar->varStatus = CoordVar;
        }
        else if (name[2] == 'b' && name[3] == 'i')  // hybi
        {
          vctb_id = ncvarid;
          nvcth_id = ncvar->dimids[0];
          ncvar->varStatus = CoordVar;
        }
        else if ((name[2] == 'a' || name[2] == 'b') && name[3] == 'm')
        {
          ncvar->varStatus = CoordVar;  // hyam or hybm
        }
      }
    }
    /*
    else if (ncvar->ndims == 0)
      {
        size_t len = strlen(name);
        if (len == 2 && name[0] == 'P' && name[1] == '0') p0_id = ncvarid;
      }
    */
  }

  // read VCT
  if (nvcth_id != CDI_UNDEFID && vcta_id != CDI_UNDEFID && vctb_id != CDI_UNDEFID)
  {
    size_t vctsize = 2 * ncdims[nvcth_id].len;
    *vct = (double *) Malloc(vctsize * sizeof(double));
    cdf_get_var_double(fileID, vcta_id, *vct);
    cdf_get_var_double(fileID, vctb_id, *vct + vctsize / 2);
    *pvctsize = vctsize;
    /*
    if (p0_id != CDI_UNDEFID)
      {
        double p0;
        cdf_get_var_double(fileID, p0_id, &p0);
      }
    */
  }
}

static void
cdf_set_ucla_dimtype(int ndims, ncdim_t *ncdims, ncvar_t *ncvars)
{
  for (int ncdimid = 0; ncdimid < ndims; ncdimid++)
  {
    ncdim_t *ncdim = &ncdims[ncdimid];
    int ncvarid = ncdim->ncvarid;
    if (ncvarid != -1)
    {
      ncvar_t *ncvar = &ncvars[ncvarid];
      if (ncdim->dimtype == CDI_UNDEFID && ncvar->units[0] == 'm')
      {
        // clang-format off
        if      (ncvar->name[0] == 'x') ncdim->dimtype = X_AXIS;
        else if (ncvar->name[0] == 'y') ncdim->dimtype = Y_AXIS;
        else if (ncvar->name[0] == 'z') ncdim->dimtype = Z_AXIS;
        // clang-format on
      }
    }
  }
}

static int
cdf_check_variables(stream_t *streamP, int nvars, ncvar_t *ncvars, size_t ntsteps, int timedimid)
{
  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->isTaxis && ncvar->ndims == 2)
    {
      ncvar->varStatus = CoordVar;
      continue;
    }

    if (ncvar->varStatus == UndefVar && ncvar->ndims > 1 && timedimid != CDI_UNDEFID && timedimid == ncvar->dimids[0])
      cdf_set_var(ncvar, DataVar);

    if (ncvar->varStatus == UndefVar)
    {
      if (ncvar->ndims == 0)
        cdf_set_var(ncvar, nvars == 1 ? DataVar : CoordVar);
      else if (ncvar->ndims > 0)
        cdf_set_var(ncvar, DataVar);
      else
      {
        ncvar->varStatus = CoordVar;
        Warning("Variable %s has an unknown type, skipped!", ncvar->name);
      }
    }

    if (ncvar->varStatus == CoordVar) continue;

    if ((ncvar->ndims > 4 && !ncvar->isCubeSphere) || ncvar->ndims > 5)
    {
      ncvar->varStatus = CoordVar;
      Warning("%d dimensional variables are not supported, skipped variable %s!", ncvar->ndims, ncvar->name);
      continue;
    }

    if (((ncvar->ndims == 4 && !ncvar->isCubeSphere) || ncvar->ndims == 5) && timedimid == CDI_UNDEFID)
    {
      ncvar->varStatus = CoordVar;
      Warning("%d dimensional variables without time dimension are not supported, skipped variable %s!", ncvar->ndims, ncvar->name);
      continue;
    }

    if (xtypeIsText(ncvar->xtype))
    {
      ncvar->varStatus = CoordVar;
      Warning("Unsupported data type (char/string), skipped variable %s!", ncvar->name);
      continue;
    }

    if (cdfInqDatatype(streamP, ncvar->xtype, ncvar->isUnsigned) == -1)
    {
      ncvar->varStatus = CoordVar;
      Warning("Unsupported data type, skipped variable %s!", ncvar->name);
      continue;
    }

    if (timedimid != CDI_UNDEFID && ntsteps == 0 && ncvar->ndims > 0)
    {
      if (timedimid == ncvar->dimids[0])
      {
        ncvar->varStatus = CoordVar;
        Warning("Number of time steps undefined, skipped variable %s!", ncvar->name);
        continue;
      }
    }
  }

  return timedimid;
}

static void
cdfVerifyVars(int nvars, ncvar_t *ncvars
              //, ncdim_t *ncdims
)
{
  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->varStatus == DataVar && ncvar->ndims > 0)
    {
      int ndims = 0;
      for (int i = 0; i < ncvar->ndims; ++i)
      {
        // clang-format off
        if      (ncvar->dimtypes[i] == T_AXIS) ndims++;
        else if (ncvar->dimtypes[i] == E_AXIS) ndims++;
        else if (ncvar->dimtypes[i] == Z_AXIS) ndims++;
        else if (ncvar->dimtypes[i] == Y_AXIS) ndims++;
        else if (ncvar->dimtypes[i] == X_AXIS) ndims++;
        // clang-format on
      }

      if (ncvar->ndims != ndims)
      {
        ncvar->varStatus = CoordVar;
        Warning("Inconsistent number of dimensions, skipped variable %s!", ncvar->name);
      }
      /* check failed with cdo option --cmor
      int zdimid = -1;
      for (int i = 0; i < ncvar->ndims; ++i)
        {
          if (ncvar->dimtypes[i] == Z_AXIS) zdimid = ncvar->dimids[i];
        }

      int zaxisID = ncvar->zaxisID;
      if (zaxisInqScalar(zaxisID) && zdimid != -1)
        {
          ncvar->varStatus = CoordVar;
          Warning("Unsupported dimension >%s<, skipped variable %s!", ncdims[zdimid].name, ncvar->name);
        }
      */
    }
  }
}

static CdiDateTime
wrf_read_timestep(int fileID, int nctimevarid, size_t tsID)
{
  enum
  {
    // position of terminator separating date and time from rest of
    dateTimeSepPos = 19,
    dateTimeStrSize = 128,
  };
  size_t start[2] = { tsID, 0 }, count[2] = { 1, dateTimeSepPos };
  char stvalue[dateTimeStrSize] = { 0 };
  cdf_get_vara_text(fileID, nctimevarid, start, count, stvalue);
  stvalue[dateTimeSepPos] = 0;

  int year = 1, month = 1, day = 1, hour = 0, minute = 0, second = 0;
  if (strlen(stvalue) == dateTimeSepPos) sscanf(stvalue, "%d-%d-%d_%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  return cdiDateTime_set(cdiEncodeDate(year, month, day), cdiEncodeTime(hour, minute, second));
}

static double
get_timevalue(int fileID, int nctimevarid, size_t ncStepIndex, double *timevarBuffer)
{
  double timevalue = 0.0;

  if (timevarBuffer) { timevalue = timevarBuffer[ncStepIndex]; }
  else
  {
    cdf_get_var1_double(fileID, nctimevarid, &ncStepIndex, &timevalue);
  }

  if (timevalue >= NC_FILL_DOUBLE || timevalue < -NC_FILL_DOUBLE) timevalue = 0.0;

  return timevalue;
}

static void
process_time_query(struct CdiQuery *query, size_t numTimesteps, CdiDateTime *vDateTimeList, int *ncStepIndices)
{
  if (query)
  {
    int queryNumSteps = cdiQueryNumSteps(query);
    int queryNumDates = cdiQueryNumDates(query);
    if (queryNumSteps > 0)
    {
      const int64_t *stepRange = cdiQueryGetSteps(query);
      int64_t startStep = adjust_negative_index(numTimesteps, stepRange[0]);
      if (startStep > (int64_t) numTimesteps) Error("query start step > maxSteps=%zu", numTimesteps);
      int64_t endStep = adjust_negative_index(numTimesteps, stepRange[1]);
      if (endStep < 0) { endStep = numTimesteps + endStep + 1; }
      for (int64_t i = 0; i < startStep - 1; ++i) { ncStepIndices[i] = -1; }
      for (int64_t i = endStep; i < (int64_t) numTimesteps; ++i) { ncStepIndices[i] = -1; }
      if (startStep <= (int64_t) numTimesteps) { cdiQueryStep(query, startStep); }
      if (endStep <= (int64_t) numTimesteps) { cdiQueryStep(query, endStep); }
    }
    else if (queryNumDates > 0)
    {
      const double *dateRange = cdiQueryGetDates(query);
      double startDateFloat = dateRange[0];
      double endDateFloat = dateRange[1];
      if (vDateTimeList)
      {
        for (size_t i = 0; i < numTimesteps; ++i)
        {
          double dateTimeFloat = cdiDateTime_to_double(vDateTimeList[i]);
          if (dateTimeFloat < startDateFloat || dateTimeFloat > endDateFloat) { ncStepIndices[i] = -1; }
        }
      }
    }
  }
}

static void
cdf_read_timesteps(size_t numTimesteps, stream_t *streamP, taxis_t *taxis0)
{
  streamP->curTsID = 0;
  streamP->rtsteps = 1;

  if (numTimesteps == 0)
  {
    cdi_create_timesteps(numTimesteps, streamP);
    cdf_create_records(streamP, 0);
  }
  else
  {
    int fileID = streamP->groupID;
    int nctimevarid = streamP->basetime.ncvarid;
    int nctimeboundsid = streamP->basetime.ncvarboundsid;
    bool hasTimesteps = (nctimevarid != CDI_UNDEFID /*&& streamP->basetime.hasUnits*/);

    int *ncStepIndices = (int *) Malloc(numTimesteps * sizeof(int));
    for (size_t tsID = 0; tsID < numTimesteps; ++tsID) { ncStepIndices[tsID] = (int) tsID; }

    CdiDateTime *vDateTimeList = NULL;

    if (hasTimesteps)
    {
      vDateTimeList = (CdiDateTime *) Malloc(numTimesteps * sizeof(CdiDateTime));

      if (streamP->basetime.isWRF)
      {
        for (size_t i = 0; i < numTimesteps; ++i) { vDateTimeList[i] = wrf_read_timestep(fileID, nctimevarid, i); }
      }
      else if (streamP->basetime.hasUnits)
      {
        double *timevarBuffer = (double *) Malloc(numTimesteps * sizeof(double));
        cdf_get_var_double(fileID, nctimevarid, timevarBuffer);
        for (size_t i = 0; i < numTimesteps; ++i)
        {
          vDateTimeList[i] = cdi_decode_timeval(get_timevalue(fileID, nctimevarid, i, timevarBuffer), taxis0);
        }
        Free(timevarBuffer);
      }
      else
      {
        hasTimesteps = false;
      }
    }

    // process step query information if available
    process_time_query(streamP->query, numTimesteps, vDateTimeList, ncStepIndices);

    size_t numSteps = 0;
    for (size_t tsID = 0; tsID < numTimesteps; ++tsID) { numSteps += (ncStepIndices[tsID] >= 0); }

    if (numTimesteps > 0 && numSteps == 0) Error("No timesteps selected!");

    cdi_create_timesteps(numSteps, streamP);

    for (size_t tsID = 0, stepID = 0; tsID < numTimesteps; ++tsID)
    {
      if (ncStepIndices[tsID] >= 0)
      {
        streamP->tsteps[stepID].ncStepIndex = ncStepIndices[tsID];
        cdf_create_records(streamP, stepID);

        taxis_t *taxis = &streamP->tsteps[stepID].taxis;
        ptaxisCopy(taxis, taxis0);

        if (hasTimesteps) taxis->vDateTime = vDateTimeList[tsID];

        stepID++;
      }
    }

    if (ncStepIndices) Free(ncStepIndices);
    if (vDateTimeList) Free(vDateTimeList);

    if (hasTimesteps)
    {
      if (nctimeboundsid != CDI_UNDEFID)
      {
        enum
        {
          numBnds = 2,
          tbNdims = 2
        };

        for (size_t tsID = 0; tsID < numSteps; ++tsID)
        {
          size_t ncStepIndex = (size_t) streamP->tsteps[tsID].ncStepIndex;
          taxis_t *taxis = &streamP->tsteps[tsID].taxis;
          size_t start[tbNdims] = { ncStepIndex, 0 };
          size_t count[tbNdims] = { 1, numBnds };
          double timeBnds[numBnds];
          cdf_get_vara_double(fileID, nctimeboundsid, start, count, timeBnds);
          for (size_t i = 0; i < numBnds; ++i)
            if (timeBnds[i] >= NC_FILL_DOUBLE || timeBnds[i] < -NC_FILL_DOUBLE) { timeBnds[i] = 0.0; }

          taxis->vDateTime_lb = cdi_decode_timeval(timeBnds[0], taxis);
          taxis->vDateTime_ub = cdi_decode_timeval(timeBnds[1], taxis);
        }
      }

      int leadtimeid = streamP->basetime.leadtimeid;
      if (leadtimeid != CDI_UNDEFID)
      {
        for (size_t tsID = 0; tsID < numSteps; ++tsID)
        {
          size_t ncStepIndex = (size_t) streamP->tsteps[tsID].ncStepIndex;
          taxis_t *taxis = &streamP->tsteps[tsID].taxis;
          cdi_set_forecast_period(get_timevalue(fileID, leadtimeid, ncStepIndex, NULL), taxis);
        }
      }
    }
  }
}

static void
stream_set_ncdims(stream_t *streamP, int ndims, ncdim_t *ncdims)
{
  int n = (ndims > MAX_DIMS_PS) ? MAX_DIMS_PS : ndims;
  CdfInfo *cdfInfo = &(streamP->cdfInfo);
  cdfInfo->ncNumDims = n;
  for (int i = 0; i < n; i++) cdfInfo->ncDimIdList[i] = ncdims[i].dimid;
  for (int i = 0; i < n; i++) cdfInfo->ncDimLenList[i] = ncdims[i].len;
}

static void
read_ncdims(int fileID, int ndims, ncdim_t *ncdims)
{
  for (int i = 0; i < ndims; i++)
  {
    if (ncdims[i].dimid == -1) continue;
    cdf_inq_dimlen(fileID, ncdims[i].dimid, &ncdims[i].len);
    cdf_inq_dimname(fileID, ncdims[i].dimid, ncdims[i].name);
  }
}

static int
find_dimidx(int nctimedimid, int ndims, ncdim_t *ncdims)
{
  for (int i = 0; i < ndims; i++)
  {
    if (nctimedimid == ncdims[i].dimid) return i;
  }
  return -1;
}

static void
check_ncgroups(int fileID)
{
  int numGroups = 0;
  char gname[CDI_MAX_NAME];
  nc_inq_grps(fileID, &numGroups, NULL);
  if (numGroups)
  {
    if (CDI_Debug)
    {
      int *ncIds = (int *) Malloc(numGroups * sizeof(int));
      nc_inq_grps(fileID, NULL, ncIds);
      for (int i = 0; i < numGroups; ++i)
      {
        int ncId = ncIds[i];
        int gndims, gnvars, gngatts, gunlimdimid;
        cdf_inq(ncId, &gndims, &gnvars, &gngatts, &gunlimdimid);
        nc_inq_grpname(ncId, gname);
        Message("%s: ndims %d, nvars %d, ngatts %d", gname, gndims, gnvars, gngatts);
      }
      Free(ncIds);
    }
    Warning("NetCDF4 sub-groups not supported! Skipped %d group%s.", numGroups, (numGroups > 1) ? "s" : "");
  }
}

static void
find_coordinates_vars(int ndims, ncdim_t *ncdims, int nvars, ncvar_t *ncvars)
{
  for (int gdimid = 0; gdimid < ndims; gdimid++)
  {
    int *dimvarid = &ncdims[gdimid].ncvarid;
    if (*dimvarid != CDI_UNDEFID) continue;

    const char *dimname = ncdims[gdimid].name;
    for (int varid = 0; varid < nvars; varid++)
    {
      ncvar_t *ncvar = &ncvars[varid];
      if (ncvar->ndims == 1 && gdimid == ncvar->dimids[0] && str_is_equal(ncvar->name, dimname))
      {
        *dimvarid = varid;
        ncvar->varStatus = CoordVar;
      }
    }
  }
}

// set time dependent data vars
static void
find_varying_data_vars1d(int timedimid, int nvars, ncvar_t *ncvars)
{
  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
  {
    ncvar_t *ncvar = &ncvars[ncvarid];
    if (ncvar->ndims == 1)
    {
      if (timedimid != CDI_UNDEFID && timedimid == ncvar->dimids[0])
      {
        if (ncvar->varStatus != CoordVar) cdf_set_var(ncvar, DataVar);
      }
      else
      {
        //  if ( ncvar->varStatus != DataVar ) cdf_set_var(ncvar, CoordVar);
      }
      // if ( ncvar->varStatus != DataVar ) cdf_set_var(ncvar, CoordVar);
    }
  }
}

static void
set_coordinates_varids(int numVars, ncvar_t *ncvars)
{
  for (int varId = 0; varId < numVars; varId++)
  {
    ncvar_t *ncvar = &ncvars[varId];
    if (ncvar->varStatus == DataVar && ncvar->ncoordvars)
    {
      for (int i = 0; i < ncvar->ncoordvars; i++)
      {
        int coordVarId = ncvar->coordvarids[i];
        if (coordVarId != CDI_UNDEFID)
        {
          ncvar_t *coordVar = &ncvars[coordVarId];
          if (coordVar->isZaxis == false && coordVar->zaxistype != CDI_UNDEFID) { coordVar->isZaxis = true; }
          // clang-format off
          if      (coordVar->isLon || coordVar->isXaxis) { ncvar->xvarid = coordVarId; }
          else if (coordVar->isLat || coordVar->isYaxis) { ncvar->yvarid = coordVarId; }
          else if (coordVar->isZaxis)                    { ncvar->zvarid = coordVarId; }
          else if (coordVar->isTaxis)                    { ncvar->tvarid = coordVarId; }
          else if (coordVar->isCharAxis)                 { ncvar->cvarids[i] = coordVarId; }
          else if (coordVar->isIndexAxis)                { ncvar->ivarid = coordVarId; }
          else if (coordVar->printWarning)
          {
            Warning("Coordinates variable %s can't be assigned!", coordVar->name);
            coordVar->printWarning = false;
          }
          // clang-format on
        }
      }
    }
  }
}

static void
process_var_query(struct CdiQuery *query, int nvars, ncvar_t *ncvars)
{
  // process var query information if available
  if (query && cdiQueryNumNames(query) > 0)
  {
    /*
    int numNames = cdiQueryNumNames(query);
    const char **names = cdiQueryGetNames(query);
    for (int ncvarid = 0; ncvarid < nvars; ++ncvarid)
    {
      ncvar_t *ncvar = &ncvars[ncvarid];
      if (ncvar->varStatus == DataVar)
      {
        bool foundName = false;
        for (int i = 0; i < numNames; ++i)
        {
          if (str_is_equal(names[i], ncvar->name))
          {
            foundName = true;
            break;
          }
        }
        if (!foundName) { ncvar->varStatus = CoordVar; }
      }
    }
    */
    for (int ncvarid = 0; ncvarid < nvars; ++ncvarid)
    {
      ncvar_t *ncvar = &ncvars[ncvarid];
      if (ncvar->varStatus == DataVar && cdiQueryName(query, ncvar->name) < 0) { ncvar->varStatus = CoordVar; }
    }
  }
}

int
cdfInqContents(stream_t *streamP)
{
  GridInfo gridInfo = { 0 };
  gridInfo.number_of_grid_used = CDI_UNDEFID;
  gridInfo.timedimid = CDI_UNDEFID;

  int vlistID = streamP->vlistID;
  int fileID = streamP->groupID;

  if (CDI_Debug) Message("streamID=%d, fileID=%d", streamP->self, fileID);

  int ndims = 0, nvars = 0, ngatts = 0, unlimdimid = 0;
  cdf_inq(fileID, &ndims, &nvars, &ngatts, &unlimdimid);

  if (CDI_Debug) Message("root: ndims=%d, nvars=%d, ngatts=%d", ndims, nvars, ngatts);

  int format = 0;
  nc_inq_format(fileID, &format);
  if (format == NC_FORMAT_NETCDF4) check_ncgroups(fileID);

  if (nvars == 0)
  {
    Warning("No arrays found!");
    return CDI_EUFSTRUCT;
  }

  // alloc ncdims
  ncdim_t *ncdims = ndims ? (ncdim_t *) Malloc((size_t) ndims * sizeof(ncdim_t)) : NULL;
  init_ncdims(ndims, ncdims);

  // alloc ncvars
  ncvar_t *ncvars = (ncvar_t *) Malloc((size_t) nvars * sizeof(ncvar_t));
  init_ncvars(nvars, ncvars, fileID);

  read_vars_info(nvars, ncvars, format);

  set_dimension_ids(nvars, ncvars, ndims, ncdims);
  read_ncdims(fileID, ndims, ncdims);

  change_dimension_ids(nvars, ncvars, ndims, ncdims);
  find_coordinates_vars(ndims, ncdims, nvars, ncvars);

  // scan global attributes
  int instID = CDI_UNDEFID;
  int modelID = CDI_UNDEFID;
  bool ucla_les = false;
  unsigned char uuidOfVGrid[CDI_UUID_SIZE] = { 0 };
  cdf_scan_global_attr(fileID, vlistID, ngatts, &instID, &modelID, &ucla_les, uuidOfVGrid, &gridInfo);

  // find time dim
  int nctimedimid = (unlimdimid >= 0) ? unlimdimid : cdf_time_dimid(fileID, ndims, ncdims, nvars, ncvars);
  int timedimid = (nctimedimid == CDI_UNDEFID) ? CDI_UNDEFID : find_dimidx(nctimedimid, ndims, ncdims);
  streamP->basetime.ncdimid = timedimid;

  size_t ntsteps = (timedimid == CDI_UNDEFID) ? 0 : ncdims[timedimid].len;
  if (ntsteps > INT_MAX)
  {
    Warning("Size limit exceeded for time dimension (limit=%d)!", INT_MAX);
    return CDI_EDIMSIZE;
  }

  if (CDI_Debug) Message("Number of timesteps = %zu", ntsteps);
  if (CDI_Debug) Message("Time dimid = %d", streamP->basetime.ncdimid);

  // set T_AXIS dimtype
  for (int gdimid = 0; gdimid < ndims; gdimid++)
  {
    if (timedimid == gdimid) { ncdims[gdimid].dimtype = T_AXIS; }
  }

  stream_set_ncdims(streamP, ndims, ncdims);

  if (CDI_Debug) cdf_print_vars(ncvars, nvars, "scan_vars_attr");

  // scan attributes of all variables
  set_vars_timetype(nvars, ncvars, timedimid);
  scan_vars_attr(nvars, ncvars, ndims, ncdims, modelID);
  verify_vars_attr(nvars, ncvars, ncdims);

  if (CDI_Convert_Cubesphere) check_cube_sphere(vlistID, nvars, ncvars, ncdims);

  if (CDI_Debug) cdf_print_vars(ncvars, nvars, "find_varying_data_vars1d");

  find_varying_data_vars1d(timedimid, nvars, ncvars);

  // find time vars
  bool timeHasUnits = false;
  bool timeHasBounds = false;
  bool timeClimatology = false;
  find_time_vars(nvars, ncvars, ncdims, timedimid, streamP, &timeHasUnits, &timeHasBounds, &timeClimatology);

  int leadtime_id = find_leadtime(nvars, ncvars, timedimid);
  if (leadtime_id != CDI_UNDEFID) ncvars[leadtime_id].varStatus = CoordVar;

  // check ncvars
  timedimid = cdf_check_variables(streamP, nvars, ncvars, ntsteps, timedimid);

  // verify coordinates vars - first scan (dimname == varname)
  bool isHybridCF = false;
  verify_coordinates_vars_1(fileID, ndims, ncdims, ncvars, timedimid, &isHybridCF);

  // verify coordinates vars - second scan (all other variables)
  verify_coordinates_vars_2(streamP, nvars, ncvars);

  if (CDI_Debug) cdf_print_vars(ncvars, nvars, "verify_coordinate_vars");

  if (ucla_les) cdf_set_ucla_dimtype(ndims, ncdims, ncvars);

  /*
  for (int ncdimid = 0; ncdimid < ndims; ncdimid++)
    {
      int ncvarid = ncdims[ncdimid].ncvarid;
      if (ncvarid != -1)
        {
          printf("coord var %d %s %s\n", ncvarid, ncvar->name, ncvar->units);
          if (ncdims[ncdimid].dimtype == X_AXIS) printf("coord var %d %s is x dim\n", ncvarid, ncvar->name);
          if (ncdims[ncdimid].dimtype == Y_AXIS) printf("coord var %d %s is y dim\n", ncvarid, ncvar->name);
          if (ncdims[ncdimid].dimtype == Z_AXIS) printf("coord var %d %s is z dim\n", ncvarid, ncvar->name);
          if (ncdims[ncdimid].dimtype == T_AXIS) printf("coord var %d %s is t dim\n", ncvarid, ncvar->name);

          if (ncvar->isLon) printf("coord var %d %s is lon\n", ncvarid, ncvar->name);
          if (ncvar->isLat) printf("coord var %d %s is lat\n", ncvarid, ncvar->name);
          if (ncvar->isZaxis) printf("coord var %d %s is lev\n", ncvarid, ncvar->name);
        }
    }
  */

  set_coordinates_varids(nvars, ncvars);

  cdf_set_dimtype(nvars, ncvars, ncdims);

  // read ECHAM VCT if present
  size_t vctsize = 0;
  double *vct = NULL;
  if (!isHybridCF) read_vct_echam(fileID, nvars, ncvars, ncdims, &vct, &vctsize);

  // process var query information if available
  process_var_query(streamP->query, nvars, ncvars);

  if (CDI_Debug) cdf_print_vars(ncvars, nvars, "cdf_define_all_grids");

  // define all grids
  gridInfo.timedimid = timedimid;
  int status = cdf_define_all_grids(streamP, vlistID, ncdims, nvars, ncvars, &gridInfo);
  if (status < 0) return status;

  // define all zaxes
  status = cdf_define_all_zaxes(streamP, vlistID, ncdims, nvars, ncvars, vctsize, vct, uuidOfVGrid);
  Free(vct);
  if (status < 0) return status;

  // verify vars
  cdfVerifyVars(nvars, ncvars);

  // select vars
  int nvarsData = 0;
  for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
    if (ncvars[ncvarid].varStatus == DataVar) nvarsData++;

  if (CDI_Debug) Message("time varid = %d", streamP->basetime.ncvarid);
  if (CDI_Debug) Message("ntsteps = %zu", ntsteps);
  if (CDI_Debug) Message("nvarsData = %d", nvarsData);

  if (nvarsData == 0)
  {
    streamP->ntsteps = 0;
    Warning("No data arrays found!");
    return CDI_EUFSTRUCT;
  }

  if (ntsteps == 0 && streamP->basetime.ncdimid == CDI_UNDEFID && streamP->basetime.ncvarid != CDI_UNDEFID) ntsteps = 1;

  // define all data variables
  cdf_define_all_vars(streamP, vlistID, instID, modelID, nvarsData, nvars, ncvars, ncdims, timedimid);

  cdf_set_chunk_info(streamP, nvars, ncvars);

  // time varID
  int nctimevarid = streamP->basetime.ncvarid;

  if (nctimevarid != CDI_UNDEFID && (!timeHasUnits || streamP->basetime.isWRF)) ncvars[nctimevarid].units[0] = 0;
  if (nctimevarid != CDI_UNDEFID && timeHasUnits) streamP->basetime.hasUnits = true;

  taxis_t taxis0;
  ptaxisInit(&taxis0);

  if (timeHasUnits)
  {
    if (set_base_time(ncvars[nctimevarid].units, &taxis0) == 1)
    {
      nctimevarid = CDI_UNDEFID;
      streamP->basetime.ncvarid = CDI_UNDEFID;
      streamP->basetime.hasUnits = false;
    }

    if (leadtime_id != CDI_UNDEFID && taxis0.type == TAXIS_RELATIVE)
    {
      streamP->basetime.leadtimeid = leadtime_id;
      taxis0.type = TAXIS_FORECAST;

      int timeunit = (ncvars[leadtime_id].units[0] != 0) ? scan_time_units(ncvars[leadtime_id].units) : -1;
      if (timeunit == -1) timeunit = taxis0.unitsValue;
      taxis0.fc_units = timeunit;
    }
  }

  if (timeHasBounds)
  {
    taxis0.hasBounds = true;
    if (timeClimatology) taxis0.climatology = true;
  }

  if (nctimevarid != CDI_UNDEFID)
  {
    ptaxisDefName(&taxis0, ncvars[nctimevarid].name);
    if (ncvars[nctimevarid].longname[0]) ptaxisDefLongname(&taxis0, ncvars[nctimevarid].longname);
    if (ncvars[nctimevarid].units[0]) ptaxisDefUnits(&taxis0, ncvars[nctimevarid].units);

    int xtype = ncvars[nctimevarid].xtype;
    int datatype = (xtype == NC_INT) ? CDI_DATATYPE_INT32 : ((xtype == NC_FLOAT) ? CDI_DATATYPE_FLT32 : CDI_DATATYPE_FLT64);
    ptaxisDefDatatype(&taxis0, datatype);
  }

  int calendar = CDI_UNDEFID;
  if (nctimevarid != CDI_UNDEFID && ncvars[nctimevarid].hasCalendar)
  {
    String attString;
    string_init(&attString);
    cdfGetAttString(fileID, nctimevarid, "calendar", &attString);
    str_to_lower(attString.data);
    calendar = attribute_to_calendar(attString.data);
    string_free(&attString);
  }

  if (streamP->basetime.isWRF) taxis0.type = TAXIS_ABSOLUTE;

  int taxisID;
  if (taxis0.type == TAXIS_FORECAST) { taxisID = taxisCreate(TAXIS_FORECAST); }
  else if (taxis0.type == TAXIS_RELATIVE) { taxisID = taxisCreate(TAXIS_RELATIVE); }
  else
  {
    taxisID = taxisCreate(TAXIS_ABSOLUTE);
    if (!timeHasUnits)
    {
      taxisDefTunit(taxisID, TUNIT_DAY);
      taxis0.unitsValue = TUNIT_DAY;
    }
  }

  if (calendar == CDI_UNDEFID && taxis0.type != TAXIS_ABSOLUTE) calendar = CALENDAR_STANDARD;

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5)
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wstrict-overflow"
#endif
  if (calendar != CDI_UNDEFID)
  {
    taxis0.calendar = calendar;
    taxisDefCalendar(taxisID, calendar);
  }
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5)
#pragma GCC diagnostic pop
#endif

  vlistDefTaxis(vlistID, taxisID);

  cdf_read_timesteps(ntsteps, streamP, &taxis0);
  taxisDestroyKernel(&taxis0);

  // free ncdims
  Free(ncdims);

  // free ncvars
  if (ncvars)
  {
    for (int ncvarid = 0; ncvarid < nvars; ncvarid++)
    {
      ncvar_t *ncvar = &ncvars[ncvarid];
      Free(ncvar->atts);
      Free(ncvar->vct);
    }
    Free(ncvars);
  }

  return 0;
}

int
cdfInqTimestep(stream_t *streamP, int tsID)
{
  if (tsID < 0 || tsID >= streamP->ntsteps) Error("tsID=%d out of range (0-%d)!", tsID, streamP->ntsteps - 1);

  streamP->curTsID = tsID;
  int numRecs = streamP->tsteps[tsID].nrecs;

  return numRecs;
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
