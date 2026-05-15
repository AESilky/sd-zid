;
; Debug Monitor for Z80 In-circuit Debugger (ZID)
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
		.title	"ZID (Z80 In-circuit Debugger) Debugger Monitor"
		.stitle	"Boot (full reset) Initialization, Breakpoint, or DOC ATTN"
BOOT		.sect	W
;		.org	0

		.input	"cmn/stddef.inc"	; stddef must be first
		.input	"cmn/board.inc"
		.input	"storage.inc"
		.input	"doccmds.inc"

		.list	1		; Don't list these included files
		.input	"diag/diag.inc"
		.eject
; ===========================
; The Debug Monitor performs the board initialization and handles the Z80 Special Reset SPRST
; that is used to regain control from target code execution when a breakpoint is hit or the
; Debug Operation Controller (DOC) signals that it wants attention (ATTN). DOC ATTN is
; signaled be an NMI when the system is in debug mode (haven't started, or have broken out of
; target mode).
;
; After board initialization is complete, the Debug Monitor does not use the alternate
; register set, the IX or IY registers, and doesn't disable or enable interrupts or change
; the interrupt mode (as regular interrupts are not used in debug mode)). Therefore, when
; the user changes those registers, enables or disables interrupts, or changes the interrupt
; mode, those changes are immediately applied. This speeds up single-step operations and 
; going into target mode.
;
; ===========================

; ===========================
;
; ROUTINE: boot - Full/Init or Breakpoint Hit or ATTN from RPM when in Target Mode.
;
; PURPOSE: Configure the board or handle a Z80 Special Reset (breakpoint or ATTN).
;
; ENTRY: _
; EXIT:  _
;
; ===========================
;
boot:
		jp	bahndlr			; Reset and Restart Entries
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
id:		.byte	"ZID DEBUG MONITOR:"	; ID
ver:		.equ	$
		.input	"version.inc"		; build date/version

nmivec:		.org	boot+66h
		jp	bahndlr			; DOC ATTN when in Debug Mode uses NMI

		.org	boot+0100h
bahndlr:	; This could be a breakpoint or DOC asking for attention
		; Or it could be a full reset
		; Treat as BRK or ATTN until we figure out for sure (save AF and SP)
		ld	(tempsp),sp
		ld	sp,tempsp
		push	af			; we can now use AF and SP
		in	a,(PCADDR_RD)		; read the PC-b0 and status. this will allow us to see if the
		ld	(brk_pc_inf),a		; reset is from a BRK or ATTN, or
		bit	ZIDINITD_B,a		; if the ZID isn't initialized, treat as Full Reset
		jp	z,doinit		;  Do the initialization
		; Break or Attention?
		and	SR_ISBRK_M|ATTN_ON_M
		jp	z,doinit		; Odd! Init'ed bit on, but not BRK or ATTN??? Go (re)init...
