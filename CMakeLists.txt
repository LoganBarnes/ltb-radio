# ##############################################################################
# A Logan Thomas Barnes project
# ##############################################################################
cmake_minimum_required( VERSION 3.23 )
project( LtbRadio VERSION 0.0.1 )

# Prevent cyclic dependencies even for static libs
set( GLOBAL_DEPENDS_NO_CYCLES ON )

# Prevent cmake from creating build artifacts in the source directory
set( CMAKE_DISABLE_IN_SOURCE_BUILD ON )

# ##############################################################################
# Options
# ##############################################################################
option(
  LTB_RADIO_USE_STRICT_FLAGS
  "Use strict flags when building"
  OFF
)

# ##############################################################################
# CMake Package Manager
# ##############################################################################
include( cmake/CPM.cmake )

# ##############################################################################
# External Packages
# ##############################################################################
include( ${CMAKE_CURRENT_LIST_DIR}/cmake/ThirdParty.cmake )

# ##############################################################################
# Configuration
# ##############################################################################
# A directory where generated files can be stored and referenced
set( LTB_RADIO_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated )

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/src/ltb/ltb_radio_config.hpp.in
  ${LTB_RADIO_GENERATED_DIR}/ltb/radio/ltb_radio_config.hpp
)

add_subdirectory( ${CMAKE_CURRENT_LIST_DIR}/res/shaders )

# ##############################################################################
# Library
# ##############################################################################
file(
  GLOB_RECURSE
  LtbRadio_CORE_SOURCE_FILES
  LIST_DIRECTORIES
  false
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/src/ltb/radio/*.cpp
)

add_library(
  LtbRadio
  ${LtbRadio_CORE_SOURCE_FILES}
)
add_library(
  LtbRadio::LtbRadio
  ALIAS
  LtbRadio
)
target_link_libraries(
  LtbRadio
  PUBLIC
  # Shaders
  LtbRadio::generate_spirv
  # Graphics
  LtbVlk::LtbVlk
  # Radio
  HackRF::hackrf
)
target_include_directories(
  LtbRadio
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
  $<BUILD_INTERFACE:${LTB_RADIO_GENERATED_DIR}>
)
target_compile_features(
  LtbRadio
  PUBLIC
  cxx_std_23
)
target_compile_definitions(
  LtbRadio
  PUBLIC
  # Deal with windows warnings and macros.
  $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
  $<$<PLATFORM_ID:Windows>:NOMINMAX>
)
set_target_properties(
  LtbRadio
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
target_compile_options(
  LtbRadio
  PUBLIC
  # Ignore any headers using angle brackets on windows
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/experimental:external>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/external:anglebrackets>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/external:W0>
  # Hopefully temporary MSVC spdlog warning
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/wd4996>
)

# ##############################################################################
# Development Settings
# ##############################################################################
if ( LTB_RADIO_USE_STRICT_FLAGS )
  target_compile_options(
    LtbRadio
    PUBLIC
    # Strict warnings/errors with gcc and clang
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Werror>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wunused>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-pedantic-errors>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Winit-self>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wold-style-cast>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Woverloaded-virtual>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wsign-conversion>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wshadow>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wmissing-declarations>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wmissing-include-dirs>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-unknown-pragmas>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wformat-truncation>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wimplicit-float-conversion>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wfloat-conversion>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fstrict-aliasing>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wstrict-aliasing>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wcast-align>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wcast-align=strict>
    # Strict warnings/errors with msvc
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX>
  )
endif ()

# ##############################################################################
# Applications
# ##############################################################################
add_subdirectory( ${CMAKE_CURRENT_LIST_DIR}/src/ltb/apps )
