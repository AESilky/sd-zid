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

do_delay:	.block	BYTE		; Flag 0/non-zero controling delay or not

gpio_val:	.block	BYTE		; Memory backing for the GPIO

; stack
;
		.align	8		; Put on a page boundary
		.block	256		; Debug monitor stack space
sbcstk:		.equ	$

