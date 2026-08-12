#ifndef GRID_H
#define GRID_H

#include "cdi.h"
#include <stdbool.h>

#include "cdi_proj.h"
#include "cdi_key.h"
#include "cdi_att.h"
#include "resource_handle.h"

typedef unsigned char mask_t;

typedef struct grid_t grid_t;

enum GridAxis
{
  GRID_AXIS_X,
  GRID_AXIS_Y,
};

enum gridPropInq
{
  GRID_PROP_MASK,
  GRID_PROP_MASK_GME,
  GRID_PROP_XVALS,
  GRID_PROP_YVALS,
  GRID_PROP_AREA,
  GRID_PROP_XBOUNDS,
  GRID_PROP_YBOUNDS,
};

struct gridVirtTable
{
  void (*destroy)(grid_t *gridP);
  grid_t *(*copy)(grid_t *gridP);
  void (*copyScalarFields)(grid_t *gridPOrig, grid_t *gridPDup);
  void (*copyArrayFields)(grid_t *gridPOrig, grid_t *gridPDup);
  void (*defIndices)(grid_t *gridP, const int64_t *indices);
  SizeType (*inqIndices)(grid_t *gridP, int64_t *indices);
  const int64_t *(*inqIndicesPtr)(grid_t *gridP);
  void (*defXVals)(grid_t *gridP, const double *xvals);
  void (*defYVals)(grid_t *gridP, const double *yvals);
  void (*defMask)(grid_t *gridP, const int *mask);
  void (*defMaskGME)(grid_t *gridP, const int *mask);
  void (*defXBounds)(grid_t *gridP, const double *xbounds);
  void (*defYBounds)(grid_t *gridP, const double *ybounds);
  void (*defArea)(grid_t *gridP, const double *area);
  double (*inqXVal)(grid_t *gridP, SizeType index);
  double (*inqYVal)(grid_t *gridP, SizeType index);
  SizeType (*inqXVals)(grid_t *gridP, double *xvals);
  SizeType (*inqXValsPart)(grid_t *gridP, int start, SizeType length, double *xvals);
  SizeType (*inqYVals)(grid_t *gridP, double *yvals);
  SizeType (*inqYValsPart)(grid_t *gridP, int start, SizeType length, double *yvals);
  const double *(*inqXValsPtr)(grid_t *gridP);
  const double *(*inqYValsPtr)(grid_t *gridP);
#ifndef USE_MPI
  int (*inqXIsc)(grid_t *gridP);
  int (*inqYIsc)(grid_t *gridP);
  SizeType (*inqXCvals)(grid_t *gridP, char **xcvals);
  SizeType (*inqYCvals)(grid_t *gridP, char **ycvals);
  const char **(*inqXCvalsPtr)(grid_t *gridP);
  const char **(*inqYCvalsPtr)(grid_t *gridP);
#endif
  double (*inqXInc)(grid_t *gridP);
  double (*inqYInc)(grid_t *gridP);
  // return true if for both grids, any xval and all yval differ
  bool (*compareXYFull)(grid_t *gridRef, grid_t *gridTest);
  // return if for both grids, x[0], y[0], x[size-1] and y[size-1] are respectively equal
  bool (*compareXYAO)(grid_t *gridRef, grid_t *gridTest);
  void (*inqArea)(grid_t *gridP, double *area);
  const double *(*inqAreaPtr)(grid_t *gridP);
  /* return 1 if inq property is set */
  int (*inqPropPresence)(grid_t *gridP, enum gridPropInq inq);
  SizeType (*inqMask)(grid_t *gridP, int *mask);
  int (*inqMaskGME)(grid_t *gridP, int *mask_gme);
  SizeType (*inqXBounds)(grid_t *gridP, double *xbounds);
  SizeType (*inqYBounds)(grid_t *gridP, double *ybounds);
  const double *(*inqXBoundsPtr)(grid_t *gridP);
  const double *(*inqYBoundsPtr)(grid_t *gridP);
  int txCode;
  int (*getPackSize)(grid_t *gridP, void *context);
  int (*getPackSizeScalars)(grid_t *gridP, void *context);
  int (*getPackSizeArrays)(grid_t *gridP, void *context);
  int (*unpack)(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id);
  grid_t *(*unpackScalars)(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context,
                           int force_id, int *memberMaskP);
  void (*unpackArrays)(grid_t *gridP, int memberMask, char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos,
                       int originNamespace, void *context);
  void (*pack)(grid_t *gridP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context);
  /* return member mask */
  int (*packScalars)(grid_t *gridP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context);
  void (*packArrays)(grid_t *gridP, int memberMask, void *packBuffer, int packBufferSize, int *packBufferPos, void *context);
};

