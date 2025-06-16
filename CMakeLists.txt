cmake_minimum_required( VERSION 3.27 FATAL_ERROR )
message(VERBOSE "Entering cdilib")

include(CMakePackageConfigHelpers)
include(CTest)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(GNUInstallDirs)

project(cdilib VERSION 2.5.1 LANGUAGES C )
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

add_library(cdilib)
add_library(cdilib::cdilib ALIAS cdilib)
set_target_properties(cdilib PROPERTIES
  POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)


option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(CDI_PIO  "Use CDI-PIO" OFF)
option(CDI_YAXT "Use the yaxt library" OFF)
option(CDI_ENABLE_CF_INTERFACE "TODO" OFF)
option(CDI_IEG "Use the extra library [default=ON]" ON)
option(CDI_LIBGRIB "GRIB support [default=ON]" ON)
option(CDI_PTHREAD "Use the pthread library [default=ON]" ON)
option(CDI_LIBGRIBEX "Use the CGRIBEX library [default=ON]" ON)
option(CDI_EXTRA "Use the extra library [default=ON]" ON)
option(CDI_SERVICE "Use the extra library [default=ON]" ON)
option(CDI_ECCODES "Use the eccodes library [default=ON]" ON)
option(CDI_NETCDF "Use the netcdf library [default=ON]" ON)
option(CDI_BUILD_APP "Build the app" ON)

mark_as_advanced(CDI_PIO)
mark_as_advanced(CDI_YAXT)
mark_as_advanced(CDI_ENABLE_CF_INTERFACE)

if(${CDI_PIO})
  add_library(libcdipio)

  find_package(MPI REQUIRED)
  target_link_libraries(libcdipio PRIVATE mpi)

  check_function_exists(mallinfo MALLINFO_FOUND)
  target_compile_definitions(libcdipio PRIVATE HAVE_MALLINFO=${MALLINFO_FOUND})
  check_function_exists(pwrite PWRITE_FOUND)
  target_compile_definitions(libcdipio PRIVATE HAVE_PWRITE=${PWRITE_FOUND})
  check_function_exists(timegm TIMEGM_FOUND)
  target_compile_definitions(libcdipio PRIVATE HAVE_TIMEGM=${TIMEGM_FOUND})

  # Finding libraries
  ### -------------- Yaxt ------------------------------
  if(${CDI_YAXT})
    include(CheckTypeSize)
    find_package(YAXT REQUIRED)
    SET(CMAKE_EXTRA_INCLUDE_FILES "mpi.h" "yaxt.h")
    check_type_size(Xt_uid YAXT_XT_UID LANGUAGE C)
    check_type_size(MPI_UINT64_T MPI_64_UINT LANGUAGE C)
    message(INFO ${MPI_UINT64_T})
    if(HAVE_MPI_64_UINT) #TODO remove NOT
      target_compile_definitions(libcdipio PRIVATE YAXT_UID_DT=MPI_UINT64_T)
    else()
      check_type_size(MPI_UNSIGNED_LONG MPI_ULONG LANGUAGE C)
      if(${YAXT_XT_UID} EQUAL ${MPI_ULONG} )
        target_compile_definitions(libcdipio PRIVATE YAXT_UID_DT=MPI_UNSIGNED_LONG)
      else()
        check_type_size(MPI_UNSIGNED_LONG_LONG MPI_ULONGLONG LANGUAGE C)
        if(${YAXT_XT_UID}  EQUAL ${MPI_ULONGLONG} )
          target_compile_definitions(libcdipio PRIVATE YAXT_UID_DT=MPI_UNSIGNED_LONG_LONG)
        endif()
      endif()
    endif()
    SET(CMAKE_EXTRA_INCLUDE_FILES)
  endif()
endif()
### -------------- Pthread ---------------------------

if(${CDI_PTHREAD})
  include(FindThreads)
  find_package(Threads REQUIRED)
  set_target_properties(Threads::Threads PROPERTIES THREADS_PREFER_PTHREAD_FLAG TRUE)
  target_compile_definitions(cdilib PRIVATE HAVE_LIBPTHREAD=1)
  target_link_libraries(cdilib pthread)
endif()

# enable default internal libs
if(${CDI_LIBGRIB})
  target_compile_definitions(cdilib PRIVATE HAVE_LIBGRIB=1 HAVE_LIBGRIB_API=1)
endif()

if(${CDI_LIBGRIBEX})
  target_compile_definitions(cdilib PRIVATE LIBCGRIBEX=1)
endif()

if(${CDI_EXTRA})
  target_compile_definitions(cdilib PRIVATE HAVE_LIBEXTRA=1)
endif()

if(${CDI_IEG})
  target_compile_definitions(cdilib PRIVATE HAVE_LIBIEG=1)
endif()

if(${CDI_SERVICE})
  target_compile_definitions(cdilib PRIVATE HAVE_LIBSERVICE=1)
endif()

# ecCodes
if(${CDI_ECCODES} OR eccodes_ROOT)
  find_package(eccodes REQUIRED)
  if (${eccodes_FOUND})
    target_compile_definitions(cdilib PRIVATE HAVE_LIBGRIB_API=${eccodes_FOUND})
    message(VERBOSE "added compile definition HAVE_LIBGRIB_API=${eccodes_FOUND}")
    target_link_libraries(cdilib eccodes)
  endif ()
endif()

# NetCDF

if(${CDI_NETCDF} OR netCDF_ROOT )
  find_package(netCDF COMPONENTS C REQUIRED)
  if (TARGET netCDF::netcdf)
    list(APPEND cdi_compile_defs
      HAVE_LIBNETCDF=${netCDF_FOUND}
      HAVE_LIBNC_DAP=${netCDF_FOUND}
      HAVE_NETCDF4=${netCDF_FOUND}
    )
    target_link_libraries(cdilib netCDF::netcdf)
  endif ()
endif()

message(VERBOSE "looking for config.h in: ${PROJECT_BINARY_DIR}/src")

#adding subdirectories
## lib and general files
target_compile_definitions(cdilib PRIVATE CDI=1 CDI_SIZE_TYPE=size_t PACKAGE_NAME="${PROJECT_NAME}" VERSION="${CMAKE_PROJECT_VERSION}")
add_subdirectory(src)

#app
if (CDI_BUILD_APP)
  add_subdirectory(app)
endif ()

#tests
if (BUILD_TESTING)
  add_subdirectory(tests)
endif ()

# --- Installation

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_INSTALL_LIBDIR}/cdi)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_LIBDIR}/cdi)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_BINDIR})


get_target_property(cdi_install_files cdilib CDI_INSTALL_HEADERS)
install(TARGETS cdilib
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cdi)
install(TARGETS cdilib EXPORT cdilibTargets DESTINATION "${CMAKE_INSTALL_LIBDIR}/cdi")
install(EXPORT cdilibTargets FILE cdilibTargets.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cdilib)

# -- create configuration files
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/cmake/cdi/cdi-config.cmake.in" "${CMAKE_BINARY_DIR}/cmake/cdi-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/cdi")
write_basic_package_version_file(
  "${CMAKE_BINARY_DIR}/cmake/cdi-config-version.cmake"
  VERSION "${PACKAGE_VERSION}"
  COMPATIBILITY SameMajorVersion)

#Install CMake configuration files to
install(
  FILES
  "${CMAKE_BINARY_DIR}/cmake/cdi-config.cmake"
  "${CMAKE_BINARY_DIR}/cmake/cdi-config-version.cmake"
  DESTINATION
  "${CMAKE_INSTALL_LIBDIR}/cmake/cdi")
