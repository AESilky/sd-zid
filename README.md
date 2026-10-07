# Z80 Utilities (primarily a Disassembler)

Structured to be used as a Git Module, this repository contains various Z80 utilities, with the primary utility being the Disassembler.

## Disassembler

The disassembler processes Z80 opcode and additional memory reads into Z80 source. The process is controlled by an external routine,
meaning that the disassembler accepts byte values from the external routine and indicates if more bytes are needed. It does not access
memory or file data on its own. This makes it extremely flexible in how it is integrated into a system or utility program.

A test program is provided that will feed all instructions to the disassembler and output the result to standard output. It also
accepts command line options to just disassemble selected instruction groups (single-byte, CB, DD, ED, or FD) or use byte data included
on the command line.

The test program can be used as an example to integrate the disassembler into a system.
