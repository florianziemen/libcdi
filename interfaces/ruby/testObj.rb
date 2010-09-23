require './CdiObj'
include CdiObj
require "pp"

ifile = ARGV[0].nil? ? "../testdata/mulval.grb" : ARGV[0]

puts "Reading file: #{ifile}"
cdi = Cdi.new(ifile);

puts "Stream: #{cdi.streamID} vlistID:#{cdi.vlistID} nvars:#{cdi.nvars}"

puts "#========== TAXES ====================================#"
cdi.taxes.each {|k,v| 
  puts k.to_s+": " + cdi.taxes[k].ntsteps.to_s
}
puts "#========== GRIDS ====================================#"
cdi.grids.each {|k,v| 
  puts [k.to_s+": ",
        v.size.to_s,
        v.xname,
        v.yname,
        v.ylongname].join(" ")
}

puts "#========== ZAXES ====================================#"
cdi.zaxes.each {|k,v|
  puts [k.to_s+": ",
        cdi.zaxes[k].size.to_s,
        cdi.zaxes[k].name,
        cdi.zaxes[k].units].join(" ")
}

puts "#========== VARIABLES ================================#"
cdi.variables.each_with_index {|k,i| 
  print k.name[0,5] + " " + k.size.to_s + " "
  puts if i%16==0
}
cdi.variables.each_with_index {|k,i| 
  print k.missval
  puts if i%16==0
}

puts "#========== VARNAMES =================================#"
puts cdi.varnames.sort.join(" ")
puts cdi.varnames.grep(/max/).join(" <-> ")

puts "#========== VARIABLE.NAME =================================#"
puts cdi.variables.collect {|v| v.longname }.join("-")
puts cdi.variables.collect {|v| v.units }.join("-")

puts "#========== CDI.VAR ======================================#"
varname = cdi.variables[1].name
pp cdi.methods.sort
var_ = cdi.variables[1]
var_.readField()
val = var_.getField()
i=0; puts "val[#{i}] = #{val[i]}"
puts "#=====================================================#"
name ="tsurf"
newvar = cdi.var[name]
puts "name ",name," var.name: ", newvar.name, " var.grids.xsize: " , newvar.grid.xsize
puts "#=====================================================#"
