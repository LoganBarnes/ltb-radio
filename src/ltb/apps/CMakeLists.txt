# ##############################################################################
# A Logan Thomas Barnes project
# ##############################################################################
function(ltb_make_app directory)
  file(
    GLOB_RECURSE
    ${directory}_app_SOURCE
    LIST_DIRECTORIES
    false
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/${directory}/*.cpp
  )

  add_executable(
    ${directory}-app
    ${${directory}_app_SOURCE}
  )
  target_link_libraries(
    ${directory}-app
    PRIVATE
    LtbRadio::LtbRadio
  )
endfunction()

ltb_make_app(radio)
