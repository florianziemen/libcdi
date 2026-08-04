#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "dmemory.h"
#include "cdi.h"
#include "cdi_int.h"
#include "cdi_utils.h"
#include "cdi_uuid.h"
#include "grid.h"
#include "grid_pack.h"
#include "resource_unpack.h"
#include "vlist.h"

int (*proj_lonlat_to_lcc_func)(struct CDI_GridProjParams gpp, size_t, double *, double *) = NULL;
int (*proj_lcc_to_lonlat_func)(struct CDI_GridProjParams gpp, double, double, size_t, double *, double *) = NULL;
int (*proj_lonlat_to_stere_func)(struct CDI_GridProjParams gpp, size_t, double *, double *) = NULL;
int (*proj_stere_to_lonlat_func)(struct CDI_GridProjParams gpp, double, double, size_t, double *, double *) = NULL;

// the value in the second pair of brackets must match the length of the longest string (including terminating NUL)
static const char Grids[][17] = {
  /*  0 */ "undefined",
  /*  1 */ "generic",
  /*  2 */ "gaussian",
  /*  3 */ "gaussian_reduced",
  /*  4 */ "lonlat",
  /*  5 */ "spectral",
  /*  6 */ "fourier",
  /*  7 */ "gme",
  /*  8 */ "trajectory",
  /*  9 */ "unstructured",
  /* 10 */ "curvilinear",
  /* 11 */ "healpix",
  /* 12 */ "projection",
  /* 13 */ "characterXY",
};

// must match table below
enum xystdname_idx
{
  grid_xystdname_grid_latlon,
  grid_xystdname_latlon,
  grid_xystdname_projection,
  grid_xystdname_char,
};
static const char xystdname_tab[][2][24] = {
  [grid_xystdname_grid_latlon] = { "grid_longitude", "grid_latitude" },
  [grid_xystdname_latlon] = { "longitude", "latitude" },
  [grid_xystdname_projection] = { "projection_x_coordinate", "projection_y_coordinate" },
  [grid_xystdname_char] = { "region", "region" },
};

static int grid_compare(void *gridP1, void *gridP2);
static void grid_destroy(void *gridP);
static void grid_print(void *gridP, FILE *fp);

const resOps gridOps = { grid_compare, grid_destroy, grid_print, gridGetPackSize, gridPack, gridTxCode };

grid_t *
grid_to_pointer(int gridID)
{
  return (grid_t *) reshGetVal(gridID, &gridOps);
}

#define gridMark4Update(gridID) reshSetStatus(gridID, &gridOps, RESH_DESYNC_IN_USE)

static bool
cdiInqAttConvertedToFloat(int gridID, int atttype, const char *attname, int attlen, double *attflt)
{
  if (atttype == CDI_DATATYPE_INT32)
  {
    int attint = 0;
    int *pattint = attlen > 1 ? (int *) Malloc((size_t) attlen * sizeof(int)) : &attint;
    cdiInqAttInt(gridID, CDI_GLOBAL, attname, attlen, pattint);
    for (int i = 0; i < attlen; ++i) { attflt[i] = (double) pattint[i]; }
    if (attlen > 1) Free(pattint);
    return true;
  }

  if (atttype == CDI_DATATYPE_FLT32 || atttype == CDI_DATATYPE_FLT64)
  {
    cdiInqAttFlt(gridID, CDI_GLOBAL, attname, attlen, attflt);
    return true;
  }

  return false;
}

static void
grid_axis_init(struct gridaxis_t *axisptr)
{
  axisptr->size = 0;
  axisptr->vals = NULL;
  axisptr->bounds = NULL;
  axisptr->flag = 0;
  axisptr->first = 0.0;
  axisptr->last = 0.0;
  axisptr->inc = 0.0;
#ifndef USE_MPI
  axisptr->clength = 0;
  axisptr->cvals = NULL;
#endif
  cdiInitKeys(&axisptr->keys);
}

enum cdiApplyRet
cdiGridApply(enum cdiApplyRet (*func)(int id, void *res, void *data), void *data)
{
  return cdiResHFilterApply(&gridOps, func, data);
}

void
grid_init(grid_t *gridP)
{
  gridP->self = CDI_UNDEFID;
  gridP->type = CDI_UNDEFID;
  gridP->datatype = CDI_UNDEFID;
  gridP->proj = CDI_UNDEFID;
  gridP->projtype = CDI_UNDEFID;
  gridP->mask = NULL;
  gridP->mask_gme = NULL;
  gridP->size = 0;

  grid_axis_init(&gridP->x);
  grid_axis_init(&gridP->y);

  gridP->indices = NULL;
  gridP->area = NULL;
  gridP->reducedPoints = NULL;
  gridP->reducedPointsSize = 0;

  gridP->gme.nd = 0;
  gridP->gme.ni = 0;
  gridP->gme.ni2 = 0;
  gridP->gme.ni3 = 0;

  gridP->trunc = 0;
  gridP->nvertex = 0;
  gridP->np = 0;
  gridP->isCyclic = CDI_UNDEFID;

  gridP->lcomplex = false;
  gridP->hasdims = true;
  gridP->name = NULL;
  gridP->vtable = &cdiGridVtable;

  cdiInitKeys(&gridP->keys);
  gridP->atts.nalloc = MAX_ATTRIBUTES;
  gridP->atts.nelems = 0;

  cdiDefVarKeyInt(&gridP->keys, CDI_KEY_DATATYPE, CDI_DATATYPE_FLT64);
#ifdef HIRLAM_EXTENSIONS
  cdiDefVarKeyInt(&gridP->keys, CDI_KEY_SCANNINGMODE, 64);
#endif

  gridP->extraData = NULL;
}

static void
grid_free_components(grid_t *gridP)
{
  void *p2free[] = { gridP->indices,  gridP->mask,     gridP->mask_gme,      gridP->x.vals, gridP->y.vals,
#ifndef USE_MPI
                     gridP->x.cvals,  gridP->y.cvals,
#endif
                     gridP->x.bounds, gridP->y.bounds, gridP->reducedPoints, gridP->area,   gridP->name };

  for (size_t i = 0; i < ARRAY_SIZE(p2free); ++i) Free(p2free[i]);

  cdiDeleteVarKeys(&(gridP->x.keys));
  cdiDeleteVarKeys(&(gridP->y.keys));
  cdiDeleteVarKeys(&(gridP->keys));
  /* 12 pio tests fail
  int gridID = gridP->self;
  if (gridID != CDI_UNDEFID) cdiDeleteAtts(gridID, CDI_GLOBAL);
  */
}

void
grid_free(grid_t *gridP)
{
  if (gridP)
  {
    grid_free_components(gridP);
    grid_init(gridP);
  }
}

grid_t *
gridNewEntry(cdiResH resH)
{
  grid_t *gridP = (grid_t *) Malloc(sizeof(grid_t));
  grid_init(gridP);

  if (resH == CDI_UNDEFID) { gridP->self = reshPut(gridP, &gridOps); }
  else
  {
    gridP->self = resH;
    reshReplace(resH, gridP, &gridOps);
  }

  return gridP;
}

static void
grid_copy_base_scalar_fields(grid_t *gridPOrig, grid_t *gridPDup)
{
  memcpy(gridPDup, gridPOrig, sizeof(grid_t));
  gridPDup->self = CDI_UNDEFID;
  cdiInitKeys(&gridPDup->keys);
  cdiCopyVarKeys(&gridPOrig->keys, &gridPDup->keys);
  cdiInitKeys(&gridPDup->x.keys);
  cdiCopyVarKeys(&gridPOrig->x.keys, &gridPDup->x.keys);
  cdiInitKeys(&gridPDup->y.keys);
  cdiCopyVarKeys(&gridPOrig->y.keys, &gridPDup->y.keys);
}

static grid_t *
grid_copy_base(grid_t *gridPOrig)
{
  grid_t *gridPDup = (grid_t *) Malloc(sizeof(*gridPDup));
  gridPOrig->vtable->copyScalarFields(gridPOrig, gridPDup);
  gridPOrig->vtable->copyArrayFields(gridPOrig, gridPDup);
  return gridPDup;
}

unsigned
cdiGridCount(void)
{
  return reshCountType(&gridOps);
}

static inline void
gridaxisSetKey(struct gridaxis_t *axisptr, int key, const char *name)
{
  if (find_key(&axisptr->keys, key) == NULL)
    cdiDefVarKeyBytes(&axisptr->keys, key, (const unsigned char *) name, (int) strlen(name) + 1);
}

void
cdiGridTypeInit(grid_t *gridP, int gridtype, size_t size)
{
  gridP->type = gridtype;
  gridP->size = size;

  // clang-format off
  if      (gridtype == GRID_LONLAT)           gridP->nvertex = 2;
  else if (gridtype == GRID_GAUSSIAN)         gridP->nvertex = 2;
  else if (gridtype == GRID_GAUSSIAN_REDUCED) gridP->nvertex = 2;
  else if (gridtype == GRID_CURVILINEAR)      gridP->nvertex = 4;
  else if (gridtype == GRID_UNSTRUCTURED)     gridP->x.size = size;
  // clang-format on

  switch (gridtype)
  {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
    case GRID_GAUSSIAN_REDUCED:
    case GRID_TRAJECTORY:
    case GRID_CURVILINEAR:
    case GRID_UNSTRUCTURED:
    case GRID_GME:
    {
      bool isTraj = gridtype == GRID_TRAJECTORY;
      gridaxisSetKey(&gridP->x, CDI_KEY_NAME, isTraj ? "tlon" : "lon");
      gridaxisSetKey(&gridP->y, CDI_KEY_NAME, isTraj ? "tlat" : "lat");
      gridaxisSetKey(&gridP->x, CDI_KEY_LONGNAME, "longitude");
      gridaxisSetKey(&gridP->y, CDI_KEY_LONGNAME, "latitude");
      gridaxisSetKey(&gridP->x, CDI_KEY_UNITS, "degrees_east");
      gridaxisSetKey(&gridP->y, CDI_KEY_UNITS, "degrees_north");
      gridaxisSetKey(&gridP->x, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_latlon][0]);
      gridaxisSetKey(&gridP->y, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_latlon][1]);
      break;
    }
#ifndef USE_MPI
    case GRID_CHARXY:
    {
      if (gridP->x.cvals) gridaxisSetKey(&gridP->x, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_char][0]);
      if (gridP->y.cvals) gridaxisSetKey(&gridP->y, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_char][1]);
      break;
    }
#endif
    case GRID_GENERIC:
    case GRID_PROJECTION:
    {
      gridaxisSetKey(&gridP->x, CDI_KEY_NAME, "x");
      gridaxisSetKey(&gridP->y, CDI_KEY_NAME, "y");
      if (gridtype == GRID_PROJECTION)
      {
        gridaxisSetKey(&gridP->x, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_projection][0]);
        gridaxisSetKey(&gridP->y, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_projection][1]);
        gridaxisSetKey(&gridP->x, CDI_KEY_UNITS, "m");
        gridaxisSetKey(&gridP->y, CDI_KEY_UNITS, "m");
      }
      break;
    }
  }
}

// used also in CDO
void
gridGenXvals(size_t xsize, double xfirst, double xlast, double xinc, double *restrict xvals)
{
  if (fabs(xinc) <= 0 && xsize > 1)
  {
    if (xfirst >= xlast)
    {
      while (xfirst >= xlast) { xlast += 360; }
      xinc = (xlast - xfirst) / (double) xsize;
    }
    else
    {
      xinc = (xlast - xfirst) / (double) (xsize - 1);
    }
  }

  for (size_t i = 0; i < xsize; ++i) { xvals[i] = xfirst + (double) i * xinc; }
}

static void
calc_gaussgrid(double *restrict yvals, size_t ysize, double yfirst, double ylast)
{
  double *yw = (double *) Malloc(ysize * sizeof(double));
  gaussian_latitudes(ysize, yvals, yw);
  Free(yw);
  for (size_t i = 0; i < ysize; i++) { yvals[i] = asin(yvals[i]) / M_PI * 180.0; }

  if (yfirst < ylast && yfirst > -90.0 && ylast < 90.0)
  {
    size_t yhsize = ysize / 2;
    for (size_t i = 0; i < yhsize; i++)
    {
      double ytmp = yvals[i];
      yvals[i] = yvals[ysize - i - 1];
      yvals[ysize - i - 1] = ytmp;
    }
  }
}

static void
gridGenYvalsGaussian(size_t ysize, double yfirst, double ylast, double *restrict yvals)
{
  const double deleps = 0.002;

  calc_gaussgrid(yvals, ysize, yfirst, ylast);

  if (!(is_equal(yfirst, 0) && is_equal(ylast, 0)))
    if (fabs(yvals[0] - yfirst) > deleps || fabs(yvals[ysize - 1] - ylast) > deleps)
    {
      bool lfound = false;
      long ny = (long) (180. / (fabs(ylast - yfirst) / (double) (ysize - 1)) + 0.5);
      ny -= ny % 2;
      if (ny > 0 && (size_t) ny > ysize && ny < 4096)
      {
        double *ytmp = (double *) Malloc((size_t) ny * sizeof(double));
        calc_gaussgrid(ytmp, (size_t) ny, yfirst, ylast);

        size_t nstart = (size_t) ny - ysize;
        for (size_t i = 0; i < ((size_t) ny - ysize); i++)
          if (fabs(ytmp[i] - yfirst) < deleps)
          {
            nstart = i;
            break;
          }

        lfound = (nstart + ysize - 1) < (size_t) ny && fabs(ytmp[nstart + ysize - 1] - ylast) < deleps;
        if (lfound)
        {
          for (size_t i = 0; i < ysize; i++) yvals[i] = ytmp[i + nstart];
        }

        Free(ytmp);
      }

      if (!lfound)
      {
        Warning("Cannot calculate gaussian latitudes for lat1 = %g latn = %g!", yfirst, ylast);
        for (size_t i = 0; i < ysize; i++) yvals[i] = 0;
        yvals[0] = yfirst;
        yvals[ysize - 1] = ylast;
      }
    }
}

static void
gridGenYvalsRegular(size_t ysize, double yfirst, double ylast, double yinc, double *restrict yvals)
{
  if (fabs(yinc) <= 0 && ysize > 1)
  {
    if (is_equal(yfirst, ylast) && is_not_equal(yfirst, 0)) ylast *= -1;

    if (yfirst > ylast) { yinc = (yfirst - ylast) / (double) (ysize - 1); }
    else if (yfirst < ylast) { yinc = (ylast - yfirst) / (double) (ysize - 1); }
    else
    {
      if (ysize % 2 != 0)
      {
        yinc = 180.0 / (double) (ysize - 1);
        yfirst = -90;
      }
      else
      {
        yinc = 180.0 / (double) ysize;
        yfirst = -90 + yinc / 2;
      }
    }
  }

  if (yfirst > ylast && yinc > 0) { yinc = -yinc; }

  for (size_t i = 0; i < ysize; i++) { yvals[i] = yfirst + (double) i * yinc; }
}

// used also in CDO
void
gridGenYvals(int gridtype, size_t ysize, double yfirst, double ylast, double yinc, double *restrict yvals)
{
  if (gridtype == GRID_GAUSSIAN || gridtype == GRID_GAUSSIAN_REDUCED)
  {
    if (ysize > 2) { gridGenYvalsGaussian(ysize, yfirst, ylast, yvals); }
    else
    {
      yvals[0] = yfirst;
      yvals[ysize - 1] = ylast;
    }
  }
  // else if (gridtype == GRID_LONLAT || gridtype == GRID_GENERIC)
  else
  {
    gridGenYvalsRegular(ysize, yfirst, ylast, yinc, yvals);
  }
  /*
    else
    Error("unable to calculate values for %s grid!", gridNamePtr(gridtype));
  */
}

/*
@Function  gridCreate
@Title     Create a horizontal Grid

@Prototype int gridCreate(int gridtype, SizeType size)
@Parameter
    @Item  gridtype  The type of the grid, one of the set of predefined CDI grid types.
                     The valid CDI grid types are @func{GRID_GENERIC}, @func{GRID_LONLAT},
                     @func{GRID_GAUSSIAN}, @func{GRID_PROJECTION}, @func{GRID_SPECTRAL},
                     @func{GRID_GME}, @func{GRID_CURVILINEAR} and @func{GRID_UNSTRUCTURED}.
    @Item  size      Number of gridpoints.

@Description
The function @func{gridCreate} creates a horizontal Grid.

@Result
@func{gridCreate} returns an identifier to the Grid.

@Example
Here is an example using @func{gridCreate} to create a regular lon/lat Grid:

@Source
#include "cdi.h"
   ...
#define  nlon  12
#define  nlat   6
   ...
double lons[nlon] = {0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330};
double lats[nlat] = {-75, -45, -15, 15, 45, 75};
int gridID;
   ...
gridID = gridCreate(GRID_LONLAT, nlon*nlat);
gridDefXsize(gridID, nlon);
gridDefYsize(gridID, nlat);
gridDefXvals(gridID, lons);
gridDefYvals(gridID, lats);
   ...
@EndSource
@EndFunction
*/
int
gridCreate(int gridtype, SizeType size)
{
  if (CDI_Debug) Message("gridtype=%s  size=%zu", gridNamePtr(gridtype), (size_t) size);

  xassert(size);

  grid_t *gridP = gridNewEntry(CDI_UNDEFID);
  if (!gridP) Error("No memory");

  int gridID = gridP->self;
  if (CDI_Debug) Message("gridID: %d", gridID);

  cdiGridTypeInit(gridP, gridtype, (size_t) size);

  return gridID;
}

static void
gridDestroyKernel(grid_t *gridP)
{
  xassert(gridP);
  grid_free_components(gridP);
  Free(gridP);
}

