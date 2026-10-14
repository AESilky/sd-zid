		.stitle	"Board Specific Definitions"
; ==============
;
; ZID (Z80 In-Circuit Debugger) Board Level I/O & Memory Definitions
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
BRKDATA		.equ	008h		; [WO] Break Data Value
BRKDATAX	.equ	009h		; [WO] Break Data Don't-Care
BRKCTRL		.equ	014h		; [WO] Break Control (BRK1=b0..3 BRK2=b4..7)
;   Breakpoint Control Values
BRK_NONE	.equ	0		;  No Break
BRK_IF		.equ	1		;  Instruction Fetch
BRK_MW		.equ	2		;  Memory Write
BRK_MR		.equ	3		;  Memory Read
BRK_AM		.equ	4		;  Any Memory Op
BRK_PW		.equ	5		;  Port (I/O) Write
BRK_PR		.equ	6		;  Port (I/O) Read
BRK_AP		.equ	7		;  Any Port (I/O) Op
;   Breakpoint Address
BRKADDRB	.equ	000h		; [WO] Break Address (base, add n-1)
BRKADDR1	.equ	000h		; [WO] Break Address 1 (upper address is upper value)
BRKADDR2	.equ	001h		; [WO] Break Address 2 (upper address is upper value)
BRKADDR3	.equ	002h		; [WO] Break Address 3 (upper address is upper value)
BRKADDR4	.equ	003h		; [WO] Break Address 4 (upper address is upper value)
BRKADEN		.equ	004h		; [WO] Break Address Upper/Lower Enable (2 bits per)
					;  1..0: BRK1 H-EN L-EN
					;  3..2: BRK2 H-EN L-EN
					;  5..4: BRK3 H-EN L-EN
					;  7..6: BRK4 H-EN L-EN
;   Break Address Enable mask-bit values. Shift left per break number
BRKADENH	.equ	10b		; Mask for HIGH ENABLE
BRKADENL	.equ	01b		; Mask for LOW ENABLE
;

;  Board Control - RW by ZID, R by SBC, RESET can be written by SBC
BRDCTRL		.equ	014h		;  Board Control
RSTREQ_B	.equ	0		;  Reset Request. Forces a (full) reset from SBC or Debugger
RSTREQ_M	.equ	00000001b	;  Mask for Reset Request
;  Board Control (only writable by ZID)
BRK1USED_B	.equ	1		;  Break1 Use DATA+DON'T_CARE
BRK1USED_M	.equ	00000001b	;  Mask for Break1 Use D&DC
BRK2USED_B	.equ	2		;  Break2n Use DATA+DON'T_CARE
BRK2USED_M	.equ	00000010b	;  Mask for Break2n Use D&DC
SYNCONLY_B	.equ	3		;  Only generate SYNC signal on Breakpoint match
SYNCONLY_M	.equ	00001000b	;  Mask for SYNC Only
SBRK_EN_B	.equ	4		;  Soft-Break (RST38) Enable
SBRK_EN_M	.equ	00010000b	;  Mask for Soft-Break Enable
SOP_B		.equ	5		;  Single Bus Operation (enable)
SOP_M		.equ	00100000b	;  Mask for SOP
SSTEP_B		.equ	6		;  Perform a Single-Step on Target-GO
SSTEP_M		.equ	01000000b	;  Mask for Single-Step
ZNMI_EN_B	.equ	7		;  Allow ZID NMI
ZNMI_EN_M	.equ	10000000b	;

;  Memory Management Unit (MMU)
MMU_BASE	.equ	010h		; [RW] MMU uses the upper address bits 15&14 to select the register (Block)
					;  The value is the region to map to the selected block
MMU_BLK_0	.equ	000h		;  Block 0 select
MMU_BLK_1	.equ	040h		;  Block 1 select
MMU_BLK_2	.equ	080h		;  Block 2 select
MMU_BLK_3	.equ	0C0h		;  Block 3 select

;  SBC General Purpose I/O Bits (pins on the board, 4 out and 4 in)
SBC_GPIO	.equ	00Ch		; [RW] Bits 0..3 are out, 4..7 are in. All can be read.

;  Target GO - Write starts counter. Data is DON'T-CARE
TGTGO		.equ	007h		; 16 Mem-OPs for TGT_NOW

;  Special Reset PC Value
SR_PC_H		.equ	006h		; PC High byte
SR_PC_L		.equ	005h		; PC Low byte

;  PC Saved (bit) read, Break Status, SBC-Mode
SPRSTINFO	.equ	012h		; This port reads multiple status bits.
					; Each time it is read it shifts the BRK-PC to the next bit
					; so each read is important after a Special Reset (Break or ATTN).
PCB_PC_B	.equ	0		;  bit-0 is break/attn saved PC Bit 15..0
PCB_PC_M	.equ	00000001b
SR_ISSBRK_B	.equ	1		;  bit-1 indicates that the Special Reset is from soft Break
SR_ISSBRK_M	.equ	00000010b
SR_ISHBRK_B	.equ	2		;  bit-2 indicates that the Special Reset is from hard Break
SR_ISHBRK_M	.equ	00000100b
; SYNCONLY Bit (bit 3) and Mask are above in the Board Control section
ATTN_ON_B	.equ	4		;  bit-4 indicates that RPM ATTN is active
ATTN_ON_M	.equ	00010000b
TGTMA_ATTN_B	.equ	5		;  bit-5 is the state of Target Mode when ATTN occurred
TGTMA_ATTN_M	.equ	00100000b
SBCMODE_B	.equ	6		;  bit-6 indicates that the target is the SBC
SBCMODE_M	.equ	01000000b
; NMI-Enable Bit (bit 7) and Mask are above in the Board Control section

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
