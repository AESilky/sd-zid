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

	.global doc_cmd		; last command from DOC

	.global dbmstk		; debug monitor stack (top)

	;
	; Target-GO code and PC locations
	;
	.global	tgtskip		; jump code
	.global tgtjump		; jump address

