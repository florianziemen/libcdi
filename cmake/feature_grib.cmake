if (CDI_LIBGRIB)
  target_compile_definitions(libcdi PRIVATE HAVE_LIBGRIB=1 HAVE_LIBGRIB_API=1)
endif ()
