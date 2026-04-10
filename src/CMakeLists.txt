add_library(libcdi)
add_library(libcdi::libcdi ALIAS libcdi)

target_compile_definitions(libcdi PRIVATE CDI=1 CDI_SIZE_TYPE=size_t PACKAGE_NAME="${PROJECT_NAME}" VERSION="${CMAKE_PROJECT_VERSION}")

# Feature configuration
include(${CMAKE_SOURCE_DIR}/cmake/feature_eccodes.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/feature_grib.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/feature_ieg.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/feature_pthread.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/feature_extra.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/feature_gribex.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/feature_netcdf.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/feature_service.cmake)

target_sources(libcdi
  INTERFACE
  cdi.h
  PRIVATE
  async_worker.c
  async_worker.h
  basetime.c
  basetime.h
  binary.c
  binary.h
  calendar.c
  calendar.h
  cdf.c
  cdf.h
  cdf_config.h
  cdf_filter.c
  cdf_filter.h
  cdf_int.c
  cdf_int.h
  cdf_lazy_grid.c
  cdf_lazy_grid.h
  cdf_read.c
  cdf_records.c
  cdf_util.c
  cdf_util.h
  cdf_write.c
  cdi_across.c
  cdi_across.h
  cdi_att.c
  cdi_att.h
  cdi_cksum.c
  cdi_cksum.h
  cdi_datetime.c
  cdi_datetime.h
  cdi_error.c
  cdi_fdb.c
  cdi_fdb.h
  cdi_get_config.c
  cdi_int.c
  cdi_int.h
  cdi_key.c
  cdi_key.h
  cdi_limits.h
  cdi_query.c
  cdi_util.c
  cdi_uuid.h
  cgribex.h
  cgribexlib.c
  cksum.c
  cksum.h
  dmemory.c
  dmemory.h
  error.c
  error.h
  exse.h
  extra.h
  extralib.c
  file.c
  file.h
  gaussian_latitudes.c
  get_num_missvals.c
  get_num_missvals.h
  grb_read.c
  grb_write.c
  gribapi.c
  gribapi.h
  gribapi_utilities.c
  gribapi_utilities.h
  grid.c
  grid.h
  ieg.h
  ieglib.c
  input_file.c
  input_file.h
  institution.c
  institution.h
  iterator.c
  iterator.h
  iterator_fallback.c
  iterator_fallback.h
  iterator_grib.c
  iterator_grib.h
  julian_date.c
  julian_date.h
  model.c
  model.h
  namespace.c
  namespace.h
  normalize_month.h
  referenceCounting.c
  referenceCounting.h
  resource_handle.c
  resource_handle.h
  serialize.c
  serialize.h
  service.h
  servicelib.c
  stream.c
  stream_cdf.h
  stream_cdf_i.c
  stream_cdf_o.c
  stream_cdf_postdef.h
  stream_cdf_time.c
  stream_cgribex.c
  stream_cgribex.h
  stream_ext.c
  stream_ext.h
  stream_grb.c
  stream_grb.h
  stream_gribapi.c
  stream_gribapi.h
  stream_ieg.c
  stream_ieg.h
  stream_read.c
  stream_record.c
  stream_scan.c
  stream_scan.h
  stream_srv.c
  stream_srv.h
  stream_var.c
  stream_write.c
  subtype.c
  subtype.h
  swap.c
  swap.h
  table.c
  table.h
  tablepar.h
  taxis.c
  taxis.h
  tsteps.c
  util.c
  varscan.c
  varscan.h
  version.c
  vlist.c
  vlist.h
  vlist_var.c
  vlist_var.h
  vlist_var_key.c
  vlist_var_pack.c
  zaxis.c
  zaxis.h
)

option(CDI_BUILD_PIO "Build unknown sources in libcdi" OFF)
mark_as_advanced(CDI_BUILD_PIO)
if (CDI_BUILD_PIO)
  target_sources(libcdi PRIVATE
    cdipio.h
    pio.c
    pio.h
    pio_cdf_int.c
    pio_cdf_int.h
    pio_client.c
    pio_client.h
    pio_comm.c
    pio_comm.h
    pio_conf.c
    pio_conf.h
    pio_dbuffer.c
    pio_dbuffer.h
    pio_dist_grid.c
    pio_dist_grid.h
    pio_id_set.h
    pio_idxlist_cache.c
    pio_idxlist_cache.h
    pio_impl.h
    pio_interface.c
    pio_interface.h
    pio_mpi_fw_at_all.c
    pio_mpi_fw_at_reblock.c
    pio_mpi_fw_ordered.c
    pio_mpinonb.c
    pio_posixasynch.c
    pio_posixfpguardsendrecv.c
    pio_posixnonb.c
    pio_record_send.c
    pio_roles.c
    pio_rpc.c
    pio_rpc.h
    pio_serialize.c
    pio_serialize.h
    pio_server.c
    pio_server.h
    pio_util.c
    pio_util.h
    pio_xmap_cache.c
    pio_xmap_cache.h
  )
endif()

option(CDI_BUILD_UNKNOWN "Build unknown sources in libcdi" ON)
mark_as_advanced(CDI_BUILD_UNKNOWN)
if (CDI_BUILD_UNKNOWN)
  target_sources( libcdi PRIVATE
    cfortran.h
    getline.c
    make_fint.c
    resource_unpack.c
    resource_unpack.h
  )
endif ()

# Support exporting all symbols on Windows
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set_target_properties(libcdi PROPERTIES PUBLIC_HEADER cdi.h)

set_property(TARGET libcdi PROPERTY CDI_INSTALL_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/cdi.h
  ${CMAKE_CURRENT_SOURCE_DIR}/calendar.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cdi_datetime.h
  ${CMAKE_CURRENT_SOURCE_DIR}/julian_date.h
)

target_include_directories(libcdi
  PRIVATE
  ${CMARE_CURRENT_BINARY_DIR}
  PUBLIC
  # where top-level project will look for the library's public headers
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  # where external projects will look for the library's public headers
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_link_libraries(libcdi PUBLIC m)
target_compile_definitions(libcdi PRIVATE PACKAGE_NAME="${PROJECT_NAME}" VERSION="${CMAKE_PROJECT_VERSION}" ${cdi_compile_defs})

# --- Installation
# packages required for installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS libcdi
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

get_target_property(cdi_install_files libcdi CDI_INSTALL_HEADERS)
install(FILES ${cdi_install_files} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# -- create configuration files
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdi/cdi-config.cmake.in" "${CMAKE_BINARY_DIR}/cmake/cdi-config.cmake"
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
