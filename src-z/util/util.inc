; ==============
;
; Utility Routines
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============
;

; ===========================
;
; @brief swapends - Swap the 'ends' of a byte value
; @ingroup util
;
; Transform a byte value where bit-7 is the most significant bit
; to a byte value where bit-0 is the most significant bit.
;
; This is primarily intended for I/O use where the port bits are
; the inverse of the values being used internally. 
;
; @entry
;	E: value to convert
; @used
;
; @return
;	D: converted value
; ===========================
;
	.global	swapends
