# Build out libraries contained in the subdirectories.
#

# Regular Expression support (mini)
add_subdirectory(regex)
# SD Card (through SPI) and FAT File System
add_subdirectory(sd_card)
# Shell (Terminal and Command processor)
add_subdirectory(shell/src)
# Z80 Disassembler
add_subdirectory(z80dasm)

