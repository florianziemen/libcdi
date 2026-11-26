# Consumer-side smoke test for an installed libcdi.
#
# Uses the library the way ParaView's CDIReader plugin does:
# find_package(CDI 2.1.0), link CDI::CDI, include cdi.h from C++ with
# CDI_SIZE_TYPE=size_t. Configure with CMAKE_PREFIX_PATH=<libcdi install prefix>.
cmake_minimum_required(VERSION 3.27)
project(cdi_smoke LANGUAGES CXX)

find_package(CDI 2.1.0 REQUIRED)
message(STATUS "Found CDI ${CDI_VERSION}: ${CDI_LIBRARIES}")

add_executable(cdi_roundtrip cdi_roundtrip.cxx)
target_compile_features(cdi_roundtrip PRIVATE cxx_std_17)
target_link_libraries(cdi_roundtrip PRIVATE CDI::CDI)

enable_testing()
foreach (format IN ITEMS nc nc4)
  add_test(NAME roundtrip_${format} COMMAND cdi_roundtrip ${format})
  # Find cdi.dll (installed to <prefix>/bin) when running on Windows.
  set_tests_properties(roundtrip_${format} PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CDI_ROOT_DIR}/bin")
endforeach ()
