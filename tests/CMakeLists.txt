
set(CMAKE_POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
if(${CDI_PIO})
  add_executable(calendar_test1  calendar_test1.c )
  add_executable(cksum_read
    cksum_read.c
    ensure_array_size.c
    ensure_array_size.h
    stream_cksum.c
    stream_cksum.h
    var_cksum.c
    var_cksum.h
  )
  add_executable(cksum_verify cksum_verify.c)
  add_executable(cksum_write
    cksum_write.c
    simple_model_helper.c
    simple_model_helper.h)
  add_executable(cksum_write_chunk
    cksum_write_chunk.c
    simple_model_helper.c
    simple_model_helper.h)
  add_executable(pio_write.run
    pio_write.c
    pio_write.h
    pio_write_setup_grid.c
    pio_write_setup_grid.h
    simple_model.c
    simple_model_helper.c
    simple_model_helper.h)

  add_executable(pio_write_parallel.run
    pio_write.c
    pio_write.h
    pio_write_setup_grid.c
    pio_write_setup_grid.h
    simple_model.c
    simple_model_helper.c
    simple_model_helper.h
  )
  target_compile_definitions(pio_write_parallel.run PRIVATE USE_MPI=1)
  target_link_libraries(pio_write_parallel.run PRIVATE libcdipio)

  add_executable(pio_write_deco2d.run
    deco2d_model.c
    pio_write.c
    pio_write.h
    pio_write_setup_grid.c
    pio_write_setup_grid.h
    simple_model_helper.c
    simple_model_helper.h
  )

  add_executable(pio_write_deco2d_parallel.run
    deco2d_model.c
    pio_write.c
    pio_write.h
    pio_write_setup_grid.c
    pio_write_setup_grid.h
    simple_model_helper.c
    simple_model_helper.h
  )
  target_compile_definitions(pio_write_deco2d_parallel.run PRIVATE USE_MPI=1)
  target_link_libraries(pio_write_deco2d_parallel.run PRIVATE libcdipio)

  add_executable(test_byteswap test_byteswap.c)
  add_executable(test_grib test_grib.c)
  add_executable(test_month_adjust test_month_adjust.c)

  add_executable(test_resource_copy test_resource_copy.c)
  target_link_libraries(test_resource_copy PRIVATE libcdiresunpack)

  add_executable(test_resource_copy_parallel test_resource_copy.c)
  target_compile_definitions(test_resource_copy_parallel PRIVATE USE_MPI=1)
  target_link_libraries(test_resource_copy_parallel PRIVATE  libcdipio libcdiresunpack)

  add_executable(test_table test_table.c)

  add_executable(test_cdf_read test_cdf_read.c)

  add_executable(test_cdf_write
    simple_model_helper.c
    simple_model_helper.h
    test_cdf_write.c)


  get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
  #list(APPEND test_lib_src
  #  var_cksum.c
  #  ensure_array_size.c
  #  stream_cksum.c
  #  simple_model.c
  #  simple_model_helper.c
  #  pio_write_setup_grid.c
  #)

  #add_library(test_lib ${test_lib_src})
  #target_include_directories(test_lib PRIVATE ../src/)
  #target_link_libraries(test_lib PRIVATE cdilib ${cdi_linked_libs})
  #
  foreach(target ${current_targets})
    message(${target})
    target_include_directories(${target} PRIVATE ../src/)
    #set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
    target_link_libraries(${target} PRIVATE cdilib)
    add_test(NAME ${target} COMMAND ${target})
    if(labels_${target})
      set_property(TEST ${target} PROPERTY LABELS labels_${target})
    endif()
  endforeach()
endif()
