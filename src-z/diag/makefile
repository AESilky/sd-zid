#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#

# Define a variable with the current date/time
.PHONY:	all clean listing

BUILD_DT 	:= $(shell date +"\"%Y%m%d-%H:%M\"")

ASM		:= uasz80
ALINK	:= ulink

BASEPATH	:= ../
CMNPATH		:= $(BASEPATH)cmn/

CMNFILES	:= $(CMNPATH)board.inc $(CMNPATH)stddef.inc

ASMOPTS	:= -li$(BASEPATH) -i./ -d _BLD_DT=$(BUILD_DT)


%.obj:	%.asm
	$(ASM) $(ASMOPTS) $<

%.lst:	%.asm
	$(ASM) $(ASMOPTS) $<

all: diag.obj

diag.obj:	makefile diag.asm diag.inc $(CMNFILES)


listing: diag.lst
	enscript --no-header --font="Courier8" diag.lst 


clean:
	rm -f *.lst *.map *.obj
