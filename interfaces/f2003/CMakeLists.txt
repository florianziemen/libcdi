find_package(Ruby REQUIRED)

set(GEN_FILE ${CMAKE_CURRENT_BINARY_DIR}/mo_cdi.f90)
set(CDI_INTERFACE_FILE ${PROJECT_SOURCE_DIR}/src/cdi.h)

add_custom_command(
  OUTPUT ${GEN_FILE}
  COMMAND ${RUBY_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/bindGen.rb
  ${CDI_INTERFACE_FILE}
  ${GEN_FILE}
  DEPENDS
  ${CDI_INTERFACE_FILE}
  ${CMAKE_CURRENT_SOURCE_DIR}/bindGen.rb
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating Fortran source mo_cdi.f90"
  VERBATIM
)

add_custom_target(generate_modules ALL
    DEPENDS ${GEN_FILE}
)
