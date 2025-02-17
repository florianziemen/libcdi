cmake_minimum_required( VERSION 3.27 FATAL_ERROR )

message(VERBOSE "Entering libcdi")
project(cdilib VERSION 2.5.1 LANGUAGES C )
set(CMAKE_C_STANDARD 11)

include(CTest)
include(CheckIncludeFile)


# Finding libraries
### -------------- Pthread ---------------------------
message("Start check for pthread")
include(FindThreads)
find_package(Threads REQUIRED)
set_target_properties(Threads::Threads PROPERTIES THREADS_PREFER_PTHREAD_FLAG TRUE)
list(APPEND cdi_compile_defs HAVE_PTHREAD=1)

# enable default internal libs
list(APPEND cdi_compile_defs
  HAVE_LIBGRIB=1
  HAVE_LIBCGRIBEX=1
  HAVE_LIBEXTRA=1
  HAVE_LIBSERVICE=1
  HAVE_LIBIEG=1
)

# ecCodes
find_package(eccodes)
if (${eccodes_FOUND})
  list(APPEND cdi_compile_defs HAVE_LIBGRIB_API=${eccodes_FOUND})
  message(VERBOSE "added compile definition HAVE_LIBGRIB_API=${eccodes_FOUND}")
else()
  message(WARNING "eccodes not found, compiling without eccodes")
endif ()

# NetCDF
find_package(netCDF COMPONENTS C REQUIRED)
if (TARGET netCDF::netcdf)
  list(APPEND cdi_compile_defs
    HAVE_LIBNETCDF=${netCDF_FOUND}
    HAVE_LIBNC_DAP=${netCDF_FOUND}
    HAVE_NETCDF4=${netCDF_FOUND}
    HAVE_LIBGRIB_API=${netCDF_FOUND}
    HAVE_LIBGRIB=${netCDF_FOUND}
  )
  message(VERBOSE "added to list of definitions: ${cdi_compile_defs} ")
else()
  message(WARNING "netcdf not found, compiling without netcdf")
endif ()

message(VERBOSE "looking for config.h in: ${PROJECT_BINARY_DIR}/src")

#adding subdirectories
## lib and general files
list(APPEND cdi_compile_defs CDI=1 CDI_SIZE_TYPE=size_t PACKAGE_NAME="${PROJECT_NAME}" VERSION="${CMAKE_PROJECT_VERSION}")
add_subdirectory(src)

#tests
add_subdirectory(app)
add_subdirectory(tests)


