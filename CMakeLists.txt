# Library: Z80 Disassembler (obj only)
#
add_library(z80dis INTERFACE)

target_sources(z80dis INTERFACE
  z80disasm.c
  z80disstr.c
)

target_include_directories(z80dis INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/include
)
#
# Configuration - Release Mode - Debug disabled by default
#
target_compile_definitions(z80dis INTERFACE
  DEBUG_MODE=0
)
# Link in the Modules/Libraries for the library
target_link_libraries(z80dis INTERFACE
)
