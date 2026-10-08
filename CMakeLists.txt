cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

message(VERBOSE "Entering libcdi")
project(libcdi VERSION 2.6.5 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type of this tree" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(CTest)
include(FetchContent)

option(CDI_ECCODES   " Use the ECCODES library [default=ON] " ON)
option(CDI_EXTRA     " Use the EXTRA library [default=ON]   " ON)
option(CDI_LIBGRIB   " GRIB support [default=ON]            " ON)
option(CDI_LIBGRIBEX " Use the CGRIBEX library [default=ON] " ON)
option(CDI_NETCDF    " Use the NETCDF library [default=ON]  " ON)
option(CDI_IEG       " Use the IEG library [default=ON]     " ON)
option(CDI_PTHREAD   " Use the pthread library [default=ON] " ON)
option(CDI_SERVICE   " Use the SERVICE library [default=ON] " ON)

option(WITH_TENSORSTORE "Build with TensorStore support" OFF)

option(CDI_HIRLAM_EXTENSIONS " Enable hirlam extensions [default=OFF] " OFF)

option(CDI_BUILD_APP "Build the app" ON)
option(CDI_ENABLE_f2003_INTERFACE "Enable fortran 2003 interface (requires ruby) [default=ON]" ON)

include(config_checks)

add_subdirectory(src)

include(feature_eccodes)
include(feature_extra)
include(feature_grib)
include(feature_gribex)
include(feature_ieg)
include(feature_netcdf)
include(feature_pthread)
include(feature_service)

if (WITH_TENSORSTORE)
  # TensorStore's CMake bootstrap (bazel_to_cmake) needs Python >= 3.10, but
  # FindPython3 prefers the Xcode framework Python 3.9 on macOS.  Point it at
  # the newest python3 found on PATH unless the user set one explicitly.
  if (NOT Python3_EXECUTABLE)
    find_program(CDI_PYTHON3 NAMES python3.14 python3.13 python3.12 python3.11 python3.10 python3)
    if (CDI_PYTHON3)
      set(Python3_EXECUTABLE "${CDI_PYTHON3}"
                CACHE FILEPATH "Python interpreter used by TensorStore's CMake scripts")
    endif()
  endif()

  FetchContent_Declare(
        tensorstore
        URL https://github.com/google/tensorstore/archive/refs/tags/v0.1.85.tar.gz
        # URL https://github.com/google/tensorstore/archive/master.tar.gz
    )

  FetchContent_MakeAvailable(tensorstore)

  set(HAVE_TENSORSTORE 1)
endif()

if (CDI_HIRLAM_EXTENSIONS)
  set(HIRLAM_EXTENSIONS 1)
endif ()

if (CMAKE_C_COMPILER)
  set(C_COMPILER "${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS}")
  set(_configs "${CMAKE_BUILD_TYPE}")
  if (NOT _configs)
    set(_configs ${CMAKE_CONFIGURATION_TYPES})
  endif ()
  string(TOUPPER "${_configs}" _configs)
  set(C_COMPILER "${CMAKE_C_COMPILER}")
  foreach (_cfg IN LISTS _configs)
    string(APPEND C_COMPILER " ${CMAKE_C_FLAGS_${_cfg}}")
  endforeach ()
endif ()

if (CMAKE_C_COMPILER_VERSION)
  set(C_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}")
endif ()

if (CMAKE_CXX_COMPILER)
  set(CXX_COMPILER "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS}")
  set(_configs "${CMAKE_BUILD_TYPE}")
  if (NOT _configs)
    set(_configs ${CMAKE_CONFIGURATION_TYPES})
  endif ()
  string(TOUPPER "${_configs}" _configs)
  set(CXX_COMPILER "${CMAKE_CXX_COMPILER}")
  foreach (_cfg IN LISTS _configs)
    string(APPEND CXX_COMPILER " ${CMAKE_CXX_FLAGS_${_cfg}}")
  endforeach ()
endif ()

if (CMAKE_CXX_COMPILER_VERSION)
  set(CXX_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
endif ()

if (CMAKE_SYSTEM_NAME)
  set(SYSTEM_TYPE "${CMAKE_SYSTEM_NAME}${CMAKE_SYSTEM_VERSION}")
endif ()

configure_file(cmake/cdi_config.h.in src/config.h @ONLY)

if (CDI_ENABLE_f2003_INTERFACE)
  add_subdirectory(interfaces/f2003)
endif ()

if (CDI_BUILD_APP)
  add_subdirectory(app)
endif ()

if (BUILD_TESTING)
  add_subdirectory(tests)
endif ()
