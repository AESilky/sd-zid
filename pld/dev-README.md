# ZID CPLD Interconnect & Interaction Architecture

The **sd-zid** hardware project utilizes a decentralized three-device programmable logic matrix to achieve zero-latency, full-speed 20MHz breakpoint tracking and bus isolation. The logic is strategically distributed across **CPLD1 (U18 - Master Control)**, **CPLD2 (U15 - Address & PC)**, and **CPLD3 (U22 - Data & Handover)**.

## 1. Physical Inter-Chip Signal Interconnects
This table tracks the explicit physical lines routing directly between the three CPLD packages on the PCB to synchronize tracking states.

| Signal Name | Source Device | Destination Device(s) | Electrical & Logical Role at 20MHz |
| :--- | :--- | :--- | :--- |
| **`b1_aeq`** | **CPLD2** (`U15`) | **CPLD1** (`U18`) | **Address Match Channel 1:** Asserts high the exact nanosecond the Z80 address bus matches the 16-bit target value or 8-bit page mask set for Breakpoint 1. |
| **`bn_aeq`** | **CPLD2** (`U15`) | **CPLD1** (`U18`) | **Address Match Channels 2–4:** A cascaded logical OR output (`b2aeq # b3aeq # b4aeq`) notifying the master that one of the secondary address targets has matched. |
| **`brk_deq`** | **CPLD3** (`U22`) | **CPLD1** (`U18`) | **Data Bus Match:** Confirms that the current byte on the Z80 data bus cleanly satisfies the bit-masked value and explicit "don't-care" requirements. |
| **`tgt_now`** | **CPLD3** (`U22`) | **CPLD1** (`U18`) | **Handover Countdown Flag:** Signals the completion of the 15-cycle "HOP, SKIP, and JUMP" transition counter, prompting CPLD1 to instantly drop the debug overlay firewall. |
| **`mem_op`** | **CPLD1** (`U18`) | **CPLD3** (`U22`) | **Counter Clock Pulse:** A qualified, clean clock pulse generated on active Z80 memory cycles (`zmreq & (zrd # zwr)` excluding refresh), driving the TGT_GO ripple timer deterministically. |
| **`pc_latch`** | **CPLD1** (`U18`) | **CPLD2** (`U15`) | **PC Capture Trigger:** Driven on Global Clock 2. Latches every valid `M1` instruction fetch while running target code, and freezes instantly on Special Reset to lock the origin execution pointer. |
| **`zid_io`** | **CPLD1** (`U18`) | **CPLD2** & **CPLD3** | **Debugger Port Enable:** High-speed qualified select line indicating a valid local debugger I/O space access (`00h–1Fh`) is active while in Debug Mode. |
| **`sbc_io`** | **CPLD1** (`U18`) | **CPLD3** (`U22`) | **SBC Port Enable:** High-speed qualified select line routing common application/SBC I/O port decodes (like your 4-In/4-Out general-purpose bits). |

***

## 2. Shared Functional Bus Matrix
This table maps how the global Z80 bus, local debug registers, and the RP2040 control lines are shared across the three programmable chips.

| Signal Domain / Lines | CPLD1 Role (`zid_main`) | CPLD2 Role (`zid_brkpcl`) | CPLD3 Role (`zid_bd_gpio`) |
| :--- | :--- | :--- | :--- |
| **Address Bus (`addr15..0`)** | Samples `addr15..14` for MMU block steering; monitors `addr7..1` for master port selection. | Ingests full `addr15..0` array under PinKeeper control for raw magnitude comparisons and PC latch tracking. | Samples lower `addr3..0` for internal state selections, TGT_GO counter trigger, and GPIO mapping. |
| **Data Bus (`d7..0`)** | Ingests parameters for master control bits, outputs MMU register statuses, and injects phantom `ZFOP` (`NOP`) during Special Reset. | Receives target breakpoint low-byte configurations; shifts out the frozen 16-bit Program Counter across Ports `05h`/`06h`. | Ingests target data/don't-care filters; handles input/output multiplexing for the 8-bit bidirectional SBC GPIO. |
| **Z80 Bus Control (`zm1`, `zmreq`, `ziorq`, `zrd`, `zwr`, `zrfsh`)** | Decodes raw bus states to track double-byte instruction prefixes (`DBI`), gates local NMI/INT lines, and drives the `rfmr_` target refresh isolation loop. | Monitors `wr` and `rd` directly on input-only lines to time range storage updates and PC shift register reads. | Monitors `wr` and `rd` on input-only pins to synchronize data registers and trigger the asynchronous `TGTGO_WR` counter. |
| **RP2040 Interface (`mod_sel`, `mwtrq`, `attn`)** | Evaluates `mod_sel` via Port `1Eh/1Fh`, reads the `mwtrq` line to insert wait states, and maps `attn` to either Special Reset or NMI routines. | Passthrough / Isolated. Unburdened by the 3.3V microcontroller communications matrix. | Passthrough / Isolated. (Bus interactions are intercepted downstream by the 3.3V transceivers). |

***

## 3. High-Speed Interaction and Coordination Loops

### A. The Special Reset & PC Freeze Loop
1. **Detection:** **CPLD1** continuously monitors execution. If an address/data trap matches (`b1_det`/`b2_det`) or the RP2040 asserts attention (`attn`), **CPLD1** raises the `zsr_req_R` latch.
2. **Synchronization:** Exactly on the rising edge of **T2 of the next M1 cycle**, **CPLD1** asserts `zrst_R` to generate the non-destructive Special Reset.
3. **The Freeze:** Dropping out of Target Mode instantly breaks the clock condition on the `pc_latch` line. **CPLD2**'s internal `[pc15..0]` registers freeze instantly, locking the exact `IF1` address that caused the break.
4. **Injection:** Simultaneously, **CPLD1** seizes control of the data lines and injects `ZFOP` (`0x00` or `0xCF`), steering the Z80 safely into the Debug Monitor vectors.

### B. The "HOP, SKIP, and JUMP" Handover Loop
1. **Arming:** The Z80 Debug Monitor writes to port `07h`. **CPLD3** captures this via `TGTGO_WR`, asserting `tgt_go_R` and enabling the `tgc` ripple counter.
2. **The HOP:** The Z80 executes a precise sequence of `NOP` instructions. **CPLD1** tracks these via memory cycles and emits the `mem_op` clock line to **CPLD3** for each execution step, advancing the counter cleanly from 1 to 7.
3. **The SKIP:** The Z80 monitor executes a local 3-byte jump instruction inside your safe Debug RAM. This ticks the **CPLD3** counter up through cycles 8 to 10.
4. **The JUMP:** The Z80 monitor executes the final absolute `JP (Saved_PC)`. The opcode fetch and subsequent address byte reads drive **CPLD3**'s counter through cycles 11 to 15.
5. **Handoff:** At exactly count `15` (`'b'1111`), **CPLD3** snaps `tgt_now` high. **CPLD1** catches this transition on the fly, activates the target transceivers, and the target code resumes full-speed execution at cycle 16 without missing a single nanosecond of bus timing.