/*
@Function  gridDestroy
@Title     Destroy a horizontal Grid

@Prototype void gridDestroy(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.

@EndFunction
*/
void
gridDestroy(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->destroy(gridP);
  reshRemove(gridID, &gridOps);
}

static void
grid_destroy(void *gridP)
{
  ((grid_t *) gridP)->vtable->destroy((grid_t *) gridP);
}

const char *
gridNamePtr(int gridtype)
{
  const int size = (int) ARRAY_SIZE(Grids);
  return (gridtype >= 0 && gridtype < size) ? Grids[gridtype] : Grids[GRID_GENERIC];
}

/*
@Function  gridDefXname
@Title     Define the name of a X-axis

@Prototype void gridDefXname(int gridID, const char *name)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  name     Name of the X-axis.

@Description
The function @func{gridDefXname} defines the name of a X-axis.

@EndFunction
*/
void
gridDefXname(int gridID, const char *name)
{
  (void) cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_NAME, name);
}

/*
@Function  gridDefXlongname
@Title     Define the longname of a X-axis

@Prototype void gridDefXlongname(int gridID, const char *longname)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  longname Longname of the X-axis.

@Description
The function @func{gridDefXlongname} defines the longname of a X-axis.

@EndFunction
*/
void
gridDefXlongname(int gridID, const char *longname)
{
  (void) cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_LONGNAME, longname);
}

/*
@Function  gridDefXunits
@Title     Define the units of a X-axis

@Prototype void gridDefXunits(int gridID, const char *units)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  units    Units of the X-axis.

@Description
The function @func{gridDefXunits} defines the units of a X-axis.

@EndFunction
*/
void
gridDefXunits(int gridID, const char *units)
{
  (void) cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_UNITS, units);
}

/*
@Function  gridDefYname
@Title     Define the name of a Y-axis

@Prototype void gridDefYname(int gridID, const char *name)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  name     Name of the Y-axis.

@Description
The function @func{gridDefYname} defines the name of a Y-axis.

@EndFunction
*/
void
gridDefYname(int gridID, const char *name)
{
  (void) cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_NAME, name);
}

/*
@Function  gridDefYlongname
@Title     Define the longname of a Y-axis

@Prototype void gridDefYlongname(int gridID, const char *longname)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  longname Longname of the Y-axis.

@Description
The function @func{gridDefYlongname} defines the longname of a Y-axis.

@EndFunction
*/
void
gridDefYlongname(int gridID, const char *longname)
{
  (void) cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_LONGNAME, longname);
}

/*
@Function  gridDefYunits
@Title     Define the units of a Y-axis

@Prototype void gridDefYunits(int gridID, const char *units)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  units    Units of the Y-axis.

@Description
The function @func{gridDefYunits} defines the units of a Y-axis.

@EndFunction
*/
void
gridDefYunits(int gridID, const char *units)
{
  (void) cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_UNITS, units);
}

/*
@Function  gridInqXname
@Title     Get the name of a X-axis

@Prototype void gridInqXname(int gridID, char *name)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  name     Name of the X-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{gridInqXname} returns the name of a X-axis.

@Result
@func{gridInqXname} returns the name of the X-axis to the parameter name.

@EndFunction
*/
void
gridInqXname(int gridID, char *name)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_NAME, name, &length);
}

/*
@Function  gridInqXlongname
@Title     Get the longname of a X-axis

@Prototype void gridInqXlongname(int gridID, char *longname)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  longname Longname of the X-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{gridInqXlongname} returns the longname of a X-axis.

@Result
@func{gridInqXlongname} returns the longname of the X-axis to the parameter longname.

@EndFunction
*/
void
gridInqXlongname(int gridID, char *longname)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_LONGNAME, longname, &length);
}

/*
@Function  gridInqXunits
@Title     Get the units of a X-axis

@Prototype void gridInqXunits(int gridID, char *units)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  units    Units of the X-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{gridInqXunits} returns the units of a X-axis.

@Result
@func{gridInqXunits} returns the units of the X-axis to the parameter units.

@EndFunction
*/
void
gridInqXunits(int gridID, char *units)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_UNITS, units, &length);
}

/*
@Function  gridInqYname
@Title     Get the name of a Y-axis

@Prototype void gridInqYname(int gridID, char *name)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  name     Name of the Y-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{gridInqYname} returns the name of a Y-axis.

@Result
@func{gridInqYname} returns the name of the Y-axis to the parameter name.

@EndFunction
*/
void
gridInqYname(int gridID, char *name)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_NAME, name, &length);
}

/*
@Function  gridInqYlongname
@Title     Get the longname of a Y-axis

@Prototype void gridInqYlongname(int gridID, char *longname)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  longname Longname of the Y-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{gridInqYlongname} returns the longname of a Y-axis.

@Result
@func{gridInqYlongname} returns the longname of the Y-axis to the parameter longname.

@EndFunction
*/
void
gridInqYlongname(int gridID, char *longname)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_LONGNAME, longname, &length);
}

/*
@Function  gridInqYunits
@Title     Get the units of a Y-axis

@Prototype void gridInqYunits(int gridID, char *units)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  units    Units of the Y-axis. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.

@Description
The function @func{gridInqYunits} returns the units of a Y-axis.

@Result
@func{gridInqYunits} returns the units of the Y-axis to the parameter units.

@EndFunction
*/
void
gridInqYunits(int gridID, char *units)
{
  int length = CDI_MAX_NAME;
  (void) cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_UNITS, units, &length);
}

static void
gridDefIndicesSerial(grid_t *gridP, const int64_t *indices)
{
  size_t size = gridP->size;
  if (size == 0) Error("Size undefined for gridID = %d", gridP->self);

  if (gridP->indices && CDI_Debug) Warning("values already defined!");
  gridP->indices = (int64_t *) Realloc(gridP->indices, size * sizeof(int64_t));
  memcpy(gridP->indices, indices, size * sizeof(int64_t));
}

void
gridDefIndices(int gridID, const int64_t *indices)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defIndices(gridP, indices);
  gridMark4Update(gridID);
}

static const int64_t *
gridInqIndicesPtrSerial(grid_t *gridP)
{
  return gridP->indices;
}

static SizeType
gridInqIndicesSerial(grid_t *gridP, int64_t *indices)
{
  size_t size = gridP->size;
  if (CDI_Debug && size == 0) Warning("size undefined for gridID = %d", gridP->self);

  if (gridP->indices)
  {
    if (size && indices) memcpy(indices, gridP->vtable->inqIndicesPtr(gridP), size * sizeof(int64_t));
  }
  else
    size = 0;

  return (SizeType) size;
}

SizeType
gridInqIndices(int gridID, int64_t *indices)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqIndices(gridP, indices);
}

void
gridDefProj(int gridID, int projID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->proj = projID;

  if (gridP->type == GRID_CURVILINEAR)
  {
    grid_t *projptr = grid_to_pointer(projID);
    const char *xdimname = cdiInqVarKeyStringPtr(&gridP->x.keys, CDI_KEY_DIMNAME);
    const char *ydimname = cdiInqVarKeyStringPtr(&gridP->y.keys, CDI_KEY_DIMNAME);
    if (xdimname && find_key(&projptr->x.keys, CDI_KEY_NAME)) cdiDefKeyString(projID, CDI_XAXIS, CDI_KEY_NAME, xdimname);
    if (ydimname && find_key(&projptr->y.keys, CDI_KEY_NAME)) cdiDefKeyString(projID, CDI_YAXIS, CDI_KEY_NAME, ydimname);
  }
}

int
gridInqProj(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->proj;
}

static int
gmapname_to_projType(const char *gmapName)
{
  if (str_is_equal(gmapName, "rotated_latitude_longitude")) return CDI_PROJ_RLL;
  if (str_is_equal(gmapName, "lambert_azimuthal_equal_area")) return CDI_PROJ_LAEA;
  if (str_is_equal(gmapName, "lambert_conformal_conic")) return CDI_PROJ_LCC;
  if (str_is_equal(gmapName, "sinusoidal")) return CDI_PROJ_SINU;
  if (str_is_equal(gmapName, "polar_stereographic")) return CDI_PROJ_STERE;
  if (str_is_equal(gmapName, "healpix")) return CDI_PROJ_HEALPIX;
  return -1;
}

int
gridInqProjType(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);

  int projType = gridP->projtype;
  if (projType == -1)
  {
    char gmapName[CDI_MAX_NAME];
    int length = CDI_MAX_NAME;
    cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapName, &length);
    if (gmapName[0]) { gridP->projtype = projType = gmapname_to_projType(gmapName); }
  }

  return projType;
}

void
gridVerifyProj(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);

  int projtype = gridInqProjType(gridID);
  if (projtype == CDI_PROJ_RLL)
  {
    gridaxisSetKey(&gridP->x, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_grid_latlon][0]);
    gridaxisSetKey(&gridP->y, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_grid_latlon][1]);
    gridaxisSetKey(&gridP->x, CDI_KEY_UNITS, "degrees");
    gridaxisSetKey(&gridP->y, CDI_KEY_UNITS, "degrees");
  }
  else if (projtype == CDI_PROJ_LCC)
  {
    gridaxisSetKey(&gridP->x, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_projection][0]);
    gridaxisSetKey(&gridP->y, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_projection][1]);
    gridaxisSetKey(&gridP->x, CDI_KEY_UNITS, "m");
    gridaxisSetKey(&gridP->y, CDI_KEY_UNITS, "m");
  }
}

/*
@Function  gridInqType
@Title     Get the type of a Grid

@Prototype int gridInqType(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqType} returns the type of a Grid.

@Result
@func{gridInqType} returns the type of the grid,
one of the set of predefined CDI grid types.
The valid CDI grid types are @func{GRID_GENERIC}, @func{GRID_LONLAT},
@func{GRID_GAUSSIAN}, @func{GRID_PROJECTION}, @func{GRID_SPECTRAL}, @func{GRID_GME},
@func{GRID_CURVILINEAR} and @func{GRID_UNSTRUCTURED}.

@EndFunction
*/
int
gridInqType(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->type;
}

/*
@Function  gridInqSize
@Title     Get the size of a Grid

@Prototype SizeType gridInqSize(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqSize} returns the size of a Grid.

@Result
@func{gridInqSize} returns the number of grid points of a Grid.

@EndFunction
*/
SizeType
gridInqSize(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return (SizeType) gridP->size;
}

static int
nsp2trunc(size_t nsp)
{
  /*  nsp = (trunc+1)*(trunc+1)              */
  /*      => trunc^2 + 3*trunc - (x-2) = 0   */
  /*                                         */
  /*  with:  y^2 + p*y + q = 0               */
  /*         y = -p/2 +- sqrt((p/2)^2 - q)   */
  /*         p = 3 and q = - (x-2)           */
  int trunc = (int) (sqrt((double) nsp * 4 + 1.) - 3) / 2;
  return trunc;
}

int
gridInqTrunc(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (gridP->trunc == 0)
  {
    if (gridP->type == GRID_SPECTRAL) gridP->trunc = nsp2trunc(gridP->size);
    /*
    else if      ( gridP->type == GRID_GAUSSIAN )
      gridP->trunc = nlat2trunc(gridP->y.size);
    */
  }

  return gridP->trunc;
}

void
gridDefTrunc(int gridID, int trunc)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (gridP->trunc != trunc)
  {
    gridMark4Update(gridID);
    gridP->trunc = trunc;
  }
}

/*
@Function  gridDefXsize
@Title     Define the number of values of a X-axis

@Prototype void gridDefXsize(int gridID, SizeType xsize)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  xsize    Number of values of a X-axis.

@Description
The function @func{gridDefXsize} defines the number of values of a X-axis.

@EndFunction
*/
void
gridDefXsize(int gridID, SizeType xsize)
{
  grid_t *gridP = grid_to_pointer(gridID);

  SizeType gridSize = gridP->size;
  if (xsize > gridSize) Error("xsize %lld is greater then gridsize %lld", (long long) xsize, (long long) gridSize);

  int gridType = gridP->type;
  if (gridType == GRID_UNSTRUCTURED && xsize != gridSize)
    Error("xsize %lld must be equal to gridsize %lld for gridtype: %s", (long long) xsize, (long long) gridSize,
          gridNamePtr(gridType));
  if (gridType == GRID_GAUSSIAN_REDUCED && xsize != 2 && xsize != gridSize)
    Error("xsize %lld must be equal to gridsize %lld for gridtype: %s", (long long) xsize, (long long) gridSize,
          gridNamePtr(gridType));

  if (gridP->x.size != (size_t) xsize)
  {
    gridMark4Update(gridID);
    gridP->x.size = (size_t) xsize;
  }

  if (gridType != GRID_UNSTRUCTURED && gridType != GRID_GAUSSIAN_REDUCED && gridType != GRID_PROJECTION)
  {
    size_t axisproduct = gridP->x.size * gridP->y.size;
    if (axisproduct > 0 && axisproduct != (size_t) gridSize)
      Error("Inconsistent grid declaration! (xsize=%zu ysize=%zu gridsize=%zu)", gridP->x.size, gridP->y.size, (size_t) gridSize);
  }
}

void
gridDefDatatype(int gridID, int datatype)
{
  cdiDefKeyInt(gridID, CDI_GLOBAL, CDI_KEY_DATATYPE, datatype);
}

int
gridInqDatatype(int gridID)
{
  int datatype = CDI_UNDEFID;
  cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);
  return datatype;
}

/*
@Function  gridInqXsize
@Title     Get the number of values of a X-axis

@Prototype SizeType gridInqXsize(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqXsize} returns the number of values of a X-axis.

@Result
@func{gridInqXsize} returns the number of values of a X-axis.

@EndFunction
*/
SizeType
gridInqXsize(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return (SizeType) gridP->x.size;
}

/*
@Function  gridDefYsize
@Title     Define the number of values of a Y-axis

@Prototype void gridDefYsize(int gridID, SizeType ysize)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  ysize    Number of values of a Y-axis.

@Description
The function @func{gridDefYsize} defines the number of values of a Y-axis.

@EndFunction
*/
void
gridDefYsize(int gridID, SizeType ysize)
{
  grid_t *gridP = grid_to_pointer(gridID);

  SizeType gridSize = gridP->size;

  if (ysize > gridSize) Error("ysize %lld is greater then gridsize %lld", (long long) ysize, (long long) gridSize);

  int gridType = gridP->type;
  if (gridType == GRID_UNSTRUCTURED && ysize != gridSize)
    Error("ysize %lld must be equal gridsize %lld for gridtype: %s", (long long) ysize, (long long) gridSize,
          gridNamePtr(gridType));

  if (gridP->y.size != (size_t) ysize)
  {
    gridMark4Update(gridID);
    gridP->y.size = (size_t) ysize;
  }

  if (gridType != GRID_UNSTRUCTURED && gridType != GRID_GAUSSIAN_REDUCED && gridType != GRID_PROJECTION)
  {
    size_t axisproduct = gridP->x.size * gridP->y.size;
    if (axisproduct > 0 && axisproduct != (size_t) gridSize)
      Error("Inconsistent grid declaration! (xsize=%zu ysize=%zu gridsize=%zu)", gridP->x.size, gridP->y.size, (size_t) gridSize);
  }
}

/*
@Function  gridInqYsize
@Title     Get the number of values of a Y-axis

@Prototype SizeType gridInqYsize(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqYsize} returns the number of values of a Y-axis.

@Result
@func{gridInqYsize} returns the number of values of a Y-axis.

@EndFunction
*/
SizeType
gridInqYsize(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return (SizeType) gridP->y.size;
}

/*
@Function  gridDefNP
@Title     Define the number of parallels between a pole and the equator

@Prototype void gridDefNP(int gridID, int np)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  np       Number of parallels between a pole and the equator.

@Description
The function @func{gridDefNP} defines the number of parallels between a pole and the equator
of a Gaussian grid.

@EndFunction
*/
void
gridDefNP(int gridID, int np)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (gridP->np != np)
  {
    gridMark4Update(gridID);
    gridP->np = np;
  }
}

/*
@Function  gridInqNP
@Title     Get the number of parallels between a pole and the equator

@Prototype int gridInqNP(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqNP} returns the number of parallels between a pole and the equator
of a Gaussian grid.

@Result
@func{gridInqNP} returns the number of parallels between a pole and the equator.

@EndFunction
*/
int
gridInqNP(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->np;
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
void
gridDefReducedPoints(int gridID, int reducedPointsSize, const int reducedPoints[])
{
  grid_t *gridP = grid_to_pointer(gridID);

  gridP->reducedPoints = (int *) Malloc((size_t) reducedPointsSize * sizeof(int));
  gridP->reducedPointsSize = reducedPointsSize;
  memcpy(gridP->reducedPoints, reducedPoints, (size_t) reducedPointsSize * sizeof(int));
  gridMark4Update(gridID);
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
void
gridInqReducedPoints(int gridID, int *reducedPoints)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (gridP->reducedPoints == 0) Error("undefined pointer!");

  memcpy(reducedPoints, gridP->reducedPoints, (size_t) gridP->reducedPointsSize * sizeof(int));
}

static SizeType
gridInqMaskSerialGeneric(grid_t *gridP, mask_t **internalMask, int *restrict mask)
{
  size_t size = gridP->size;

  if (CDI_Debug && size == 0) Warning("Size undefined for gridID = %d", gridP->self);

  const mask_t *restrict mask_src = *internalMask;
  if (mask_src)
  {
    if (mask && size > 0)
      for (size_t i = 0; i < size; ++i) { mask[i] = (int) mask_src[i]; }
  }
  else
    size = 0;

  return (SizeType) size;
}

static SizeType
gridInqMaskSerial(grid_t *gridP, int *mask)
{
  return gridInqMaskSerialGeneric(gridP, &gridP->mask, mask);
}

int
gridInqMask(int gridID, int *mask)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqMask(gridP, mask);
}

