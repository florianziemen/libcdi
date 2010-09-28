import os, numpy
import CdiObj

sourcefile  = "BOT.grb"
zonalfile   = "zonavg-BOT.grb"
cdoOperator = "zonavg"
cmd         = ' '.join(['cdo',cdoOperator, sourcefile, zonalfile])

print cmd

os.popen(cmd).read()

cdi = CdiObj.Cdi(zonalfile)

# get all codes and names
for code in cdi.codes:

  # display gridvalues
  var = cdi.varByCode[code]
  #print '# GRID info ===========================================#'
  #g = var.grid
  #for v in g.xvalues:
    #print v
  #for v in g.yvalues:
    #print v
  print "VAR: ", var.name, ' size:' ,var.size
  var.getValues()
  #for v in var.values:
    #print v
  # convert to numpy
  vals = numpy.array(var.values)
  lats = numpy.array(var.grid.yvalues)
  northInd = numpy.where(lats >= 0)
  southInd = numpy.where(lats <  0)
  print 'Global Mean:     ',vals.mean()
  print 'Global Max|Min:  ',vals.max(),'|',vals.min()
  print 'North  Mean:     ',vals[northInd].mean()
  print 'South  Mean:     ',vals[southInd].mean()
  print '#============================================#'
