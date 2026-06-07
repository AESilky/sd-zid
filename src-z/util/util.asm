; ==============
;
; Utility Routines
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============
;
		.stitle	"Utilities"
CODE		.sect	W

		.input	"util.inc"


	.align	2
;
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

