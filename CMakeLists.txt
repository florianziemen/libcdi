cmake_minimum_required( VERSION 3.30 FATAL_ERROR )

message("Entering libcdi")
project(libcdi VERSION 2.5.1 LANGUAGES C )
set(CMAKE_C_STANDARD 11)

include(CTest)
include(CheckIncludeFile)

# enable default internal libs
add_compile_definitions(HAVE_LIBGRIB=1)
add_compile_definitions(HAVE_LIBCGRIBEX=1)
add_compile_definitions(HAVE_LIBEXTRA=1)
add_compile_definitions(HAVE_LIBSERVICE=1)
add_compile_definitions(HAVE_LIBIEG=1)

# ecCodes
find_package(eccodes)
if (${eccodes_FOUND})
  add_compile_definitions(HAVE_LIBGRIB_API=${eccodes_FOUND})
  message(STATUS "added compile definition HAVE_LIBGRIB_API=${eccodes_FOUND}")
else()
  message(WARNING "netcdf not found, compiling without netcdf")
endif ()

# NetCDF
find_package(netCDF COMPONENTS C REQUIRED)
check_include_file("netcdf.h" ${netcdf_flag} C)
if (${netCDF_FOUND})
    message("added compile definition HAVE_LIBNETCDF=${netCDF_FOUND}")
    message("added compile definition HAVE_NETCDF4=${netCDF_FOUND}")
    add_compile_definitions(HAVE_LIBNETCDF=${netCDF_FOUND})
    add_compile_definitions(HAVE_NETCDF4=${netCDF_FOUND})
else()
  message(WARNING "netcdf not found, compiling without netcdf")
endif ()

message("looking for config.h in: ${PROJECT_BINARY_DIR}/src")
include_directories( "${PROJECT_BINARY_DIR}/src/config.h")


#adding subdirectories
## lib and general files
add_subdirectory(src)
## cdi executable
add_subdirectory(app)

#tests
add_subdirectory( tests )

