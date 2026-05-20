# RP2040 Module Program Interface

The RP2040 Module, referred to as 'RPM' runs the Debugging Operation Control (DOC) code. All high-level debugging operations are performed by DOC. When operations require something from the Target or need to update the Target or the code execution (Z80 register values and modes) the DOC raises the Attention signal (ATTN) which causes either a Special-Reset (ZSR) or an NMI depending on whether it is Target Mode (Z80 is executing Target code) or Debug Mode (Z80 is executing Debugging Code - DC).

## Z80 RPM Communication

The Z80 communicates with the RPM using two I/O Ports, the Control Port and the Data Port. The DC can trigger communication by writing specific values to the Control Port. The RPM triggers communication by raising ATTN. For either case the Z80 performs all the data transfers. The RPM is not able to write to or read from the ZID Z80 or Local bus.

The Debug operations use a specific conversation sequence and bits to differentiate the debug operations from operations that the SBC (Target) might be performing with the RPM, which might be in progress when the debug operations need to be performed.

## Control Port Debug Bit-7

Bit-7 of the control port is reserved for use for debugging operations. When the DC is writing to the RPM it always sets bit-7 (ON), the other bits vary depending on the status or operation.

When the SBC (Target) is using the RPM, writes to the control port will always be with bit-7 unset (OFF). A separate RPM Programming Manual covers the non-debugging functions of the RPM.

## Operation Sequence

The following sections cover the operations performed by the DC and DOC.

### System Start, Full/Board Reset

At system start-up or for a full/board reset, the DC saves register A and the flags (F) in memory in order to determine whether it is a Full or a Special Reset (SR). It reads the Breakpoint register to check the ZID Initialized bit. If the ZID bit doesn't indicate that the ZID has been initialized it is assumed to be a Full/Board Reset.

Once it has been determined that it is a Full/Board Reset the DC does a RAM check to verify the RAM is usable. If the check fails, FF is written to the RPM control port and the failing low address is written to the RPM data port. The upper address will be in the range 40-7F. Following those two writes the failing location is continually written and read to aid hardware debugging.

If the RAM checks out, the DC sets up some memory values and establishes a stack to allow for more typical operation. When that is complete, the DC writes 70 to the RPM control port and goes into a IDLE state waiting for the RPM to signal with ATTN.

The following table describes the operations between the DOC and DC for a Full/Board Reset

|Debug Monitor  |Dir|Meaning or Debug Operation Control     |
|---------------|---|---------------------------------------|
|C:80.          | > | DC initialized. In DEBUG Mode.        |
| IDLE          |   |                                       |
|               |   |prepares CMD:0 data                    |
|.              |.  |ATTN (will generate NMI)               |
|C:82.          | > | OK, READING COMMAND (Target Suspended)|
|D RD 1         | < |CMD: Z80 Reset State                   |
|C:81.          | > | OP DONE.                              |
| IDLE.         |   |                                       |
|               |   |prepares CMD:1 data                    |
|.              |.  |ATTN                                   |
|C:82.          | > | OK, READING COMMAND (Target Suspended)|
|D RD 1         | < |CMD: Breakpoint Count.                 |
|D WR 1         | > | Number of breakpoints                 |
|C:81.          | > | OP DONE.                              |
| IDLE.         |   |                                       |

### Target Go

When the DOC completes initialization it will set preakpoints if they exist. Breakpoint command are covered in a following section. To run the target system, the DOC executes the Target Go sequence as follows:

|Debug Monitor  |Dir|Meaning or Debug Operation Control     |
|---------------|---|---------------------------------------|
|               |   |prepares CMD:n data                    |
|.              |.  |ATTN                                   |
|C:82.          | > | OK, READING COMMAND                   |
|D RD 1         | < |CMD: Target Go.                        |
|C;88.          | > | Target Go Sequence Started            |
|.              |   | (mode will transition to Target Mode) |

### Exit Target Mode

The user can instruct the DOC to exit Target Mode (halt target code execution). The sequence to exit Target Mode is:

|Debug Monitor  |Dir|Meaning or Debug Operation Control     |
|---------------|---|---------------------------------------|
|               |   |prepares CMD:n data                    |
|.              |.  |ATTN (will generate SPRST)             |
|               |   | DBM saves target reg.                 |
|C:82.          | > | OK, READING COMMAND                   |
|D RD 1         | < |CMD: Get Regs All+.                    |
|D WR 28        | > |AF,BC,DE,HL,PC,SP,IX,IY,               |
|               |.  |AF',BC',DE',HL',IE/ID,IV               |
|C;81.          | > | OP DONE                               |
| IDLE.         |.  |                                       |

|Debug Monitor  |Dir|Meaning or Debug Operation Control     |
|---------------|---|---------------------------------------|
|               |   |prepares CMD:n data                    |
|.              |.  |ATTN                                   |
|C:82.          | > | OK, READING COMMAND                   |
|D RD 1         | < |CMD: Load Regs All+.                   |
|D RD 28        | < |AF,BC,DE,HL,PC,SP,IX,IY,               |
|               |.  |AF',BC',DE',HL',IE+IM,IV               |
|C;81.          | > | OP DONE                               |
| IDLE.         |.  |                                       |

### Breakpoint Hit

When a breakpoint is hit the DBM saves the target registers and notifies the DOC

|Debug Monitor  |Dir|Meaning or Debug Operation Control     |
|---------------|---|---------------------------------------|
|               |   | DBM saves target reg.                 |
|C:84           | > | BREAKPOINT HIT                        |
|               |   | (WAIT holds DBM until DOC ready)      |
|               |   | DOC prepares CMD:n data               |
|               |   | releases WAIT                         |
|D RD 1         | < | CMD: Send Regs All+                   |
|D WR 28        | < |AF,BC,DE,HL,PC,SP,IX,IY,               |
|               |.  |AF',BC',DE',HL',IE+IM,IV               |
|C;81.          | > | OP DONE                               |
| IDLE.         |.  |                                       |
