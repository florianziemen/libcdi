if(${CDI_IEG})
  target_compile_definitions(libcdi PRIVATE HAVE_LIBIEG=1)
endif()
