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

function vlistCreate():Cint
    ccall((:vlistCreate, "libcdi"), Cint, ())
end

function vlistDefVar(vlistID::Cint, gridID::Cint, zaxisID::Cint, timetype::TIME_t)
    ccall((:vlistDefVar, "libcdi"), Cint, (Cint, Cint, Cint, Cint), vlistID, gridID, zaxisID, timetype)
end

function vlistDefVarName(vlistID::Cint, varID::Cint, name::AbstractString)
    ccall((:vlistDefVarName, "libcdi"), Cvoid, (Cint, Cint, Cstring), vlistID, varID, name)
end

function taxisCreate(taxistype::TAXIS_t)
    return ccall((:taxisCreate, "libcdi"), Cint, (Cint,), taxistype)
end

function vlistDefTaxis(vlistID::Cint, taxisID::Cint)
    ccall((:vlistDefTaxis, "libcdi"), Cvoid, (Cint, Cint), vlistID, taxisID)
end

function streamOpenWrite(path::AbstractString, filetype::FileTypes_t)
    return ccall((:streamOpenWrite, "libcdi"), Cint, (Cstring, Cint), path, filetype)
end

function cdiStringError(cdiErrno::Cint)
    str = ccall((:cdiStringError, "libcdi"), Cstring, (Cint,), cdiErrno)
    return unsafe_string(str)
end

function streamDefVlist(streamID::Cint, vlistID::Cint)
    ccall((:streamDefVlist, "libcdi"), Cvoid, (Cint, Cint), streamID, vlistID)
end

function taxisDefVdate(taxisID::Cint, date::Int64)
    ccall((:taxisDefVdate, "libcdi"), Cvoid, (Cint, Int64), taxisID, date)
end

function taxisDefVtime(taxisID::Cint, time::Number)
    ccall((:taxisDefVtime, "libcdi"), Cvoid, (Cint, Cint), taxisID, time)
end

function streamDefTimestep(streamID::Cint, tsID::Number)::Cint
    return ccall((:streamDefTimestep, "libcdi"), Cint, (Cint, Cint), streamID, tsID)
end

function streamWriteVar(streamID::Cint, varID::Cint, data::Vector{T}, nmiss::Number) where {T <: Number}
    ccall((:streamWriteVar, "libcdi"), Cvoid, (Cint, Cint, Ptr{Cdouble}, Csize_t), streamID, varID, data, nmiss)
end

function streamClose(streamID::Cint)
    ccall((:streamClose, "libcdi"), Cvoid, (Cint,), streamID)
end

function vlistDestroy(vlistID::Cint)
    ccall((:vlistDestroy, "libcdi"), Cvoid, (Cint,), vlistID)
end

function taxisDestroy(taxisID::Cint)
    ccall((:taxisDestroy, "libcdi"), Cvoid, (Cint,), taxisID)
end

function zaxisDestroy(zaxisID::Cint)
    ccall((:zaxisDestroy, "libcdi"), Cvoid, (Cint,), zaxisID)
end

function gridDestroy(gridID::Cint)
    ccall((:gridDestroy, "libcdi"), Cvoid, (Cint,), gridID)
end

export gridCreate, gridDefXsize, gridDefYsize, gridDefXvals, gridDefYvals
export zaxisCreate, zaxisDefLevels, vlistCreate, vlistDefVar, vlistDefVarName
export taxisCreate, vlistDefTaxis, streamOpenWrite, cdiStringError
export streamDefVlist, taxisDefVdate, taxisDefVtime, streamDefTimestep, streamWriteVar
export streamClose, vlistDestroy, taxisDestroy, zaxisDestroy, gridDestroy

end #module cdi
