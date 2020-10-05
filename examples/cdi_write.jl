
include("./../interfaces/julia/libcdi.jl")
using .cdi

println("cdi_write example has started")
println(instances(cdi.GRID))
println(instances(GRID))

const size = 200
gridID = gridCreate(LONLAT, convert(Csize_t, size))