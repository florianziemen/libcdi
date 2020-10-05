module cdi

@enum GRID::Cint begin
    GENERIC             = 1  # Generic grid
    GAUSSIAN            = 2  # Regular Gaussian lon/lat grid
    GAUSSIAN_REDUCED    = 3  # Reduced Gaussian lon/lat grid
    LONLAT              = 4  # Regular longitude/latitude grid
    SPECTRAL            = 5  # Spherical harmonic coefficients
    FOURIER             = 6  # Fourier coefficients
    GME                 = 7  # Icosahedral-hexagonal GME grid
    TRAJECTORY          = 8  # Trajectory
    UNSTRUCTURED        = 9  # General unstructured grid
    CURVILINEAR         = 10 # Curvilinear grid
    PROJECTION          = 12 # Projected coordinates
    CHARXY              = 13 # One horizontal character dimension

    PROJ_RLL            = 21 # Rotated Latitude Longitude
    PROJ_LCC            = 22 # Lambert Conformal Conic
    PROJ_LAEA           = 23 # Lambert Azimuthal Equal Area
    PROJ_SINU           = 24 # Sinusoidal
    PROJ_STERE          = 25 # Polar stereographic
end

export GRID, LONLAT

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