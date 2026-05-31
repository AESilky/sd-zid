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
; These are sent from the DM to the DOC when the DM initializes, a breakpoint is
; hit, and in response to commands sent from the DOC.
;
DMINIT		.equ	080h		; DM has finished initialization
DMOPDONE	.equ	081h		; DOC requested operation complete
DMOKCMDRD	.equ	082h		; OK, reading command
DMBRKHIT	.equ	084h		; Breakpoint hit
DMTGTGO		.equ	088h		; DM transitioning to Target Mode
					; (so don't interrupt for a bit)

DMRSTEXEC	.equ	0C1h		; DM executed a RST instruction (ERROR)

; ==============
; DOC Commands for the DM.
;
; In the descriptions for these commands, the _direction_ is relative to the
; DOC. So, 'GET REGISTERS+' means that the DOC wants all of the registers from
; the DC. While, 'PUT REGISTERS+' means that the DM should read all the register
; values from the DOC and assign them (as it known how to do).
;
; When the DM finishes processing the command (reading/writing data and doing
; any associated work) the DM writes DMOPDONE to the control port.
;
; All 16-bit transfers are low-byte then high-byte.
;

; --------------
; Do Nothing, just go IDLE.
;
DCNOP		.equ	000h

; --------------
; Is the target the (same board) SBC?
;
; DC Sends to the data register:
; 1 BYTE: 0=FALSE, Non-Zero=TRUE
;
DCISSBC		.equ	008h

;; ==============
;;
;; 0x Commands (other than NOP and SBC Mode) are primarily used when going
;; TO/FROM Target Mode.
;;
;; These are used to get and set the complete Z80 state en-mass.
;;

; --------------
; GET the complete set of registers (a different command can be used to just
; get the primary, alternate, or single, register)
;
; DC Sends to the data register:
; 26 BYTES: IE/ID(bit-2),IV,(alts)F',A',C',B',E',D',L',H',
;	    (pri)C,B,E,D,L,H,F,A,SP[l/h],PC[l/h],IX[l/h],IY[l/h],
;     
;
DCGREGALL	.equ	001h

; --------------
; PUT the complete set of registers (a different command can be used to just
; put the primary, alternate, or single, register)
;
; DC Reads from the data register:
; 26 BYTES: IE/ID(bit-2),IV,(alts)F',A',C',B',E',D',L',H',
;	    (pri)C,B,E,D,L,H,F,A,SP[l/h],PC[l/h],IX[l/h],IY[l/h],
;
DCPREGALL	.equ	002h

; --------------
; Get IE/ID(bit-2) and IV
;
; DC Sends to the data register
; 2 BYTES: IE/ID(bit-2),IV
DCGIEDV		.equ	004h

; --------------
; Put IE/ID(bit-2) and IV
;
; DC Reads from the data register
; 2 BYTES: IE/ID(bit-2),IV
DCPIEDV		.equ	005h

; --------------
; GET a (byte) register. The register is encoded in the lower 4 bits (using a
; value that is in the same order as the registers in the 'ALL' commands. The
; value can be used as an index if adjusted to skip IED and I.
; (this is not the encoding used by Zilog for the registers in OP Codes)
;   F': 0000
;   A': 0001
;   C': 0010
;   B': 0011
;   D': 0100
;   E': 0101
;   L': 0110
;   H': 0111
;   F : 1000
;   A : 1001
;   C : 1010
;   B : 1011
;   D : 1100
;   E : 1101
;   L : 1110
;   H : 1111
;
; DM Sends to the data register:
; 1 BYTE
;
DCGREG		.equ	010h		; 10..1F

;
; Register mask values for the low 4-bits for DCGREG and DCPREG
DCRFP_M		.equ	00h
DCRAP_M		.equ	01h
DCRCP_M		.equ	02h
DCRBP_M		.equ	03h
DCREP_M		.equ	04h
DCRDP_M		.equ	05h
DCRLP_M		.equ	06h
DCRHP_M		.equ	07h
DCRF_M		.equ	08h
DCRA_M		.equ	09h
DCRC_M		.equ	0Ah
DCRB_M		.equ	0Bh
DCRE_M		.equ	0Ch
DCRD_M		.equ	0Dh
DCRL_M		.equ	0Eh
DCRH_M		.equ	0Fh


;; ==============
;;
;; 2x and 3x Commands Get/Put single registers, bytes and pages of memory,
;; and ports (I/O).
;;

; --------------
; PUT a register. The register is encoded in the lower 4 bits
; (the same a GET REG)
;   F': 0000
;   A': 0001
;   C': 0010
;   B': 0011
;   D': 0100
;   E': 0101
;   L': 0110
;   H': 0111
;   F : 1000
;   A : 1001
;   C : 1010
;   B : 1011
;   D : 1100
;   E : 1101
;   L : 1110
;   H : 1111
;
; DC Reads from the data register:
; 1 BYTE
;
DCPREG		.equ	020h		; 20..2F

; --------------
; GET a 16-bit register. The register is encoded in the lower 2 bits
; in the same order as the registers in the 'ALL' commands.
;   SP: 00 
;   PC: 01
;   IX: 10
;   IY: 11
;   
; DC Sends to the data register:
; 2 BYTES (L/H)
;
DCGRP		.equ	030h		; 30..33

; --------------
; PUT a 16-bit register. The register is encoded in the lower 2 bits
;   SP: 00 
;   PC: 01
;   IX: 10
;   IY: 11
;   
; DC Reads from the data register:
; 2 BYTES (L/H)
;
DCPRP		.equ	034h		; 34..37

; --------------
; GET a byte of memory.
;   
; DC Reads from the data register:
; 2 BYTES (L/H): ADDRESS
; DC Sends to the data register:
; 1 BYTE: VALUE
;
DCGMB		.equ	038h

; --------------
; PUT a byte of memory.
;   
; DC Reads from the data register:
; 3 BYTES (L/H): ADDRESS, VALUE
;
DCPMB		.equ	039h

; --------------
; GET a page of memory.
;   
; DC Reads from the data register:
; 2 BYTES (L/H): START ADDRESS
; DC Sends to the data register:
; 256 BYTES: VALUE0, VALUE1, ..., VALUE255
;
DCGMP		.equ	03Ah

; --------------
; PUT a page of memory.
;   
; DC Reads from the data register:
; 258 BYTES (L/H): ADDRESS, VALUE0, VALUE1, ..., VALUE255
;
DCPMP		.equ	03Bh

; --------------
; GET a byte from a port.
;   
; DC Reads from the data register:
; 2 BYTES (L/H): ADDRESS
; DC Sends to the data register:
; 1 BYTE: VALUE
;
DCGPB		.equ	03Ch

; --------------
; PUT a byte to a port.
;   
; DC Reads from the data register:
; 3 BYTES (L/H): ADDRESS, VALUE
;
DCPPB		.equ	03Dh


;; ==============
;;
;; 4x Commands Get the number of secondary breakpoints, and configure
;; all the breakpoints.
;;

; --------------
; GET the number of 'secondary' breakpoint addresses supported.
; The ZID has a 'primary' breakpoint that supports address, data+mask, operation
; and 1 or more 'secondary' breakpoints that share an operation, but can have
; difference addresses.
;
; DC Sends to the data register:
; 1 BYTE: Number
;
DCGBRKCNT	.equ	040h

; --------------
; SET Break-1 parameters.
; The ZID's 'primary' breakpoint supports address, data+mask, operation.
; 
; This command completely specifies the breakpoint.
; This command includes a bit to indicate if the break should be enabled or
; disabled. There is also a separate command to just enable/disable breakpoints.
;
; Specifying Break-1 requires:
; OPERATION (one of): IF,MW,MR,AM,PW,PR,AP (3-bits)
; ENABLE: TRUE/FALSE (1-bit)
; USE-ADDRESS: TRUE/FALSE (1-bit)
; ADDRESS: LOW/HIGH
; DATA: BYTE
; MASK (DATA DON'T CARE BITS): BYTE (bits: 1 is don't care)
;
; DC Reads from the data register:
; 5 BYTES: UA[4]|EN[3]|OP[2..0], ADDR(L/H), DATA, MASK
;
DCPBRKS1	.equ	041h

DCBRKOP_IF	.equ	01h		; Instruction Fetch
DCBRKOP_MW	.equ	02h		; Memory Write
DCBRKOP_MR	.equ	03h		; Memory Read
DCBRKOP_AM	.equ	04h		; Any Memory Operation (except Refresh)
DCBRKOP_PW	.equ	05h		; Port (I/O) Write
DCBRKOP_PR	.equ	06h		; Port (I/O) Read
DCBRKOP_AP	.equ	07h		; Any Port (I/O) Operation
;
DCBRKEN_M	.equ	00001000b	; Enable/Disable (bit mask)
;
DCBRKUA_M	.equ	00010000b	; Use Address (bit mask)

; --------------
; SET Break-2..n address.
; The ZID's 'secondary' breakpoints support address, and a shared operation.
; 
; This range of commands specifies the breakpoint address 2..N.
; It is possible to use the second breakpoint operation without any addresses.
; To do that, only the `DCBRK2OP` command needs to be used, and the
; `USE ADDRESS` bit cleared.
;
; The Command Code indicates the Break Number to set the address for:
; 	82: Break-2
;	83: Break-3
;	...
;	8E: Break-14 (the maximum supported)
;
; DC Reads from the data register:
; 2 BYTES: ADDR(L/H)
;
DCPBRK2NA	.equ	042h		; 42..4E Break-2..Break-14

; --------------
; SET Break-2..n operation.
; The ZID's 'secondary' breakpoints support address, and a shared operation.
; 
; This sets the operation, enable/disable, and use-address for all the
; secondary breakpoints.
;
; OPERATION (one of): IF,MW,MR,AM,PW,PR,AP (3-bits)
; ENABLE: TRUE/FALSE (1-bit)
; USE-ADDRESS: TRUE/FALSE (1-bit)
; (same value and bits as for the `DCGBRKS1` command)
;
; DC Reads from the data register:
; 1 BYTE: UA[4]|EN[3]|OP[2..0]
;
DCPBRK2NO	.equ	04Fh


;; ==============
;;
;; 5x Commands are tests and board exorcisers.
;;

