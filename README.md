# ZID Z80 In-circuit Debug Board

The Z80 In-circuit-Debugger is intended to provide in-circuit (hardware and software) debugging functionality 
for a Z80 based system.

## Features

The board plugs in in place of the Z80 and provides:

1. Breakpoint push-button switch. Generates the Z80 'special' reset signal when pressed.
2. Breakpoint hardware. Generates the Z80 'special' reset when selected conditions are met (see below for details)
3. LED indicators for the Z80 control signals (WAIT, HALT, RFSH, M1, MREQ, IORQ, RD, WR)

## Breakpoint Conditions

The breakpoint logic allows selecting:

1. Instruction fetch from a specific address
2. Instruction fetch from any address (used for single-step)
3. Memory read from a specific address
4. Memory write to a specific address
5. Any memory operation on a specific address
6. I/O read from a specific address (16 or 8 bit address)
7. I/O write to a specific address (16 or 8 bit address)
8. Any I/O operation on a specific address (16 or 8 bit address)

## High-Level Debug Monitor Description

This section provides a very high-level description of the Debug Monitor functionality. For complete information, see the ZID User Manual.

1. Display the CPU registers
2. Set the value of CPU registers that will be used when code is run
3. Display the contents of memory
4. Modify the contents of RAM memory
5. Display data from an I/O port
6. Write data to an I/O port
7. Run code and break:
   1. at one of 8 'soft' breakpoints (break location in RAM)
   2. at any single location
   3. when a memory write occurs to a specific address
   4. when a memory read occurs from a specific address
   5. when an I/O write occurs to a specific port (8 or 16 bit address)
   6. when an I/O read occurs from a specific port (8 or 16 bit address)
8. Single-step code (in RAM or ROM)
9. Perform operations that can be helpful in troubleshooting/repairing hardware problems, including:
   1. repetitive write of a specific data byte to a memory location
   2. repetitive read from a memory location *
   3. repetitive write of a specific data byte to a port
   4. repetitive read from a port
   5. run a memory test using a number of different approaches (data patterns)

### Z80 Special Reset Generation

The circuit to generate the Z80 special reset is from the Zilog patent, U.S. Patent 4,486,827. It is triggered by either the press of a push-button switch or the breakpoint condition met circuit.
