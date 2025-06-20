add_executable(cdi_append cdi_append.c)
add_executable(cdi_copy cdi_copy.c)
add_executable(cdi_read cdi_read.c)
add_executable(cdi_write cdi_write.c)
add_executable(cdi_write_ens cdi_write_ens.c)
add_executable(cdi_write_hybrid cdi_write_hybrid.c)
add_executable(cdi_write_local cdi_write_local.c)
add_executable(cdi_write_relativ cdi_write_relativ.c)
add_executable(cdi_write_const cdi_write_const.c)

get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
foreach(target ${current_targets})
  #target_include_directories(${target} PRIVATE
  #  ../../libcdi/interfaces/
  #  ../../libcdi/src/)
  target_link_libraries(${target} PRIVATE cdilib)
endforeach()