struct gridaxis_t
{
  size_t size;  // number of values
  short flag;   // 0: undefined 1:vals 2:first+inc
  double first, last, inc;
  double *vals;
  double *bounds;
  cdi_keys_t keys;
#ifndef USE_MPI
  int clength;
  char **cvals;
#endif
};

// GME Grid
struct grid_gme_t
{
  int nd, ni, ni2, ni3;  // parameter for GRID_GME
};

struct grid_t
{
  char *name;
  int self;
  size_t size;
  int type;      // grid type
  int datatype;  // grid data type (used only internal in gridComplete())
  int proj;      // grid projection
  int projtype;  // grid projection type
  mask_t *mask;
  mask_t *mask_gme;
  int64_t *indices;
  double *area;
  struct grid_gme_t gme;
  int trunc;  // parameter for GRID_SPECTRAL
  int nvertex;
  int *reducedPoints;
  int reducedPointsSize;
  int np;                // number of parallels between a pole and the equator
  signed char isCyclic;  // three possible states:
                         // -1 if unknown,
                         //  0 if found not cyclic, or
                         //  1 for global cyclic grids
  bool lcomplex;
  bool hasdims;
  struct gridaxis_t x;
  struct gridaxis_t y;
  const struct gridVirtTable *vtable;
  cdi_keys_t keys;
  cdi_atts_t atts;
  void *extraData;
};

static inline bool
grid_is_irregular(int gridType)
{
  return (gridType == GRID_UNSTRUCTURED || gridType == GRID_CURVILINEAR);
}

static inline size_t
grid_axis_size(const grid_t *g, enum GridAxis axis)
{
  if (grid_is_irregular(g->type)) return g->size;
  return (axis == GRID_AXIS_Y) ? g->y.size : g->x.size;
}

grid_t *gridNewEntry(cdiResH resH);

void grid_init(grid_t *gridP);
void cdiGridTypeInit(grid_t *gridP, int gridtype, size_t size);
void grid_free(grid_t *gridP);
grid_t *grid_to_pointer(int gridID);
extern const struct gridVirtTable cdiGridVtable;

unsigned cdiGridCount(void);

void gridVerifyProj(int gridID);

double gridInqXincInMeter(int gridID);
double gridInqYincInMeter(int gridID);

// const double *gridInqXvalsPtr(int gridID);
// const double *gridInqYvalsPtr(int gridID);

const char **gridInqXCvalsPtr(int gridID);
const char **gridInqYCvalsPtr(int gridID);

// const double *gridInqXboundsPtr(int gridID);
// const double *gridInqYboundsPtr(int gridID);
const double *gridInqAreaPtr(int gridID);

int gridInqPropPresence(int gridID, enum gridPropInq inq);

int gridGenerate(const grid_t *grid);

// int gridIsEqual(int gridID1, int gridID2);

void cdiGridGetIndexList(unsigned, int *);

/* apply func to each grid */
enum cdiApplyRet cdiGridApply(enum cdiApplyRet (*func)(int id, void *res, void *data), void *data);

struct addIfNewRes
{
  int Id;
  int isNew;
};

struct addIfNewRes cdiVlistAddGridIfNew(int vlistID, grid_t *grid, int mode);

int gridVerifyProjParamsLCC(struct CDI_GridProjParams *gpp);
int gridVerifyProjParamsSTERE(struct CDI_GridProjParams *gpp);
int gridVerifyProjParamsHEALPIX(struct CDI_GridProjParams *gpp);

int gridInqParamsHEALPIX(int gridID, struct CDI_GridProjParams *gridProjParams);

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
