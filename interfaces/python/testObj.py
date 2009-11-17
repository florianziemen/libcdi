import CdiObj

ifile = "../testdata/mulval.nc"

cdi = CdiObj.Cdi(ifile)

print 'Stream: ',cdi.streamID,' vlistID:',cdi.vlistID,' nvars:{d}', cdi.nvars

print '#========== TAXES ====================================#'
for k in range(cdi.taxes.size()):
  print  k,": ", cdi.taxes[k].ntsteps

print '#========== GRIDS ====================================#'
for k in range(cdi.grids.size()):
  print k,": ", cdi.grids[k].size,' ', cdi.grids[k].xname,' ', cdi.grids[k].yname,' ', cdi.grids[k].ylongname 

print "#========== ZAXES ====================================#"
for k in range(cdi.zaxes.size()):
  print k,": ", cdi.zaxes[k].size,' ', cdi.zaxes[k].name,' ', cdi.zaxes[k].units

print "#========== VARIABLES ================================#"
for k in range(cdi.variables.size()):
  v = cdi.variables[k]
  print v.name," ",v.size, " ", v.missval

print "#========== VARIABLEcdi.NAMES =================================#"
for k in range(cdi.variables.size()):
  print cdi.variables[k].longname,' ',cdi.variables[k].units

print "#========== VAR ======================================#"
var = cdi.variables[9]
var.readField()
val = var.getField()
i=0; print 'val[',i,'] = ',val[i]
print "#=====================================================#"
name ="tsurf"
newvar = cdi.var[name]
print "name ",name," var.name: ", newvar.name, " var.grids.xsize: " , newvar.grid.xsize
print "#=====================================================#"