static void
gridDefMaskSerial(grid_t *gridP, const int *mask)
{
  size_t size = gridP->size;
  if (size == 0) Error("Size undefined for gridID = %d", gridP->self);

  if (mask == NULL)
  {
    if (gridP->mask)
    {
      Free(gridP->mask);
      gridP->mask = NULL;
    }
  }
  else
  {
    if (gridP->mask == NULL)
      gridP->mask = (mask_t *) Malloc(size * sizeof(mask_t));
    else if (CDI_Debug)
      Warning("grid mask already defined!");

    for (size_t i = 0; i < size; ++i) gridP->mask[i] = (mask_t) (mask[i] != 0);
  }
}

void
gridDefMask(int gridID, const int *mask)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defMask(gridP, mask);
  gridMark4Update(gridID);
}

static int
gridInqMaskGMESerial(grid_t *gridP, int *mask_gme)
{
  return (int) gridInqMaskSerialGeneric(gridP, &gridP->mask_gme, mask_gme);
}

int
gridInqMaskGME(int gridID, int *mask)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqMaskGME(gridP, mask);
}

static void
gridDefMaskGMESerial(grid_t *gridP, const int *mask)
{
  size_t size = gridP->size;
  if (size == 0) Error("Size undefined for gridID = %d", gridP->self);

  if (gridP->mask_gme == NULL)
    gridP->mask_gme = (mask_t *) Malloc(size * sizeof(mask_t));
  else if (CDI_Debug)
    Warning("mask already defined!");

  for (size_t i = 0; i < size; ++i) gridP->mask_gme[i] = (mask_t) (mask[i] != 0);
}

void
gridDefMaskGME(int gridID, const int *mask)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defMaskGME(gridP, mask);
  gridMark4Update(gridID);
}

static void
copy_darray(size_t n, const double *restrict in, double *restrict out)
{
#ifdef _OPENMP
#pragma omp parallel for if (n > 99999) default(shared) schedule(static)
#endif
  for (size_t i = 0; i < n; ++i) { out[i] = in[i]; }
}

static SizeType
gridInqXValsSerial(grid_t *gridP, double *xvals)
{
  size_t size = grid_axis_size(gridP, GRID_AXIS_X);
  if (CDI_Debug && size == 0) Warning("size undefined for gridID = %d", gridP->self);

  if (gridP->x.vals)
  {
    if (size && xvals) copy_darray(size, gridP->vtable->inqXValsPtr(gridP), xvals);
  }
  else
    size = 0;

  return (SizeType) size;
}

static SizeType
gridInqXValsPartSerial(grid_t *gridP, int start, SizeType length, double *xvals)
{
  size_t size = grid_axis_size(gridP, GRID_AXIS_X);
  if (CDI_Debug && size == 0) Warning("size undefined for gridID = %d", gridP->self);

  if (gridP->x.vals)
  {
    if (size && xvals && (size_t) length <= size)
    {
      const double *gridP_xvals = gridP->vtable->inqXValsPtr(gridP);
      memcpy(xvals, gridP_xvals + start, (size_t) length * sizeof(double));
    }
  }
  else
    size = 0;

  return (SizeType) size;
}

#ifndef USE_MPI
static SizeType
gridInqXCvalsSerial(grid_t *gridP, char **xcvals)
{
  if (gridP->type != GRID_CHARXY) Error("Function only valid for grid type 'GRID_CHARXY'.");

  size_t size = gridP->x.size;
  size_t maxclength = 0;

  const char **gridP_xcvals = gridP->vtable->inqXCvalsPtr(gridP);
  if (gridP_xcvals && size && xcvals)
  {
    maxclength = (size_t) gridP->x.clength;
    for (size_t i = 0; i < size; i++) memcpy(xcvals[i], gridP_xcvals[i], maxclength * sizeof(char));
  }

  return (SizeType) maxclength;
}

static int
gridInqXIscSerial(grid_t *gridP)
{
  /*
  if ( gridP->type != GRID_CHARXY )
    Error("Axis type is 'char' but grid is not type 'GRID_CHARXY'.");
  */
  return gridP->x.clength;
}
#endif

/*
@Function  gridInqXvals
@Title     Get all values of a X-axis

@Prototype SizeType gridInqXvals(int gridID, double *xvals)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  xvals    Pointer to the location into which the X-values are read.
                    The caller must allocate space for the returned values.

@Description
The function @func{gridInqXvals} returns all values of the X-axis.

@Result
Upon successful completion @func{gridInqXvals} returns the number of values and
the values are stored in @func{xvals}.
Otherwise, 0 is returned and @func{xvals} is empty.

@EndFunction
*/
SizeType
gridInqXvals(int gridID, double *xvals)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXVals(gridP, xvals);
}

SizeType
gridInqXvalsPart(int gridID, int start, SizeType length, double *xvals)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXValsPart(gridP, start, length, xvals);
}

SizeType
gridInqXCvals(int gridID, char **xcvals)
{
#ifndef USE_MPI
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXCvals(gridP, xcvals);
#else
  (void) gridID;
  (void) xcvals;
  return 0;
#endif
}

int
gridInqXIsc(int gridID)
{
#ifndef USE_MPI
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXIsc(gridP);
#else
  (void) gridID;
  return 0;
#endif
}

static void
gridDefXValsSerial(grid_t *gridP, const double *xvals)
{
  size_t size = grid_axis_size(gridP, GRID_AXIS_X);
  if (size == 0) Error("Size undefined for gridID = %d", gridP->self);

  if (gridP->x.vals && CDI_Debug) Warning("values already defined!");
  gridP->x.vals = (double *) Realloc(gridP->x.vals, size * sizeof(double));
  copy_darray(size, xvals, gridP->x.vals);
}

#ifndef USE_MPI
static SizeType
gridInqYCvalsSerial(grid_t *gridP, char **ycvals)
{
  if (gridP->type != GRID_CHARXY) Error("Function only valid for grid type 'GRID_CHARXY'.");

  size_t size = gridP->y.size;
  size_t maxclength = 0;

  const char **gridP_ycvals = gridP->vtable->inqYCvalsPtr(gridP);
  if (gridP_ycvals && size && ycvals)
  {
    maxclength = (size_t) gridP->y.clength;
    for (size_t i = 0; i < size; i++) memcpy(ycvals[i], gridP_ycvals[i], maxclength);
  }

  return (SizeType) maxclength;
}

static int
gridInqYIscSerial(grid_t *gridP)
{
  // if ( gridP->type != GRID_CHARXY ) Error("Axis type is 'char' but grid is not type 'GRID_CHARXY'.");
  return gridP->y.clength;
}
#endif

/*
@Function  gridDefXvals
@Title     Define the values of a X-axis

@Prototype void gridDefXvals(int gridID, const double *xvals)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  xvals    X-values of the grid.

@Description
The function @func{gridDefXvals} defines all values of the X-axis.

@EndFunction
*/
void
gridDefXvals(int gridID, const double *xvals)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defXVals(gridP, xvals);
  gridMark4Update(gridID);
}

static SizeType
gridInqYValsSerial(grid_t *gridP, double *yvals)
{
  size_t size = grid_axis_size(gridP, GRID_AXIS_Y);
  if (CDI_Debug && size == 0) Warning("size undefined for gridID = %d!", gridP->self);

  if (gridP->y.vals)
  {
    if (size && yvals) copy_darray(size, gridP->vtable->inqYValsPtr(gridP), yvals);
  }
  else
    size = 0;

  return (SizeType) size;
}

static SizeType
gridInqYValsPartSerial(grid_t *gridP, int start, SizeType length, double *yvals)
{
  size_t size = grid_axis_size(gridP, GRID_AXIS_Y);
  if (CDI_Debug && size == 0) Warning("size undefined for gridID = %d!", gridP->self);

  if (gridP->y.vals)
  {
    if (size && yvals && (size_t) length <= size)
    {
      const double *gridP_yvals = gridP->vtable->inqYValsPtr(gridP);
      memcpy(yvals, gridP_yvals + start, (size_t) length * sizeof(double));
    }
  }
  else
    size = 0;

  return (SizeType) size;
}

/*
@Function  gridInqYvals
@Title     Get all values of a Y-axis

@Prototype SizeType gridInqYvals(int gridID, double *yvals)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  yvals    Pointer to the location into which the Y-values are read.
                    The caller must allocate space for the returned values.

@Description
The function @func{gridInqYvals} returns all values of the Y-axis.

@Result
Upon successful completion @func{gridInqYvals} returns the number of values and
the values are stored in @func{yvals}.
Otherwise, 0 is returned and @func{yvals} is empty.

@EndFunction
*/
SizeType
gridInqYvals(int gridID, double *yvals)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYVals(gridP, yvals);
}

SizeType
gridInqYvalsPart(int gridID, int start, SizeType size, double *yvals)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYValsPart(gridP, start, size, yvals);
}

SizeType
gridInqYCvals(int gridID, char **ycvals)
{
#ifndef USE_MPI
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYCvals(gridP, ycvals);
#else
  (void) gridID;
  (void) ycvals;
  return 0;
#endif
}

int
gridInqYIsc(int gridID)
{
#ifndef USE_MPI
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYIsc(gridP);
#else
  (void) gridID;
  return 0;
#endif
}

static void
gridDefYValsSerial(grid_t *gridP, const double *yvals)
{
  size_t size = grid_axis_size(gridP, GRID_AXIS_Y);
  if (size == 0) Error("Size undefined for gridID = %d!", gridP->self);

  if (gridP->y.vals && CDI_Debug) Warning("Values already defined!");

  gridP->y.vals = (double *) Realloc(gridP->y.vals, size * sizeof(double));
  copy_darray(size, yvals, gridP->y.vals);
}

/*
@Function  gridDefYvals
@Title     Define the values of a Y-axis

@Prototype void gridDefYvals(int gridID, const double *yvals)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  yvals    Y-values of the grid.

@Description
The function @func{gridDefYvals} defines all values of the Y-axis.

@EndFunction
*/
void
gridDefYvals(int gridID, const double *yvals)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defYVals(gridP, yvals);
  gridMark4Update(gridID);
}

static double
gridInqXValSerial(grid_t *gridP, SizeType index)
{
  return gridP->x.vals ? gridP->x.vals[index] : 0;
}

double
gridInqXval(int gridID, SizeType index)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXVal(gridP, index);
}

static double
gridInqYValSerial(grid_t *gridP, SizeType index)
{
  return gridP->y.vals ? gridP->y.vals[index] : 0;
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
double
gridInqYval(int gridID, SizeType index)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYVal(gridP, index);
}

static double
grid_calc_increment(size_t size, const double *vals)
{
  if (size > 1)
  {
    double inc = (vals[size - 1] - vals[0]) / (double) (size - 1);
    double abs_inc = fabs(inc);
    for (size_t i = 1; i < size; ++i)
      if (fabs(fabs(vals[i - 1] - vals[i]) - abs_inc) > 0.01 * abs_inc)
      {
        inc = 0.0;
        break;
      }

    return inc;
  }

  return 0.0;
}

static double
grid_calc_increment_in_meter(size_t size, const double *vals)
{
  if (size > 1)
  {
    double inc = (vals[size - 1] - vals[0]) / (double) (size - 1);
    return round(fabs(inc));
  }

  return 0.0;
}

static double
gridInqXIncBase(grid_t *gridP)
{
  if (fabs(gridP->x.inc) <= 0 && gridP->x.vals)
  {
    size_t xsize = gridP->x.size;
    if (xsize > 1)
    {
      const double *xvals = gridP->vtable->inqXValsPtr(gridP);
      gridP->x.inc = grid_calc_increment(xsize, xvals);
    }
  }

  return gridP->x.inc;
}

double
gridInqXincInMeter(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  const double *xvals = gridP->vtable->inqXValsPtr(gridP);

  if (fabs(gridP->x.inc) <= 0 && xvals)
  {
    size_t xsize = gridP->x.size;
    if (xsize > 1) gridP->x.inc = grid_calc_increment_in_meter(xsize, xvals);
  }

  return gridP->x.inc;
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
double
gridInqXinc(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXInc(gridP);
}

static double
gridInqYIncBase(grid_t *gridP)
{
  if (fabs(gridP->y.inc) <= 0 && gridP->y.vals)
  {
    size_t ysize = gridP->y.size;
    if (ysize > 1)
    {
      const double *yvals = gridP->vtable->inqYValsPtr(gridP);
      gridP->y.inc = grid_calc_increment(ysize, yvals);
    }
  }

  return gridP->y.inc;
}

double
gridInqYincInMeter(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  const double *yvals = gridP->vtable->inqYValsPtr(gridP);

  if (fabs(gridP->y.inc) <= 0 && yvals)
  {
    size_t ysize = gridP->y.size;
    if (ysize > 1) gridP->y.inc = grid_calc_increment_in_meter(ysize, yvals);
  }

  return gridP->y.inc;
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
double
gridInqYinc(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYInc(gridP);
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
void
gridInqParamRLL(int gridID, double *xpole, double *ypole, double *angle)
{
  *xpole = 0;
  *ypole = 0;
  *angle = 0;

  static const char projection[] = "rotated_latitude_longitude";
  char name[CDI_MAX_NAME + 1];
  int length = CDI_MAX_NAME;
  cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, name, &length);
  if (name[0] && str_is_equal(name, projection))
  {
    int atttype, attlen;

    int natts, nfound = 0;
    cdiInqNatts(gridID, CDI_GLOBAL, &natts);

    for (int iatt = 0; iatt < natts; ++iatt)
    {
      cdiInqAtt(gridID, CDI_GLOBAL, iatt, name, &atttype, &attlen);
      if (attlen == 1)
      {
        double *attflt;
        // clang-format off
        if      (str_is_equal(name, "grid_north_pole_longitude")) attflt = xpole;
        else if (str_is_equal(name, "grid_north_pole_latitude") ) attflt = ypole;
        else if (str_is_equal(name, "north_pole_grid_longitude")) attflt = angle;
        else continue;
        // clang-format on
        bool valid = cdiInqAttConvertedToFloat(gridID, atttype, name, attlen, attflt);
        if ((nfound += valid) == 3) return;
      }
    }
  }
  else
    Warning("%s mapping parameter missing!", projection);
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
void
gridDefParamRLL(int gridID, double xpole, double ypole, double angle)
{
  cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_UNITS, "degrees");
  cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_UNITS, "degrees");

  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARNAME, "rotated_pole");

  const char *gmapname = "rotated_latitude_longitude";
  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapname);
  cdiDefAttTxt(gridID, CDI_GLOBAL, "grid_mapping_name", (int) (strlen(gmapname)), gmapname);
  cdiDefAttFlt(gridID, CDI_GLOBAL, "grid_north_pole_longitude", CDI_DATATYPE_FLT64, 1, &xpole);
  cdiDefAttFlt(gridID, CDI_GLOBAL, "grid_north_pole_latitude", CDI_DATATYPE_FLT64, 1, &ypole);
  if (is_not_equal(angle, 0)) cdiDefAttFlt(gridID, CDI_GLOBAL, "north_pole_grid_longitude", CDI_DATATYPE_FLT64, 1, &angle);

  grid_t *gridP = grid_to_pointer(gridID);
  gridP->projtype = CDI_PROJ_RLL;

  gridVerifyProj(gridID);
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
void
gridInqParamGME(int gridID, int *nd, int *ni, int *ni2, int *ni3)
{
  grid_t *gridP = grid_to_pointer(gridID);

  *nd = gridP->gme.nd;
  *ni = gridP->gme.ni;
  *ni2 = gridP->gme.ni2;
  *ni3 = gridP->gme.ni3;
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
void
gridDefParamGME(int gridID, int nd, int ni, int ni2, int ni3)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (gridP->gme.nd != nd)
  {
    gridP->gme.nd = nd;
    gridP->gme.ni = ni;
    gridP->gme.ni2 = ni2;
    gridP->gme.ni3 = ni3;
    gridMark4Update(gridID);
  }
}

/*
@Function
@Title

@Prototype
@Parameter
    @Item  Grid identifier

@EndFunction
*/
void
gridChangeType(int gridID, int gridtype)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (CDI_Debug) Message("Changed grid type from %s to %s", gridNamePtr(gridP->type), gridNamePtr(gridtype));

  if (gridP->type != gridtype)
  {
    gridP->type = gridtype;
    gridMark4Update(gridID);
  }
}

static void
check_cyclic_reg2d(grid_t *gridP)
{
  size_t xsize = gridP->x.size;
  const double *xvals = gridP->vtable->inqXValsPtr(gridP);
  if (xvals && xsize > 1)
  {
    double xval1 = xvals[0];
    double xval2 = xvals[1];
    double xvaln = xvals[xsize - 1];
    if (xval2 < xval1) { xval2 += 360; }
    if (xvaln < xval1) { xvaln += 360; }

    if (is_not_equal(xval1, xvaln))
    {
      double xinc = xval2 - xval1;
      if (is_equal(xinc, 0)) { xinc = (xvaln - xval1) / (double) (xsize - 1); }

      double x0 = xvaln + xinc - 360;
      if (fabs(x0 - xval1) < 0.01 * xinc) { gridP->isCyclic = 1; }
    }
  }
}

