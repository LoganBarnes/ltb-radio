# ##############################################################################
# A Logan Thomas Barnes project
# ##############################################################################

find_package(Git REQUIRED)

# Graphics API
find_package( Vulkan REQUIRED )

# ltb-vlk is a git submodule. When it is checked out, build against the local
# copy so changes made there are picked up immediately. Otherwise download the
# commit the submodule pins.
if ( NOT DEFINED CPM_LtbVlk_SOURCE AND EXISTS ${PROJECT_SOURCE_DIR}/ltb-vlk/CMakeLists.txt )
  set( CPM_LtbVlk_SOURCE ${PROJECT_SOURCE_DIR}/ltb-vlk )
endif ()

if ( DEFINED CPM_LtbVlk_SOURCE )
  message( STATUS "ltb-vlk source: ${CPM_LtbVlk_SOURCE}" )
  set( LTB_VLK_COMMIT_HASH "local" )
else ()
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD:ltb-vlk
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE LTB_VLK_COMMIT_HASH
    RESULT_VARIABLE GIT_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if ( GIT_RESULT EQUAL 0 )
    message( STATUS "ltb-vlk commit hash: ${LTB_VLK_COMMIT_HASH}" )
  else ()
    message( FATAL_ERROR "Failed to get ltb-vlk commit hash." )
  endif ()
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

# Audio output
cpmaddpackage(
  NAME
  miniaudio
  GITHUB_REPOSITORY
  mackron/miniaudio
  GIT_TAG
  0.11.24
  DOWNLOAD_ONLY
  YES
)

if ( miniaudio_ADDED )
  # Built separately so the project's strict flags don't apply to it.
  add_library(
    miniaudio
    STATIC
    ${miniaudio_SOURCE_DIR}/miniaudio.c
  )
  add_library(
    miniaudio::miniaudio
    ALIAS
    miniaudio
  )
  target_include_directories(
    miniaudio
    SYSTEM
    PUBLIC
    $<BUILD_INTERFACE:${miniaudio_SOURCE_DIR}>
  )
  target_compile_definitions(
    miniaudio
    PUBLIC
    MA_NO_DECODING
    MA_NO_ENCODING
    MA_NO_GENERATION
    MA_NO_RESOURCE_MANAGER
    MA_NO_NODE_GRAPH
    MA_NO_ENGINE
  )
  if ( APPLE )
    target_link_libraries(
      miniaudio
      PUBLIC
      "-framework CoreFoundation"
      "-framework CoreAudio"
      "-framework AudioToolbox"
    )
  elseif ( UNIX )
    target_link_libraries(
      miniaudio
      PUBLIC
      pthread
      m
      dl
    )
  endif ()
endif ()

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
