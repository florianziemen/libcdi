import sys,os
sys.path.insert(0,os.getcwd())
import Cdi

print("# BEGIN PYTHON TEST ==================================#")
ifile = "../mulval.nc"

cdi = Cdi.Cdi(ifile)

print('Stream: ',cdi.streamID,' vlistID:',cdi.vlistID,' nvars:{d}', cdi.nvars)

print('#========== TAXES ====================================#')
for k, tax in cdi.taxes.items():
  print(k,": ", tax.unitname)
  print(k,": ", tax.ntsteps)

print('#========== GRIDS ====================================#')
for k, grid in cdi.grids.items():
  print(k,": ", grid.size,' ', grid.xname,' ', grid.yname,' ', grid.ylongname) 

print("#========== ZAXES ====================================#")
for k, zax in cdi.zaxes.items():
  print(k,": ", zax.size,' ', zax.name,' ', zax.units)

print("#========== VARIABLES ================================#")
for var in cdi.variables:
  print(k,var.name," ",var.size, " ", var.missval)
  print(var.longname,' ',var.units)

print("#========== VAR by index ======================================#")
var = cdi.variables[1]
var.getValues()
val = var.values
i=0; print('val[',i,'] = ',val[i])
i=1; print('val[',i,'] = ',val[i])
i=2; print('val[',i,'] = ',val[i])
i=3; print('val[',i,'] = ',val[i])
i=4; print('val[',i,'] = ',val[i])
i=5; print('val[',i,'] = ',val[i])
print("#========= Var by name ===============================#")
name ="tsurf"
newvar = cdi.var[name]
print("name ",name," var.name: ", newvar.name, " var.grids.xsize: " , newvar.grid.xsize)
print("#========= Var by code ===============================#")
code = 169
newvar = cdi.varByCode[code]
newvar.sinfo()
print("# END PYTHON TEST ====================================#")
