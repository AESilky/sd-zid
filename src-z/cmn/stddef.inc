	.stitle	"Standard Definitions"
; ==============
;
; Standard Definitions.
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============
;

BYTE		.equ	1		; Length of byte
WORD		.equ	2		; Length of word
BYTE_BITS	.equ	8		; Number of Bits in a Byte
WORD_BITS	.equ	16		; Number of Bits in a Word
ONE_PAGE	.equ	256		; Bytes in a Page
ONE_KB		.equ	1024		; Bytes in a Kilobyte

EOS		.equ	0FFh		; End of String
NULL		.equ	0		;

JP_OP		.equ	0C3h		; Z80 JP Opcode
JP_ENTRY_LEN	.equ	3		; Number of bytes for a JP Entry

RET_OP		.equ	0C9h		; Z80 RET Opcode
