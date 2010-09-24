import os
import CdiObj

sourcefile  = "BOT.grb"
zonalfile   = "zonavg-BOT.grb"
cdoOperator = "zonavg"
cmd         = ' '.join(['cdo',cdoOperator, sourcefile, zonalfile])

print cmd

os.popen(cmd).read()

cdi = CdiObj.Cdi(zonalfile)

# get all codes and names
myCodes = [176, 96]

# display gridvalues
#var = cdi.varByCode[176]
#print "VAR: ", var.name, ' size:' ,var.size
#g = var.grid
#print var.xvalues.first
#print '#============================================#'
#print var.yvalues
#print '#============================================#'
