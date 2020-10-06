@enum ByteOrder::Cint begin
  CDI_BIGENDIAN = 0 #  Byte order BIGENDIAN
  CDI_LITTLEENDIAN = 1 #  Byte order LITTLEENDIAN
  CDI_PDPENDIAN = 2
end



@enum NumberType::Cint begin
  CDI_REAL = 1 #  Real numbers
  CDI_COMP = 2 #  Complex numbers
  CDI_BOTH = 3 #  Both numbers
end



@enum ErrorIdentifier::Cint begin
  CDI_EEOF = -1 #  The end of file was encountered
  CDI_ETMOF = -9 #  Too many open files
  CDI_ESYSTEM = -10 #  Operating system error
  CDI_EINVAL = -20 #  Invalid argument
  CDI_EISDIR = -21 #  Is a directory
  CDI_EISEMPTY = -22 #  Is empty
  CDI_EUFTYPE = -23 #  Unsupported file type
  CDI_ELIBNAVAIL = -24 #  xxx library not available
  CDI_EUFSTRUCT = -25 #  Unsupported file structure
  CDI_EUNC4 = -26 #  Unsupported NetCDF4 structure
  CDI_EDIMSIZE = -27 #  Invalid dimension size
  CDI_ELIMIT = -99 #  Internal limits exceeded
end



@enum FileTypes::Cint begin
  CDI_FILETYPE_GRB = 1 #  File type GRIB
  CDI_FILETYPE_GRB2 = 2 #  File type GRIB version 2
  CDI_FILETYPE_NC = 3 #  File type NetCDF
  CDI_FILETYPE_NC2 = 4 #  File type NetCDF version 2 (64-bit offset)
  CDI_FILETYPE_NC4 = 5 #  File type NetCDF version 4
  CDI_FILETYPE_NC4C = 6 #  File type NetCDF version 4 (classic)
  CDI_FILETYPE_NC5 = 7 #  File type NetCDF version 5 (64-bit data)
  CDI_FILETYPE_SRV = 8 #  File type SERVICE
  CDI_FILETYPE_EXT = 9 #  File type EXTRA
  CDI_FILETYPE_IEG = 10 #  File type IEG
end



@enum CompressTypes::Cint begin
  CDI_COMPRESS_NONE = 0
  CDI_COMPRESS_SZIP = 1
  CDI_COMPRESS_AEC = 2
  CDI_COMPRESS_ZIP = 3
  CDI_COMPRESS_JPEG = 4
end



@enum dataTypes::Cint begin
  CDI_DATATYPE_PACK = 0
  CDI_DATATYPE_PACK1 = 1
  CDI_DATATYPE_PACK2 = 2
  CDI_DATATYPE_PACK3 = 3
  CDI_DATATYPE_PACK4 = 4
  CDI_DATATYPE_PACK5 = 5
  CDI_DATATYPE_PACK6 = 6
  CDI_DATATYPE_PACK7 = 7
  CDI_DATATYPE_PACK8 = 8
  CDI_DATATYPE_PACK9 = 9
  CDI_DATATYPE_PACK10 = 10
  CDI_DATATYPE_PACK11 = 11
  CDI_DATATYPE_PACK12 = 12
  CDI_DATATYPE_PACK13 = 13
  CDI_DATATYPE_PACK14 = 14
  CDI_DATATYPE_PACK15 = 15
  CDI_DATATYPE_PACK16 = 16
  CDI_DATATYPE_PACK17 = 17
  CDI_DATATYPE_PACK18 = 18
  CDI_DATATYPE_PACK19 = 19
  CDI_DATATYPE_PACK20 = 20
  CDI_DATATYPE_PACK21 = 21
  CDI_DATATYPE_PACK22 = 22
  CDI_DATATYPE_PACK23 = 23
  CDI_DATATYPE_PACK24 = 24
  CDI_DATATYPE_PACK25 = 25
  CDI_DATATYPE_PACK26 = 26
  CDI_DATATYPE_PACK27 = 27
  CDI_DATATYPE_PACK28 = 28
  CDI_DATATYPE_PACK29 = 29
  CDI_DATATYPE_PACK30 = 30
  CDI_DATATYPE_PACK31 = 31
  CDI_DATATYPE_PACK32 = 32
  CDI_DATATYPE_CPX32 = 64
  CDI_DATATYPE_CPX64 = 128
  CDI_DATATYPE_FLT32 = 132
  CDI_DATATYPE_FLT64 = 164
  CDI_DATATYPE_INT8 = 208
  CDI_DATATYPE_INT16 = 216
  CDI_DATATYPE_INT32 = 232
  CDI_DATATYPE_UINT8 = 308
  CDI_DATATYPE_UINT16 = 316
  CDI_DATATYPE_UINT32 = 332
  CDI_DATATYPE_INT = 251
  CDI_DATATYPE_FLT = 252
  CDI_DATATYPE_TXT = 253
  CDI_DATATYPE_CPX = 254
  CDI_DATATYPE_UCHAR = 255
  CDI_DATATYPE_LONG = 256
  CDI_DATATYPE_UINT = 257
