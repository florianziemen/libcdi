#ifndef  GRID_H
#define  GRID_H

#include "cdi.h"
#include <stdbool.h>

#include "cdi_key.h"
#include "cdi_att.h"


typedef unsigned char mask_t;

typedef struct grid_t grid_t;

struct gridVirtTable
{
  void (*destroy)(grid_t *gridptr);
  grid_t *(*copy)(grid_t *gridptr);
  void (*copyScalarFields)(grid_t *gridptrOrig, grid_t *gridptrDup);
  void (*copyArrayFields)(grid_t *gridptrOrig, grid_t *gridptrDup);
  void (*defXVals)(grid_t *gridptr, const double *xvals);
  void (*defYVals)(grid_t *gridptr, const double *yvals);
  void (*defMask)(grid_t *gridptr, const int *mask);
  void (*defMaskGME)(grid_t *gridptr, const int *mask);
  void (*defXBounds)(grid_t *gridptr, const double *xbounds);
  void (*defYBounds)(grid_t *gridptr, const double *ybounds);
  void (*defArea)(grid_t *gridptr, const double *area);
  double (*inqXVal)(grid_t *gridptr, SizeType index);
  double (*inqYVal)(grid_t *gridptr, SizeType index);
  SizeType (*inqXVals)(grid_t *gridptr, double *xvals);
  SizeType (*inqXValsPart)(grid_t *gridptr, int start, SizeType length, double *xvals);
  SizeType (*inqYVals)(grid_t *gridptr, double *yvals);
  SizeType (*inqYValsPart)(grid_t *gridptr, int start, SizeType length, double *yvals);
  const double *(*inqXValsPtr)(grid_t *gridptr);
  const double *(*inqYValsPtr)(grid_t *gridptr);
#ifndef USE_MPI
  int (*inqXIsc)(grid_t *gridptr);
  int (*inqYIsc)(grid_t *gridptr);
  SizeType (*inqXCvals)(grid_t *gridptr, char **xcvals);
  SizeType (*inqYCvals)(grid_t *gridptr, char **ycvals);
  const char **(*inqXCvalsPtr)(grid_t *gridptr);
  const char **(*inqYCvalsPtr)(grid_t *gridptr);
#endif
  // return if for both grids, all xval and all yval are equal
  bool (*compareXYFull)(grid_t *gridRef, grid_t *gridTest);
  // return if for both grids, x[0], y[0], x[size-1] and y[size-1] are respectively equal
  bool (*compareXYAO)(grid_t *gridRef, grid_t *gridTest);
  void (*inqArea)(grid_t *gridptr, double *area);
  const double *(*inqAreaPtr)(grid_t *gridptr);
  int (*hasArea)(grid_t *gridptr);
  SizeType (*inqMask)(grid_t *gridptr, int *mask);
  int (*inqMaskGME)(grid_t *gridptr, int *mask_gme);
  SizeType (*inqXBounds)(grid_t *gridptr, double *xbounds);
  SizeType (*inqYBounds)(grid_t *gridptr, double *ybounds);
  const double *(*inqXBoundsPtr)(grid_t *gridptr);
  const double *(*inqYBoundsPtr)(grid_t *gridptr);
};

struct gridaxis_t {
  size_t      size;                   // number of values
  short       flag;                   // 0: undefined 1:vals 2:first+inc
  double      first, last, inc;
  double     *vals;
  double     *bounds;
  cdi_keys_t  keys;
#ifndef USE_MPI
  int         clength;
  char      **cvals;
#endif
};

// GME Grid
struct grid_gme_t {
  int     nd, ni, ni2, ni3;           // parameter for GRID_GME
};

struct grid_t {
  char       *name;
  int         self;
  size_t      size;
  int         type;                   // grid type
  int         datatype;               // grid data type (used only internal in gridComplete())
  int         proj;                   // grid projection
  int         projtype;               // grid projection type
  mask_t     *mask;
  mask_t     *mask_gme;
  double     *area;
  struct grid_gme_t  gme;
  int         trunc;                  // parameter for GRID_SPECTRAL
  int         nvertex;
  int        *reducedPoints;
  int         reducedPointsSize;
  int         np;                     // number of parallels between a pole and the equator
  signed char isCyclic;               // three possible states:
                                      // -1 if unknown,
                                      //  0 if found not cyclic, or
                                      //  1 for global cyclic grids
  bool        lcomplex;
  bool        hasdims;
  struct gridaxis_t x;
  struct gridaxis_t y;
  const struct gridVirtTable *vtable;
  cdi_keys_t  keys;
  cdi_atts_t  atts;
};


void grid_init(grid_t *gridptr);
void cdiGridTypeInit(grid_t *gridptr, int gridtype, size_t size);
void grid_free(grid_t *gridptr);
grid_t *grid_to_pointer(int gridID);
extern const struct gridVirtTable cdiGridVtable;

unsigned cdiGridCount(void);

void gridVerifyProj(int gridID);

double gridInqXincInMeter(int gridID);
double gridInqYincInMeter(int gridID);

//const double *gridInqXvalsPtr(int gridID);
//const double *gridInqYvalsPtr(int gridID);

const char **gridInqXCvalsPtr(int gridID);
const char **gridInqYCvalsPtr(int gridID);

//const double *gridInqXboundsPtr(int gridID);
//const double *gridInqYboundsPtr(int gridID);
const double *gridInqAreaPtr(int gridID);

int gridGenerate(const grid_t *grid);

//int gridIsEqual(int gridID1, int gridID2);

void cdiGridGetIndexList(unsigned, int *);

void
gridUnpack(char * unpackBuffer, int unpackBufferSize,
           int * unpackBufferPos, int originNamespace, void *context,
           int force_id);

struct addIfNewRes
{
  int Id;
  int isNew;
};

struct addIfNewRes cdiVlistAddGridIfNew(int vlistID, grid_t *grid, int mode);

int gridVerifyProjParamsLCC(struct CDI_GridProjParams *gpp);
int gridVerifyProjParamsSTERE(struct CDI_GridProjParams *gpp);

bool isGaussianLatitudes(SizeType nlats, const double *latitudes);
void gaussianLatitudes(SizeType nlats, double *latitudes, double *weights);

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
