
include("./../interfaces/julia/libcdi.jl")
using .cdi

println("cdi_write example has started")

const lons = convert(Vector{Number}, [0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330])
const lats = convert(Vector{Number}, [-75, -45, -15, 15, 45, 75])
const levs = convert(Vector{Number}, [101300, 92500, 85000, 50000, 20000])

const var1 = Vector{Float64}(undef, length(lons) * length(lats))
const var2 = Vector{Float64}(undef, length(lons) * length(lats) * length(levs))
const size = length(lons) * length(lats)

gridID = gridCreate(LONLAT, size)
gridDefXsize(gridID, length(lons))
gridDefYsize(gridID, length(lats))
gridDefXvals(gridID, lons)
gridDefYvals(gridID, lats)
