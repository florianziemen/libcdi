#ifndef _GRID_H
#define _GRID_H


typedef struct {
  int     self;
  int     type;                   /* grid type                      */
  int     prec;                   /* grid precision                 */
  int    *mask;
  double *xvals;
  double *yvals;
  double *area;
  double *xbounds;
  double *ybounds;
  double  xfirst, yfirst;
  double  xlast, ylast;
  double  xinc, yinc;
  double  xpole, ypole, angle;    /* rotated north pole             */
  int     isCyclic;               /* TRUE for global cyclic grids   */
  int     isRotated;              /* TRUE for rotated grids         */
  int     xdef;                   /* 0: undefined 1:xvals 2:x0+xinc */
  int     ydef;                   /* 0: undefined 1:yvals 2:y0+yinc */
  int     nd, ni, ni2, ni3;       /* parameter for GRID_GME         */
  int     trunc;                  /* parameter for GRID_SPECTEAL    */
  int     nvertex;
  int    *rowlon;
  int     nrowlon;
  int     size;
  int     xsize;
  int     ysize;
  int     locked;
  char    xname[256];
  char    yname[256];
  char    xlongname[256];
  char    ylongname[256];
  char    xstdname[256];
  char    ystdname[256];
  char    xunits[256];
  char    yunits[256];
}
GRID;


const double *gridInqXvalsPtr(int gridID);
const double *gridInqYvalsPtr(int gridID);

double *gridInqXboundsPtr(int gridID);
double *gridInqYboundsPtr(int gridID);
const double *gridInqAreaPtr(int gridID);

int gridCompare(int gridID, GRID grid);
int gridGenerate(GRID grid);

void gme_grid(int gridsize, double *rlon, double *rlat,
	      double *blon, double *blat, int *imask,
              int ni, int nd, int ni2, int ni3);

double rls_to_rl(double phis, double rlas, double polphi, double pollam);
double phs_to_ph(double phis, double rlas, double polphi);
double rl_to_rls(double phi, double rla, double polphi, double pollam);
double ph_to_phs(double phi, double rla, double polphi, double pollam);

#endif
