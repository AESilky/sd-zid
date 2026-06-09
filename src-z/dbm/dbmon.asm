	.title	"ZID (Z80 In-circuit Debugger) Debugger Monitor"
	.stitle	"Boot (full reset) Initialization, Breakpoint, or DOC ATTN"
;
; Debug Monitor for Z80 In-circuit Debugger (ZID)
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
BOOT		.sect	W
;		.org	0

		.list	1		; 1 = Don't list these included files, >=2 will list
		.input	"board.inc"
		.input	"storage.inc"
		.input	"doccmds.inc"
		.input	"diag/diag.inc"

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
		jp	bahdlr			; Reset and Restart Entries
rst08:		.org	boot+08h
		jp	bar8hdlr		; ! RST8 used in place of SPRST when debugging with ZED
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
		jp	dmdocattn		; DOC ATTN when in Debug Mode uses NMI

		.align	4
		;--------------------------------------------------------------
		; Break|ATTN handler from RST8 (for debugging with ZED)
		;  The RST8 gets injected twice when due to ATTN.
		; ! ZZZ the breadboard doesn't have the PC latch, so
		; ! we'll get the PC off the stack and then handle like a SPRST
		;
bar8hdlr:	.equ	$
		ld	(tempsp),sp
		ld	sp,tempsp
		push	af			; we can now use AF and SP
		ld	a,(zflag)
		set	zf_dbzed,a		; indicate that we are debugging with ZED (RST8 rather than SPRST)
		ld	(zflag),a
		jp	dbzent

		.org	boot+0100h
		;--------------------------------------------------------------
		; This could be a breakpoint or DOC asking for attention
		; Or it could be a full reset
		; Treat as BRK or ATTN until we figure out for sure (save AF and SP)
		;
bahdlr:		.equ	$
		ld	(tempsp),sp
		ld	sp,tempsp
		push	af			; we can now use AF and SP
dbzent:		in	a,(PCADDR_RD)		; read the PC-b0 and status. this will allow us to see if the
		ld	(brk_pc_inf),a		; reset is from a BRK or ATTN, or
		bit	ZIDINITD_B,a		; if the ZID isn't initialized, treat as Full Reset
		jp	z,doinit		;  Do the initialization
		; Break (hard or soft) or Attention?
		and	SR_ISHBRK_M|SR_ISSBRK_M|ATTN_ON_M
		jp	z,doinit		; Odd! Init'ed bit on, but not BRK or ATTN??? Go (re)init...
