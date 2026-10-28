# ##############################################################################
# A Logan Thomas Barnes project
# ##############################################################################
file(
  GLOB_RECURSE LtbRadio_SHADERS
  LIST_DIRECTORIES false
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/*.vert
  ${CMAKE_CURRENT_LIST_DIR}/*.geom
  ${CMAKE_CURRENT_LIST_DIR}/*.tesc
  ${CMAKE_CURRENT_LIST_DIR}/*.tese
  ${CMAKE_CURRENT_LIST_DIR}/*.frag
  ${CMAKE_CURRENT_LIST_DIR}/*.comp
)
file(
  GLOB_RECURSE LtbRadio_SHADER_INCLUDES
  LIST_DIRECTORIES false
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/*.glsl
)

add_library(
  ltb_cfd_generate_spirv
  INTERFACE
)
add_library(
  LtbRadio::generate_spirv
  ALIAS
  ltb_cfd_generate_spirv
)

# Generate the SPIR-V file for each shader
foreach( SHADER_FILE ${LtbRadio_SHADERS} )
  set( SPIRV_FILE ${SHADER_FILE}.spv )

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND $<TARGET_FILE:Vulkan::glslc>
    ARGS ${SHADER_FILE} -o ${SPIRV_FILE}
    DEPENDS ${SHADER_FILE} ${LtbRadio_SHADER_INCLUDES}
    VERBATIM
  )

  set_source_files_properties(
    ${SPIRV_FILE}
    PROPERTIES
    GENERATED TRUE
  )
  target_sources(
    ltb_cfd_generate_spirv
    PRIVATE
    ${SPIRV_FILE}
  )
endforeach()
