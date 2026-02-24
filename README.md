# ZMB Debug Board

The Z80-InCircuit-Debugger is intended to provide in-circuit debugging functionality.

## Features

The board plugs in in place of the Z80 and provides:

1. Jumper selectable memory wait-state generation, 0 to 8 wait states on memory operations.
2. Jumper selectable I/O wait-state generation, 0 to 8 wait states on I/O operations.
3. Reset push-button switch for Board/System reset. Holds the board/system in reset as long as the switch is pressed.
4. Reset push-button switch for CPU only reset. Holds the CPU in reset as long as the switch is pressed.
5. Breakpoint push-button switch. Generates the Z80 'special' reset signal when pressed.
6. Breakpoint hardware. Generates the Z80 'special' reset when selected conditions are met (see below for details)
7. LED indicators for the Z80 control signals (WAIT, HALT, RFSH, M1, MREQ, IORQ, RD, WR)
8. Optional Der Blinkenlights for A0-23 and D0-7. These are powered by a USB-C (power only) connector.

## Breakpoint Conditions

The breakpoint logic allows selecting:

1. Instruction fetch from a specific address (24 or 16 bit address)
2. Instruction fetch from any address (used for single-step)
3. Memory read from a specific address (24 or 16 bit address)
4. Memory write to a specific address (24 or 16 bit address)
5. Any memory operation on a specific address (24 or 16 bit address)
6. I/O read from a specific address (16 or 8 bit address)
7. I/O write to a specific address (16 or 8 bit address)
8. Any I/O operation on a specific address (16 or 8 bit address)

When the condition is met the Z80 'special' reset is generated, then the PC address of the following (ignored) instruction fetch address is recorded. Along with the PC address, the breakpoint condition that was met is also recorded. The recorded address and breakpoint condition can be read using I/O operations.

## High-Level Debug Monitor Description

This section provides a very high-level description of the Debug Monitor functionality. For complete information, see the Mini-ZDB Manual.

The monitor uses the terminal input/output subsystem of the ZMB to communicate with the user. It works stand-alone and in conjunction with the ZMB Debug Board (ZDB) to:

1. Display the CPU registers
2. Set the value of CPU registers that will be used when code is run
3. Display the contents of memory (ZDB not required)
4. Modify the contents of RAM memory (ZDB not required)
5. Display data from an I/O port (ZDB not required)
6. Write data to an I/O port (ZDB not required)
7. Run code and break:
   1. at one of 10 'soft' breakpoints (break location in RAM, ZDB not required)
   2. at any single location (location in RAM or ROM, ZDB required)
   3. when a memory write occurs to a specific address (ZDB required)
   4. when a memory read occurs from a specific address (ZDB required)
   5. when an I/O write occurs to a specific port (8 or 16 bit address, ZDB required)
   6. when an I/O read occurs from a specific port (8 or 16 bit address, ZDB required)
8. Single-step code (in RAM or ROM, ZDB required)
9. Perform operations that can be helpful in troubleshooting/repairing hardware problems, including:
   1. repetitive write of a specific data byte to a memory location
   2. repetitive read from a memory location *
   3. repetitive write of a specific data byte to a port
   4. repetitive read from a port
   5. run a memory test using a number of different approaches (data patterns)
10. Write a data file (ROM image, or portion) to the Flash. The source of the image can be:
    1. A file on the SD Card
    2. Received from the host computer using the serial port
11. Save an image of the Flash (or portion of it), as a data file. The destination can be:
    1. A file on the SD Card
    2. Sent to the host computer using the serial port

## RC80 Bus Debug Board (ZDB) Key Operation Details

This section provides details of how the ZDB performs key operations.

### Wait-State Generation

The ZDB can introduce a selectable number of wait-states in memory and/or I/O operations. This can allow using expansion boards that are not capable of running at the speed of the ZMB, but weren't designed to request wait-states.

The circuit that generates the WAIT- signal is similar for both memory and I/O. Two identical circuits are used, one for memory and one for I/O.

1. MREQ-/IORQ- holds a shift register (of the applicable circuit) in reset while the signal isn't active, resulting in all 8 of the shift register outputs being low.
2. When the MREQ-/IORQ- signal becomes active, the shift register clocks a 1 in on each rising edge of the CPU clock.
3. As 1 is clocked in, it appears on the shift register outputs from 1-8.
4. A jumper is placed on 1 of 9 positions:
   1. GND, which never generates a WAIT- signal
   2. One of the 8 shift register outputs, which results in 1 wait state through 8 wait states being generated

### Z80 Special Reset Generation

The circuit to generate the Z80 special reset is from the Zilog patent, U.S. Patent 4,486,827. It is triggered by either the press of a push-button switch or the breakpoint condition met circuit.
