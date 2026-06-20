; ==============
;
; Boot/Start-Up Board Diagnostics
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============
;
		.stitle	"Diagnostics"
CODE		.sect	W

		.input	"diag.inc"

		.list	1
		;.input	"cmn/stddef.inc"	; stddef must be first
		.input	"cmn/board.inc"

;;**************
			.global	DIAG_
			.align	4		; Not required, but makes debugging easier
DIAG_:			.equ	$


ramchk_ns:	; Return in IY
		ld	d,h 		; save the start page
		ld	e,c 		; save the page count
		xor	a		; start with 0
		ld	l,a		; start with beginning of page
		; fill all
ramchk_0:	ld	b,0		; byte count (256)
ramchk_1:	ld	(hl),a
		inc	a		; next value
		inc	hl		; next location
		djnz	ramchk_1	; continue writing this page
		inc	a
		dec	c		; next page
		jr	nz,ramchk_0
ramchk_2:	; all ram written, read it back
		xor	a		; back to 0
		ld	l,a
		ld	h,d 		; get the start page
		ld	c,e 		; get the page count
ramchk_00:	ld	b,0 		; byte count
ramchk_01:	ld	e,(hl)		; read the byte
		cp	e 		; check it
		jr	nz,ramchk_err
		inc	a
		inc	hl
		djnz	ramchk_01	; next byte
		inc	a
		dec	c
		jr	nz,ramchk_00 	; next page
		; all ram compares, return Z
		;
ramchk_err:	; register values and NZ flag are what we need to return
		jp	(iy)


ramfill_ns:	; Fill ram: From HL, for C pages, with byte in A
		; Return to IY
		ld	b,0
_rf2:		ld	(hl),a		; do a page
		inc	hl
		djnz	_rf2
		dec	c		; next page
		jr	nz,_rf2
		jp	(iy)

		.byte	"!diag!"
		.end
