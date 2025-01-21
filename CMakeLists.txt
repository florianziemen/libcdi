cmake_minimum_required( VERSION 3.30 FATAL_ERROR )

project(libcdi VERSION 2.5.1 LANGUAGES C )
set(CMAKE_C_STANDARD 11)

add_compile_definitions(CDI=1)
add_compile_definitions(PACKAGE_NAME="libcdi")
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

include(CheckIncludeFile)

#NetCDF
find_package(netCDF COMPONENTS C REQUIRED)
check_include_file("netcdf.h" ${netcdf_flag} C)
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

