	.stitle	"Storage"
;
; Storage for the ZID Debug Monitor
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
		.list	1
		.input	"cmn/stddef.inc"	; stddef must be first
		.input	"storage.inc"
BSS		.sect

; Temp stack and locations for register storage
;
; The registers are pushed as a pair, but labeled individually for access
; (!!! This order is important. It matches the save/restore and transfer to/from DOC order. !!!)
regied:		.block	BYTE		; 'f' when af pushed to save 'i', P/V is IE/ID
regi:		.block	BYTE
regfx:		.block	BYTE
regax:		.block	BYTE
regcx:		.block	BYTE
regbx:		.block	BYTE
regex:		.block	BYTE
regdx:		.block	BYTE
reglx:		.block	BYTE
reghx:		.block	BYTE
regsav_:	.equ	$		; this is where we start for tgtgo
regc:		.block	BYTE
regb:		.block	BYTE
rege:		.block	BYTE
regd:		.block	BYTE
regl:		.block	BYTE
regh:		.block	BYTE
rstgtgo_:	.equ	$		; Used for restore AF on Target-GO
regf:		.block	BYTE
rega:		.block	BYTE
regsp:		.block	WORD
regpc:		.block	WORD
regix:		.block	WORD
regiy:		.block	WORD
;
regsav2:	.equ	$		; Used for 2nd save operation (PC, SP, and AF) 
		.block	32		; room for temporary stack (not expected to be used)
tempaf:		.block	WORD
tempsp:		.block	WORD
asave:		.block	BYTE
z80im:		.block	BYTE
tflag		.block	BYTE		; Target state/status bits (see .inc for bits)
zflag		.block	BYTE		; ZID state/status bits (see .inc for bits)
;
brk_pc_inf:	.block	BYTE		; Used to save the initial read from this port
					; since each time the port is read it shifts
					; to the next bit for the PC read.
;
; Breakpoint Information
;
baucnt:		.block	BYTE		; Breakpoint Address Unit Count
brk1data:	.block	BYTE		; Break-1 Data Value
brk1mask:	.block	BYTE		; Break-1 Data Mask
brk1cond:	.block	BYTE		; Break-1 Condition
brk1en:		.block	BYTE		; Break-1 enabled flag (separate from the condition)
brkNcond:	.block	BYTE		; Break-2N Condition
brkNen:		.block	BYTE		; Break-2N enabled flag (separate from the condition)
brk2addr:	.block	WORD		; Break-2 Address
brk2aen:	.block	BYTE		; Break-2 Address Enabled
brk3addr:	.block	WORD		; Break-3 Address
brk3aen:	.block	BYTE		; Break-3 Address Enabled
brk4addr:	.block	WORD		; Break-4 Address
brk4aen:	.block	BYTE		; Break-4 Address Enabled
brk5addr:	.block	WORD		; Break-5 Address
brk5aen:	.block	BYTE		; Break-5 Address Enabled
brk6addr:	.block	WORD		; Break-6 Address
brk6aen:	.block	BYTE		; Break-6 Address Enabled
brk7addr:	.block	WORD		; Break-7 Address
brk7aen:	.block	BYTE		; Break-7 Address Enabled
brk8addr:	.block	WORD		; Break-8 Address
brk8aen:	.block	BYTE		; Break-8 Address Enabled
brk9addr:	.block	WORD		; Break-9 Address
brk9aen:	.block	BYTE		; Break-9 Address Enabled
brk10addr:	.block	WORD		; Break-10 Address
brk10aen:	.block	BYTE		; Break-10 Address Enabled
brk11addr:	.block	WORD		; Break-11 Address
brk11aen:	.block	BYTE		; Break-11 Address Enabled
brk12addr:	.block	WORD		; Break-12 Address
brk12aen:	.block	BYTE		; Break-12 Address Enabled
brk13addr:	.block	WORD		; Break-13 Address
brk13aen:	.block	BYTE		; Break-13 Address Enabled
brk14addr:	.block	WORD		; Break-14 Address
brk14aen:	.block	BYTE		; Break-14 Address Enabled
;
;
doc_cmd:	.block	BYTE		; Last command from DOC

		.align	2		; just for debugging.
		; JP gets put in 'tgtskip' and Target PC gets put in 'tgtjump'
tgtskip:	.block	BYTE		; Target hop,SKIP,jump	[mem_op: 2]
tgtjump:	.block	WORD		; Target hop,skip,JUMP  [mem_op: 1,0] (address set to Target PC)
					; And we are now running in the Target!!!

STK		.sect
		.block	254		; Debug monitor stack space (not much stack is used)
dbmstk:		.equ	$