static void
check_cyclic_curvilinear(grid_t *gridP)
{
  enum
  {
    numVertices = 4
  };
  size_t xsize = gridP->x.size;
  size_t ysize = gridP->y.size;
  const double *xvals = gridP->vtable->inqXValsPtr(gridP);
  const double *yvals = gridP->vtable->inqYValsPtr(gridP);
  const double (*xbounds)[numVertices] = (const double (*)[numVertices]) gridP->vtable->inqXBoundsPtr(gridP);

  bool lcheck = true;
  if (yvals && xvals)
  {
    if ((fabs(yvals[0] - yvals[xsize - 1]) > fabs(yvals[0] - yvals[xsize * ysize - xsize]))
        && (fabs(yvals[xsize * ysize - xsize] - yvals[xsize * ysize - 1]) > fabs(yvals[xsize - 1] - yvals[xsize * ysize - 1])))
      lcheck = false;
  }
  else
    lcheck = false;

  if (lcheck && xvals && xsize > 1)
  {
    size_t nc = 0;
    for (size_t j = 0; j < ysize; ++j)
    {
      size_t i1 = j * xsize, i2 = j * xsize + 1, in = j * xsize + (xsize - 1);
      double val1 = xvals[i1], val2 = xvals[i2], valn = xvals[in];
      double xinc = fabs(val2 - val1);

      if (val1 < 1 && valn > 300) val1 += 360;
      if (valn < 1 && val1 > 300) valn += 360;
      if (val1 < -179 && valn > 120) val1 += 360;
      if (valn < -179 && val1 > 120) valn += 360;
      if (fabs(valn - val1) > 180) val1 += 360;

      double x0 = valn + copysign(xinc, val1 - valn);

      nc += fabs(x0 - val1) < 0.5 * xinc;
    }
    gridP->isCyclic = nc > ysize / 2;
  }

  if (lcheck && xbounds && xsize > 1)
  {
    bool isCyclic = true;
    for (size_t j = 0; j < ysize; ++j)
    {
      size_t i1 = j * xsize;
      size_t i2 = j * xsize + (xsize - 1);
      for (size_t k1 = 0; k1 < numVertices; ++k1)
      {
        double val1 = xbounds[i1][k1];
        for (size_t k2 = 0; k2 < numVertices; ++k2)
        {
          double val2 = xbounds[i2][k2];

          if (val1 < 1 && val2 > 300) val1 += 360;
          if (val2 < 1 && val1 > 300) val2 += 360;
          if (val1 < -179 && val2 > 120) val1 += 360;
          if (val2 < -179 && val1 > 120) val2 += 360;
          if (fabs(val2 - val1) > 180) val1 += 360;

          if (fabs(val1 - val2) < 0.001) goto foundCloseVertices;
        }
      }
      // all vertices more than 0.001 degrees apart
      isCyclic = false;
      break;
    foundCloseVertices:;
    }
    gridP->isCyclic = isCyclic;
  }
}

static void
grid_check_cyclic(grid_t *gridP)
{
  gridP->isCyclic = 0;

  if (gridP->type == GRID_GAUSSIAN || gridP->type == GRID_LONLAT) { check_cyclic_reg2d(gridP); }
  else if (gridP->type == GRID_CURVILINEAR) { check_cyclic_curvilinear(gridP); }
}

int
gridIsCyclic(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  if (gridP->isCyclic == CDI_UNDEFID) grid_check_cyclic(gridP);
  return gridP->isCyclic;
}

static bool
compareXYvals(grid_t *gridRef, grid_t *gridTest)
{
  bool differ = false;
  size_t xsizeTest = grid_axis_size(gridTest, GRID_AXIS_X);
  size_t xsizeRef = (size_t) gridRef->vtable->inqXVals(gridRef, NULL);
  if (xsizeTest != xsizeRef) return true;

  if (xsizeTest > 0)
  {
    const double *xvalsRef = gridRef->vtable->inqXValsPtr(gridRef);
    const double *xvalsTest = gridTest->vtable->inqXValsPtr(gridTest);
    if (!xvalsTest) return true;

    for (size_t i = 0; i < xsizeTest; ++i)
      if (fabs(xvalsTest[i] - xvalsRef[i]) > 1.e-10) return true;
  }

  size_t ysizeTest = grid_axis_size(gridTest, GRID_AXIS_Y);
  size_t ysizeRef = (size_t) gridRef->vtable->inqYVals(gridRef, NULL);
  if (ysizeTest != ysizeRef) return true;

  if (ysizeTest > 0)
  {
    const double *yvalsRef = gridRef->vtable->inqYValsPtr(gridRef);
    const double *yvalsTest = gridTest->vtable->inqYValsPtr(gridTest);
    if (!yvalsTest) return true;

    for (size_t i = 0; i < ysizeTest; ++i)
      if (fabs(yvalsTest[i] - yvalsRef[i]) > 1.e-10) return true;
  }

  return differ;
}

static bool
compareXYvals2(grid_t *gridRef, grid_t *gridTest)
{
  size_t gridsize = gridTest->size;
  bool differ = ((gridTest->x.vals == NULL) ^ (gridRef->x.vals == NULL)) || ((gridTest->y.vals == NULL) ^ (gridRef->y.vals == NULL))
                || ((gridTest->x.bounds == NULL) ^ (gridRef->x.bounds == NULL))
                || ((gridTest->y.bounds == NULL) ^ (gridRef->y.bounds == NULL));

  typedef double (*inqVal)(grid_t *grid, SizeType index);
  inqVal inqXValRef = gridRef->vtable->inqXVal, inqYValRef = gridRef->vtable->inqYVal, inqXValTest = gridTest->vtable->inqXVal,
         inqYValTest = gridTest->vtable->inqYVal;

  if (!differ && gridTest->x.vals)
    differ = fabs(inqXValTest(gridTest, 0) - inqXValRef(gridRef, 0)) > 1.e-9
             || fabs(inqXValTest(gridTest, (SizeType) (gridsize - 1)) - inqXValRef(gridRef, (SizeType) (gridsize - 1))) > 1.e-9;

  if (!differ && gridTest->y.vals)
    differ = fabs(inqYValTest(gridTest, 0) - inqYValRef(gridRef, 0)) > 1.e-9
             || fabs(inqYValTest(gridTest, (SizeType) gridsize - 1) - inqYValRef(gridRef, (SizeType) gridsize - 1)) > 1.e-9;

  return differ;
}

static bool
compare_bounds(const grid_t *grid, const grid_t *gridRef)
{
  bool differ = false;

  if ((grid->x.bounds && !gridRef->x.bounds) || (!grid->x.bounds && gridRef->x.bounds) || (grid->y.bounds && !gridRef->y.bounds)
      || (!grid->y.bounds && gridRef->y.bounds))
    differ = true;

  return differ;
}

static bool
compare_lonlat(int gridID, const grid_t *grid, const grid_t *gridRef)
{
  bool differ = false;
  /*
    printf("gridID      %d\n", gridID);
    printf("grid.xdef   %d\n", grid->x.flag);
    printf("grid.ydef   %d\n", grid->y.flag);
    printf("grid.xsize  %zu\n", grid->x.size);
    printf("grid.ysize  %zu\n", grid->y.size);
    printf("grid.xfirst %f\n", grid->x.first);
    printf("grid.yfirst %f\n", grid->y.first);
    printf("grid.xfirst %f\n", gridInqXval(gridID, 0));
    printf("grid.yfirst %f\n", gridInqYval(gridID, 0));
    printf("grid.xinc   %f\n", grid->x.inc);
    printf("grid.yinc   %f\n", grid->y.inc);
    printf("grid.xinc   %f\n", gridInqXinc(gridID));
    printf("grid.yinc   %f\n", gridInqYinc(gridID));
  */
  if (grid->x.size == gridRef->x.size && grid->y.size == gridRef->y.size)
  {
    if (grid->x.flag == 2 && grid->y.flag == 2)
    {
      if (!(is_equal(grid->x.first, 0) && is_equal(grid->x.last, 0) && is_equal(grid->x.inc, 0))
          && !(is_equal(grid->y.first, 0) && is_equal(grid->y.last, 0) && is_equal(grid->y.inc, 0))
          && is_not_equal(grid->x.first, grid->x.last) && is_not_equal(grid->y.first, grid->y.last))
      {
        if (is_not_equal(grid->x.first, gridInqXval(gridID, 0)) || is_not_equal(grid->y.first, gridInqYval(gridID, 0)))
        {
          differ = true;
        }
        if (!differ && fabs(grid->x.inc) > 0 && fabs(fabs(grid->x.inc) - fabs(gridRef->x.inc)) > fabs(grid->x.inc / 1000))
        {
          differ = true;
        }
        if (!differ && fabs(grid->y.inc) > 0 && fabs(fabs(grid->y.inc) - fabs(gridRef->y.inc)) > fabs(grid->y.inc / 1000))
        {
          differ = true;
        }
      }
    }
    else if (grid->x.vals && grid->y.vals)
      differ = gridRef->vtable->compareXYFull((grid_t *) gridRef, (grid_t *) grid);

    if (!differ) differ = compare_bounds(grid, gridRef);
  }
  else
    differ = true;

  return differ;
}

static bool
compare_projection(int gridID, const grid_t *grid, const grid_t *gridRef)
{
  bool differ = compare_lonlat(gridID, grid, gridRef);

  if (!differ)
  {
    // printf(">%s< >%s<\n", cdiInqVarKeyString(&grid->keys, CDI_KEY_GRIDMAP_VARNAME), cdiInqVarKeyString(&gridRef->keys,
    // CDI_KEY_GRIDMAP_VARNAME)); printf(">%s< >%s<\n", cdiInqVarKeyString(&grid->keys, CDI_KEY_GRIDMAP_NAME),
    // cdiInqVarKeyString(&gridRef->keys, CDI_KEY_GRIDMAP_NAME));
    // if (!str_is_equal(cdiInqVarKeyString(&grid->keys, CDI_KEY_GRIDMAP_VARNAME), cdiInqVarKeyString(&gridRef->keys,
    // CDI_KEY_GRIDMAP_VARNAME))) return true; if (!str_is_equal(cdiInqVarKeyString(&grid->keys, CDI_KEY_GRIDMAP_NAME),
    // cdiInqVarKeyString(&gridRef->keys, CDI_KEY_GRIDMAP_NAME))) return true;
  }

  return differ;
}

static bool
compare_generic(const grid_t *grid, const grid_t *gridRef)
{
  bool differ = false;

  if (grid->x.size == gridRef->x.size && grid->y.size == gridRef->y.size)
  {
    if (grid->x.flag == 1 && grid->y.flag == 1 && grid->x.vals && grid->y.vals)
      differ = gridRef->vtable->compareXYFull((grid_t *) gridRef, (grid_t *) grid);
  }
  else if ((grid->y.size == 0 || grid->y.size == 1) && grid->x.size == gridRef->x.size * gridRef->y.size) {}
  else
    differ = true;

  return differ;
}

static bool
compare_gaussian(int gridID, const grid_t *grid, const grid_t *gridRef)
{
  const double cmp_eps = 0.0015;
  bool differ = false;

  if (grid->x.size == gridRef->x.size && grid->y.size == gridRef->y.size)
  {
    if (grid->x.flag == 2 && grid->y.flag == 2)
    {
      if (!(is_equal(grid->x.first, 0) && is_equal(grid->x.last, 0) && is_equal(grid->x.inc, 0))
          && !(is_equal(grid->y.first, 0) && is_equal(grid->y.last, 0)))
        if (fabs(grid->x.first - gridInqXval(gridID, 0)) > cmp_eps || fabs(grid->y.first - gridInqYval(gridID, 0)) > cmp_eps
            || (fabs(grid->x.inc) > 0 && fabs(fabs(grid->x.inc) - fabs(gridRef->x.inc)) > fabs(grid->x.inc / 1000)))
        {
          differ = true;
        }
    }
    else if (grid->x.vals && grid->y.vals)
      differ = gridRef->vtable->compareXYFull((grid_t *) gridRef, (grid_t *) grid);

    if (!differ) differ = compare_bounds(grid, gridRef);
  }
  else
    differ = true;

  return differ;
}

static bool
compare_curvilinear(const grid_t *grid, const grid_t *gridRef)
{
  bool differ = false;

  /*
    printf("gridID      %d\n", gridID);
    printf("grid.xsize  %d\n", grid->x.size);
    printf("grid.ysize  %d\n", grid->y.size);
    printf("grid.xfirst %f\n", grid->x.vals[0]);
    printf("grid.yfirst %f\n", grid->y.vals[0]);
    printf("grid xfirst %f\n", gridInqXval(gridID, 0));
    printf("grid yfirst %f\n", gridInqYval(gridID, 0));
    printf("grid.xlast  %f\n", grid->x.vals[grid->size-1]);
    printf("grid.ylast  %f\n", grid->y.vals[grid->size-1]);
    printf("grid xlast  %f\n", gridInqXval(gridID, grid->size-1));
    printf("grid ylast  %f\n", gridInqYval(gridID, grid->size-1));
    printf("grid.nv     %d\n", grid->nvertex);
    printf("grid nv     %d\n", gridInqNvertex(gridID));
  */
  if (grid->x.size == gridRef->x.size && grid->y.size == gridRef->y.size)
    differ = gridRef->vtable->compareXYAO((grid_t *) gridRef, (grid_t *) grid);

  return differ;
}

static bool
compare_unstructured(const grid_t *grid, const grid_t *gridRef, bool compareCoord)
{
  bool differ = false;

  unsigned char uuid1[CDI_UUID_SIZE] = { 0 };
  unsigned char uuid2[CDI_UUID_SIZE] = { 0 };
  int length = CDI_UUID_SIZE;
  cdiInqVarKeyBytes(&gridRef->keys, CDI_KEY_UUID, uuid1, &length);
  length = CDI_UUID_SIZE;
  cdiInqVarKeyBytes(&grid->keys, CDI_KEY_UUID, uuid2, &length);
  differ = ((!cdiUUIDIsNull(uuid1) || !cdiUUIDIsNull(uuid2)) && memcmp(uuid1, uuid2, CDI_UUID_SIZE) != 0);
  if (!differ)
  {
    int numberA = cdiInqVarKeyInt(&grid->keys, CDI_KEY_NUMBEROFGRIDUSED);
    int numberB = cdiInqVarKeyInt(&gridRef->keys, CDI_KEY_NUMBEROFGRIDUSED);
    int positionA = cdiInqVarKeyInt(&grid->keys, CDI_KEY_NUMBEROFGRIDINREFERENCE);
    int positionB = cdiInqVarKeyInt(&gridRef->keys, CDI_KEY_NUMBEROFGRIDINREFERENCE);
    if (compareCoord)
    {
      differ = (grid->nvertex != gridRef->nvertex || (numberA > 0 && positionA != positionB)
                || gridRef->vtable->compareXYFull((grid_t *) gridRef, (grid_t *) grid));
    }
    else
    {
      if (((grid->x.vals == NULL) ^ (gridRef->x.vals == NULL)) && ((grid->y.vals == NULL) ^ (gridRef->y.vals == NULL)))
      {
        int nvertexA = grid->nvertex, nvertexB = gridRef->nvertex;
        differ = (nvertexA && nvertexB && (nvertexA != nvertexB))
                 || ((numberA && numberB && (numberA != numberB)) || (numberA && numberB && positionA != positionB));
      }
      else
      {
        differ = (grid->nvertex != gridRef->nvertex || numberA != numberB || (numberA > 0 && positionA != positionB)
                  || gridRef->vtable->compareXYAO((grid_t *) gridRef, (grid_t *) grid));
      }
    }
  }

  return differ;
}

static bool
gridCompare(int gridID, const grid_t *grid, bool compareCoord)
{
  bool differ = true;
  const grid_t *gridRef = grid_to_pointer(gridID);

  if (grid->type == gridRef->type || grid->type == GRID_GENERIC)
  {
    if (grid->size == gridRef->size)
    {
      differ = false;
      if (grid->type == GRID_LONLAT) { differ = compare_lonlat(gridID, grid, gridRef); }
      else if (grid->type == GRID_PROJECTION) { differ = compare_projection(gridID, grid, gridRef); }
      else if (grid->type == GRID_GENERIC) { differ = compare_generic(grid, gridRef); }
      else if (grid->type == GRID_GAUSSIAN) { differ = compare_gaussian(gridID, grid, gridRef); }
      else if (grid->type == GRID_CURVILINEAR) { differ = compare_curvilinear(grid, gridRef); }
      else if (grid->type == GRID_UNSTRUCTURED) { differ = compare_unstructured(grid, gridRef, compareCoord); }
    }
  }

  int scanningModeA = cdiInqVarKeyInt(&grid->keys, CDI_KEY_SCANNINGMODE);
  int scanningModeB = cdiInqVarKeyInt(&gridRef->keys, CDI_KEY_SCANNINGMODE);
  if (scanningModeA != scanningModeB)
  {
    // often grid definition may differ in UV-relativeToGrid
    differ = true;
#ifdef HIRLAM_EXTENSIONS
    if (cdiDebugExt >= 200)
      printf("gridCompare(gridID=%d): Differs: scanningModeA [%d] != scanningModeB(gridID) [%d]\n", gridID, scanningModeA,
             scanningModeB);
#endif  // HIRLAM_EXTENSIONS
  }

  return differ;
}

int
cmp_key_int(const cdi_keys_t *keysp1, const cdi_keys_t *keysp2, int key)
{
  int v1 = cdiInqVarKeyInt(keysp1, key);
  int v2 = cdiInqVarKeyInt(keysp2, key);
  return (v1 != v2);
}

