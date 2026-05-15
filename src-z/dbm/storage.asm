;
; Storage for the ZID Debug Monitor
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
		.stitle	"Storage"

		.list	1
		.input	"cmn/stddef.inc"	; stddef must be first
		.list	2
		.input	"storage.inc"
		.eject

BSS		.sect

; Temp stack and locations for register storage
;
; The registers are pushed as a pair, but labeled individually for access
; (this order is important!!!)
regsav:		.block	BYTE		; room for the 'f' when af pushed to save 'i'
regi:		.block	BYTE
regfx:		.block	BYTE
regax:		.block	BYTE
regcx:		.block	BYTE
regbx:		.block	BYTE
regex:		.block	BYTE
regdx:		.block	BYTE
reglx:		.block	BYTE
reghx:		.block	BYTE
regix:		.block	WORD
regiy:		.block	WORD
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
;
regsav2:	.equ	$		; Used for 2nd save operation (PC, SP, and AF) 
		.block	32		; room for temporary stack (not expected to be used)
tempaf:		.block	WORD
tempsp:		.block	WORD
asave:		.block	BYTE
z80im:		.block	BYTE
tflag		.block	BYTE		; Target state/status bits
;
brk_pc_inf:	.block	BYTE		; Used to save the initial read from this port
					; since each time the port is read it shifts
					; to the next bit for the PC read.
;
doc_cmd:	.block	BYTE		; Last command from DOC

		.align	2		; just for debugging.
		; JP gets put in 'tgtskip' and Target PC gets put in 'tgtjump'
tgtskip:	.block	BYTE		; Target hop,SKIP,jump	[mem_op: 2]
tgtjump:	.block	WORD		; Target hop,skip,JUMP  [mem_op: 1,0] (address set to Target PC)
					; And we are now running in the Target!!!

		.align	2		; Put on a word boundary
		.block	256		; Debug monitor stack space
dbmstk:		.equ	$

