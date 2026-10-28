# ##############################################################################
# A Logan Thomas Barnes project
# ##############################################################################

find_package(Git REQUIRED)

# Graphics API
find_package( Vulkan REQUIRED )

execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse HEAD:ltb-vlk
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE LTB_VLK_COMMIT_HASH
  RESULT_VARIABLE GIT_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (GIT_RESULT EQUAL 0)
  message(STATUS "ltb-vlk commit hash: ${LTB_VLK_COMMIT_HASH}")
else()
  message(FATAL_ERROR "Failed to get ltb-vlk commit hash.")
endif ()

cpmaddpackage(
  NAME
  LtbVlk
  GITHUB_REPOSITORY
  LoganBarnes/ltb-vlk
  GIT_TAG
  ${LTB_VLK_COMMIT_HASH}
)

# Macos:
# brew install pkg-config
# brew install libusb
#
# Ubuntu:
# sudo apt install pkg-config libusb-1.0-0-dev
cpmaddpackage(
  NAME
  hackrf
  GITHUB_REPOSITORY
  greatscottgadgets/hackrf
  GIT_TAG
  v2026.01.3
  SOURCE_SUBDIR host
)

if (${LTB_RADIO_BUILD_TESTS})
  cpmaddpackage(
    NAME
    googletest
    GITHUB_REPOSITORY
    google/googletest
    GIT_TAG
    v1.15.2
    OPTIONS
    "INSTALL_GTEST OFF"
    "gtest_force_shared_crt ON"
  )

  # Enable testing
  include(CTest)
endif ()
