# ZID Z80 In-circuit Debug Board

The Z80 In-circuit-Debugger is intended to provide in-circuit (hardware and software) debugging functionality
for a Z80 based system.

## Features

The debugger probe plugs in in place of the Z80. The debugger provides:

1. Breakpoint hardware. Stops code execution when specified conditions are met (see below for details)
2. One breakpoint includes the ability to check a data value, including Don't-Care bits.
3. LED indicators for key Z80 control signals (WAIT, HALT, BUSACK)

## Breakpoint Conditions

The breakpoint logic allows selecting:

1. Instruction fetch from a specific address
2. Memory read from a specific address
3. Memory write to a specific address
4. Any memory operation on a specific address
5. I/O read from a specific address (16 or 8 bit address)
6. I/O write to a specific address (16 or 8 bit address)
7. Any I/O operation on a specific address (16 or 8 bit address)

## High-Level Debug Monitor Description

This section provides a very high-level description of the Debug Monitor functionality. For complete information, see the ZID User Manual.

1. Display the CPU registers
2. Set the value of CPU registers that will be used when code is run
3. Display the contents of memory
4. Modify the contents of RAM memory
5. Display data from an I/O port
6. Write data to an I/O port
7. Run code and break when one of two specified conditions are met:
   1. Breakpoint One can be any of the conditions listed in the previous section, plus it includes the ability to specify a data value that must match what is written/read, including Don't-Care bits.
   2. Breakpoint Two through N share a condition that can be different from that of breakpoint One.
8. Single-step code (in RAM or ROM)
9. Perform operations that can be helpful in troubleshooting/repairing hardware problems, including:
   1. repetitive write of a specific data byte to a memory location
   2. repetitive read from a memory location *
   3. repetitive write of a specific data byte to a port
   4. repetitive read from a port
   5. run a memory test using a number of different approaches (data patterns)