brkattn:	; Break or Attention Common
		; Save the rest of the registers
		ld	sp,regf			; Target SP already in 'tempsp' and af pushed on tempsp
		push	hl
		push	de
		push	bc
		; do IX, IY and the alternates
		push	iy
		push	ix
		ex	af,af'
		exx
		push	hl
		push	de
		push	bc
		push	af
		; put the primaries back (the DM doesn't use IX, IY, or alternate registers)
		exx
		ex	af,af'
		; get the IV and IE state
		ld	a,i
		push	af		; this saves i
		ld	hl,tflag
		res	tf_ie,(hl)	; mark as int disabled
		jp	po,sr0		; p/v flag = int dis (set by 'ld a,i' above)
		set	tf_ie,(hl)	; int enabled
sr0:		; we have the state of IFF2, disable ints (we don't use them)
		di
		; Read in and build up the saved PC
		; bit-15 was read to get the break status, get it back and build up the rest
		xor	a
		ld	d,a
		ld	e,a
		ld	a,(brk_pc_inf)
		; do the upper byte
		ld	b,8
pcrd1:		and	00000001b
		sla	d
		or	d
		ld	d,a
		in	a,(PCADDR_RD)	; each read reads 1 bit from 14 to 0 (15 was read at reset)
		djnz	pcrd1
		; do the lower byte
		ld	b,8
pcrd2:		and	00000001b
		sla	e
		or	e
		ld	e,a
		in	a,(PCADDR_RD)
		djnz	pcrd2
		; DE now contains the PC saved by the Special Reset
		ld	sp,tempaf	; get the target 'sp' and 'af' saved initially
		pop	bc		; 'af' into bc
		pop	hl		; 'sp' less 2
		inc	hl		;  adjust it
		inc	hl		; 'sp' now in hl
		; target: PC in DE, SP in HL, AF in BC
		ld	sp,regsav2
		push	de		; PC
		push	hl		; SP
		push	bc		; AF
		; All target registers and the Ints-Enabled state saved 
		ld	sp,dbmstk	; Set the Debug Monitor Stack
		;
		; Was this SPRST from a breakpoint or ATTN
		;
		ld	a,(brk_pc_inf)
		bit	SR_ISBRK_B,a
		jr	nz,sr_isbrk
		;
		; SPRST was from DOC wanting Attention
		ld	a,DCOKCMDRD
		jr	doccmdrd
sr_isbrk:	ld	a,DCBRKHIT
doccmdrd:	out	(RPMCTRL),a	; on ATTN this returns quickly. BRKHIT takes longer due to waking DOC up.
		in	a,(RPMDATA)
		ld	(doc_cmd),a
		;
		; process the DOC command
		;
		jp	doccmdprc

		.eject
		.org	boot+300h
;; =============
;; Restart Handlers - None of these are expected.
;;
;; Load B with the RST Number and jump to a common routine that
;; sends a status to the DOC and HALTs
;;
;;
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
		.org	onrst08+20h
onrst:		; Send an error status to DOC
		ld	a,DCRSTEXEC
		out	(RPMCTRL),a
		halt

		.eject
		.org	boot+380h
;; =============
;; Process a DOC command
;;
;; @Params
;;	A: Command from DOC (also in 'doc_cmd')
;; @Used
;;	A, C
;;
doccmdprc:	; ZZZ for now, just write it back to the RPM Data port for test and stop
		out	(RPMDATA),a
docwait_:	jr	docwait_	; Wait for the DOC to ask us to do something



		.eject
		.org	boot+400h
;; =============
;; Debug Monitor (and board) Initialization.
;;
;; The board has just done a Full Reset (power on or push button)
;; Initialize the required functionality of the board and set up the Debug Monitor (DM)
;;
;; The PC and AF registers were saved in RAM in case it was a Special Reset (SPRST)
;; for a Breakpoint Hit or DOC Attention, but it was determined that it was a Power-On
;; or Push-Button (full) reset, so those values aren't needed.
;;
;; To initialize, we do:
;; 1) RAM Test (verify the operation of the DM section of the RAM)
;; 2) Fill RAM with 0 (assuming the test passed)
;; 3) Set the CPLD flag indicating that ZID-DM Init is completed
;; 4) Notify the DOC that initialization is complete, or that it failed
;;
;; This is the only place that we use the IY register
doinit:		ld	iy,init2	; IY holds the address to return to (as we clobber ram)
		ld	hl,DB_RAMBASE
		ld	c,DB_RAMPC
		jp	ramchk_ns	; test our ram (note that we don't test the whole board)
		jr	z,init2		;  passed...
		; !!! MEMORY ERROR !!!
		; HL hold address of error, A expected, E read
		; continuously write and read it (read to D to not mess up E)
imemerr:	ld	(hl),a
		ld	d,(hl)
		jr	imemerr
init2:		; fill ram with 0
		ld	hl,DB_RAMBASE
		ld	(hl),0
		ld	de,DB_RAMBASE+1
		ld	bc,(DB_RAMPC*ONE_PAGE)-1
		dec	bc
		ldir
		; put the JP instruction in ram for the Target hop, skip, jump
		ld	a,JP_OP
		ld	(tgtskip),a
		;
		; that's it (the CPLD takes care of most of the init)
		; Setting the ZID initialized bit in the CPLD also allows
		; NMI via the DOC ATTN.
		;
		; No need to read and modify, as the SBC can't have run yet.
		ld	a,ZIDINIT_M|SYNCONLY_M	; also set breaks to SYNC
		out	(BRDCTRL),a
		; now let DOC know that we've initialized
		; and process its first request
		ld	a,DCINIT
		jp	doccmdrd	

		.eject
		.org	boot+500h
;;**************
;; @brief `tgtgo` Run the target code.
;; @ingroup dbmon
;;
;; Restores the target registers, notifies DOC that we are transitioning
;; to Target Mode, triggers the TGT_GO count down, loads the target PC
;; in the jump location, then jumps to the (ram located) target jump -
;; this is referred to as the, 'HOP, SKIP, and JUMP' to the Target.
;; 
;; @entry
;;	NONE
;;**************
tgtgo:		; restore the target registers...
		; Note: IV, IX, IY, and alternates are always valid
		;	(they aren't used by the DM), so there is no
		;	need to restore them.
		;
		ld	sp,regsav_
		pop	bc
		pop	de
		pop	hl
		;
		; don't do AF and SP until we are ready to go
		;
		ld	a,DCTGTGO	; Tell DOC we are going to Target Mode
		out	(RPMCTRL),a	;  so we don't want it to bother us for a while!
		ld	sp,(regpc)	; Get Target PC and put
		ld	(tgtjump),sp	; into ram at the JP instruction
		; do interrupt enable/disable
		ld	a,(tflag)
		bit	tf_ie,a		; ints enabled?
		jr	z,tgtgo1
		ei
tgtgo1:		ld	sp,rstgtgo_	; get ready to get AF
		out	(TGTGO),a	; And the countdown begins (no turning back now)...
		pop	af		; get AF	[mem_op: 15,14,13]
		ld	sp,(regsp)	; get SP	[mem_op: 12,11,10,9]
		nop			;		[mem_op: 8]
		nop			;		[mem_op: 7]
		nop			;		[mem_op: 6]
		jp	tgtskip		; HOP,skip,jump to target [mem_op: 5,4,3]
					; (SKIP,JUMP are [mem_op: 2,1,0])


		.byte	"!dbmon!"
		.end

