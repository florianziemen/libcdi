#ifndef _GRID_H
#define _GRID_H

#include "cdi.h"
#include <stdbool.h>

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
  double (*inqXVal)(grid_t *gridptr, int index);
  double (*inqYVal)(grid_t *gridptr, int index);
  int (*inqXVals)(grid_t *gridptr, double *xvals);
  int (*inqYVals)(grid_t *gridptr, double *yvals);
  const double *(*inqXValsPtr)(grid_t *gridptr);
  const double *(*inqYValsPtr)(grid_t *gridptr);
  /* return if for both grids, all xval and all yval are equal */
  bool (*compareXYFull)(grid_t *gridRef, grid_t *gridTest);
  /* return if for both grids, x[0], y[0], x[size-1] and y[size-1] are
   * respectively equal */
  bool (*compareXYAO)(grid_t *gridRef, grid_t *gridTest);
  void (*inqArea)(grid_t *gridptr, double *area);
  const double *(*inqAreaPtr)(grid_t *gridptr);
  int (*hasArea)(grid_t *gridptr);
  int (*inqMask)(grid_t *gridptr, int *mask);
  int (*inqMaskGME)(grid_t *gridptr, int *mask_gme);
  int (*inqXBounds)(grid_t *gridptr, double *xbounds);
  int (*inqYBounds)(grid_t *gridptr, double *ybounds);
  const double *(*inqXBoundsPtr)(grid_t *gridptr);
  const double *(*inqYBoundsPtr)(grid_t *gridptr);
};

struct gridaxis_t {
  char    name[CDI_MAX_NAME];
  char    longname[CDI_MAX_NAME];
  char    units[CDI_MAX_NAME];
  char    dimname[CDI_MAX_NAME];
  const char *stdname;
  int     size;                  // number of values
  short   flag;                  // 0: undefined 1:vals 2:first+inc
  double  first, last, inc;
  double *vals;
  double *bounds;
};

// Lambert Conformal Conic
struct grid_lcc_t {
  double  originLon;
  double  originLat;
  double  lonParY;
  double  lat1;
  double  lat2;
  double  xinc;
  double  yinc;
  int     projflag;
  int     scanflag;
  short   defined;
};

// GME Grid
struct grid_gme_t {
  int     nd, ni, ni2, ni3;       /* parameter for GRID_GME         */
};

struct grid_t {
  char    vdimname[CDI_MAX_NAME];
  char    mapname[CDI_MAX_NAME];
  char    mapping[CDI_MAX_NAME];
  char   *name;
  int     self;
  int     type;                   /* grid type                      */
  int     prec;                   /* grid precision                 */
  int     proj;                   /* grid projection                */
  int     projtype;               /* grid projection type           */
  mask_t *mask;
  mask_t *mask_gme;
  double *area;
  struct grid_lcc_t  lcc;
  struct grid_gme_t  gme;
  short   isCyclic;               /* TRUE for global cyclic grids   */
  int     number, position;       /* parameter for GRID_REFERENCE   */
  int     trunc;                  /* parameter for GRID_SPECTEAL    */
  int     nvertex;
  char   *reference;
  unsigned char uuid[CDI_UUID_SIZE]; /* uuid for grid reference        */
  int    *rowlon;
  int     nrowlon;
  int     size;
  int     np;                     /* number of parallels between a pole and the equator */
  bool    lcomplex;
  bool    hasdims;
  long uvRelativeToGrid;           /* Some models deliver wind U,V relative to the grid-cell */
  struct gridaxis_t x;
  struct gridaxis_t y;
  const struct gridVirtTable *vtable;
  cdi_atts_t atts;
};


void grid_init(grid_t *gridptr);
void cdiGridTypeInit(grid_t *gridptr, int gridtype, int size);
void grid_free(grid_t *gridptr);
grid_t *grid_to_pointer(int gridID);
extern const struct gridVirtTable cdiGridVtable;

unsigned cdiGridCount(void);

void gridVerifyProj(int gridID);

const double *gridInqXvalsPtr(int gridID);
const double *gridInqYvalsPtr(int gridID);

const double *gridInqXboundsPtr(int gridID);
const double *gridInqYboundsPtr(int gridID);
const double *gridInqAreaPtr(int gridID);

const char *gridInqReferencePtr(int gridID);

int gridGenerate(const grid_t *grid);

//int gridIsEqual(int gridID1, int gridID2);

void cdiGridGetIndexList(unsigned, int * );

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
