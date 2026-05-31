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
SBC_ROMBASE	.equ	00000h
SBC_ROMPC	.equ	080h
SBC_RAMBASE	.equ	08000h
SBC_RAMPC	.equ	080h

; Memory locations
;
	.global gpio_val	; Last value of GPIO
	
	.global sbcstk		; SBC stack (top)