end



@enum Chunks::Cint begin
  CDI_CHUNK_AUTO = 1 #  use default chunk size
  CDI_CHUNK_GRID = 2
  CDI_CHUNK_LINES = 3
end



@enum GRID::Cint begin
  GRID_GENERIC = 1 #  Generic grid
  GRID_GAUSSIAN = 2 #  Regular Gaussian lon/lat grid
  GRID_GAUSSIAN_REDUCED = 3 #  Reduced Gaussian lon/lat grid
  GRID_LONLAT = 4 #  Regular longitude/latitude grid
  GRID_SPECTRAL = 5 #  Spherical harmonic coefficients
  GRID_FOURIER = 6 #  Fourier coefficients
  GRID_GME = 7 #  Icosahedral-hexagonal GME grid
  GRID_TRAJECTORY = 8 #  Trajectory
  GRID_UNSTRUCTURED = 9 #  General unstructured grid
  GRID_CURVILINEAR = 10 #  Curvilinear grid
  GRID_PROJECTION = 12 #  Projected coordinates
  GRID_CHARXY = 13 #  One horizontal character dimension
  CDI_PROJ_RLL = 21 #  Rotated Latitude Longitude
  CDI_PROJ_LCC = 22 #  Lambert Conformal Conic
  CDI_PROJ_LAEA = 23 #  Lambert Azimuthal Equal Area
  CDI_PROJ_SINU = 24 #  Sinusoidal
  CDI_PROJ_STERE = 25 #  Polar stereographic
end



@enum ZAXIS::Cint begin
  ZAXIS_SURFACE = 0 #  Surface level
  ZAXIS_GENERIC = 1 #  Generic level
  ZAXIS_HYBRID = 2 #  Hybrid level
  ZAXIS_HYBRID_HALF = 3 #  Hybrid half level
  ZAXIS_PRESSURE = 4 #  Isobaric pressure level in Pascal
  ZAXIS_HEIGHT = 5 #  Height above ground
  ZAXIS_DEPTH_BELOW_SEA = 6 #  Depth below sea level in meters
  ZAXIS_DEPTH_BELOW_LAND = 7 #  Depth below land surface in centimeters
  ZAXIS_ISENTROPIC = 8 #  Isentropic
  ZAXIS_TRAJECTORY = 9 #  Trajectory
  ZAXIS_ALTITUDE = 10 #  Altitude above mean sea level in meters
  ZAXIS_SIGMA = 11 #  Sigma level
  ZAXIS_MEANSEA = 12 #  Mean sea level
  ZAXIS_TOA = 13 #  Norminal top of atmosphere
  ZAXIS_SEA_BOTTOM = 14 #  Sea bottom
  ZAXIS_ATMOSPHERE = 15 #  Entire atmosphere
  ZAXIS_CLOUD_BASE = 16 #  Cloud base level
  ZAXIS_CLOUD_TOP = 17 #  Level of cloud tops
  ZAXIS_ISOTHERM_ZERO = 18 #  Level of 0o C isotherm
  ZAXIS_SNOW = 19 #  Snow level
  ZAXIS_LAKE_BOTTOM = 20 #  Lake or River Bottom
  ZAXIS_SEDIMENT_BOTTOM = 21 #  Bottom Of Sediment Layer
  ZAXIS_SEDIMENT_BOTTOM_TA = 22 #  Bottom Of Thermally Active Sediment Layer
  ZAXIS_SEDIMENT_BOTTOM_TW = 23 #  Bottom Of Sediment Layer Penetrated By Thermal Wave
  ZAXIS_MIX_LAYER = 24 #  Mixing Layer
  ZAXIS_REFERENCE = 25 #  zaxis reference number
  ZAXIS_CHAR = 26 #  Area types
end



@enum TIME::Cint begin
  TIME_CONSTANT = 0 #  Time constant
  TIME_VARYING = 1 #  Time varying
  TIME_VARIABLE = 1 #  obsolate, use TIME_VARYING
end



@enum TimeSteps::Cint begin
  TSTEP_INSTANT = 1 #  Instant
  TSTEP_AVG = 2 #  Average
  TSTEP_ACCUM = 3 #  Accumulation
  TSTEP_MAX = 4 #  Maximum
  TSTEP_MIN = 5 #  Minimum
  TSTEP_DIFF = 6 #  Difference
  TSTEP_RMS = 7 #  Root mean square
  TSTEP_SD = 8 #  Standard deviation
  TSTEP_COV = 9 #  Covariance
  TSTEP_RATIO = 10 #  Ratio
  TSTEP_SUM = 11 #  Summation
  TSTEP_RANGE = 12
  TSTEP_INSTANT2 = 13
  TSTEP_INSTANT3 = 14
end



@enum TAXIS::Cint begin
  TAXIS_ABSOLUTE = 1
  TAXIS_RELATIVE = 2
  TAXIS_FORECAST = 3
end



