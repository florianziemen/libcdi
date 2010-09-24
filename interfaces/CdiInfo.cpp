#include <stdio.h>
#include <iostream>
#include "cdi.hpp"

int main() {
  int varno;
  std::string _name;
  CdiVariable var;

  Cdi info("./testdata/mulval.nc");
 
  std::cout << "#===============================" << std::endl;
  for (int i = 0; i < info.nvars; i++)
    std::cout << info.codes[i] << ' ';
  std::cout << std::endl;
  for (int i = 0; i < info.nvars; i++)
    std::cout << info.varnames[i] << ' ';
  std::cout << std::endl;

  std::cout << "#===============================" << std::endl;
  std::cout << "#= Available GridIDs are: ";
  for( std::map<int, CdiGrid>::iterator iter = info.grids.begin(); iter != info.grids.end(); ++iter ) {
    std::cout << (*iter).first << ' ';
  };std::cout << std::endl;
  std::cout << "first grid:" << info.grids[0].size << std::endl;
  if ( info.grids[0].hasXYValues )
    for (int k = 0; k < info.grids[0].xsize; k++)
      std::cout << info.grids[0].xvalues[k] << ' ';
  else
    std::cout << "No XValues!" << std::endl;
  std::cout << std::endl;
  std::cout << "second grid:" << info.grids[1].size << std::endl;
  if ( info.grids[1].hasXYValues )
    for (int k = 0; k < info.grids[1].xsize; k++)
      std::cout << info.grids[1].xvalues[k] << ' ';
  else
    std::cout << "No XValues!" << std::endl;
  std::cout << std::endl;
  std::cout << "#===============================" << std::endl;
  varno = 4;
  info.variables[varno].sinfo(); 
  var = info.variables[varno];
  var = info.var["tsurf"];
  var = info.varByCode[176];
  var.sinfo();
  std::cout << "#===============================" << std::endl;
  std::cout << "Gridname:" << var.grid.name << std::endl;
  std::cout << "GridBounds: NumberOfCorners = " << var.grid.ncorner <<  std::endl;
  if ( var.grid.hasXYValues ) {
  std::cout << "xvals "   << var.grid.xvals[0]       << std::endl;
  std::cout << "yvals "   << var.grid.yvals[0]       << std::endl;
  std::cout << "xvalues " << var.grid.xvalues[0] << std::endl;
  std::cout << "xvalues " << var.grid.xvalues.front() << std::endl;
  std::cout << "xvalues " << var.grid.xvalues.back() << std::endl;
  }
  else
    std::cout << "no grid values available" << std::endl;
  if ( var.grid.hasBounds ) {
  std::cout << "xbnds "   << var.grid.xbnds[0]       << std::endl;
  std::cout << "ybnds "   << var.grid.ybnds[0]       << std::endl;
  std::cout << "xbounds " << var.grid.xbounds[0] << std::endl;
  std::cout << "xbounds " << var.grid.xbounds.front() << std::endl;
  std::cout << "xbounds " << var.grid.xbounds.back() << std::endl;
  }
  else
    std::cout << "no grid bounds available" << std::endl;

  std::cout << "#===============================" << std::endl;
  printf("Stream:%d\n"         , var.streamID);
  printf("Taxis (unit):%d\n"   , var.taxis.unit);
  printf("Taxis (ntsteps):%d\n", var.taxis.ntsteps);
  printf("Taxis (type):%d\n"   , var.taxis.type);
  printf("Zaxis (unit):%d\n"   , var.taxis.unit);
  printf("Zaxis (levels):%d\n" , var.zaxis.size);
  printf("Zaxis (type):%d\n"   , var.zaxis.type);
  printf("Zaxis (ltype):%d\n"  , var.zaxis.ltype);
  printf("Zaxis (prec):%d\n"   , var.zaxis.prec);
  printf("Zaxis (levels): varno:%f -1:%f\n", var.zaxis.levels[varno], var.zaxis.levels[var.zaxis.size-1]);

  std::cout << "# field values ================" << std::endl;
  var.getValues();
  std::cout << "values[0]  = " << var.values[0] <<  std::endl;
  std::cout << "values.back() = " << var.values.back() <<  std::endl;
  std::cout << "# field values on different levels ================" << std::endl;
  var.getValuesWithLevel();
  for ( int ilev=0; ilev < var.zaxis.size; ilev++)
  {
    std::cout << "Level:" << ilev << std::endl;
    std::cout << "valuesWithLevel["  << ilev << "][0]  = " << var.valuesWithLevel[ilev][0] << ' ';
    std::cout << "valuesWithLevel["  << ilev << "][-1] = " << var.valuesWithLevel[ilev][var.grid.size-1] << std::endl;
  }
  std::cout << "# field values (FLOAT) ================" << std::endl;
  vector<float> floats;
  floats = var.getFValues();
  std::cout << "floats[0]  = " << floats[0] <<  std::endl;
  std::cout << "floats.back() = " << floats.back() <<  std::endl;

  std::cout << "# field values on different levels (FLOAT) ================" << std::endl;
  vector< vector<float> > floatsWithLevel;
  floatsWithLevel = var.getFValuesWithLevel();
  for (int i = 0 ; i < floatsWithLevel.size();++i)
  {
    std::cout << "floatsWithLevel[" << i << "] = " << floatsWithLevel[i].front() << ' ' << floatsWithLevel[i].back() << std::endl;
  }


  std::cout << "#===============================" << std::endl;
  std::cout << "#== Reading from the var map ===" << std::endl;
  _name = info.variables[varno+10].name;
  std::cout << "#== Use var: " << _name << " ==== (code " << info.variables[varno+10].code << ")" << std::endl;
  var = info.var[_name];
  std::cout << "#==== longname: " << var.longname << std::endl;
  std::cout << "#==== code:     " << var.code << std::endl;
  std::cout << "#==== size:     " << var.size << std::endl;
  std::cout << "#==== gridtype: " << var.grid.type << std::endl;
  std::cout << "#==== unit:     " << var.units << std::endl;
  std::cout << "#==== zdim Name:" << var.zaxis.name << std::endl;
  std::cout << "#==== zlevels:  " << var.zaxis.size << std::endl;
  std::cout << "#===============================" << std::endl;


  return 0;
}