brkattn:	; Break or Attention Common
		; Save the rest of the registers
		ld	sp,regf			; Target SP already in 'tempsp' and af pushed on tempsp
		push	hl
		push	de
		push	bc
		; do the alternates
		ex	af,af'
		exx
		push	hl
		push	de
		push	bc
		push	af
		; put the primaries back (the DM doesn't use IX, IY, or alternate registers after init)
		exx
		ex	af,af'
		; get the IV and IE state
		ld	a,i
		push	af		; this saves i and p/v which indicates Ints Enabled/Disabled
		; we have the state of IFF2, disable ints (we don't use them)
		di
		;
		; Clear the Single-Step control bit (don't want it on unless doing a SS)
		in	a,(BRDCTRL)
		and	~SSTEP_M
		out	(BRDCTRL),a
		;
		; Read in and build up the saved PC
		ld	a,(zflag)
		bit	zf_dbzed,a
		jr	z,sr0		; Special Reset => (not RST8 (ZED/SELF) debugging)
		;
		; Debugging using RST8 rather than SPRST (to allow ZED to be used)
		;
		res	zf_dbzed,a	; clear the DB-ZED flag for next BRK/ATTN
		ld	(zflag),a
		; Tell DOC we'll be right there so that it will remove ATTN
		ld	a,DMBRT
		out	(RPMCTRL),a
		ld	sp,tempaf	; get the target 'sp' and 'af' saved initially
		pop	bc		; 'AF' into BC
		ld	sp,dbmstk	; set our stack (for the calls below)
		ld	hl,(tempsp)	; get the saved SP into HL
		; ATTN generates 2 forced RST8 so the PC was the first PC PUSH, then there was a 2nd
		inc	hl		; remove the 2nd PC push
		inc	hl
		; now the pushed PC needs to be read from target memory (for ZED DB we make sure target SP is valid)
		call	trdbyte		; low byte
		ld	d,e
		inc	hl
		call	trdbyte		; high byte
		ld	a,e
		ld	e,d
		ld	d,a		; now DE has the correct order
		; Since the forced RST8 instruction was executed, the pushed PC is actually 1 too much
		dec	de		; Now DE has the correct PC
		inc	hl		; Now HL has correct SP
		ld	(tempsp),hl	; save for resuming
		; registers now match what SPRST builds up below: PC in DE, SP in HL, AF in BC
		jr	sr1		;  so bypass the PC read & build steps
		;	
sr0:		; bit-15 was read to get the break status, get it back and build up the rest
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
		pop	hl		; 'sp'
		;
sr1:		; target: PC in DE, SP in HL, AF in BC
		ld	sp,regsav2
		push	iy		; do IY and IX now, so the 2-byte registers are together
		push	ix
		push	de		; PC
		push	hl		; SP
		push	bc		; AF
		; All target registers and the Ints-Enabled state saved 
		ld	sp,dbmstk	; Set the Debug Monitor Stack
		;
		; Was this SPRST from a breakpoint, single-step, or ATTN
		;
		ld	a,(brk_pc_inf)
		and	SR_ISHBRK_M|SR_ISSBRK_M
		ld	a,DMBRKHIT
		jp	nz,doccmdrd	; Hard or Soft Breakpoint =>
		;
		; ATTN or Single-Step
		ld	a,DMOKCMDRD
		jp	doccmdrd


	.stitle	"RESTART Handlers"
		.org	boot+300h
;; =============
;; Restart Handlers - None of these are expected.
;;
;; Load B with the RST Number and jump to a common routine to
;; send a status to the DOC and HALT
;;
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
		.align	4	; to aid debugging
onrst:		; Send an error status to DOC and HALT (lights the HALT LED)
		ld	a,DMRSTEXEC
		out	(RPMCTRL),a
		halt		; Doc can wake us up if it wants


	.stitle	"DOC (Debug Operations Controller) Interaction"
		.org	boot+400h
;; =============
;; Send DOC our status and then go IDLE
;;
;; @Params
;;	A: Status to send
;; @Return
;;	NO - This goes IDLE waiting for wake-up by DOC
;;
docstat:	out	(RPMCTRL),a
docwait_:	ld	sp,dbmstk
dw2_:		jr	dw2_		; Wait for the DOC to ask us to do something

		.align	4
doccmdrd:	out	(RPMCTRL),a	; on ATTN this returns quickly. DMBRKHIT takes longer due to waking DOC up.
		in	a,(RPMDATA)
		ld	(doc_cmd),a
		;
		; process the DOC command
		;
		jp	doccmdprc

		.align	4
		; DOC wants ATTN and we are already in Debug Mode (so no need to do any register saving)
dmdocattn:	; We come here from an NMI, but we don't need to return.
		ld	a,DMOKCMDRD	; sending this to DOC will cause it to turn off ATTN
		jr	doccmdrd


		.align	4
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
		jp	docwait_


	.stitle	"TARGET Operations"
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
;;	Target registers and requested PC in our Target Storage Locations
;;**************
tgtgo:		; make sure Single-Step is off
		in	a,(BRDCTRL)
		and	~SSTEP_M
		out	(BRDCTRL),a
		jr	tgthop
		.align	4		; to aid debugging
tgtstep:	; make sure Single-Step is on
		in	a,(BRDCTRL)
		or	SSTEP_M
		out	(BRDCTRL),a
		;
		; Common 'HOP' code for GO and Single-Step
		;
tgthop:		ld	a,DMTGTGO	; Tell DOC we are going to Target Mode (RUN or SingleStep)
		out	(RPMCTRL),a	;  so we don't want it to bother us for a while!
		ld	sp,(regpc)	; Get Target PC and put
		ld	(tgtjump),sp	; into ram at the JP instruction
		;
		; restore the target registers...
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
		; do interrupt enable/disable
		ld	a,(regied)
		bit	ie_b,a		; ints enabled?
		jr	z,tgthop1
		ei
tgthop1:	ld	sp,rstgtgo_	; get ready to get AF
		pop	af		; get AF
		out	(TGTGO),a	; And the countdown begins:
		ld	sp,(regsp)	; get SP	[mem_op: 1,2,3,4, 5,6]
		nop			; [mem_op: 7]
		nop			; [mem_op: 8]
		nop			; [mem_op: 9]
					; done with HOP. Now...
		jp	tgtskip		; skip & jump to target [mem_op: 10,11,12]
					; (SKIP,JUMP are [mem_op: 13,14,15 - 16 in TGT])

	.stitle	"Target Write Byte"
		.align	4		; to aid debugging
;;**************
;; @brief `twrbyte` Write the byte in E to the target address in HL.
;; @ingroup dbmon
;;
;; @entry
;;	E: Byte to write
;;	HL: Address
;; @uses
;;	A
;;**************
twrbyte:	ld	a,(zflag)
		bit	zf_usezid,a	; Use the ZID's memory?
		jr	nz,wrbytez	;  yes->
		; set Single-Op
		in	a,(BRDCTRL)
		or	SOP_M
		out	(BRDCTRL),a
		out	(TGTGO),a	; Countdown begins:
		nop			; MEMOP 1
		nop			; MEMOP 2
		nop			; MEMOP 3
wrbytez:	ld	(hl),e		; MEMOP 4 - MEMOP 5 write the byte
		; clear Single-Op
		and	~SOP_M
		out	(BRDCTRL),a
		ret


	.stitle	"Target Read Byte"
		.align	4		; to aid debugging
;;**************
;; @brief `trdbyte` Read a byte from the target address in HL into E.
;; @ingroup dbmon
;;
;; @entry
;;	HL: Address
;; @uses
;;	A
;; @return
;;	E: Byte read
;;**************
trdbyte:	ld	a,(zflag)
		bit	zf_usezid,a	; Use the ZID's memory?
		jr	nz,rdbytez	;  yes->
		; set Single-Op
		in	a,(BRDCTRL)
		or	SOP_M
		out	(BRDCTRL),a
		out	(TGTGO),a	; Countdown begins:
		nop			; MEMOP 1
		nop			; MEMOP 2
		nop			; MEMOP 3
rdbytez:	ld	e,(hl)		; MEMOP 4 - MEMOP 5 read the byte
		; clear Single-Op
		and	~SOP_M
		out	(BRDCTRL),a
		ret

	.stitle	"Target Out Byte"
		.align	4		; to aid debugging
;;**************
;; @brief `toutbyte` Output the byte in E to the target port in BC.
;; @ingroup dbmon
;;
;; @entry
;;	E: Byte to output
;;	BC: Address
;; @uses
;;	A
;;**************
toutbyte:	ld	a,(zflag)
		bit	zf_usezid,a	; Use the ZID's port?
		jr	nz,obytez	;  yes->
		; set Single-Op
		in	a,(BRDCTRL)
		or	SOP_M
		out	(BRDCTRL),a
		out	(TGTGO),a	; Countdown begins:
		nop			; MEMOP 1
		nop			; MEMOP 2
obytez:	out	(c),e			; MEMOP 3&4 - output the byte
		; clear Single-Op
		and	~SOP_M
		out	(BRDCTRL),a
		ret


	.stitle	"Target In Byte"
		.align	4		; to aid debugging
;;**************
;; @brief `tinbyte` Input a byte from the target port in BC into E.
;; @ingroup dbmon
;;
;; @entry
;;	BC: Address
;; @uses
;;	A
;; @return
;;	E: Byte input
;;**************
tinbyte:	ld	a,(zflag)
		bit	zf_usezid,a	; Use the ZID's memory?
		jr	nz,inbytez	;  yes->
		; set Single-Op
		in	a,(BRDCTRL)
		or	SOP_M
		out	(BRDCTRL),a
		out	(TGTGO),a	; Countdown begins:
		nop			; MEMOP 1
		nop			; MEMOP 2
inbytez:	in	e,(c)		; MEMOP 3&4 - input the byte
		; clear Single-Op
		and	~SOP_M
		out	(BRDCTRL),a
		ret


	.stitle	"Initialization"
		.align	4
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
;; 3) Determine the number of Breakpoint Address Units
;; 4) Initialize memory structures for the breakpoint units (both 1 and 2N)
;; 5) Set the CPLD flag indicating that ZID-DM Init is completed
;; 6) Notify the DOC that initialization is complete, or that it failed
;;
;; This is the only place that we use the IY register. Once initialization is complete
;; only the primary base registers are used.
;;
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
		ldir
		; put the JP instruction in ram for the Target hop, skip, jump
		ld	a,JP_OP
		ld	(tgtskip),a
		;
		; Determine how many Breakpoint Address Units (BAU) there are.
		; 1 & 2 are on the main board, so they should be there, but
		; the check has to shift through them, so we check them as well.
		;
		xor	a
		ld	e,a		; keep the count in E
		; to test, we shift an enable bit through the address enable
		; registers and check the feedback at every other enable
		; (the high enable)
		;
		; first, clear the enables
		out	(BRKENCLR),a	; (the data doesn't matter)
		dec	a		; put a 1 in bit 7 for the enable
		out	(BRKENCLK),a	; shift an enable into Break-1 addr low enable
		xor	a		;  only run a single enable bit through the registers
		out	(BRKENCLK),a	;  high enable
		ld	b,14		; this is the most we support
initbrkt:	in	a,(PCADDR_RD)	; this includes the break hardware detect bit
		and	BRK_HWD_M
		jr	z,initbrkc	; if the read bit is zero (pin HIGH) the hardware isn't there
		; hardware is there
		inc	e		; bump the count
		xor	a		;  make sure to shift in a 0 to the lower level
		out	(BRKENCLK),a	;  next - low enable
		out	(BRKENCLK),a	;  next - high enable
		djnz	initbrkt
initbrkc:	; we have the hardware breakpoint count in E
		ld	a,e
		ld	(baucnt),a
		;
		; that's it (the CPLD takes care of most of the init)
		; Setting the ZID initialized bit in the CPLD also allows
		; NMI via the DOC ATTN.
		;
		; No need to read and modify, as the SBC can't have run yet.
		ld	a,ZIDINIT_M|SYNCONLY_M	; also set breaks to SYNC
		out	(BRDCTRL),a
		; now let DOC know that we've initialized
		;  and go idle...
		ld	a,DMINIT
		jp	docstat	

		.align	2
		.byte	"!dbmon!"
		.end

