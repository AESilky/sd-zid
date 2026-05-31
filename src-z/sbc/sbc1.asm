;
; Single Board Computer Example #1 (to run on the ZID board)
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
		.title	"Single Board Computer (SBC) to run on the ZID board as a target"
		.stitle	"SBC Test #1 - Boot"
BOOT		.sect	W
;		.org	0

		.input	"cmn/stddef.inc"	; stddef must be first
		.input	"cmn/board.inc"
		.input	"storage.inc"

		.list	1		; Don't list these included files
		.input	"diag/diag.inc"
		.eject
; ===========================
; This is a simple test app to run on the ZID board SBC hardware as a target.
; It is simple, in that it doesn't use interrupts or memory mapping, it just cycles
; through the LEDs. But it provides something to test with.
; 
; ===========================

; ===========================
;
; ROUTINE: boot - Initialize the board, including a memory test. We leave the memory
;		with the data from the test (rather than filling it), so that there
;		is something to inspect.
;
; PURPOSE: Configure the board, do a memory test, and then cycle the LEDs.
;
; ENTRY: Power-Up or Push-Button Reset
; EXIT:  _
;
; ===========================
;
boot:
		jp	onreset			; Reset and Restart Entries
rst08:		.org	boot+08h
		jp	onrst08
rst10:		.org	boot+10h
		jp	onrst10
rst18:		.org	boot+18h
		jp	onrst18
rst20:		.org	boot+20h
		jp	onrst20
rst28:		.org	boot+28h
		jp	onrst28
rst30:		.org	boot+30h
		jp	onrst30
rst38:		.org	boot+38h
		jp	onrst38
		
		.org	boot+40h
id:		.byte	"SBC Test #1:"	; ID
ver:		.equ	$
		.input	"version.inc"		; build date/version

nmivec:		.org	boot+66h
		jp	onnmi

		.org	boot+0100h
onreset:	ld	iy,init2	; IY holds the address to return to (as we clobber ram)
		ld	hl,SBC_RAMBASE
		ld	c,SBC_RAMPC
		jp	ramchk_ns	; test our ram (note that we don't test the whole board)
		jr	z,init2		;  passed...
		; !!! MEMORY ERROR !!!
		; HL hold address of error, A expected, E read
		; continuously write and read it (read to D to not mess up E)
imemerr:	ld	(hl),a
		ld	d,(hl)
		jr	imemerr
init2:		; don't fill the ram (some data to look at)
		; that's it (the CPLD takes care of most of the init)
		jp	main

		.eject
		.org	boot+500h
;; =============
;; NMI and Restart Handlers - None of these are expected.
;;
;; Load B with the RST Number and jump to a common routine that
;; sends a status to the DOC and HALTs
;;
;;
onnmi:		ld	b,66h
		jr	onrst
onrst08:	ld	b,08h
		jr	onrst
onrst10:	ld	b,10h
		jr	onrst
onrst18:	ld	b,18h
		jr	onrst
onrst20:	ld	b,20h
		jr	onrst
onrst28:	ld	b,28h
		jr	onrst
onrst30:	ld	b,30h
		jr	onrst
onrst38:	ld	b,38h
		jr	onrst
		.align	4
onrst:		; Halt (will cause the HALT LED to come on if debugging)
		halt

		.align	8
main:		; Do something interesting...
		in	a,(BRDCTRL)
		or	SBC_USE_M
		out	(BRDCTRL),a
		xor	a
		ld	(gpio_val),a	; clear our GPIO backing
		ld	l,a
		ld	b,a
		ld	c,SBC_GPIO
m1:		ld	e,l
		call	swapends	; have bit-0 change the slowest on the out pins
		out	(c),d		; port write (something to test break with)
		in	a,(c)		; port read (something to test break with)
		ld	(gpio_val),a	; memory write (something to test break with)
		ld	h,a
		ld	a,(gpio_val)	; memory read (something to test break with)
m2:		djnz	m2		; delay a bit
		inc	l
		jr	m1


swapends:	ld	d,0
		bit	7,e
		jr	z,se1
		set	0,d
se1:		bit	6,e
		jr	z,se2
		set	1,d
se2:		bit	5,e
		jr	z,se3
		set	2,d
se3:		bit	4,e
		jr	z,se4
		set	3,d
se4:		bit	3,e
		jr	z,se5
		set	4,d
se5:		bit	2,e
		jr	z,se6
		set	5,d
se6:		bit	1,e
		jr	z,se7
		set	6,d
se7:		bit	0,e
		ret	z
		set	7,d
		ret

		.byte	"!sbc1!"
		.end

