; ==============
;
; Boot/Start-Up Board Diagnostics
;
; Copyright 2025-26 AESilky
; SPDX-License-Identifier: MIT License
;
; ==============
;

;;**************
;; @brief Perform a basic Paged Memory check.
;; @ingroup diag
;;
;; This is not an exhaustive test, rather it is a basic check that the memory paging
;; is functional.
;;
;; The routine does not use the stack. It modifies the Paging and Scratch registers
;; and if paging checks out it leaves the contents of the paging registers set to
;; a memory mapping that matches the paging disabled configuration when ROM/RAM-50/50
;; is in use. It enables paging during the test, but returns with paging disabled.
;;
;; Since paging does get enabled during the test, it is possible (if things aren't
;; working correctly) that the routine could go astray and not return.
;;
;; A byte of RAM in the first paging-disabled region will be read from. RAM in a
;; region that isn't used when paging is disabled will be written to. If paging isn't
;; functioning it is possible that the RAM in the first paging-disabled region will
;; be written to (that is part of the test).
;;
;; @entry
;;	IY: Return address
;; @used
;;	A,B,C,D,E,H,L
;; @return
;;	Z: Success
;;	NZ: Failure
;;		BC: Page Register Address in error
;;		 A: Expected
;;		 E: Actual
;;		 L: 0=Scratch 1=Page
;;**************
	.global	pgingchk_ns

;;**************
;; @brief Perform a basic Paging and Scratch Register check without using Stack.
;; @ingroup diag
;;
;; Assures that the Paging Registers and Scratch Registers are functional.
;;
;; The routine does not use the stack. It modifies the Paging and Scratch registers
;; and does not restore them. Memory paging should not be enabled.
;;
;; @entry
;;	IY: Return address
;; @used
;;	A,B,C,D,E,H,L
;; @return
;;	Z: Success
;;	NZ: Failure
;;		BC: Page Register Address in error
;;		 A: Expected
;;		 E: Actual
;;		 L: 0=Scratch 1=Page
;;**************
	.global	pgrgchk_ns

;;**************
;; @brief Perform a basic RAM read/write check on a block of memory without using Stack.
;; @ingroup diag
;;
;; Assures that the block of memory can be written and read and that memory within the
;; block is unique (when a location is written it is not read back at another address).
;;
;; The routine does not use the stack, as memory might not be working.
;;
;; The block must start at a page boundary and the size is in pages (256 bytes).
;;
;; @entry
;;	IY: Return address
;;	 H: Start page
;;	 C: Number of pages to check (0 will check 256 pages)
;; @used
;;	NONE
;; @return
;;	Z: Success
;;	NZ: Failure
;;		HL: Address in error
;;		 A: Expected
;;		 E: Actual
;;**************
	.global	ramchk_ns

;;**************
;; @brief Fill pages of memory with a value.
;; @ingroup diag
;;
;; Fills the memory starting at HL for the number of pages in C with the
;; value in A.
;;
;; The routine does not use the stack, as it might write over it.
;;
;; The block must start at a page boundary and the size is in pages (256 bytes).
;;
;; @entry
;;	IY: Return address
;;	HL: Start address
;;	 C: Number of pages to check (0 will fill 256 pages)
;;	 A: Value
;; @used
;;	A, BC, HL
;; @return
;;	NONE
;;**************
	.global ramfill_ns
