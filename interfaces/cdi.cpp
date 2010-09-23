#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>

#include "cdi.hpp"
#include "cdi.h"

/*
 * CdiGrid
 * {
 */
CdiGrid::CdiGrid() { gridID = -1; }
CdiGrid::CdiGrid(int gridid) {
  char _name[CHARSIZE];
  char _xname[CHARSIZE], _xlongname[CHARSIZE], _xstdname[CHARSIZE], _xunits[CHARSIZE];
  char _yname[CHARSIZE], _ylongname[CHARSIZE], _ystdname[CHARSIZE], _yunits[CHARSIZE];      

  gridID = gridid;
  type   = gridInqType(gridID);
  strcpy(_name,gridNamePtr(type));
  size   = gridInqSize(gridID);
  xsize  = gridInqXsize(gridID);
  ysize  = gridInqXsize(gridID);
  xvals  = (double *) malloc(xsize*sizeof(double));
  yvals  = (double *) malloc(ysize*sizeof(double));
  prec   = gridInqPrec(gridID);
  gridInqXvals(gridID    , xvals);
  gridInqYvals(gridID    , yvals);
  gridInqXname(gridID    , _xname);
  gridInqXlongname(gridID, _xlongname);
  gridInqXstdname(gridID , _xstdname);
  gridInqXunits(gridID   , _xunits);
  gridInqYname(gridID    , _yname);
  gridInqYlongname(gridID, _ylongname);
  gridInqYstdname(gridID , _ystdname);
  gridInqYunits(gridID   , _yunits);
  xname     = _xname;
  xlongname = _xlongname;
  xstdname  = _xstdname;
  xunits    = _xunits;
  yname     = _yname;
  ylongname = _ylongname;
  ystdname  = _ystdname;
  yunits    = _yunits;

  hasBounds = (gridInqXbounds(gridID,NULL) && gridInqYbounds(gridID,NULL));
  if (hasBounds)
  {
    ncorner  = ( gridInqType(gridID) == GRID_CELL ) ? gridInqNvertex(gridID) : 4;
    ncorner *= size;
    xbounds  = (double *) malloc((ncorner*size)*sizeof(double));
    ybounds  = (double *) malloc((ncorner*size)*sizeof(double));
    gridInqYbounds(gridID, xbounds);
    gridInqXbounds(gridID, ybounds);
  }
  else
    ncorner = 0;
}
CdiGrid::~CdiGrid() { gridID = -1; }

void
CdiGrid::readFloatBounds()
{
  if (hasBounds)
  {
    xboundsF = (float *) malloc(ncorner*size*sizeof(float));
    yboundsF = (float *) malloc(ncorner*size*sizeof(float));
    xboundsF = (float *) xbounds;
    yboundsF = (float *) ybounds;
  }
  else
    std::cout << "No bounds available." << std::endl;
}

void
CdiGrid::readFloatVals()
{
  xvalsF   = (float *) malloc(xsize*sizeof(float));
  yvalsF   = (float *) malloc(ysize*sizeof(float));
  xvalsF   = (float *) xvals;
  yvalsF   = (float *) yvals;
}
/* } */

/*
 * CdiTaxis
 * {
 */
CdiTaxis::CdiTaxis() { taxisID = -1; }
CdiTaxis::CdiTaxis(int vlistID) {
  taxisID   = vlistInqTaxis(vlistID);
  type      = taxisInqType(taxisID);
  ntsteps   = vlistNtsteps(vlistID);
  hasBounds = taxisHasBounds(taxisID);
  vdate     = taxisInqVdate(taxisID);
  vtime     = taxisInqVtime(taxisID);
  rdate     = taxisInqRdate(taxisID);
  rtime     = taxisInqRtime(taxisID);
  calendar  = taxisInqCalendar(taxisID);
  unit      = taxisInqTunit(taxisID);
  unitname  = tunitNamePtr(taxisID);
}
CdiTaxis::~CdiTaxis() { if (taxisID >= 0) taxisID = -1; }
/* } */

/*
 * CdiZaxis
 * {
 */
CdiZaxis::CdiZaxis() { zaxisID = -1; }
CdiZaxis::CdiZaxis(int zaxisid) {
  char _name[CHARSIZE], _longname[CHARSIZE], _units[CHARSIZE];

  zaxisID = zaxisid;
  size    = zaxisInqSize(zaxisID);
  prec    = zaxisInqPrec(zaxisID);
  type    = zaxisInqType(zaxisID);
  ltype   = zaxisInqLtype(zaxisID);
  levels  = (double *) malloc(size*sizeof(double));
  lbounds = (double *) malloc(size*sizeof(double));
  ubounds = (double *) malloc(size*sizeof(double));
  weights = (double *) malloc(size*sizeof(double));
  zaxisInqLevels(zaxisID  , levels);
  zaxisInqLbounds(zaxisID , lbounds);
  zaxisInqUbounds(zaxisID , ubounds);
  zaxisInqWeights(zaxisID , weights);
  zaxisInqName(zaxisid    , _name);
  zaxisInqLongname(zaxisid, _longname);
  zaxisInqUnits(zaxisid   , _units);
  name     = _name;
  longname = _longname;
  units    = _units;
}
CdiZaxis::~CdiZaxis() { if (zaxisID >= 0) zaxisID = -1; }
/* } */

/*
 * CdiVariable
 * {
 */