int
grid_compare(void *gridP1, void *gridP2)
{
  grid_t *g1 = (grid_t *) gridP1;
  grid_t *g2 = (grid_t *) gridP2;
  enum
  {
    equal = 0,
    differ = -1
  };

  xassert(g1);
  xassert(g2);

  if (cdiInqVarKeyInt(&g1->keys, CDI_KEY_DATATYPE) != cdiInqVarKeyInt(&g2->keys, CDI_KEY_DATATYPE)) return differ;
  if (g1->type != g2->type) return differ;
  if (g1->isCyclic != g2->isCyclic) return differ;
  if (g1->x.flag != g2->x.flag) return differ;
  if (g1->y.flag != g2->y.flag) return differ;
  if (g1->gme.nd != g2->gme.nd) return differ;
  if (g1->gme.ni != g2->gme.ni) return differ;
  if (g1->gme.ni2 != g2->gme.ni2) return differ;
  if (g1->gme.ni3 != g2->gme.ni3) return differ;
  if (cmp_key_int(&g1->keys, &g2->keys, CDI_KEY_NUMBEROFGRIDUSED)) return differ;
  if (cmp_key_int(&g1->keys, &g2->keys, CDI_KEY_NUMBEROFGRIDINREFERENCE)) return differ;
  if (g1->trunc != g2->trunc) return differ;
  if (g1->nvertex != g2->nvertex) return differ;
  if (g1->reducedPointsSize != g2->reducedPointsSize) return differ;
  if (g1->size != g2->size) return differ;
  if (g1->x.size != g2->x.size) return differ;
  if (g1->y.size != g2->y.size) return differ;
  if (g1->lcomplex != g2->lcomplex) return differ;

  if (is_not_equal(g1->x.first, g2->x.first)) return differ;
  if (is_not_equal(g1->y.first, g2->y.first)) return differ;
  if (is_not_equal(g1->x.last, g2->x.last)) return differ;
  if (is_not_equal(g1->y.last, g2->y.last)) return differ;
  if (is_not_equal(g1->x.inc, g2->x.inc)) return differ;
  if (is_not_equal(g1->y.inc, g2->y.inc)) return differ;
  if (cmp_key_int(&g1->keys, &g2->keys, CDI_KEY_SCANNINGMODE)) return differ;

  {
    const double *restrict g1_xvals = g1->vtable->inqXValsPtr(g1), *restrict g2_xvals = g2->vtable->inqXValsPtr(g2);
    if ((g1_xvals != NULL) ^ (g2_xvals != NULL)) return differ;
    if (g1_xvals)
    {
      size_t size = grid_axis_size(g1, GRID_AXIS_X);
      xassert(size);
      for (size_t i = 0; i < size; i++)
        if (is_not_equal(g1_xvals[i], g2_xvals[i])) return differ;
    }
  }

  {
    const double *restrict g1_yvals = g1->vtable->inqYValsPtr(g1), *restrict g2_yvals = g2->vtable->inqYValsPtr(g2);
    if ((g1_yvals != NULL) ^ (g2_yvals != NULL)) return differ;
    if (g1_yvals)
    {
      size_t size = grid_axis_size(g1, GRID_AXIS_Y);
      xassert(size);
      for (size_t i = 0; i < size; i++)
        if (is_not_equal(g1_yvals[i], g2_yvals[i])) return differ;
    }
  }

  {
    const double *restrict g1_area = g1->vtable->inqAreaPtr(g1), *restrict g2_area = g2->vtable->inqAreaPtr(g2);
    if ((g1_area != NULL) ^ (g2_area != NULL)) return differ;
    if (g1_area)
    {
      size_t size = g1->size;
      xassert(size);

      for (size_t i = 0; i < size; i++)
        if (is_not_equal(g1_area[i], g2_area[i])) return differ;
    }
  }

  {
    const double *restrict g1_xbounds = g1->vtable->inqXBoundsPtr(g1), *restrict g2_xbounds = g2->vtable->inqXBoundsPtr(g2);
    if ((g1_xbounds != NULL) ^ (g2_xbounds != NULL)) return differ;
    if (g1_xbounds)
    {
      xassert(g1->nvertex);
      size_t size = g1->nvertex * grid_axis_size(g1, GRID_AXIS_X);
      xassert(size);

      for (size_t i = 0; i < size; i++)
        if (is_not_equal(g1_xbounds[i], g2_xbounds[i])) return differ;
    }
  }

  {
    const double *restrict g1_ybounds = g1->vtable->inqYBoundsPtr(g1), *restrict g2_ybounds = g2->vtable->inqYBoundsPtr(g2);
    if ((g1_ybounds != NULL) ^ (g2_ybounds != NULL)) return differ;
    if (g1_ybounds)
    {
      xassert(g1->nvertex);
      size_t size = g1->nvertex * grid_axis_size(g1, GRID_AXIS_Y);
      xassert(size);

      for (size_t i = 0; i < size; i++)
        if (is_not_equal(g1_ybounds[i], g2_ybounds[i])) return differ;
    }
  }

  if (!str_is_equal(cdiInqVarKeyString(&g1->x.keys, CDI_KEY_NAME), cdiInqVarKeyString(&g2->x.keys, CDI_KEY_NAME))) return differ;
  if (!str_is_equal(cdiInqVarKeyString(&g1->y.keys, CDI_KEY_NAME), cdiInqVarKeyString(&g2->y.keys, CDI_KEY_NAME))) return differ;
  if (!str_is_equal(cdiInqVarKeyString(&g1->x.keys, CDI_KEY_LONGNAME), cdiInqVarKeyString(&g2->x.keys, CDI_KEY_LONGNAME)))
    return differ;
  if (!str_is_equal(cdiInqVarKeyString(&g1->y.keys, CDI_KEY_LONGNAME), cdiInqVarKeyString(&g2->y.keys, CDI_KEY_LONGNAME)))
    return differ;
  if (!str_is_equal(cdiInqVarKeyString(&g1->x.keys, CDI_KEY_UNITS), cdiInqVarKeyString(&g2->x.keys, CDI_KEY_UNITS))) return differ;
  if (!str_is_equal(cdiInqVarKeyString(&g1->y.keys, CDI_KEY_UNITS), cdiInqVarKeyString(&g2->y.keys, CDI_KEY_UNITS))) return differ;
  if (!str_is_equal(cdiInqVarKeyString(&g1->x.keys, CDI_KEY_STDNAME), cdiInqVarKeyString(&g2->x.keys, CDI_KEY_STDNAME)))
    return differ;
  if (!str_is_equal(cdiInqVarKeyString(&g1->y.keys, CDI_KEY_STDNAME), cdiInqVarKeyString(&g2->y.keys, CDI_KEY_STDNAME)))
    return differ;

  if (!str_is_equal(cdiInqVarKeyString(&g1->y.keys, CDI_KEY_REFERENCEURI), cdiInqVarKeyString(&g2->y.keys, CDI_KEY_REFERENCEURI)))
    return differ;

  if (g1->mask)
  {
    xassert(g1->size);
    if (!g2->mask) return differ;
    if (memcmp(g1->mask, g2->mask, g1->size * sizeof(mask_t)) != 0) return differ;
  }
  else if (g2->mask)
    return differ;

  if (g1->mask_gme)
  {
    xassert(g1->size);
    if (!g2->mask_gme) return differ;
    if (memcmp(g1->mask_gme, g2->mask_gme, g1->size * sizeof(mask_t)) != 0) return differ;
  }
  else if (g2->mask_gme)
    return differ;

  unsigned char uuid1[CDI_UUID_SIZE] = { 0 };
  unsigned char uuid2[CDI_UUID_SIZE] = { 0 };
  int length = CDI_UUID_SIZE;
  cdiInqVarKeyBytes(&g1->keys, CDI_KEY_UUID, uuid1, &length);
  length = CDI_UUID_SIZE;
  cdiInqVarKeyBytes(&g2->keys, CDI_KEY_UUID, uuid2, &length);
  if (memcmp(uuid1, uuid2, CDI_UUID_SIZE) != 0) return differ;

  return equal;
}

static void
grid_complete(grid_t *grid)
{
  int gridID = grid->self;

  if (grid->datatype != CDI_UNDEFID) cdiDefKeyInt(gridID, CDI_GLOBAL, CDI_KEY_DATATYPE, grid->datatype);

  int gridtype = grid->type;
  switch (gridtype)
  {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
    case GRID_UNSTRUCTURED:
    case GRID_CURVILINEAR:
    case GRID_GENERIC:
    case GRID_PROJECTION:
    case GRID_HEALPIX:
    case GRID_CHARXY:
    {
      if (grid->x.size > 0) gridDefXsize(gridID, (SizeType) grid->x.size);
      if (grid->y.size > 0) gridDefYsize(gridID, (SizeType) grid->y.size);

      if (gridtype == GRID_GAUSSIAN) gridDefNP(gridID, grid->np);

      if (grid->nvertex > 0) gridDefNvertex(gridID, grid->nvertex);

      if (grid->x.flag == 2)
      {
        assert(gridtype != GRID_UNSTRUCTURED && gridtype != GRID_CURVILINEAR);
        double *xvals = (double *) Malloc(grid->x.size * sizeof(double));
        gridGenXvals(grid->x.size, grid->x.first, grid->x.last, grid->x.inc, xvals);
        grid->x.vals = xvals;
        // gridDefXinc(gridID, grid->x.inc);
      }

      if (grid->y.flag == 2)
      {
        assert(gridtype != GRID_UNSTRUCTURED && gridtype != GRID_CURVILINEAR);
        double *yvals = (double *) Malloc(grid->y.size * sizeof(double));
        gridGenYvals(gridtype, grid->y.size, grid->y.first, grid->y.last, grid->y.inc, yvals);
        grid->y.vals = yvals;
        // gridDefYinc(gridID, grid->y.inc);
      }

      if (grid->projtype == CDI_PROJ_RLL)
      {
        const char *name = cdiInqVarKeyString(&grid->x.keys, CDI_KEY_NAME);
        if (name[0] == 0 || name[0] == 'x') cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_NAME, "rlon");
        name = cdiInqVarKeyString(&grid->y.keys, CDI_KEY_NAME);
        if (name[0] == 0 || name[0] == 'y') cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_NAME, "rlat");
        name = cdiInqVarKeyString(&grid->x.keys, CDI_KEY_LONGNAME);
        if (name[0] == 0) cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_LONGNAME, "longitude in rotated pole grid");
        name = cdiInqVarKeyString(&grid->y.keys, CDI_KEY_LONGNAME);
        if (name[0] == 0) cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_LONGNAME, "latitude in rotated pole grid");
        name = cdiInqVarKeyString(&grid->x.keys, CDI_KEY_UNITS);
        if (name[0] == 0) cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_UNITS, "degrees");
        name = cdiInqVarKeyString(&grid->y.keys, CDI_KEY_UNITS);
        if (name[0] == 0) cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_UNITS, "degrees");
        cdiDefKeyString(gridID, CDI_XAXIS, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_grid_latlon][0]);
        cdiDefKeyString(gridID, CDI_YAXIS, CDI_KEY_STDNAME, xystdname_tab[grid_xystdname_grid_latlon][1]);
      }

      if (gridtype == GRID_UNSTRUCTURED)
      {
        int number = cdiInqVarKeyInt(&grid->keys, CDI_KEY_NUMBEROFGRIDUSED);
        if (number > 0)
        {
          cdiDefKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDUSED, number);
          int position = cdiInqVarKeyInt(&grid->keys, CDI_KEY_NUMBEROFGRIDINREFERENCE);
          if (position > 0) cdiDefKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDINREFERENCE, position);
        }
      }

      break;
    }
    case GRID_GAUSSIAN_REDUCED:
    {
      gridDefNP(gridID, grid->np);
      gridDefYsize(gridID, (SizeType) grid->y.size);
      if (grid->x.flag == 2)
      {
        double xvals[2] = { grid->x.first, grid->x.last };
        gridDefXsize(gridID, 2);
        gridDefXvals(gridID, xvals);
      }

      if (grid->y.flag == 2)
      {
        double *yvals = (double *) Malloc(grid->y.size * sizeof(double));
        gridGenYvals(gridtype, grid->y.size, grid->y.first, grid->y.last, grid->y.inc, yvals);
        grid->y.vals = yvals;
        // gridDefYinc(gridID, grid->y.inc);
      }
      break;
    }
    case GRID_SPECTRAL:
    {
      gridDefTrunc(gridID, grid->trunc);
      if (grid->lcomplex) gridDefComplexPacking(gridID, 1);
      break;
    }
    case GRID_FOURIER:
    {
      gridDefTrunc(gridID, grid->trunc);
      break;
    }
    case GRID_GME:
    {
      gridDefParamGME(gridID, grid->gme.nd, grid->gme.ni, grid->gme.ni2, grid->gme.ni3);
      break;
    }
      /*
    case GRID_GENERIC:
      {
        if ( grid->x.size > 0 && grid->y.size > 0 )
          {
            gridDefXsize(gridID, grid->x.size);
            gridDefYsize(gridID, grid->y.size);
            if ( grid->x.vals ) gridDefXvals(gridID, grid->x.vals);
            if ( grid->y.vals ) gridDefYvals(gridID, grid->y.vals);
          }
        break;
      }
      */
    case GRID_TRAJECTORY:
    {
      gridDefXsize(gridID, 1);
      gridDefYsize(gridID, 1);
      break;
    }
    default:
    {
      Error("Gridtype %s unsupported!", gridNamePtr(gridtype));
      break;
    }
  }
}

// Used only in iterator_grib.c
int
gridGenerate(const grid_t *grid)
{
  int gridType = grid->type;
  int gridID = gridCreate(gridType, (SizeType) grid->size);
  grid_t *restrict gridP = grid_to_pointer(gridID);
  cdiCopyVarKey(&grid->keys, CDI_KEY_DATATYPE, &gridP->keys);
  gridP->x.size = grid->x.size;
  gridP->y.size = grid->y.size;
  gridP->np = grid->np;
  gridP->nvertex = grid->nvertex;
  gridP->x.flag = grid->x.flag;
  int valdef_group1 = 0;
  static const int valdef_group1_tab[]
      = { GRID_LONLAT, GRID_GAUSSIAN, GRID_UNSTRUCTURED, GRID_CURVILINEAR, GRID_GENERIC, GRID_PROJECTION, GRID_HEALPIX };
  for (size_t i = 0; i < ARRAY_SIZE(valdef_group1_tab); ++i) { valdef_group1 |= (gridType == valdef_group1_tab[i]); }
  if (valdef_group1 && grid->x.flag == 1)
  {
    gridDefXvals(gridID, grid->x.vals);
    if (grid->x.bounds) gridDefXbounds(gridID, grid->x.bounds);
  }
  gridP->x.first = grid->x.first;
  gridP->x.last = grid->x.last;
  gridP->x.inc = grid->x.inc;
  gridP->y.flag = grid->y.flag;
  if ((valdef_group1 || gridType == GRID_GAUSSIAN_REDUCED) && grid->y.flag == 1)
  {
    gridDefYvals(gridID, grid->y.vals);
    if (grid->y.bounds) gridDefYbounds(gridID, grid->y.bounds);
  }
  gridP->y.first = grid->y.first;
  gridP->y.last = grid->y.last;
  gridP->y.inc = grid->y.inc;
  if (valdef_group1 && grid->area) gridDefArea(gridID, grid->area);

  cdiCopyVarKey(&grid->keys, CDI_KEY_NUMBEROFGRIDUSED, &gridP->keys);
  cdiCopyVarKey(&grid->keys, CDI_KEY_NUMBEROFGRIDINREFERENCE, &gridP->keys);
  cdiCopyVarKey(&grid->keys, CDI_KEY_REFERENCEURI, &gridP->keys);

  cdiCopyVarKey(&grid->keys, CDI_KEY_SCANNINGMODE, &gridP->keys);

  if (gridType == GRID_PROJECTION) gridP->name = strdup(grid->name);
  if (gridType == GRID_GAUSSIAN_REDUCED) gridDefReducedPoints(gridID, (int) grid->y.size, grid->reducedPoints);
  gridP->trunc = grid->trunc;
  gridP->lcomplex = grid->lcomplex;
  gridP->gme.nd = grid->gme.nd;
  gridP->gme.ni = grid->gme.ni;
  gridP->gme.ni2 = grid->gme.ni2;
  gridP->gme.ni3 = grid->gme.ni3;

  grid_complete(gridP);

  cdiCopyVarKey(&grid->keys, CDI_KEY_UUID, &gridP->keys);

  return gridID;
}

static void
grid_copy_base_array_fields(grid_t *gridPOrig, grid_t *gridPDup)
{
  size_t reducedPointsSize = gridPOrig->reducedPointsSize > 0 ? (size_t) gridPOrig->reducedPointsSize : (size_t) 0;
  if (reducedPointsSize)
  {
    gridPDup->reducedPoints = (int *) Malloc(reducedPointsSize * sizeof(int));
    memcpy(gridPDup->reducedPoints, gridPOrig->reducedPoints, reducedPointsSize * sizeof(int));
  }

  if (gridPOrig->x.vals != NULL)
  {
    size_t size = grid_axis_size(gridPOrig, GRID_AXIS_X);
    gridPDup->x.vals = (double *) Malloc(size * sizeof(double));
    memcpy(gridPDup->x.vals, gridPOrig->x.vals, size * sizeof(double));
  }

  if (gridPOrig->y.vals != NULL)
  {
    size_t size = grid_axis_size(gridPOrig, GRID_AXIS_Y);
    gridPDup->y.vals = (double *) Malloc(size * sizeof(double));
    memcpy(gridPDup->y.vals, gridPOrig->y.vals, size * sizeof(double));
  }

  if (gridPOrig->x.bounds != NULL)
  {
    size_t size = gridPOrig->nvertex * grid_axis_size(gridPOrig, GRID_AXIS_X);
    gridPDup->x.bounds = (double *) Malloc(size * sizeof(double));
    memcpy(gridPDup->x.bounds, gridPOrig->x.bounds, size * sizeof(double));
  }

  if (gridPOrig->y.bounds != NULL)
  {
    size_t size = gridPOrig->nvertex * grid_axis_size(gridPOrig, GRID_AXIS_Y);
    gridPDup->y.bounds = (double *) Malloc(size * sizeof(double));
    memcpy(gridPDup->y.bounds, gridPOrig->y.bounds, size * sizeof(double));
  }

  size_t size = gridPOrig->size;
  const double *gridPOrig_area = gridPOrig->vtable->inqAreaPtr(gridPOrig);
  if (gridPOrig_area != NULL)
  {
    gridPDup->area = (double *) Malloc(size * sizeof(double));
    memcpy(gridPDup->area, gridPOrig_area, size * sizeof(double));
  }

  if (gridPOrig->mask != NULL)
  {
    gridPDup->mask = (mask_t *) Malloc(size * sizeof(mask_t));
    memcpy(gridPDup->mask, gridPOrig->mask, size * sizeof(mask_t));
  }

  if (gridPOrig->mask_gme != NULL)
  {
    gridPDup->mask_gme = (mask_t *) Malloc(size * sizeof(mask_t));
    memcpy(gridPDup->mask_gme, gridPOrig->mask_gme, size * sizeof(mask_t));
  }
}

