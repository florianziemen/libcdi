# NetCDF
if (CDI_NETCDF OR netCDF_ROOT )
  find_package(NetCDF COMPONENTS C REQUIRED)
  if (TARGET netCDF::netcdf)
    list(APPEND cdi_compile_defs
      HAVE_LIBNETCDF=${netCDF_FOUND}
      HAVE_LIBNC_DAP=${netCDF_FOUND}
      HAVE_NETCDF4=${netCDF_FOUND}
    )
    target_link_libraries(libcdi PUBLIC netCDF::netcdf)
  elseif (TARGET NetCDF::NetCDF)
    list(APPEND cdi_compile_defs
           HAVE_LIBNETCDF=${NetCDF_FOUND}
           HAVE_LIBNC_DAP=${NetCDF_FOUND}
           HAVE_NETCDF4=${NetCDF_FOUND}
    )
    target_link_libraries(libcdi PUBLIC NetCDF::NetCDF)
  endif ()
endif ()
