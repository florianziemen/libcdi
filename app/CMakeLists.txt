add_executable(cdi
  cdi.c printinfo.c
)

target_include_directories(cdi PRIVATE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)

target_compile_definitions(cdi PRIVATE HAVE_CONFIG_H=1)
target_include_directories(cdi PRIVATE "${PROJECT_BINARY_DIR}/src")
target_link_libraries(cdi PRIVATE libcdi)

if (WITH_TENSORSTORE)
  add_executable(tsq
    tsq.cpp
  )
  target_include_directories(tsq PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
  target_include_directories(tsq PRIVATE "${PROJECT_BINARY_DIR}/src")
  target_compile_definitions(tsq PRIVATE HAVE_CONFIG_H=1)
  target_link_libraries(tsq PRIVATE libcdi)
  #  target_link_libraries(tsq PRIVATE tensorstore::tensorstore tensorstore::all_drivers)
  target_compile_options(tsq PRIVATE -O3)
  target_compile_features(tsq PRIVATE cxx_std_23)
endif()
