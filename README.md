# ZID - Z80 In-circuit Debugger (ICD) and Single Board Computer (SBC)

The Z80 In-circuit-Debugger is intended to provide in-circuit (hardware and software) debugging functionality
for a Z80 based system.

## Features (ICD)

The debugger probe plugs in in place of the Z80. The debugger provides:

1. 20MHz operation. This allows debugging high speed boards. The actual 'top-speed' of the target depends on how tight the timing of the target board is, but debugging 16MHz targets should be handled without problems. Some boards might need the clock speed reduced for debugging.
2. Compact probe that connects via a small 40-pin FFC/FPC cable
3. Four probe configurations that allow the cable to exit in different directions (probe in different orientations)
4. Breakpoint hardware. Stops code execution when specified conditions are met, even with code in read-only memory (see below for details)
5. Breakpoint hardware allows checking two different conditions. For example; Port Write and Instruction Fetch
6. Two breakpoint addresses with a condition that includes the ability to not only check the operation but also a data value, including Don't-Care bits.
7. Up to six other address units that share a condition (for example 6 different instruction fetch locations)
8. Breakpoint conditions being met can either stop target code execution or simply provide a SYNC signal (can be used to trigger an oscilloscope/analyzer)
9. External breakpoint trigger allows stopping target code execution when some signal is generated externally
10. LED indicators for key Z80 control signals (WAIT, HALT, BUSACK)
11. Completely buffered/isolated target signals; meaning that:
    * Problems with the target hardware will not keep the debugger from running
    * Test features like, continuous memory or port write or read can be used to track down hardware problems or exercise new hardware
    * Z80 control signals on the target are held in an inactive state when not running target code or accessing target resources (debug mode). This can make it easier to debug target board hardware by quieting signals to allow getting an analyzer ready to capture an operation.
    * Z80 MREQ can optionally be passed to the target during refresh cycles when in debug mode (for that rare dynamic memory setup)
    * The target WAIT signal can be disconnected from the debugger to help debug hardware that is holding WAIT on
12. Switch that immediately runs the target system on boot/reset (no Debugger functionality is initialized and is not available)

## Breakpoint Conditions

The breakpoint logic allows selecting:

1. Instruction fetch from a specific address
2. Memory read from a specific address
3. Memory write to a specific address
4. Any memory operation (other than instruction fetch) on a specific address
5. Port (I/O) read from a specific address (16 or 8 bit address) or any address
6. Port write to a specific address (16 or 8 bit address) or any address
7. Any Port operation on a specific address (16 or 8 bit address) or any address
8. One breakpoint can also check for a specific data value for any operation except instruction fetch (for IF the data isn't valid soon enough, but a break could be set for Memory Read with the data value - it just breaks after the instruction execution)
9. The data value break also allows specifying Don't-Care bits. This allows breaking on a port write of a specific bit, for example.
10. 'Soft' breakpoint detection can be enabled. This allows generating a break by including a RST (8/38) in the code. When the RST is detected the hardware treats it as a break and enters Debug mode.

A fully populated ZID has:

1. Two breakpoint addresses that share Condition-A, that can specify a Data-Value, with Don't-Care bits
2. Six breakpoint addresses that share Condition-B
3. All breakpoints are handled in hardware and don't require any modifications to the target system board or code

## High-Level Debug Monitor Description

This section provides a very high-level description of the Debug Monitor functionality. For complete information, see the ZID User Manual.

The user interface is through a USB port connected terminal (or terminal emulator).

1. Display the CPU registers
2. Set the value of CPU registers that will be used when code is run
3. Display the contents of memory
4. Modify the contents of RAM memory
5. Display data from an I/O port
6. Write data to an I/O port
7. Run code and break when one of two specified conditions are met:
   1. Breakpoint One and Two can be any of the conditions listed in the previous section, plus the ability to specify a data value that must match what is written/read, but allowing Don't-Care bits.
   2. Breakpoint Three through N (up to 8 total) share a condition that can be different from that of breakpoint One and Two.
8. Single-step code (even in ROM)
9. Perform operations that can be helpful in troubleshooting/repairing hardware problems, including:
   1. repetitive write of a specific data byte to a memory location
   2. repetitive read from a memory location
   3. repetitive write of a specific data byte to a port
   4. repetitive read from a port
   5. run a memory test using a number of different approaches (data patterns)
10. Additional functionality like:
    1. An HP-16C (like) calculator
    2. Selection of number base (Decimal, Hex, Octal, Binary (auto switches to Hex for certain operations))
    3. Entry of values in any base irrespective of the set base by using base indicators
    4. Store and recall multiple configurations that include the number base, breakpoint conditions, calculator register values, etc.

## Single Board Computer (SBC)

In addition to the Hardware Debugger, the board functions as an Single Board Computer (SBC). It's an SBC that:

1. has an MMU that bank switches 128K Flash (ROM) and 128K SRAM in 4 16K blocks (64K) from 8 regions of ROM and 8 regions of RAM.
2. none of the Debugger ROM/RAM need be available while running the SBC code (2K of Flash and 1K of RAM needs to be reserved for the Debugger from the upper region of the Flash and SRAM)
3. 4 output port bits that are available as buffered signals on a connector
4. 4 input port bits that are read from buffered signals on a connector
5. RP2040 module that provides:
   1. USB Terminal port
   2. eeprom storage
   3. RTC via I2C
   4. SD Card via SPI. The SD card can be used as CP/M disk.
6. runs at 20MHz with 0 wait states for RAM access, 1 wait state for Flash access, and 0 additional wait states for the 4-IN and 4-OUT port bits. The RP2040 modules adds various wait states depending on the operation.
7. the ability to use the debug probe cable and port I/O cable to connect to a 3 position RC2014-Z80 backplane.
   1. The RP2040 module interrupt signal can go to the backplane
   2. The backplane supports a bus-master accessing the SBC memory and ports
8. code and port operations can be completely debugged using the hardware debugger functionality without requiring any accommodations.

All of this is in a compact 3.5" x 5.0" board. The backplane is a 4.0" x 5.0" board with 3 RC2014-Z80 positions and includes a connector and port decoding for an OLED or TFT display panel. The backplane can be positioned on the same plane as the SBC or can be mouned back to back (the connecting cable makes a 'U'). Power is provided via a USB-C connection.
