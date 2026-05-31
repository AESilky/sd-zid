;
; Storage for the ZID Debug Monitor
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
		.title	"Single Board Computer (SBC) to run on the ZID board as a target"
		.stitle	"Storage"

		.list	1
		.input	"cmn/stddef.inc"	; stddef must be first
		.list	2
		.input	"storage.inc"
		.eject

BSS		.sect

gpio_val:	.block	BYTE

; stack
;
; The registers are pushed as a pair, but labeled individually for access
; (this order is important!!!)
		.align	2		; Put on a word boundary
		.block	256		; Debug monitor stack space
sbcstk:		.equ	$

