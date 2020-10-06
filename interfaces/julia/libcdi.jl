module cdi

include("cdi_macro.jl")

export GRID

function gridCreate(gridtype::GRID, size::Number):Cint
    return ccall((:gridCreate, "libcdi"),Cint, (Cint, Csize_t), gridtype, size)
end

function gridDefXsize(gridID::Cint, size::Number)
    ccall((:gridDefXsize, "libcdi"), Cvoid, (Cint, Csize_t), gridID, size)
end

function gridDefYsize(gridID::Cint, size::Number)
    ccall((:gridDefYsize, "libcdi"), Cvoid, (Cint, Csize_t), gridID, size)
end

function gridDefXvals(gridID::Cint, xvals::Vector{Number})
    ccall((:gridDefXvals, "libcdi"), Cvoid, (Cint, Ptr{Cdouble}), gridID, xvals)
end

function gridDefYvals(gridID::Cint, yvals::Vector{Number})
    ccall((:gridDefYvals, "libcdi"), Cvoid, (Cint, Ptr{Cdouble}), gridID, yvals)
end



export gridCreate, gridDefXsize, gridDefYsize, gridDefXvals, gridDefYvals

end #module cdi