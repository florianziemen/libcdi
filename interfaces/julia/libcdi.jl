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

function gridCreate(gridtype::GRID, size::Unsigned):Cint
    println("Create Grid")
    return ccall((:gridCreate, "libcdi"), Cint, (Cint, Csize_t), gridtype, size)
end

export GRID, LONLAT
export gridCreate

end #module cdi