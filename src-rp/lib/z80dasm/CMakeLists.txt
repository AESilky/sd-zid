# Library: Z80 Disassembler (obj only)
#
add_library(z80dasm INTERFACE)

target_sources(z80dasm INTERFACE
  z80disasm.c
  z80disstr.c
)

target_include_directories(z80dasm INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
# Link in the Modules/Libraries for the library
target_link_libraries(z80dasm INTERFACE
)
