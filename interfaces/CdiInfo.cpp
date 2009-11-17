#include <stdio.h>
#include <iostream>
#include "cdi.hpp"

int main() {
  int varno;
  std::string _name;
  CdiVariable var;

  Cdi info("./testdata/mulval.nc");
 
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
  std::cout << "#===============================" << std::endl;
  varno = 2;
  info.variables[varno].print(); 
  var = info.variables[varno];
  std::cout << "#===============================" << std::endl;
  std::cout << "Gridname:%s" << std::endl;
  std::cout << var.grid.name << std::endl;
  std::cout << "GridBounds: NumberOfCorners = " << var.grid.ncorner <<  std::endl;
//  std::cout << "xbounds " << var.grid.xbounds[0] << ' ' << var.grid.xbounds[1000] << ' ';
//  std::cout << "ybounds " << var.grid.ybounds[0] << ' ' << var.grid.ybounds[1000] << std::endl;
//  var.grid.readFloatBounds();
//  std::cout << "xboundsF " << var.grid.xboundsF[0] << ' ' << var.grid.xboundsF[1000] << ' ';
//  std::cout << "yboundsF " << var.grid.yboundsF[0] << ' ' << var.grid.yboundsF[1000] << std::endl;
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

  var.readField();
  var.readFieldF();
  var.readFieldWithLevel();
  var.readFieldWithLevelF();
  std::cout << "field[0]  = " << var.field[0] << ' ';
  std::cout << "field[-1] = " << var.field[var.grid.size-1] << std::endl;
  std::cout << "fieldF[0]  = " << var.fieldF[0] << ' ';
  std::cout << "fieldF[-1] = " << var.fieldF[var.grid.size-1] << std::endl;
  for ( int ilev=0; ilev < var.zaxis.size; ilev++)
  {
    std::cout << "Level:" << ilev << std::endl;
    std::cout << "fieldWithLevel[" << ilev << "][0]  = " << var.fieldWithLevel[ilev][0] << ' ';
    std::cout << "fieldWithLevel[" << ilev << "][-1] = " << var.fieldWithLevel[ilev][var.grid.size-1] << std::endl;
    std::cout << "fieldWithLevelF[" << ilev << "][0]  = " << var.fieldWithLevelF[ilev][0] << ' ';
    std::cout << "fieldWithLevelF[" << ilev << "][-1] = " << var.fieldWithLevelF[ilev][var.grid.size-1] << std::endl;
  }
  std::cout << "#===============================" << std::endl;
  std::cout << "#== Reading from the var map ===" << std::endl;
  _name = info.variables[varno+10].name;
  std::cout << "#== Use var: " << _name << " ==== (varno " << varno+10 << ")" << std::endl;
  var = info.var[_name];
  std::cout << "#==== longname: " << var.longname << std::endl;
  std::cout << "#==== size:     " << var.size << std::endl;
  std::cout << "#==== gridtype: " << var.grid.type << std::endl;
  std::cout << "#==== unit:     " << var.units << std::endl;
  std::cout << "#==== zdim Name:" << var.zaxis.name << std::endl;
  std::cout << "#==== zlevels:  " << var.zaxis.size << std::endl;
  std::cout << "#===============================" << std::endl;

  return 0;
}
