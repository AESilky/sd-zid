# Build out submodule libraries contained in the subdirectories.
#

# Regular Expression support (mini)
add_subdirectory(regex)
# SD Card (through SPI) and FAT File System
add_subdirectory(sd_card)
# Shell (Terminal and Command processor)
add_subdirectory(shell/src)
# Z80 Disassembler
add_subdirectory(z80dasm)

# String (const char*) keyed Map (submodule)
# Doesn't have a CMake build, so we implement it here
add_library(csmap INTERFACE)

target_sources(csmap INTERFACE
    Csmap/src/map.c
)

target_include_directories(csmap INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/Csmap/src
)

