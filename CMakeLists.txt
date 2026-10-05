cmake_minimum_required( VERSION 3.27 FATAL_ERROR )

message(VERBOSE "Entering libcdi")
project(libcdi VERSION 2.6.5 LANGUAGES C )
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)


list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(CTest)
include(CheckIncludeFile)

option(CDI_PTHREAD "Use the pthread library [default=ON]" ON)
option(CDI_LIBGRIB "GRIB support [default=ON]" ON)
option(CDI_LIBGRIBEX "Use the CGRIBEX library [default=ON]" ON)
option(CDI_EXTRA "Use the extra library [default=ON]" ON)
option(CDI_IEG "Use the extra library [default=ON]" ON)
option(CDI_SERVICE "Use the extra library [default=ON]" ON)
option(CDI_ECCODES "Use the eccodes library [default=ON]" ON)
option(CDI_NETCDF "Use the netcdf library [default=ON]" ON)
option(CDI_BUILD_APP "Build the app" ON)
option(CDI_ENABLE_f2003_INTERFACE "Enable fortran 2003 interface (requires ruby) [default=ON]" ON)

add_subdirectory(src)

if(CDI_ENABLE_f2003_INTERFACE)
  add_subdirectory(interfaces/f2003)
endif()

if (CDI_BUILD_APP)
  add_subdirectory(app)
endif ()

if (BUILD_TESTING)
  add_subdirectory(tests)
endif ()
