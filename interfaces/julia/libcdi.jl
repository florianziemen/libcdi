module cdi

include("cdi_macro.jl")

function gridCreate(gridtype::GRID_t, size::Number):Cint
    return ccall((:gridCreate, "libcdi"),Cint, (Cint, Csize_t), gridtype, size)
end

function gridDefXsize(gridID::Cint, size::Number)
    ccall((:gridDefXsize, "libcdi"), Cvoid, (Cint, Csize_t), gridID, size)
end

function gridDefYsize(gridID::Cint, size::Number)
    ccall((:gridDefYsize, "libcdi"), Cvoid, (Cint, Csize_t), gridID, size)
end

function gridDefXvals(gridID::Cint, xvals::Vector{T}) where {T <: Number}
    ccall((:gridDefXvals, "libcdi"), Cvoid, (Cint, Ptr{Cdouble}), gridID, xvals)
end

function gridDefYvals(gridID::Cint, yvals::Vector{T}) where {T <: Number}
    ccall((:gridDefYvals, "libcdi"), Cvoid, (Cint, Ptr{Cdouble}), gridID, yvals)
end

function zaxisCreate(zaxistype::ZAXIS_t, size::Number):Cint
    return ccall((:zaxisCreate, "libcdi"), Cint, (Cint, Cint), zaxistype, size)
end

function zaxisDefLevels(zaxisID::Cint, levels::Vector{T}) where {T <: Number}
    ccall((:zaxisDefLevels, "libcdi"), Cvoid, (Cint, Ptr{Cdouble}), zaxisID, levels)
end


export gridCreate, gridDefXsize, gridDefYsize, gridDefXvals, gridDefYvals, zaxisCreate, zaxisDefLevels

end #module cdi