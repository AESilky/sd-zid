; ==============
;
; ZID (Z80 In-Circuit Debugger) RP Module Debug Operation Controller (DOC)
;  Command and Response Constants - SEE: RPM-Progifc.md for the interface
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============
; Debug Monitor (DM) to Debug Operation Controller (DOC) State/Status indicators
;
DCINIT:		.equ	080h		; DC has finished initialization
DCOPDONE:	.equ	081h		; DC requested operation complete
DCOKCMDRD	.equ	082h		; OK, reading command
DCBRKHIT	.equ	084h		; Breakpoint hit
DCTGTGO		.equ	088h		; DC transitioning to Target Mode
					; (so don't interrupt for a bit)

DCRSTEXEC	.equ	0C1h		; DC executed a RST instruction (not expected)

