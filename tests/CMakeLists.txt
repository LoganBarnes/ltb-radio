# ##############################################################################
# A Logan Thomas Barnes project
# ##############################################################################
file(
  GLOB_RECURSE
  LtbRadio_TEST_SOURCE_FILES
  LIST_DIRECTORIES
  false
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

add_executable(
  ltb-radio-tests
  ${LtbRadio_TEST_SOURCE_FILES}
)
target_link_libraries(
  ltb-radio-tests
  PRIVATE
  LtbRadio::LtbRadio
  GTest::gtest_main
)

include( GoogleTest )
gtest_discover_tests( ltb-radio-tests )
