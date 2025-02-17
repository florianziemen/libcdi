cmake_minimum_required( VERSION 3.27 FATAL_ERROR )

message(VERBOSE "Entering libcdi")
project(cdilib VERSION 2.5.1 LANGUAGES C )
set(CMAKE_C_STANDARD 11)

include(CTest)
include(CheckIncludeFile)


# Finding libraries
### -------------- Pthread ---------------------------

option(enable_pthread "Use the pthread library [default=ON]" ON)
if(${enable_pthread})
  include(FindThreads)
  find_package(Threads REQUIRED)
  set_target_properties(Threads::Threads PROPERTIES THREADS_PREFER_PTHREAD_FLAG TRUE)
  list(APPEND cdi_compile_defs HAVE_PTHREAD=1)
endif()

# enable default internal libs
option(enable_libgrib "GRIB support [default=ON]" ON)
if(${enable_libgrib})
  list(APPEND cdi_compile_defs HAVE_LIBGRIB=1)
endif()

option(enable_cgribex "Use the CGRIBEX library [default=ON]" ON)
if(${enable_cgribex})
  list(APPEND cdi_compile_defs LIBCGRIBEX=1)
endif()

option(enable_extra "Use the extra library [default=ON]" ON)
if(${enable_extra})
  list(APPEND cdi_compile_defs HAVE_LIBEXTRA=1)
endif()

option(enable_ieg "Use the extra library [default=ON]" ON)
if(${enable_ieg})
  list(APPEND cdi_compile_defs HAVE_LIBIEG=1)
endif()

option(enable_service "Use the extra library [default=ON]" ON)
if(${enable_service})
  list(APPEND cdi_compile_defs HAVE_LIBSERVICE=1)
endif()


# ecCodes
option(enable_eccodes "Use the eccodes library [default=ON]" ON)
if(${enable_eccodes} OR eccodes_ROOT)
  find_package(eccodes)
  if (${eccodes_FOUND})
    list(APPEND cdi_compile_defs HAVE_LIBGRIB_API=${eccodes_FOUND})
    message(VERBOSE "added compile definition HAVE_LIBGRIB_API=${eccodes_FOUND}")
  else()
    message(WARNING "eccodes not found, compiling without eccodes")
  endif ()
endif()

# NetCDF
option(enable_netcdf "Use the netcdf library [default=ON]" ON)
if(${enable_netcdf} OR netCDF_ROOT )
  find_package(netCDF COMPONENTS C REQUIRED)
  if (TARGET netCDF::netcdf)
    list(APPEND cdi_compile_defs
      HAVE_LIBNETCDF=${netCDF_FOUND}
      HAVE_LIBNC_DAP=${netCDF_FOUND}
      HAVE_NETCDF4=${netCDF_FOUND}
      HAVE_LIBGRIB_API=${netCDF_FOUND}
      HAVE_LIBGRIB=${netCDF_FOUND}
    )
  else()
    message(WARNING "netcdf target not found, compiling without netcdf")
  endif ()
endif()

message(VERBOSE "looking for config.h in: ${PROJECT_BINARY_DIR}/src")

#adding subdirectories
## lib and general files
list(APPEND cdi_compile_defs CDI=1 CDI_SIZE_TYPE=size_t PACKAGE_NAME="${PROJECT_NAME}" VERSION="${CMAKE_PROJECT_VERSION}")
add_subdirectory(src)

#tests
add_subdirectory(app)
add_subdirectory(tests)