/*
@Function  gridDuplicate
@Title     Duplicate a horizontal Grid

@Prototype int gridDuplicate(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridDuplicate} duplicates a horizontal Grid.

@Result
@func{gridDuplicate} returns an identifier to the duplicated Grid.

@EndFunction
*/
int
gridDuplicate(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  grid_t *gridPnew = gridP->vtable->copy(gridP);
  int gridIDnew = reshPut(gridPnew, &gridOps);
  gridPnew->self = gridIDnew;
  return gridIDnew;
}

void
gridCompress(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);

  int gridtype = gridP->type;
  if (gridtype == GRID_UNSTRUCTURED)
  {
    if (gridP->mask_gme != NULL)
    {
      size_t gridsize = gridP->size;
      size_t nv = (size_t) gridP->nvertex;
      double *restrict area = (double *) gridP->vtable->inqAreaPtr(gridP),
                       *restrict xvals = (double *) gridP->vtable->inqXValsPtr(gridP),
                       *restrict yvals = (double *) gridP->vtable->inqYValsPtr(gridP),
                       *restrict xbounds = (double *) gridP->vtable->inqXBoundsPtr(gridP),
                       *restrict ybounds = (double *) gridP->vtable->inqYBoundsPtr(gridP);
      mask_t *restrict mask_gme = gridP->mask_gme;
      size_t *restrict selection = (size_t *) Malloc(gridsize * sizeof(selection[0]));
      size_t nselect;
      {
        size_t j = 0;
        for (size_t i = 0; i < gridsize; i++)
        {
          selection[j] = i;
          j += (mask_gme[i] != 0);
        }
        nselect = j;
      }
      selection = (size_t *) Realloc(selection, nselect * sizeof(selection[0]));
      if (xvals)
        for (size_t i = 0; i < nselect; i++) xvals[i] = xvals[selection[i]];
      if (yvals)
        for (size_t i = 0; i < nselect; i++) yvals[i] = yvals[selection[i]];
      if (area)
        for (size_t i = 0; i < nselect; i++) area[i] = area[selection[i]];
      if (xbounds)
        for (size_t i = 0; i < nselect; i++)
          for (size_t iv = 0; iv < nv; iv++) xbounds[i * nv + iv] = xbounds[selection[i] * nv + iv];
      if (ybounds)
        for (size_t i = 0; i < nselect; i++)
          for (size_t iv = 0; iv < nv; iv++) ybounds[i * nv + iv] = ybounds[selection[i] * nv + iv];
      Free(selection);

      /* fprintf(stderr, "grid compress %d %d %d\n", i, j, gridsize); */
      gridsize = nselect;
      gridP->size = gridsize;
      gridP->x.size = gridsize;
      gridP->y.size = gridsize;

      double **resizeP[] = { &gridP->x.vals, &gridP->y.vals, &gridP->area, &gridP->x.bounds, &gridP->y.bounds };
      size_t newSize[] = { gridsize, gridsize, gridsize, nv * gridsize, nv * gridsize };
      for (size_t i = 0; i < ARRAY_SIZE(resizeP); ++i)
        if (*(resizeP[i])) *(resizeP[i]) = (double *) Realloc(*(resizeP[i]), newSize[i] * sizeof(double));

      Free(gridP->mask_gme);
      gridP->mask_gme = NULL;
      gridMark4Update(gridID);
    }
  }
  else
    Warning("Unsupported grid type: %s", gridNamePtr(gridtype));
}

static void
gridDefAreaSerial(grid_t *gridP, const double *area)
{
  size_t size = gridP->size;
  if (size == 0) Error("size undefined for gridID = %d", gridP->self);

  if (gridP->area == NULL)
    gridP->area = (double *) Malloc(size * sizeof(double));
  else if (CDI_Debug)
    Warning("values already defined!");

  memcpy(gridP->area, area, size * sizeof(double));
}

void
gridDefArea(int gridID, const double *area)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defArea(gridP, area);
  gridMark4Update(gridID);
}

static void
gridInqAreaSerial(grid_t *gridP, double *area)
{
  if (gridP->area) memcpy(area, gridP->area, gridP->size * sizeof(double));
}

void
gridInqArea(int gridID, double *area)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->inqArea(gridP, area);
}

static int
gridInqPropPresenceBase(grid_t *gridP, enum gridPropInq inq)
{
  bool present = false;
  switch (inq)
  {
    case GRID_PROP_MASK: present = gridP->mask != NULL; break;
    case GRID_PROP_MASK_GME: present = gridP->mask != NULL; break;
    case GRID_PROP_AREA: present = gridP->area != NULL; break;
    case GRID_PROP_XVALS: present = gridP->x.vals != NULL; break;
    case GRID_PROP_YVALS: present = gridP->y.vals != NULL; break;
    case GRID_PROP_XBOUNDS: present = gridP->x.bounds != NULL; break;
    case GRID_PROP_YBOUNDS: present = gridP->y.bounds != NULL; break;
  }
  return present;
}

int
gridInqPropPresence(int gridID, enum gridPropInq inq)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqPropPresence(gridP, inq);
}

int
gridHasArea(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqPropPresence(gridP, GRID_PROP_AREA);
}

static const double *
gridInqAreaPtrBase(grid_t *gridP)
{
  return gridP->area;
}

const double *
gridInqAreaPtr(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqAreaPtr(gridP);
}

void
gridDefNvertex(int gridID, int nvertex)
{
  grid_t *gridP = grid_to_pointer(gridID);
  if (gridP->nvertex != nvertex)
  {
    gridP->nvertex = nvertex;
    gridMark4Update(gridID);
  }
}

int
gridInqNvertex(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->nvertex;
}

static void
gridDefBoundsGeneric(grid_t *gridP, const double *bounds, size_t regularSize, double **field)
{
  size_t nvertex = (size_t) gridP->nvertex;
  if (nvertex == 0)
  {
    Warning("nvertex undefined for gridID = %d. Cannot define bounds!", gridP->self);
    return;
  }

  bool isIrregular = grid_is_irregular(gridP->type);
  size_t size = nvertex * (isIrregular ? gridP->size : regularSize);
  if (size == 0) Error("size undefined for gridID = %d", gridP->self);

  if (*field == NULL && size) { *field = (double *) Malloc(size * sizeof(double)); }
  else if (CDI_Debug) { Warning("values already defined!"); }

  copy_darray(size, bounds, *field);
}

static void
gridDefXBoundsSerial(grid_t *gridP, const double *xbounds)
{
  gridDefBoundsGeneric(gridP, xbounds, gridP->x.size, &gridP->x.bounds);
}

/*
@Function  gridDefXbounds
@Title     Define the bounds of a X-axis

@Prototype void gridDefXbounds(int gridID, const double *xbounds)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  xbounds  X-bounds of the grid.

@Description
The function @func{gridDefXbounds} defines all bounds of the X-axis.

@EndFunction
*/
void
gridDefXbounds(int gridID, const double *xbounds)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defXBounds(gridP, xbounds);
  gridMark4Update(gridID);
}

static SizeType
gridInqXBoundsSerial(grid_t *gridP, double *xbounds)
{
  size_t size = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_X);

  if (gridP->x.bounds)
  {
    if (size && xbounds) copy_darray(size, gridP->vtable->inqXBoundsPtr(gridP), xbounds);
  }
  else
    size = 0;

  return (SizeType) size;
}

/*
@Function  gridInqXbounds
@Title     Get the bounds of a X-axis

@Prototype SizeType gridInqXbounds(int gridID, double *xbounds)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  xbounds  Pointer to the location into which the X-bounds are read.
                    The caller must allocate space for the returned values.

@Description
The function @func{gridInqXbounds} returns the bounds of the X-axis.

@Result
Upon successful completion @func{gridInqXbounds} returns the number of bounds and
the bounds are stored in @func{xbounds}.
Otherwise, 0 is returned and @func{xbounds} is empty.

@EndFunction
*/
SizeType
gridInqXbounds(int gridID, double *xbounds)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXBounds(gridP, xbounds);
}

static const double *
gridInqXBoundsPtrSerial(grid_t *gridP)
{
  return gridP->x.bounds;
}

const double *
gridInqXboundsPtr(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXBoundsPtr(gridP);
}

static void
gridDefYBoundsSerial(grid_t *gridP, const double *ybounds)
{
  gridDefBoundsGeneric(gridP, ybounds, gridP->y.size, &gridP->y.bounds);
}

//----------------------------------------------------------------------------
// Parallel Version
//----------------------------------------------------------------------------

SizeType
gridInqXboundsPart(int gridID, int start, SizeType size, double *xbounds)
{
  grid_t *gridP = grid_to_pointer(gridID);
  const double *gridP_xbounds = gridP->vtable->inqXBoundsPtr(gridP);
  if (gridP_xbounds && size && xbounds) memcpy(xbounds, gridP_xbounds + start, (size_t) size * sizeof(double));

  return size;
}

SizeType
gridInqYboundsPart(int gridID, int start, SizeType size, double *ybounds)
{
  grid_t *gridP = grid_to_pointer(gridID);
  const double *gridP_ybounds = gridP->vtable->inqYBoundsPtr(gridP);
  if (gridP_ybounds && size && ybounds) memcpy(ybounds, gridP_ybounds + start, (size_t) size * sizeof(double));

  return size;
}

/*
@Function  gridDefYbounds
@Title     Define the bounds of a Y-axis

@Prototype void gridDefYbounds(int gridID, const double *ybounds)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  ybounds  Y-bounds of the grid.

@Description
The function @func{gridDefYbounds} defines all bounds of the Y-axis.

@EndFunction
*/
void
gridDefYbounds(int gridID, const double *ybounds)
{
  grid_t *gridP = grid_to_pointer(gridID);
  gridP->vtable->defYBounds(gridP, ybounds);
  gridMark4Update(gridID);
}

static SizeType
gridInqYBoundsSerial(grid_t *gridP, double *ybounds)
{
  size_t size = gridP->nvertex * grid_axis_size(gridP, GRID_AXIS_Y);

  if (gridP->y.bounds)
  {
    if (size && ybounds) copy_darray(size, gridP->vtable->inqYBoundsPtr(gridP), ybounds);
  }
  else
    size = 0;

  return (SizeType) size;
}

/*
@Function  gridInqYbounds
@Title     Get the bounds of a Y-axis

@Prototype SizeType gridInqYbounds(int gridID, double *ybounds)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  ybounds  Pointer to the location into which the Y-bounds are read.
                    The caller must allocate space for the returned values.

@Description
The function @func{gridInqYbounds} returns the bounds of the Y-axis.

@Result
Upon successful completion @func{gridInqYbounds} returns the number of bounds and
the bounds are stored in @func{ybounds}.
Otherwise, 0 is returned and @func{ybounds} is empty.

@EndFunction
*/
SizeType
gridInqYbounds(int gridID, double *ybounds)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYBounds(gridP, ybounds);
}

static const double *
gridInqYBoundsPtrSerial(grid_t *gridP)
{
  return gridP->y.bounds;
}

const double *
gridInqYboundsPtr(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYBoundsPtr(gridP);
}

void
printDblsPrefixAutoBrk(FILE *fp, int dig, const char prefix[], size_t n, const double vals[])
{
  size_t nbyte0 = strlen(prefix);
  fputs(prefix, fp);
  size_t nbyte = nbyte0;
  for (size_t i = 0; i < n; i++)
  {
    if (nbyte > 80)
    {
      fprintf(fp, "\n%*s", (int) nbyte0, "");
      nbyte = nbyte0;
    }
    nbyte += (size_t) fprintf(fp, "%.*g ", dig, vals[i]);
  }
  fputs("\n", fp);
}

static inline void *
resizeBuffer(void **buf, size_t *bufSize, size_t reqSize)
{
  if (reqSize > *bufSize)
  {
    *buf = Realloc(*buf, reqSize);
    *bufSize = reqSize;
  }
  return *buf;
}

static void
gridPrintAttributes(FILE *fp, int gridID)
{
  int cdiID = gridID;
  int varID = CDI_GLOBAL;
  int atttype, attlen;
  char attname[CDI_MAX_NAME + 1];
  void *attBuf = NULL;
  size_t attBufSize = 0;

  int natts;
  cdiInqNatts(cdiID, varID, &natts);

  for (int iatt = 0; iatt < natts; ++iatt)
  {
    cdiInqAtt(cdiID, varID, iatt, attname, &atttype, &attlen);

    if (attlen == 0) continue;

    if (atttype == CDI_DATATYPE_TXT)
    {
      size_t attSize = (size_t) (attlen + 1) * sizeof(char);
      char *atttxt = (char *) resizeBuffer(&attBuf, &attBufSize, attSize);
      cdiInqAttTxt(cdiID, varID, attname, attlen, atttxt);
      atttxt[attlen] = 0;
      fprintf(fp, "ATTR_TXT: %s = \"%s\"\n", attname, atttxt);
    }
    else if (atttype == CDI_DATATYPE_INT8 || atttype == CDI_DATATYPE_UINT8 || atttype == CDI_DATATYPE_INT16
             || atttype == CDI_DATATYPE_UINT16 || atttype == CDI_DATATYPE_INT32 || atttype == CDI_DATATYPE_UINT32)
    {
      size_t attSize = (size_t) attlen * sizeof(int);
      int *attint = (int *) resizeBuffer(&attBuf, &attBufSize, attSize);
      cdiInqAttInt(cdiID, varID, attname, attlen, &attint[0]);
      if (attlen == 1)
        fprintf(fp, "ATTR_INT: %s =", attname);
      else
        fprintf(fp, "ATTR_INT_%d: %s =", attlen, attname);
      for (int i = 0; i < attlen; ++i) fprintf(fp, " %d", attint[i]);
      fprintf(fp, "\n");
    }
    else if (atttype == CDI_DATATYPE_FLT32 || atttype == CDI_DATATYPE_FLT64)
    {
      size_t attSize = (size_t) attlen * sizeof(double);
      double *attflt = (double *) resizeBuffer(&attBuf, &attBufSize, attSize);
      int dig = (atttype == CDI_DATATYPE_FLT64) ? 15 : 7;
      cdiInqAttFlt(cdiID, varID, attname, attlen, attflt);
      if (attlen == 1)
        fprintf(fp, "ATTR_FLT: %s =", attname);
      else
        fprintf(fp, "ATTR_FLT_%d: %s =", attlen, attname);
      for (int i = 0; i < attlen; ++i) fprintf(fp, " %.*g", dig, attflt[i]);
      fprintf(fp, "\n");
    }
  }

  Free(attBuf);
}

