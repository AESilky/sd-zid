# SilkyDESIGN Shell, Command Processor, and Terminal Interface for Raspberry Pi Pico micro-controllers.
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
if (NOT DEFINED PICO_BOARD)
  set(PICO_BOARD pico)  # Regular RP2040 (Pico)
  #set(PICO_BOARD pico2)  # RP2350 (Pico2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Building in Debug mode. (EAS)")
  # Use fully-deoptimized debug build for true single-step and data viewing.
  set(PICO_DEOPTIMIZED_DEBUG 1)
  # Add debug-specific compiler flags and definitions
  add_compile_options(
    -O0     # No optimizations
  )
  add_compile_definitions(DEBUG_MODE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "Building in Release mode. (EAS)")
  set(PICO_DEOPTIMIZED_DEBUG 0)
  add_compile_options(
    -O2     # Optimize
  )
  add_compile_definitions(NDEBUG)
else()
    message(STATUS "Building with unknown configuration: ${CMAKE_BUILD_TYPE}")
endif()

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.2.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.2.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# For debug output from USB (pass -DPICO_STDIO_USB=1) this ensures we don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 0)
endif()
if (NOT DEFINED PICO_STDIO_USB_POST_CONNECT_WAIT_DELAY_MS)
    set(PICO_STDIO_USB_POST_CONNECT_WAIT_DELAY_MS 0)
endif()
if (NOT DEFINED PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE)
    set(PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE 1)    # Go to Boot Loader if BAUD goes to 1200
endif()

# If the PICO STDIO isn't defined default to USB
if (NOT DEFINED PICO_STDIO_UART AND NOT DEFINED PICO_STDIO_USB)
  set(PICO_STDIO_UART 0)
  set(PICO_STDIO_USB 1)
endif()

project(SDL_PSHELL C CXX ASM)

add_compile_options(
  -Wall
  -Wno-format               # int != int32_t as far as the compiler is concerned
  -Wno-unused-function
  -Wno-maybe-uninitialized
  #-H                       # list include header paths
)
add_compile_definitions(
  PICO_MALLOC_PANIC
  PICO_USE_MALLOC_MUTEX
  PICO_STDIO_USB_CONNECTION_WITHOUT_DTR
  PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
  #DEBUG_TRACE_ENABLE
)

# Initialize the SDK
pico_sdk_init()

if (NOT DEFINED NAME_BASE)
  set(VERSION "1.0")
  set(NAME_BASE "SDL_PSHELL")
endif()

add_subdirectory(src)
add_subdirectory(test)
