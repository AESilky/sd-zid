cmake_minimum_required(VERSION 3.13.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Building in Debug mode. (EAS)")
  # Use de-optimized debug build for true single-step and data viewing.
  # Use optimized debug build to avoid timing problems.
  # Add debug-specific compiler flags and definitions
  add_compile_options(
    -O0     # No optimizations
  )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "Building in Release mode. (EAS)")
  add_compile_options(
    -O2     # Fully Optimize
  )
  add_compile_definitions(NDEBUG)
else()
    message(STATUS "Building with unknown configuration: ${CMAKE_BUILD_TYPE}")
endif()

set(VERSION "1.0")
set(NAME_BASE "z80disasm")


project(z80disasm VERSION ${VERSION} LANGUAGES C)

add_compile_options(
  -Wall
  -Wno-format               # int != int32_t as far as the compiler is concerned
  -Wno-uninitialized
  #-H                       # list include header paths
)
add_compile_definitions(
  XYZZY=42
)


# Use 'target_include_directories` rather than `include_directories` except for the main include dir.
include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/src/include
)

#
# Build the subdirectories
#
add_subdirectory(src)
add_subdirectory(test)
