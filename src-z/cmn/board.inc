		.stitle	"Board Specific Definitions"
; ==============
;
; ZID (Z80 In-Circuit Debugger) Board Level Definitions
; This includes the SBC definitions.
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============
;

		.list	1
		.input	"stddef.inc"
;
; I/O Device Addresses
;

;  Breakpoints
BRK1DATA	.equ	000h		; [WO] Break-1 Data Value
BRK1DREQ	.equ	002h		; [WO] Break-1 Data Required
BRKCTRL		.equ	004h		; [WO] Break Control (BRK1=b0..3 BRK2=b4..7)
;   Breakpoint Control Values
BRK_NONE	.equ	0		;  No Break
BRK_IF		.equ	1		;  Instruction Fetch
BRK_MW		.equ	2		;  Memory Write
BRK_MR		.equ	3		;  Memory Read
BRK_AM		.equ	4		;  Any Memory Op
BRK_PW		.equ	5		;  Port (I/O) Write
BRK_PR		.equ	6		;  Port (I/O) Read
BRK_AP		.equ	7		;  Any Port (I/O) Op
BRK_REQAD	.equ	8		;  Break Require Address (OR'ed with Control)
BRK1CSHFT	.equ	0		;   Shift (right) for Break-1 control shift
BRK2CSHFT	.equ	4		;   Shift (right) for Break-2 control shift
;
BRKADCLK	.equ	016h		; Break Address Clock
BRKENCLK	.equ	018h		; Break Address Enable Clock
BRKENCLR	.equ	01Ah		; Break Address Enable Clear

;  Board Control (available to ZID and SBC)
BRDCTRL		.equ	006h		; [RW] Board Control
SBC_USE_B	.equ	0		;  Bit-0 for use as desired by SBC
SBC_USE_M	.equ	00000001b	;  Mask for SBC-Bit
BRKREQ_B	.equ	1		;  Break Request, used by SBC to request a break (go to Debugger)
BRKREQ_M	.equ	00000010b	;  Mask for Break Request
RSTREQ_B	.equ	2		;  Reset Request. Forces a (full) reset from SBC or Debugger
RSTREQ_M	.equ	00000100b	;  Mask for Reset Request
;  Board Control (only available to ZID)
SYNCONLY_B	.equ	3		;  Only generate SYNC signal on Breakpoint match
SYNCONLY_M	.equ	00001000b	;  Mask for SYNC Only
SBRK_EN_B	.equ	4		;  Soft-Break (RST38) Enable
SBRK_EN_M	.equ	00010000b	;  Mask for Soft-Break Enable
SOP_B		.equ	5		;  Single Bus Operation (enable)
SOP_M		.equ	00100000b	;  Mask for SOP
SSTEP_B		.equ	6		;  Perform a Single-Step on Target-GO
SSTEP_M		.equ	01000000b	;  Mask for Single-Step
ZIDINIT_B	.equ	7		;  ZID (Debug Monitor) has been initialized
ZIDINIT_M	.equ	10000000b	;  Mask for ZID Init

;  Memory Management Unit (MMU)
MMU_BASE	.equ	008h		; [RW] MMU uses the upper address bits 15&14 to select the register (Block)
					;  The value is the region to map to the selected block
MMU_BLK_0	.equ	000h		;  Block 0 select
MMU_BLK_1	.equ	040h		;  Block 1 select
MMU_BLK_2	.equ	080h		;  Block 2 select
MMU_BLK_3	.equ	0C0h		;  Block 3 select

;  SBC General Purpose I/O Bits (pins on the board, 4 out and 4 in)
SBC_GPIO	.equ	00Ah		; [RW] Bits 0..3 are out, 4..7 are in. All can be read.

;  Target GO - Write starts counter. Data is DON'T-CARE
TGTGO		.equ	010h		; 16 Mem-OPs for TGT_NOW, 8 for SOP_NOW (Mem/Port W/R)

;  PC Saved (bit) read, Break Address HW Detect, Break Status, SBC-Mode
PCADDR_RD	.equ	01Ch		; This port reads multiple status bits.
					; Each time it is read it shifts the BRK-PC to the next bit
					; so each read is important after a Break or ATTN.
PCB_PC_B	.equ	0		;  bit-0 is break/attn saved PC Bit 15..0
PCB_PC_M	.equ	00000001b
BRK_HWD_B	.equ	1		;  bit-1 is Break Hardware Detect
BRK_HWD_M	.equ	00000010b
SR_ISSBRK_B	.equ	2		;  bit-2 indicates that the Special Reset is from soft Break
SR_ISSBRK_M	.equ	00000100b
SR_ISHBRK_B	.equ	3		;  bit-3 indicates that the Special Reset is from hard Break
SR_ISHBRK_M	.equ	00001000b
ATTN_ON_B	.equ	4		;  bit-4 indicates that RPM ATTN is active
ATTN_ON_M	.equ	00010000b
TGTMODE_B	.equ	5		;  bit-5 indicates that the board is in Target Mode
TGTMODE_M	.equ	00100000b
SBCMODE_B	.equ	6		;  bit-6 indicates that the target is the SBC
SBCMODE_M	.equ	01000000b
ZIDINITD_B	.equ	7		;  bit-7 used to track whether the ZID is initialized
ZIDINITD_M	.equ	10000000b

;  RP Module - It is the Debug Operation Controller (DOC) as well as peripherals for the SBC
RPMODBASE	.equ	01Eh		; RP2040 Module Base
RPMCTRL		.equ	RPMODBASE+0	;   Module Control
RPMDATA		.equ	RPMODBASE+1	;   Module Data

;
; Constants for the SBC-MMU (mapping is fixed for the Debugger)
ONE_BLOCK	.equ	(ONE_KB*16)	; Bytes in a Block (with ZID MMU)
ONE_REGION	.equ	(ONE_KB*16)	; Bytes in a Region (with ZID MMU)
PAGES_PER_BLOCK	.equ	(ONE_BLOCK/ONE_PAGE)	; Number of 256-Byte Pages in an 16K-Byte Block
MEM_ROM_SEL	.equ	0000b		; ROM is mapped with bit-4=0
MEM_RAM_SEL	.equ	1000b		; RAM is mapped with bit-4=1
MEMBLK0_ADR	.equ	00000h		; Block-0 Z80 Memory start address 
MEMBLK1_ADR	.equ	04000h		; Block-1 Z80 Memory start address 
MEMBLK2_ADR	.equ	08000h		; Block-2 Z80 Memory start address 
MEMBLK3_ADR	.equ	0C000h		; Block-3 Z80 Memory start address 