CdiVariable::CdiVariable() { size = -1; }
CdiVariable::CdiVariable(int streamid,int vlistid, int varid) {
  char _name[CHARSIZE],_longname[CHARSIZE], _units[CHARSIZE], _stdname[CHARSIZE];
  streamID = streamid;
  vlistID  = vlistid;
  varID    = varid;
  vlistInqVar(vlistID        , varID, &gridID    , &zaxisID, &timeID);
  vlistInqVarName(vlistID    , varID, _name);
  vlistInqVarLongname(vlistID, varID, _longname);
  vlistInqVarStdname(vlistID , varID, _stdname);
  vlistInqVarUnits(vlistID   , varID, _units);
  taxisID  = vlistInqTaxis(vlistID);
  size     = vlistInqVarSize(vlistID, varID);
  datatype = vlistInqVarDatatype(vlistID,varID);
  missval  = vlistInqVarMissval(vlistID,varID);
  name     = _name;
  longname = _longname;
  stdname  = _stdname;
  units    = _units;
}
CdiVariable::~CdiVariable(){ size = -1; }

void
CdiVariable::sinfo() { std::cout << name << " (" << longname << ")|units: " << units << "|size:"<< size <<  std::endl; }

double *
CdiVariable::getValuesAsPointer()
{
  int levelID = 0, tsID = 0, nmiss;
  int vdate, vtime, nrecs;
  double *field;

  nrecs = streamInqTimestep(streamID, tsID);
  vdate = taxisInqVdate(taxisID);
  vtime = taxisInqVtime(taxisID);
  field = (double *) malloc(grid.size*sizeof(double));
  streamReadVarSlice(streamID, varID, levelID, field, &nmiss);

  return field;
}

double **
CdiVariable::getValuesWithLevelAsPointer(int tsID)
{
  int    levelID, nmiss, nrecs;
  double **fieldWithLevel, *field;

  nrecs          = streamInqTimestep(streamID, tsID);
  fieldWithLevel = (double **) malloc(zaxis.size*sizeof(double *));
  field          = (double *)  malloc(grid.size*sizeof(double));
  for (levelID = 0; levelID < zaxis.size; levelID++)
  {
    streamReadVarSlice(streamID, varID, levelID, field, &nmiss);
    fieldWithLevel[levelID] = field;
  }
  return fieldWithLevel;
}

void
CdiVariable::getValues()
{
  double *field = getValuesAsPointer();
  values.resize(grid.size);
  std::copy(field, field + grid.size, values.begin());
}

void
CdiVariable::getValuesWithLevel(int tsID) {
  double **fieldWithLevel = getValuesWithLevelAsPointer(tsID);

  valuesWithLevel.resize(zaxis.size);
  for (int levelID = 0; levelID < zaxis.size; levelID++)
  {
    valuesWithLevel[levelID].resize(grid.size);
    std::copy(fieldWithLevel[levelID], fieldWithLevel[levelID] + grid.size, valuesWithLevel[levelID].begin());
  }
}

vector<float>
CdiVariable::getFValues()
{
  if (values.empty()) getValues();
  vector<float> retval(values.begin(),values.end());
  return retval;
}


vector< vector<float> >
CdiVariable::getFValuesWithLevel(int tsID)
{
  if (valuesWithLevel.empty()) getValuesWithLevel();
  vector< vector<float> > retval;
  for (vector< vector<double> >::const_iterator it = valuesWithLevel.begin(); it != valuesWithLevel.end(); it++)
  {
    vector<float> fvalues((*it).begin(),(*it).end());
    retval.push_back(fvalues);
  }
  return retval;
}

/* } */

/*
 * Cdi
 * {
 */
Cdi::Cdi(const char *path)  {
  streamID = streamOpenRead(path);
  vlistID  = streamInqVlist(streamID);
  nvars    = vlistNvars(vlistID);

  getTaxes();
  getZaxes();
  getGrids();
  getVars();
}

Cdi::~Cdi() {
  std::cout << "MSG from destructor:" << std::endl;
  std::map<int,CdiGrid>::iterator each;
  for (each = grids.begin(); each != grids.end(); ++each)
  {
    std::cout << (*each).second.size << " | ";
  }
  std::cout << std::endl;
  std::cout << "Taxis Size: " << (*taxes.begin()).second.ntsteps << std::endl;
}

void
Cdi::getTaxes() {
  int taxisID;
  ntaxes         = 1;
  taxisID        = vlistInqTaxis(vlistID);
  taxes[taxisID] = CdiTaxis(vlistID);
}

void
Cdi::getZaxes() {
  int zaxisID;
  nzaxes = vlistNzaxis(vlistID);
  for (int i = 0; i < nzaxes; i++)
  {
    zaxisID        = vlistZaxis(vlistID, i);
    zaxes[zaxisID] = CdiZaxis(zaxisID);
  }
}

void
Cdi::getGrids() {
  int gridID;
  ngrids = vlistNgrids(vlistID);
  for (int i = 0; i < ngrids; i++)
  {
    gridID        = vlistGrid(vlistID, i);
    grids[gridID] = CdiGrid(gridID);
  }
}

void
Cdi::getVars() {
  char name[CHARSIZE];
  int varID;
  std::cout << vlistID << std::endl;
  for (varID = 0; varID < nvars; varID++)
  {
    codes.push_back(vlistInqVarCode(vlistID, varID));

    vlistInqVarName(vlistID, varID, name);
    varnames.push_back(name);

    CdiVariable _var = CdiVariable(streamID,vlistID,varID);
    _var.grid        = grids[_var.gridID];
    _var.zaxis       = zaxes[_var.zaxisID];
    _var.taxis       = taxes[_var.taxisID];
  
    variables.push_back(_var);

    var[name] = _var;
  }
}
/* } */
