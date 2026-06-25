; ==============
;
; ZID (Z80 In-circuit Debugger) Debug Monitor Data Storage
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============

; ROM and RAM start and page count
;
DB_ROMBASE	.equ	00000h
DB_ROMPC	.equ	040h
DB_RAMBASE	.equ	04000h
DB_RAMPC	.equ	040h

; Memory locations
;
;  Registers
;  (!!! This order is important. It matches the save/restore and transfer to/from DOC order. !!!)
	.global	regied
ie_b		.equ	2	;  P/V is IE
	.global	regi
	.global	regfx
	.global	regax
	.global	regcx
	.global	regbx
	.global	regex
	.global	regdx
	.global	reglx
	.global	reghx
	.global regsav_
	.global	regc
	.global	regb
	.global	rege
	.global	regd
	.global	regl
	.global	regh
	.global	rstgtgo_
	.global	regf
	.global	rega
	.global	regsp
	.global	regpc
	.global	regix
	.global	regiy
;
	.global	regsav2
	.global	tempaf		; Temporary AF storage (saved by 'push')
	.global	tempsp		; Temporary SP storage (SP saved here, then set here)
	.global	asave
	.global	z80im		; Target Interrupt Mode
	.global tflag		; Target state/status bits
	.global zflag		; ZID state/status bits
zf_usezid	.equ	7	;  bit-7: Use ZID memory and ports for 'target' ops
zf_dbzed	.equ	6	;  bit-6: Using ZED to debug (RST8 rather than SPRST)
	.global brk_pc_inf	; save initial read from the Break-PC and Information port

;
; Breakpoint Information
;
	.global baucnt		; Breakpoint Address Unit Count
	.global brk1data	; Break-1 Data Value
	.global brk1dreq	; Break-1 Data Mask
	.global brk1cond	; Break-1 Condition
	.global brk1en		; Break-1 enabled flag (separate from the condition)
	.global brkNcond	; Break-2N Condition
	.global brkNen		; Break-2N enabled flag (separate from the condition)
	.global brk2addr	; Break-2 Address
	.global brk2aen		; Break-2 Address Enabled
	.global brk3addr	; Break-3 Address
	.global brk3aen		; Break-3 Address Enabled
	.global brk4addr	; Break-4 Address
	.global brk4aen		; Break-4 Address Enabled
	.global brk5addr	; Break-5 Address
	.global brk5aen		; Break-5 Address Enabled
	.global brk6addr	; Break-6 Address
	.global brk6aen		; Break-6 Address Enabled
	.global brk7addr	; Break-7 Address
	.global brk7aen		; Break-7 Address Enabled
	.global brk8addr	; Break-8 Address
	.global brk8aen		; Break-8 Address Enabled
	.global brk9addr	; Break-9 Address
	.global brk9aen		; Break-9 Address Enabled
	.global brk10addr	; Break-10 Address
	.global brk10aen	; Break-10 Address Enabled
	.global brk11addr	; Break-11 Address
	.global brk11aen	; Break-11 Address Enabled
	.global brk12addr	; Break-12 Address
	.global brk12aen	; Break-12 Address Enabled
	.global brk13addr	; Break-13 Address
	.global brk13aen	; Break-13 Address Enabled
	.global brk14addr	; Break-14 Address
	.global brk14aen	; Break-14 Address Enabled
brkalen_m:	.equ	00000001b	; Address Low enabled
brkahen_m:	.equ	00000010b	; Address High enabled

	.global doc_cmd		; last command from DOC

	.global dbmstk		; debug monitor stack (top)

	;
	; Target-GO code and PC locations
	;
	.global	tgtskip		; jump code
	.global tgtjump		; jump address

