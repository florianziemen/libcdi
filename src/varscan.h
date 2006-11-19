#ifndef _VARSCAN_H
#define _VARSCAN_H

#ifndef _GRID_H
#  include "grid.h"
#endif


void varAddRecord(int recID, int code, int gridID, int leveltype, int lbounds,
		  int level1, int level2, int prec, int *pvarID, int *plevelID,
		  int numavg, int codetable);

void varDefVCT(size_t vctsize, double *vctptr);

int  varDefGrid(int vlistID, GRID grid, int mode);
int  varDefZaxis(int vlistID, int leveltype, int nlevels, double *levels, int lbounds, double *levels1, double *levels2,
		 int vctsize, double *vct, char *name, char *longname, char *units, int prec, int mode);

void varDefSzip(int varID, int szip);
void varDefInst(int varID, int instID);
int  varInqInst(int varID);
void varDefModel(int varID, int modelID);
int  varInqModel(int varID);
void varDefTable(int varID, int tableID);
int  varInqTable(int varID);

int  zaxisCompare(int zaxisID, int leveltype, int nlevels, int lbounds, double *levels, char *longname, char *units);

#endif
