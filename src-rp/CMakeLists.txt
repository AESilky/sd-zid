# SilkyDESIGN RP2040 Retro Module primary project build (CMake) file.
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_PLATFORM "rp2040-arm-s")
set(PICO_BOARD pico)  # Regular RP2040 (Pico)
#set(PICO_BOARD pico2)  # RP2350 (Pico2)
#set(PICO_BOARD adafruit_kb2040)  # Adafruit RP2040 Kee Boar
#set(PICO_BOARD waveshare_rp2040_zero)  # Waveshare RP2040 Zero

set(PICO_USE_MALLOC_MUTEX 1)
set(PICO_QUEUE_MAX_LEVEL 1)
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
    set(PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE 1)    # Go to Boot Loader is BAUD goes to 1200
endif()

# Don't enable the UART STDIO globally. Enabling is handled in 'debug_support' in the code.
set(PICO_STDIO_UART 0)
# Don't enable the USB STDIO globally. Enabling is handled in 'debug_support' in the code.
set(PICO_STDIO_USB 0)


project(DOC_SBC C CXX ASM)


add_compile_options(
  -Wall
  -Wno-format               # int != int32_t as far as the compiler is concerned
  -Wno-unused-function
  -Wno-maybe-uninitialized
  #-H                       # list include header paths
)
add_compile_definitions(
  PICO_INCLUDE_RTC_DATETIME=1
  PICO_MALLOC_PANIC
  PICO_USE_MALLOC_MUTEX
  PICO_QUEUE_MAX_LEVEL
  PICO_USE_STACK_GUARDS
  PICO_STACK_SIZE=4096
  PICO_CORE1_STACK_SIZE=4096
  #PICO_DEBUG_MALLOC
  PICO_MAX_SHARED_IRQ_HANDLERS=6u
  PICO_STDIO_USB_CONNECTION_WITHOUT_DTR
  PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
  #DEBUG_TRACE_ENABLE
)

# Initialize the SDK
pico_sdk_init()

set(VERSION "1.0")
set(NAME_BASE "DOCSBC")

#
# Top-level sources
#
set(TL_SRC
main.c
board.c
multicore.c
pio_sm.c
util.c
)

#
# Add executable (output) with Debug Mode enabled by default
#
add_executable(docsbc
  ${TL_SRC}
)
#
# Add executable (output) with Debug Mode enabled by default
#
add_executable(docsbc_db
  ${TL_SRC}
)
#
# Add executable (output) for Stand-Alone Test with Debug Mode enabled by default
#   We only build 'Stand-Alone Test' in debug mode, as it is only used for testing.
#
add_executable(docsbc_sat
  ${TL_SRC}
)

# Use 'target_include_directories` rather than `include_directories` except for the main include dir.
include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_include_directories(docsbc BEFORE PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_include_directories(docsbc_db BEFORE PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_include_directories(docsbc_sat BEFORE PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

set(PICO_INCLUDES
)

# Include paths for the entire application
set(APP_INCLUDES
  ${CMAKE_CURRENT_LIST_DIR}/calculator/include
  ${CMAKE_CURRENT_LIST_DIR}/cmt/include
  $(CMAKE_CURRENT_LIST_DIR)/dbusc/include
  ${CMAKE_CURRENT_LIST_DIR}/debugging/include
  ${CMAKE_CURRENT_LIST_DIR}/dc/include
  ${CMAKE_CURRENT_LIST_DIR}/hwrt/include
  ${CMAKE_CURRENT_LIST_DIR}/number/include
  ${CMAKE_CURRENT_LIST_DIR}/picohlp/include
)

# Our Modules (direct functionality)
set(CORE_LIBS
  calculator
  cmt
  debugging
  app
  hwrt
  picoutil
  dbusc
  dbusc_cmd
  debug_cmd
  dc
  dc_cmd
  pico_cmd
)

#
# Pico Libraries/Modules
set(PICO_LIBS
  hardware_adc
  hardware_clocks
  hardware_dma
  hardware_exception
  hardware_i2c
  hardware_pio
  hardware_rtc
  hardware_spi
  hardware_timer
  pico_multicore
  pico_status_led
  pico_stdlib
  pico_stdio_uart
  pico_stdio_usb
)

#
# Configuration - Debug Mode disabled by default
#
pico_set_program_name(docsbc ${NAME_BASE})
pico_set_program_version(docsbc ${VERSION})
target_compile_definitions(docsbc PUBLIC
  DEBUG_MODE=0
  SHELL_ENABLE
  SHELL_NOCASE
)
# Add the required include file paths for this module
target_include_directories(docsbc PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${APP_INCLUDES}
  ${PICO_INCLUDES}
)
# Link in the Modules/Libraries for the top-level
target_link_libraries(docsbc PUBLIC
  ${CORE_LIBS}
  ${PICO_LIBS}
  ${EXT_LIBS}
)
pico_set_linker_script(docsbc ${CMAKE_SOURCE_DIR}/memmap_2040_cstm.ld)
pico_add_extra_outputs(docsbc)


#
# Configuration - Debug Mode enabled by default
#
pico_set_program_name(docsbc_db "${NAME_BASE}_DB")
pico_set_program_version(docsbc_db ${VERSION})
target_compile_definitions(docsbc_db PUBLIC
  DEBUG_MODE=1
  SHELL_ENABLE
  SHELL_NOCASE
)
# Add the required include file paths for this module
target_include_directories(docsbc_db PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${APP_INCLUDES}
  ${PICO_INCLUDES}
)
# Link in the Modules/Libraries for the top-level
target_link_libraries(docsbc_db
  ${CORE_LIBS}
  ${PICO_LIBS}
  ${EXT_LIBS}
)
pico_set_linker_script(docsbc_db ${CMAKE_SOURCE_DIR}/memmap_2040_cstm.ld)
pico_add_extra_outputs(docsbc_db)

#
# Configuration - Stand-Alone Test, (Debug Mode enabled by default)
#
pico_set_program_name(docsbc_sat "${NAME_BASE}_SAT")
pico_set_program_version(docsbc_sat ${VERSION})
target_compile_definitions(docsbc_sat PUBLIC
	DEBUG_MODE=1
    SATEST
    SHELL_ENABLE
    SHELL_NOCASE
)
# Add the required include file paths for this module
target_include_directories(docsbc_sat PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${APP_INCLUDES}
  ${PICO_INCLUDES}
)
# Link in the Modules/Libraries for the top-level
target_link_libraries(docsbc_sat
  ${CORE_LIBS}
  ${PICO_LIBS}
  ${EXT_LIBS}
)
pico_set_linker_script(docsbc_sat ${CMAKE_SOURCE_DIR}/memmap_2040_cstm.ld)
pico_add_extra_outputs(docsbc_sat)


#
# Build the subdirectories
#
add_subdirectory(calculator)
add_subdirectory(cmt)
add_subdirectory(dbusc)
add_subdirectory(debugging)
add_subdirectory(dc)
add_subdirectory(hwrt)
add_subdirectory(number)
add_subdirectory(picohlp)
#
add_subdirectory(app)
add_subdirectory(dskops)

#
# External libraries
set(EXT_LIBS
)
add_subdirectory(lib)
