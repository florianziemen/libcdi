if(${CDI_SERVICE})
  target_compile_definitions(libcdi PRIVATE HAVE_LIBSERVICE=1)
endif()
