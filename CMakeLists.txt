cmake_minimum_required( VERSION 3.27 FATAL_ERROR )

message(VERBOSE "Entering cdilib")
project(cdilib VERSION 2.5.1 LANGUAGES C )
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

add_library(cdilib)
add_library(cdilib::cdilib ALIAS cdilib)

include(CTest)
include(CheckIncludeFile)

# Finding libraries
### -------------- Pthread ---------------------------

option(CDI_PTHREAD "Use the pthread library [default=ON]" ON)
if(${CDI_PTHREAD})
  include(FindThreads)
  find_package(Threads REQUIRED)
  set_target_properties(Threads::Threads PROPERTIES THREADS_PREFER_PTHREAD_FLAG TRUE)
  list(APPEND cdi_compile_defs HAVE_PTHREAD=1)
  list(APPEND cdi_linked_libs pthread)
endif()

# enable default internal libs
option(CDI_LIBGRIB "GRIB support [default=ON]" ON)
if(${CDI_LIBGRIB})
  list(APPEND cdi_compile_defs
       HAVE_LIBGRIB=1
       HAVE_LIBGRIB_API=1)
endif()

option(CDI_LIBGRIBEX "Use the CGRIBEX library [default=ON]" ON)
if(${CDI_LIBGRIBEX})
  list(APPEND cdi_compile_defs LIBCGRIBEX=1)
endif()

option(CDI_EXTRA "Use the extra library [default=ON]" ON)
if(${CDI_EXTRA})
  list(APPEND cdi_compile_defs HAVE_LIBEXTRA=1)
endif()

option(CDI_IEG "Use the extra library [default=ON]" ON)
if(${CDI_IEG})
  list(APPEND cdi_compile_defs HAVE_LIBIEG=1)
endif()

option(CDI_SERVICE "Use the extra library [default=ON]" ON)
if(${CDI_SERVICE})
  list(APPEND cdi_compile_defs HAVE_LIBSERVICE=1)
endif()

# ecCodes
option(CDI_ECCODES "Use the eccodes library [default=ON]" ON)
if(${CDI_ECCODES} OR eccodes_ROOT)
  find_package(eccodes REQUIRED)
  if (${eccodes_FOUND})
    list(APPEND cdi_linked_libs eccodes)
  endif ()
endif()

# NetCDF
option(CDI_NETCDF "Use the netcdf library [default=ON]" ON)
if(${CDI_NETCDF} OR netCDF_ROOT )
  find_package(netCDF COMPONENTS C REQUIRED)
  if (TARGET netCDF::netcdf)
    list(APPEND cdi_compile_defs
      HAVE_LIBNETCDF=${netCDF_FOUND}
      HAVE_LIBNC_DAP=${netCDF_FOUND}
      HAVE_NETCDF4=${netCDF_FOUND}
    )
    list(APPEND cdi_linked_libs netCDF::netcdf)
  endif ()
endif()

message(VERBOSE "looking for config.h in: ${PROJECT_BINARY_DIR}/src")

#adding subdirectories
## lib and general files
list(APPEND cdi_compile_defs CDI=1 CDI_SIZE_TYPE=size_t PACKAGE_NAME="${PROJECT_NAME}" VERSION="${CMAKE_PROJECT_VERSION}")
add_subdirectory(src)

#app
option(CDI_BUILD_APP "Build the app" ON)
if (CDI_BUILD_APP)
  add_subdirectory(app)
endif ()

#tests
if (BUILD_TESTING)
  add_subdirectory(tests)
endif ()
