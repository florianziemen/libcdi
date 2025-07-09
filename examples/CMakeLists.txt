list(C_EXAMPLES
  cdi_append.c
  cdi_copy.c
  cdi_read.c
  cdi_write.c
  cdi_write_ens.c
  cdi_write_hybrid.c
  cdi_write_local.c
  cdi_write_relativ.c
  cdi_write_const.c)

set(FORTRAN_EXAMPLES
  cdi_read_f2003.f90
  cdi_write_f2003.f90
)

# Loop over C executables
foreach(c_src ${C_EXAMPLES})
  get_filename_component(name ${c_src} NAME_WE)  # Get name without extension
  add_executable(${name} ${c_src})
  target_link_libraries(${name} PRIVATE cdilib)
  # Optional: target_compile_features(${name} PRIVATE c_std_99)
endforeach()

# Loop over Fortran executables
foreach(f_src ${FORTRAN_EXAMPLES})
  get_filename_component(name ${f_src} NAME_WE)
  set_source_files_properties(${f_src} PROPERTIES LANGUAGE Fortran)
  add_executable(${name} ${f_src})
  target_link_libraries(${name} PRIVATE libcdi_f2003)
endforeach()
