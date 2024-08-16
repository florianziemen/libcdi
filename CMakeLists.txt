# cmake options:
#
#       -DCMAKE_INSTALL_PREFIX=/path/to/install

cmake_minimum_required( VERSION 3.12 FATAL_ERROR )

project(libcdi VERSION 2.4.0 LANGUAGES C )
set(CMAKE_C_STANDARD 11)

include(CheckIncludeFile)

configure_file (
  "${PROJECT_SOURCE_DIR}/cmake/cdi_config.h.in"
  "${PROJECT_SOURCE_DIR}/src/config.h"
  )

find_package(hdf5 REQUIRED)
if(hdf5_FOUND)
    message(STATUS "HDF5 shaed found")
else()
    message(FATAL_ERROR "HDF shared not found")
endif()

#NetCDF
find_package(netCDF COMPONENTS C REQUIRED)
set(netcdf_flag HAVE_LIBNETCDF)
set(netcdf_flag ${HAVE_LIBNETCDF})
check_include_files("netcdf.h" ${netcdf_flag} C)
if (${netCDF_FOUND})
    message(STATUS "added compile definition HAVE_LIBNETCDF=${netCDF_FOUND}")
    add_compile_definitions(HAVE_LIBNETCDF=${netCDF_FOUND})
else()
  message(WARNING "netcdf not found, compiling without netcdf")
endif ()


#adding subdirectories
## lib and general files
add_subdirectory(src)
## cdi executable
add_subdirectory(app)