static void
gridPrintKernel(int gridID, int opt, FILE *fp)
{
  char attstr[CDI_MAX_NAME];
  char attstr2[CDI_MAX_NAME];
  size_t nxvals = (size_t) gridInqXvals(gridID, NULL);
  size_t nyvals = (size_t) gridInqYvals(gridID, NULL);

  int type = gridInqType(gridID);
  size_t gridsize = (size_t) gridInqSize(gridID);
  size_t xsize = (size_t) gridInqXsize(gridID);
  size_t ysize = (size_t) gridInqYsize(gridID);
  int nvertex = gridInqNvertex(gridID);
  int datatype;
  cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);

  int dig = (datatype == CDI_DATATYPE_FLT64) ? 15 : 7;

  fprintf(fp,
          "gridtype  = %s\n"
          "gridsize  = %zu\n",
          gridNamePtr(type), gridsize);

  if (type != GRID_GME)
  {
    if (type != GRID_UNSTRUCTURED && type != GRID_SPECTRAL && type != GRID_FOURIER)
    {
      if (xsize > 0) fprintf(fp, "xsize     = %zu\n", xsize);
      if (ysize > 0) fprintf(fp, "ysize     = %zu\n", ysize);
    }

    if (nxvals > 0)
    {
      int length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_NAME, attstr, &length);
      if (attstr[0]) fprintf(fp, "xname     = %s\n", attstr);
      length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_DIMNAME, attstr2, &length);
      if (attstr2[0] && !str_is_equal(attstr, attstr2)) fprintf(fp, "xdimname  = %s\n", attstr2);
      length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_LONGNAME, attstr, &length);
      if (attstr[0]) fprintf(fp, "xlongname = %s\n", attstr);
      length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_XAXIS, CDI_KEY_UNITS, attstr, &length);
      if (attstr[0]) fprintf(fp, "xunits    = %s\n", attstr);
    }

    if (nyvals > 0)
    {
      int length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_NAME, attstr, &length);
      if (attstr[0]) fprintf(fp, "yname     = %s\n", attstr);
      length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_DIMNAME, attstr2, &length);
      if (attstr2[0] && !str_is_equal(attstr, attstr2)) fprintf(fp, "ydimname  = %s\n", attstr2);
      length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_LONGNAME, attstr, &length);
      if (attstr[0]) fprintf(fp, "ylongname = %s\n", attstr);
      length = CDI_MAX_NAME;
      cdiInqKeyString(gridID, CDI_YAXIS, CDI_KEY_UNITS, attstr, &length);
      if (attstr[0]) fprintf(fp, "yunits    = %s\n", attstr);
    }

    if (type == GRID_UNSTRUCTURED && nvertex > 0) fprintf(fp, "nvertex   = %d\n", nvertex);
  }

  switch (type)
  {
    case GRID_LONLAT:
    case GRID_GAUSSIAN:
    case GRID_GAUSSIAN_REDUCED:
    case GRID_GENERIC:
    case GRID_PROJECTION:
    case GRID_CURVILINEAR:
    case GRID_UNSTRUCTURED:
    case GRID_CHARXY:
    {
      if (type == GRID_GAUSSIAN || type == GRID_GAUSSIAN_REDUCED) fprintf(fp, "np        = %d\n", gridInqNP(gridID));

      if (type == GRID_UNSTRUCTURED)
      {
        int number = 0;
        cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDUSED, &number);
        if (number > 0)
        {
          fprintf(fp, "number    = %d\n", number);
          int position = 0;
          cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDINREFERENCE, &position);
          if (position >= 0) fprintf(fp, "position  = %d\n", position);
        }

        int length;
        if (CDI_NOERR == cdiInqKeyLen(gridID, CDI_GLOBAL, CDI_KEY_REFERENCEURI, &length))
        {
          char reference_link[8192];
          length = sizeof(reference_link);
          cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_REFERENCEURI, reference_link, &length);
          fprintf(fp, "uri       = %s\n", reference_link);
        }
      }

      if (nxvals > 0)
      {
        double xfirst = 0.0, xinc = 0.0;
        if (type == GRID_LONLAT || type == GRID_GAUSSIAN || type == GRID_PROJECTION || type == GRID_GENERIC)
        {
          xfirst = gridInqXval(gridID, 0);
          xinc = gridInqXinc(gridID);
        }

        if (is_not_equal(xinc, 0) && opt)
        {
          fprintf(fp,
                  "xfirst    = %.*g\n"
                  "xinc      = %.*g\n",
                  dig, xfirst, dig, xinc);
        }
        else
        {
          double *xvals = (double *) Malloc(nxvals * sizeof(double));
          gridInqXvals(gridID, xvals);
          printDblsPrefixAutoBrk(fp, dig, "xvals     = ", nxvals, xvals);
          Free(xvals);
        }
      }

      if (nyvals > 0)
      {
        double yfirst = 0.0, yinc = 0.0;
        if (type == GRID_LONLAT || type == GRID_GENERIC || type == GRID_PROJECTION)
        {
          yfirst = gridInqYval(gridID, 0);
          yinc = gridInqYinc(gridID);
        }

        if (is_not_equal(yinc, 0) && opt)
        {
          fprintf(fp,
                  "yfirst    = %.*g\n"
                  "yinc      = %.*g\n",
                  dig, yfirst, dig, yinc);
        }
        else
        {
          double *yvals = (double *) Malloc(nyvals * sizeof(double));
          gridInqYvals(gridID, yvals);
          printDblsPrefixAutoBrk(fp, dig, "yvals     = ", nyvals, yvals);
          Free(yvals);
        }
      }

      if (type == GRID_PROJECTION) gridPrintAttributes(fp, gridID);

      break;
    }
    case GRID_SPECTRAL:
    {
      fprintf(fp,
              "truncation = %d\n"
              "complexpacking = %d\n",
              gridInqTrunc(gridID), gridInqComplexPacking(gridID));
      break;
    }
    case GRID_FOURIER:
    {
      fprintf(fp, "truncation = %d\n", gridInqTrunc(gridID));
      break;
    }
    case GRID_GME:
    {
      int nd, ni, ni2, ni3;
      gridInqParamGME(gridID, &nd, &ni, &ni2, &ni3);
      fprintf(fp, "ni        = %d\n", ni);
      break;
    }
    default:
    {
      fprintf(stderr, "Unsupported grid type: %s\n", gridNamePtr(type));
      break;
    }
  }
}

void
grid_print(void *voidptr, FILE *fp)
{
  grid_t *gridP = (grid_t *) voidptr;
  int gridID = gridP->self;

  xassert(gridP);

  gridPrintKernel(gridID, 0, fp);

  int datatype = CDI_UNDEFID;
  cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_DATATYPE, &datatype);

  fprintf(fp,
          "datatype  = %d\n"
          "nd        = %d\n"
          "ni        = %d\n"
          "ni2       = %d\n"
          "ni3       = %d\n"
          "trunc     = %d\n"
          "lcomplex  = %d\n"
          "reducedPointsSize   = %d\n",
          datatype, gridP->gme.nd, gridP->gme.ni, gridP->gme.ni2, gridP->gme.ni3, gridP->trunc, gridP->lcomplex,
          gridP->reducedPointsSize);
}

static const double *
gridInqXValsPtrSerial(grid_t *gridP)
{
  return gridP->x.vals;
}

#ifndef USE_MPI
static const char **
gridInqXCvalsPtrSerial(grid_t *gridP)
{
  return (const char **) gridP->x.cvals;
}
#endif

const double *
gridInqXvalsPtr(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXValsPtr(gridP);
}

#ifndef USE_MPI
const char **
gridInqXCvalsPtr(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqXCvalsPtr(gridP);
}
#endif

static const double *
gridInqYValsPtrSerial(grid_t *gridP)
{
  return gridP->y.vals;
}

#ifndef USE_MPI
static const char **
gridInqYCvalsPtrSerial(grid_t *gridP)
{
  return (const char **) gridP->y.cvals;
}
#endif

const double *
gridInqYvalsPtr(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYValsPtr(gridP);
}

#ifndef USE_MPI
const char **
gridInqYCvalsPtr(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);
  return gridP->vtable->inqYCvalsPtr(gridP);
}
#endif

void
gridProjParamsInit(struct CDI_GridProjParams *gpp)
{
  // clang-format off
  gpp->mv      = CDI_Grid_Missval;   // Missing value
  gpp->lon_0   = CDI_Grid_Missval;   // The East longitude of the meridian which is parallel to the Y-axis
  gpp->lat_0   = CDI_Grid_Missval;   // Latitude of the projection origin
  gpp->lat_1   = CDI_Grid_Missval;   // First latitude from the pole at which the secant cone cuts the sphere
  gpp->lat_2   = CDI_Grid_Missval;   // Second latitude at which the secant cone cuts the sphere
  gpp->a       = CDI_Grid_Missval;   // Semi-major axis or earth radius in metres (optional)
  gpp->b       = CDI_Grid_Missval;   // Semi-minor axis in metres (optional)
  gpp->rf      = CDI_Grid_Missval;   // Inverse flattening (1/f) (optional)
  gpp->xval_0  = CDI_Grid_Missval;   // Longitude of the first grid point in degree (optional)
  gpp->yval_0  = CDI_Grid_Missval;   // Latitude of the first grid point in degree (optional)
  gpp->x_0     = CDI_Grid_Missval;   // False easting (optional)
  gpp->y_0     = CDI_Grid_Missval;   // False northing (optional)
  gpp->x_SP    = CDI_Grid_Missval;   // Longitude of southern pole
  gpp->y_SP    = CDI_Grid_Missval;   // Latitude of southern pole
  gpp->nside   = 0;                  // HEALPix number of points along a side (number of data points should be = 12 * nside * nside)
  gpp->order   = -1;                 // HEALPix ordering convention (0:ring; 1:nested)
  // clang-format on
}

static void
gridDefParamsCommon(int gridID, struct CDI_GridProjParams gpp)
{
  if (is_not_equal(gpp.a, gpp.mv))
  {
    if (is_not_equal(gpp.b, gpp.mv))
    {
      cdiDefAttFlt(gridID, CDI_GLOBAL, "semi_major_axis", CDI_DATATYPE_FLT64, 1, &gpp.a);
      cdiDefAttFlt(gridID, CDI_GLOBAL, "semi_minor_axis", CDI_DATATYPE_FLT64, 1, &gpp.b);
    }
    else
    {
      cdiDefAttFlt(gridID, CDI_GLOBAL, "earth_radius", CDI_DATATYPE_FLT64, 1, &gpp.a);
    }
  }
  if (is_not_equal(gpp.rf, gpp.mv)) cdiDefAttFlt(gridID, CDI_GLOBAL, "inverse_flattening", CDI_DATATYPE_FLT64, 1, &gpp.rf);
  if (is_not_equal(gpp.x_0, gpp.mv)) cdiDefAttFlt(gridID, CDI_GLOBAL, "false_easting", CDI_DATATYPE_FLT64, 1, &gpp.x_0);
  if (is_not_equal(gpp.y_0, gpp.mv)) cdiDefAttFlt(gridID, CDI_GLOBAL, "false_northing", CDI_DATATYPE_FLT64, 1, &gpp.y_0);
  if (is_not_equal(gpp.xval_0, gpp.mv))
    cdiDefAttFlt(gridID, CDI_GLOBAL, "longitudeOfFirstGridPointInDegrees", CDI_DATATYPE_FLT64, 1, &gpp.xval_0);
  if (is_not_equal(gpp.yval_0, gpp.mv))
    cdiDefAttFlt(gridID, CDI_GLOBAL, "latitudeOfFirstGridPointInDegrees", CDI_DATATYPE_FLT64, 1, &gpp.yval_0);
  if (is_not_equal(gpp.x_SP, gpp.mv))
    cdiDefAttFlt(gridID, CDI_GLOBAL, "longitudeOfSouthernPoleInDegrees", CDI_DATATYPE_FLT64, 1, &gpp.x_SP);
  if (is_not_equal(gpp.y_SP, gpp.mv))
    cdiDefAttFlt(gridID, CDI_GLOBAL, "latitudeOfSouthernPoleInDegrees", CDI_DATATYPE_FLT64, 1, &gpp.y_SP);
}

/*
@Function  gridDefParamsLCC
@Title     Define the parameters of a Lambert Conformal Conic grid

@Prototype void gridDefParamsLCC(int gridID, struct CDI_GridProjParams gridProjParams)
@Parameter
    @Item  gridID          Grid ID, from a previous call to @fref{gridCreate}.
    @Item  gridProjParams  Grid projection parameters.

@Description
The function @func{gridDefParamsLCC} defines the parameters of a Lambert Conformal Conic grid.

@EndFunction
*/
void
gridDefParamsLCC(int gridID, struct CDI_GridProjParams gpp)
{
  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARNAME, "Lambert_Conformal");

  const char *gmapname = "lambert_conformal_conic";
  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapname);
  cdiDefAttTxt(gridID, CDI_GLOBAL, "grid_mapping_name", (int) (strlen(gmapname)), gmapname);
  int nlats = 0;
  double lats[2];
  lats[nlats++] = gpp.lat_1;
  if (is_not_equal(gpp.lat_1, gpp.lat_2)) lats[nlats++] = gpp.lat_2;
  cdiDefAttFlt(gridID, CDI_GLOBAL, "standard_parallel", CDI_DATATYPE_FLT64, nlats, lats);
  cdiDefAttFlt(gridID, CDI_GLOBAL, "longitude_of_central_meridian", CDI_DATATYPE_FLT64, 1, &gpp.lon_0);
  cdiDefAttFlt(gridID, CDI_GLOBAL, "latitude_of_projection_origin", CDI_DATATYPE_FLT64, 1, &gpp.lat_0);

  gridDefParamsCommon(gridID, gpp);

  grid_t *gridP = grid_to_pointer(gridID);
  gridP->projtype = CDI_PROJ_LCC;

  if (gridP->type != GRID_PROJECTION) gridP->type = GRID_PROJECTION;

  gridVerifyProj(gridID);
}

/*
@Function  gridInqParamsLCC
@Title     Get the parameter of a Lambert Conformal Conic grid

@Prototype void gridInqParamsLCC(int gridID, struct CDI_GridProjParams *gpp)
@Parameter
    @Item  gridID          Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  gridProjParams  Grid projection parameters.

@Description
The function @func{gridInqParamsLCC} returns the parameter of a Lambert Conformal Conic grid.

@EndFunction
*/
int
gridInqParamsLCC(int gridID, struct CDI_GridProjParams *gpp)
{
  int status = -1;
  if (gridInqType(gridID) != GRID_PROJECTION) return status;

  gridProjParamsInit(gpp);

  status = -2;
  const char *projection = "lambert_conformal_conic";
  char gmapname[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapname, &length);
  if (gmapname[0] && str_is_equal(gmapname, projection))
  {
    char attname[CDI_MAX_NAME + 1];

    int natts;
    cdiInqNatts(gridID, CDI_GLOBAL, &natts);

    if (natts) status = 0;

    for (int iatt = 0; iatt < natts; ++iatt)
    {
      int atttype, attlen;
      cdiInqAtt(gridID, CDI_GLOBAL, iatt, attname, &atttype, &attlen);
      if (attlen > 2) continue;

      double attflt[2];
      if (cdiInqAttConvertedToFloat(gridID, atttype, attname, attlen, attflt))
      {
        // clang-format off
        if      (str_is_equal(attname, "earth_radius"))                       gpp->a      = attflt[0];
        else if (str_is_equal(attname, "semi_major_axis"))                    gpp->a      = attflt[0];
        else if (str_is_equal(attname, "semi_minor_axis"))                    gpp->b      = attflt[0];
        else if (str_is_equal(attname, "inverse_flattening"))                 gpp->rf     = attflt[0];
        else if (str_is_equal(attname, "longitude_of_central_meridian"))      gpp->lon_0  = attflt[0];
        else if (str_is_equal(attname, "latitude_of_projection_origin"))      gpp->lat_0  = attflt[0];
        else if (str_is_equal(attname, "false_easting"))                      gpp->x_0    = attflt[0];
        else if (str_is_equal(attname, "false_northing"))                     gpp->y_0    = attflt[0];
        else if (str_is_equal(attname, "longitudeOfFirstGridPointInDegrees")) gpp->xval_0 = attflt[0];
        else if (str_is_equal(attname, "latitudeOfFirstGridPointInDegrees"))  gpp->yval_0 = attflt[0];
        else if (str_is_equal(attname, "longitudeOfSouthernPoleInDegrees"))   gpp->x_SP   = attflt[0];
        else if (str_is_equal(attname, "latitudeOfSouthernPoleInDegrees"))    gpp->y_SP   = attflt[0];
        else if (str_is_equal(attname, "standard_parallel"))
        {
          gpp->lat_1 = attflt[0];
          gpp->lat_2 = (attlen == 2) ? attflt[1] : attflt[0];
        }
        // clang-format on
      }
    }
  }

  return status;
}

int
gridVerifyProjParamsLCC(struct CDI_GridProjParams *gpp)
{
  static bool lwarn = true;

  if (lwarn)
  {
    // lwarn = false;
    const char *projection = "lambert_conformal_conic";
    if (is_equal(gpp->lon_0, gpp->mv)) Warning("%s mapping parameter %s missing!", projection, "longitude_of_central_meridian");
    if (is_equal(gpp->lat_0, gpp->mv)) Warning("%s mapping parameter %s missing!", projection, "latitude_of_central_meridian");
    if (is_equal(gpp->lat_1, gpp->mv)) Warning("%s mapping parameter %s missing!", projection, "standard_parallel");
    if (is_not_equal(gpp->x_0, gpp->mv) && is_not_equal(gpp->y_0, gpp->mv)
        && (is_equal(gpp->xval_0, gpp->mv) || is_equal(gpp->yval_0, gpp->mv)))
    {
      if (proj_lcc_to_lonlat_func)
      {
        gpp->xval_0 = -gpp->x_0;
        gpp->yval_0 = -gpp->y_0;
        proj_lcc_to_lonlat_func(*gpp, 0.0, 0.0, (SizeType) 1, &gpp->xval_0, &gpp->yval_0);
      }
      if (is_equal(gpp->xval_0, gpp->mv) || is_equal(gpp->yval_0, gpp->mv))
        Warning("%s mapping parameter %s missing!", projection,
                "longitudeOfFirstGridPointInDegrees and latitudeOfFirstGridPointInDegrees");
    }
  }

  return 0;
}

int
gridVerifyProjParamsSTERE(struct CDI_GridProjParams *gpp)
{
  static bool lwarn = true;

  if (lwarn)
  {
    // lwarn = false;
    const char *projection = "polar_stereographic";
    if (is_equal(gpp->lon_0, gpp->mv))
      Warning("%s mapping parameter %s missing!", projection, "straight_vertical_longitude_from_pole");
    if (is_equal(gpp->lat_0, gpp->mv)) Warning("%s mapping parameter %s missing!", projection, "latitude_of_projection_origin");
    if (is_equal(gpp->lat_1, gpp->mv)) Warning("%s mapping parameter %s missing!", projection, "standard_parallel");
    if (is_not_equal(gpp->x_0, gpp->mv) && is_not_equal(gpp->y_0, gpp->mv)
        && (is_equal(gpp->xval_0, gpp->mv) || is_equal(gpp->yval_0, gpp->mv)))
    {
      if (proj_stere_to_lonlat_func)
      {
        gpp->xval_0 = -gpp->x_0;
        gpp->yval_0 = -gpp->y_0;
        proj_stere_to_lonlat_func(*gpp, 0.0, 0.0, (SizeType) 1, &gpp->xval_0, &gpp->yval_0);
      }
      if (is_equal(gpp->xval_0, gpp->mv) || is_equal(gpp->yval_0, gpp->mv))
        Warning("%s mapping parameter %s missing!", projection,
                "longitudeOfFirstGridPointInDegrees and latitudeOfFirstGridPointInDegrees");
    }
  }

  return 0;
}

int
gridVerifyProjParamsHEALPIX(struct CDI_GridProjParams *gpp)
{
  static bool lwarn = true;

  if (lwarn)
  {
    lwarn = false;
    const char *projection = "healpix";
    if (is_equal(gpp->nside, -1)) Error("%s mapping parameter %s missing!", projection, "nside");
    if (is_equal(gpp->order, -1)) Error("%s mapping parameter %s missing!", projection, "order");
    if (gpp->nside == 0) Error("%s mapping parameter %s unsupported!", projection, "nside", gpp->nside);
    if (gpp->order != 0 && gpp->order != 1) Error("%s mapping parameter %s=%d unsupported!", projection, "order", gpp->order);
  }

  return 0;
}

