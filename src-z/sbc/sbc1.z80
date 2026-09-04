	.title	"Single Board Computer (SBC) to run on the ZID board as a target"
	.stitle	"SBC Test #1 - Boot"
;
; Single Board Computer Example #1 (to run on the ZID board)
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
BOOT		.sect	W
;		.org	0

		.list	1		; Don't list these included files
		.input	"cmn/board.inc"
		.input	"storage.inc"
		.input	"diag/diag.inc"
		.input	"util/util.inc"
; ===========================
; This is a simple test app to run on the ZID board SBC hardware as a target.
;
; It is 'simple', in that it doesn't use interrupts or memory mapping, it just cycles
; through the LEDs. But it provides something to test with, and it does do an initial
; RAM test and leaves the test pattern (doesn't clear it to 0), so that also
; provides something to look at with the Debug Monitor.
;
; Other than Page-0 reset/restart/nmi vector jumps, the code is put up in
; higher ROM so that PC values won't conflict with Debug Monitor locations so
; that breakpoints set for Debug Monitor code won't be hit be the SBC code.
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
id:		.byte	"SBC Test #1:"		; ID
ver:		.equ	$
		.input	"version.inc"		; build date/version
_ver:		.byte	0

nmivec:		.org	boot+66h
		jp	onnmi


		.stitle	"POR Initialization"
		.org	boot+1000h
runtime:	.equ	$
; ---------------------------------------------------------------------------
;	Power-On-Reset Initialization
; ---------------------------------------------------------------------------
;
onreset:	.equ	$
		ld	iy,init2	; IY holds the address to return to (as we clobber ram)
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
init2:		; don't fill the ram (ram test leaves some data to look at)
		;
		; For debugging with ZID/ZED on Breadboard (which doesn't have
		; the PC Latch, so it uses RST8) set memory block 1 (4000-7FFF)
		; to RAM so our SP can be in RAM that is also RAM in DEBUG MODE
		ld	b,MMU_BLK_1
		ld	c,MMU_BASE
		ld	a,MEM_RAM_SEL|7	; Map RAM Region-7 into Block-1 (same as DEBUG)
		out	(c),a
		; Clear the two locations we actually use and set the stack
		ld	sp,sbcstk
		xor	a
		ld	(do_delay),a
		ld	(gpio_val),a
		;
		; put some interesting values in the ALT registers and IX
		di
		ld	a,011h
		ld	i,a		; No interrupts are used, this just for test
		exx
		ld	b,00bh
		ld	c,00ch
		ld	d,00dh
		ld	e,00eh
		ld	h,'H'
		ld	l,'L'
		exx
		ld	ix,0F00Dh
		; that's it (the CPLD takes care of most of the init)
		jp	main



		.stitle	"NMI and RST Handlers"
		.org	runtime+100h
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

		.stitle	"Main application"
		.align	8	; Page boundary
main:		; Do something interesting...
		in	a,(BRDCTRL)
		or	SBC_USE_M
		out	(BRDCTRL),a
		xor	a
		ld	(gpio_val),a	; clear our GPIO backing
		ld	l,a
		ld	b,a
		dec	a		; put FF in our 'do delay' value
		ld	(do_delay),a	; debugger can set to 0 to avoid the delay loop
		ld	c,SBC_GPIO
m1:		ld	e,l
		call	swapends	; have bit-0 change the slowest on the out pins
		out	(c),d		; port write (something to test break with)
		in	a,(c)		; port read (something to test break with)
		ld	(gpio_val),a	; memory write (something to test break with)
		ld	h,a
		inc	l
		ld	a,(do_delay)	; see if we should delay (also something to test break with)
		or	a
		jr	z,m1		; no delay =>
		ld	b,a		; amount to delay
		; delay a bit
m2:		djnz	m2
		jr	m1


		.align	8
do_rst:		.equ	$
	; Request a RESET using the CTRL Bit. Put a couple ops on either side
	;  for debugging
	;
		xor	a
		ld	b,a
		in	a,(BRDCTRL)
		set	RSTREQ_B,a
		out	(BRDCTRL),a
		; We don't expect to get here
		ld	b,a
		ld	c,a
		halt

		.align	4
do_brk:		.equ	$
	; Request a BREAK using the CTRL Bit. Put a couple ops on either side
	;  for debugging
	;
		xor	a
		ld	b,a
		in	a,(BRDCTRL)
		set	BRKREQ_B,a
		out	(BRDCTRL),a
		; We may get here. Debugging will tell
		ld	b,a
		halt


		.align	4
do_sbrk:	.equ	$
	; Do a SOFT BREAK. This will break if SOFT_BREAK is enabled in the
	; CTRL port.
	;
	; Put a couple ops on either side for debugging
	;
		xor	a
		ld	b,a
		dec	a
		rst	38h		; This will trigger a break if enabled
		; We may get here. Debugging will tell
		ld	b,a
		halt


		.align	2
		.byte	"!sbc1!"
		.end