/*
@Function  gridDefParamsSTERE
@Title     Define the parameter of a Polar stereographic grid

@Prototype void gridDefParamsSTERE(int gridID, struct CDI_GridProjParams gridProjParams)
@Parameter
    @Item  gridID          Grid ID, from a previous call to @fref{gridCreate}.
    @Item  gridProjParams  Grid projection parameters.

@Description
The function @func{gridDefParamsSTERE} defines the parameter of a Polar stereographic grid.

@EndFunction
*/
void
gridDefParamsSTERE(int gridID, struct CDI_GridProjParams gpp)
{
  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARNAME, "Polar_Stereographic");

  const char *gmapname = "polar_stereographic";
  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapname);
  cdiDefAttTxt(gridID, CDI_GLOBAL, "grid_mapping_name", (int) (strlen(gmapname)), gmapname);
  cdiDefAttFlt(gridID, CDI_GLOBAL, "standard_parallel", CDI_DATATYPE_FLT64, 1, &gpp.lat_1);
  cdiDefAttFlt(gridID, CDI_GLOBAL, "straight_vertical_longitude_from_pole", CDI_DATATYPE_FLT64, 1, &gpp.lon_0);
  cdiDefAttFlt(gridID, CDI_GLOBAL, "latitude_of_projection_origin", CDI_DATATYPE_FLT64, 1, &gpp.lat_0);

  gridDefParamsCommon(gridID, gpp);

  grid_t *gridP = grid_to_pointer(gridID);
  gridP->projtype = CDI_PROJ_STERE;

  gridVerifyProj(gridID);
}

void
gridDefParamsHEALPIX(int gridID, struct CDI_GridProjParams gpp)
{
  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_VARNAME, "healpix");

  const char *gmapname = "healpix";
  cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapname);
  cdiDefAttTxt(gridID, CDI_GLOBAL, "grid_mapping_name", (int) (strlen(gmapname)), gmapname);

  cdiDefAttInt(gridID, CDI_GLOBAL, "healpix_nside", CDI_DATATYPE_INT32, 1, &gpp.nside);
  const char *orderName = (gpp.order == 1) ? "nested" : "ring";
  cdiDefAttTxt(gridID, CDI_GLOBAL, "healpix_order", (int) (strlen(orderName)), orderName);

  // gridDefParamsCommon(gridID, gpp);

  grid_t *gridP = grid_to_pointer(gridID);
  gridP->projtype = CDI_PROJ_HEALPIX;

  // gridVerifyProj(gridID);
}

/*
@Function  gridInqParamsSTERE
@Title     Get the parameter of a Polar stereographic grid

@Prototype void gridInqParamsSTERE(int gridID, struct CDI_GridProjParams *gpp)
@Parameter
    @Item  gridID    Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.
    @Item  gridProjParams  Grid projection parameters.

@Description
The function @func{gridInqParamsSTERE} returns the parameter of a Polar stereographic grid.

@EndFunction
*/
int
gridInqParamsSTERE(int gridID, struct CDI_GridProjParams *gpp)
{
  int status = -1;
  if (gridInqType(gridID) != GRID_PROJECTION) return status;

  gridProjParamsInit(gpp);

  status = -2;
  const char *projection = "polar_stereographic";
  char gmapname[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapname, &length);
  if (gmapname[0] && str_is_equal(gmapname, projection))
  {
    int atttype, attlen;
    char attname[CDI_MAX_NAME + 1];

    int natts;
    cdiInqNatts(gridID, CDI_GLOBAL, &natts);

    if (natts) status = 0;

    for (int iatt = 0; iatt < natts; ++iatt)
    {
      cdiInqAtt(gridID, CDI_GLOBAL, iatt, attname, &atttype, &attlen);
      if (attlen > 2) continue;

      double attflt[2];
      if (cdiInqAttConvertedToFloat(gridID, atttype, attname, attlen, attflt))
      {
        // clang-format off
        if      (str_is_equal(attname, "earth_radius"))                          gpp->a      = attflt[0];
        else if (str_is_equal(attname, "semi_major_axis"))                       gpp->a      = attflt[0];
        else if (str_is_equal(attname, "semi_minor_axis"))                       gpp->b      = attflt[0];
        else if (str_is_equal(attname, "inverse_flattening"))                    gpp->rf     = attflt[0];
        else if (str_is_equal(attname, "standard_parallel"))                     gpp->lat_1  = attflt[0];
        else if (str_is_equal(attname, "straight_vertical_longitude_from_pole")) gpp->lon_0  = attflt[0];
        else if (str_is_equal(attname, "latitude_of_projection_origin"))         gpp->lat_0  = attflt[0];
        else if (str_is_equal(attname, "false_easting"))                         gpp->x_0    = attflt[0];
        else if (str_is_equal(attname, "false_northing"))                        gpp->y_0    = attflt[0];
        else if (str_is_equal(attname, "longitudeOfFirstGridPointInDegrees"))    gpp->xval_0 = attflt[0];
        else if (str_is_equal(attname, "latitudeOfFirstGridPointInDegrees"))     gpp->yval_0 = attflt[0];
        // clang-format on
      }
    }
  }

  return status;
}

int
gridInqParamsHEALPIX(int gridID, struct CDI_GridProjParams *gpp)
{
  int status = -1;
  if (gridInqType(gridID) != GRID_PROJECTION) return status;

  gridProjParamsInit(gpp);

  status = -2;
  const char *projection = "healpix";
  char gmapname[CDI_MAX_NAME];
  int length = CDI_MAX_NAME;
  cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_GRIDMAP_NAME, gmapname, &length);
  if (gmapname[0] && str_is_equal(gmapname, projection))
  {
    int atttype, attlen;
    char attname[CDI_MAX_NAME + 1];

    int natts;
    cdiInqNatts(gridID, CDI_GLOBAL, &natts);

    if (natts) status = 0;

    for (int iatt = 0; iatt < natts; ++iatt)
    {
      cdiInqAtt(gridID, CDI_GLOBAL, iatt, attname, &atttype, &attlen);

      if (atttype == CDI_DATATYPE_TXT)
      {
        char attstring[256];
        if (cdiInqAttTxt(gridID, CDI_GLOBAL, attname, (int) sizeof(attstring), attstring) == 0)
        {
          attstring[attlen] = 0;
          if (str_is_equal(attname, "healpix_order")) gpp->order = strStartsWith(attstring, "nest");
        }
      }
      else
      {
        if (attlen > 2) continue;
        double attflt[2];
        if (cdiInqAttConvertedToFloat(gridID, atttype, attname, attlen, attflt))
        {
          // clang-format off
          if      (str_is_equal(attname, "earth_radius"))                          gpp->a      = attflt[0];
          else if (str_is_equal(attname, "semi_major_axis"))                       gpp->a      = attflt[0];
          else if (str_is_equal(attname, "semi_minor_axis"))                       gpp->b      = attflt[0];
          else if (str_is_equal(attname, "inverse_flattening"))                    gpp->rf     = attflt[0];
          else if (str_is_equal(attname, "longitudeOfFirstGridPointInDegrees"))    gpp->xval_0 = attflt[0];
          else if (str_is_equal(attname, "healpix_nside"))                         gpp->nside  = (int) lround(attflt[0]);
          // clang-format on
        }
      }
    }
  }

  return status;
}

void
gridDefComplexPacking(int gridID, int lcomplex)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (gridP->lcomplex != lcomplex)
  {
    gridP->lcomplex = (lcomplex != 0);
    gridMark4Update(gridID);
  }
}

int
gridInqComplexPacking(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);

  return (int) gridP->lcomplex;
}

void
gridDefHasDims(int gridID, int hasdims)
{
  grid_t *gridP = grid_to_pointer(gridID);

  if (gridP->hasdims != (hasdims != 0))
  {
    gridP->hasdims = (hasdims != 0);
    gridMark4Update(gridID);
  }
}

int
gridInqHasDims(int gridID)
{
  grid_t *gridP = grid_to_pointer(gridID);

  return (int) gridP->hasdims;
}

/*
@Function  gridDefNumber
@Title     Define the reference number for an unstructured grid

@Prototype void gridDefNumber(int gridID, int number)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  number   Reference number for an unstructured grid.

@Description
The function @func{gridDefNumber} defines the reference number for an unstructured grid.

@EndFunction
*/
void
gridDefNumber(int gridID, int number)
{
  cdiDefKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDUSED, number);
}

/*
@Function  gridInqNumber
@Title     Get the reference number to an unstructured grid

@Prototype int gridInqNumber(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqNumber} returns the reference number to an unstructured grid.

@Result
@func{gridInqNumber} returns the reference number to an unstructured grid.
@EndFunction
*/
int
gridInqNumber(int gridID)
{
  int number = 0;
  cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDUSED, &number);
  return number;
}

/*
@Function  gridDefPosition
@Title     Define the position of grid in the reference file

@Prototype void gridDefPosition(int gridID, int position)
@Parameter
    @Item  gridID     Grid ID, from a previous call to @fref{gridCreate}.
    @Item  position   Position of grid in the reference file.

@Description
The function @func{gridDefPosition} defines the position of grid in the reference file.

@EndFunction
*/
void
gridDefPosition(int gridID, int position)
{
  cdiDefKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDINREFERENCE, position);
}

/*
@Function  gridInqPosition
@Title     Get the position of grid in the reference file

@Prototype int gridInqPosition(int gridID)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqPosition} returns the position of grid in the reference file.

@Result
@func{gridInqPosition} returns the position of grid in the reference file.
@EndFunction
*/
int
gridInqPosition(int gridID)
{
  int position = 0;
  cdiInqKeyInt(gridID, CDI_GLOBAL, CDI_KEY_NUMBEROFGRIDINREFERENCE, &position);
  return position;
}

/*
@Function  gridDefReference
@Title     Define the reference URI for an unstructured grid

@Prototype void gridDefReference(int gridID, const char *reference)
@Parameter
    @Item  gridID      Grid ID, from a previous call to @fref{gridCreate}.
    @Item  reference   Reference URI for an unstructured grid.

@Description
The function @func{gridDefReference} defines the reference URI for an unstructured grid.

@EndFunction
*/
void
gridDefReference(int gridID, const char *reference)
{
  if (reference)
  {
    cdiDefKeyString(gridID, CDI_GLOBAL, CDI_KEY_REFERENCEURI, reference);
    gridMark4Update(gridID);
  }
}

/*
@Function  gridInqReference
@Title     Get the reference URI to an unstructured grid

@Prototype char *gridInqReference(int gridID, char *reference)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqReference} returns the reference URI to an unstructured grid.

@Result
@func{gridInqReference} returns the reference URI to an unstructured grid.
@EndFunction
*/
int
gridInqReference(int gridID, char *reference)
{
  int length = 0;
  if (CDI_NOERR == cdiInqKeyLen(gridID, CDI_GLOBAL, CDI_KEY_REFERENCEURI, &length))
  {
    if (reference) cdiInqKeyString(gridID, CDI_GLOBAL, CDI_KEY_REFERENCEURI, reference, &length);
  }

  return length;
}

/*
@Function  gridDefUUID
@Title     Define the UUID for an unstructured grid

@Prototype void gridDefUUID(int gridID, const char *uuid)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate}.
    @Item  uuid     UUID for an unstructured grid.

@Description
The function @func{gridDefUUID} defines the UUID for an unstructured grid.

@EndFunction
*/
void
gridDefUUID(int gridID, const unsigned char uuid[CDI_UUID_SIZE])
{
  cdiDefKeyBytes(gridID, CDI_GLOBAL, CDI_KEY_UUID, uuid, CDI_UUID_SIZE);

  gridMark4Update(gridID);
}

/*
@Function  gridInqUUID
@Title     Get the UUID to an unstructured grid

@Prototype void gridInqUUID(int gridID, char *uuid)
@Parameter
    @Item  gridID   Grid ID, from a previous call to @fref{gridCreate} or @fref{vlistInqVarGrid}.

@Description
The function @func{gridInqUUID} returns the UUID to an unstructured grid.

@Result
@func{gridInqUUID} returns the UUID to an unstructured grid to the parameter uuid.
@EndFunction
*/
void
gridInqUUID(int gridID, unsigned char uuid[CDI_UUID_SIZE])
{
  memset(uuid, 0, CDI_UUID_SIZE);
  int length = CDI_UUID_SIZE;
  cdiInqKeyBytes(gridID, CDI_GLOBAL, CDI_KEY_UUID, uuid, &length);
}

void
cdiGridGetIndexList(unsigned ngrids, int *gridIndexList)
{
  reshGetResHListOfType(ngrids, gridIndexList, &gridOps);
}

struct gridCompareSearchState
{
  int resIDValue;
  const grid_t *queryKey;
};

static enum cdiApplyRet
gridCompareSearch(int id, void *res, void *data)
{
  struct gridCompareSearchState *state = (struct gridCompareSearchState *) data;
  (void) res;
  if (gridCompare(id, state->queryKey, true) == false)
  {
    state->resIDValue = id;
    return CDI_APPLY_STOP;
  }
  else
    return CDI_APPLY_GO_ON;
}

// Add grid (which must be Malloc'ed to vlist if not already found)
struct addIfNewRes
cdiVlistAddGridIfNew(int vlistID, grid_t *grid, int mode)
{
  /*
    mode: 0 search in vlist and grid table
          1 search in grid table only
          2 search in grid table only and don't store the grid in vlist
   */
  bool gridIsDefinedGlobal = false;
  bool gridIsDefined = false;
  int gridID = CDI_UNDEFID;
  vlist_t *vlistP = vlist_to_pointer(vlistID);

  int ngrids = vlistP->ngrids;

  if (mode == 0)
    for (int index = 0; index < ngrids; index++)
    {
      if ((gridID = vlistP->gridIDs[index]) != CDI_UNDEFID)
      {
        if (gridCompare(gridID, grid, false) == false)
        {
          gridIsDefined = true;
          break;
        }
      }
      else
        Error("Internal problem: undefined gridID in vlist %d, position %u!", vlistID, index);
    }

  if (!gridIsDefined)
  {
    struct gridCompareSearchState query;
    query.queryKey = grid;  // = { .queryKey = grid };
    if ((gridIsDefinedGlobal = (cdiGridApply(gridCompareSearch, &query) == CDI_APPLY_STOP))) gridID = query.resIDValue;

    if (mode == 1 && gridIsDefinedGlobal)
      for (int index = 0; index < ngrids; index++)
        if (vlistP->gridIDs[index] == gridID)
        {
          gridIsDefinedGlobal = false;
          break;
        }
  }

  if (!gridIsDefined)
  {
    if (!gridIsDefinedGlobal)
    {
      grid->self = gridID = reshPut(grid, &gridOps);
      grid_complete(grid);
    }
    if (mode < 2)
    {
      if (ngrids >= MAX_GRIDS_PS) Error("Internal limit exceeded, MAX_GRIDS_PS=%d needs to be increased!", MAX_GRIDS_PS);
      vlistP->gridIDs[ngrids] = gridID;
      vlistP->ngrids++;
    }
  }

  return (struct addIfNewRes) { .Id = gridID, .isNew = (!gridIsDefined && !gridIsDefinedGlobal) };
}

const struct gridVirtTable cdiGridVtable = {
  .destroy = gridDestroyKernel,
  .copy = grid_copy_base,
  .copyScalarFields = grid_copy_base_scalar_fields,
  .copyArrayFields = grid_copy_base_array_fields,
  .defIndices = gridDefIndicesSerial,
  .inqIndices = gridInqIndicesSerial,
  .inqIndicesPtr = gridInqIndicesPtrSerial,
  .defXVals = gridDefXValsSerial,
  .defYVals = gridDefYValsSerial,
  .defMask = gridDefMaskSerial,
  .defMaskGME = gridDefMaskGMESerial,
  .defXBounds = gridDefXBoundsSerial,
  .defYBounds = gridDefYBoundsSerial,
  .defArea = gridDefAreaSerial,
  .inqXVal = gridInqXValSerial,
  .inqYVal = gridInqYValSerial,
  .inqXVals = gridInqXValsSerial,
  .inqXValsPart = gridInqXValsPartSerial,
  .inqYVals = gridInqYValsSerial,
  .inqYValsPart = gridInqYValsPartSerial,
  .inqXValsPtr = gridInqXValsPtrSerial,
  .inqYValsPtr = gridInqYValsPtrSerial,
#ifndef USE_MPI
  .inqXIsc = gridInqXIscSerial,
  .inqYIsc = gridInqYIscSerial,
  .inqXCvals = gridInqXCvalsSerial,
  .inqYCvals = gridInqYCvalsSerial,
  .inqXCvalsPtr = gridInqXCvalsPtrSerial,
  .inqYCvalsPtr = gridInqYCvalsPtrSerial,
#endif
  .inqXInc = gridInqXIncBase,
  .inqYInc = gridInqYIncBase,
  .compareXYFull = compareXYvals,
  .compareXYAO = compareXYvals2,
  .inqArea = gridInqAreaSerial,
  .inqAreaPtr = gridInqAreaPtrBase,
  .inqPropPresence = gridInqPropPresenceBase,
  .inqMask = gridInqMaskSerial,
  .inqMaskGME = gridInqMaskGMESerial,
  .inqXBounds = gridInqXBoundsSerial,
  .inqYBounds = gridInqYBoundsSerial,
  .inqXBoundsPtr = gridInqXBoundsPtrSerial,
  .inqYBoundsPtr = gridInqYBoundsPtrSerial,
  .txCode = GRID,
  .getPackSize = gridGetPackSizeBase,
  .getPackSizeScalars = gridGetPackSizeScalars,
  .getPackSizeArrays = gridGetPackSizeArrays,
  .unpackScalars = gridUnpackScalars,
  .unpackArrays = gridUnpackArrays,
  .pack = gridPackBase,
  .packScalars = gridPackScalars,
  .packArrays = gridPackArrays,
};

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
