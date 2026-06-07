#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#

.PHONY:	all clean listcode

# Define a variable with the current date/time
BUILD_DT 	:= $(shell date +"\"%Y%m%d-%H:%M\"")

ASM		:= uasz80
ALINK	:= ulink

BASEPATH	:= ../
CMNPATH		:= $(BASEPATH)cmn/

CMNFILES	:= $(CMNPATH)stddef.inc

ASMOPTS	:= -li$(BASEPATH) -li$(CMNPATH) -i./ -d _BLD_DT=$(BUILD_DT)

PRNOPTS := --media=Letter --landscape --lines-per-page=68 --no-header --font="Courier8" --truncate-lines --no-job-header


%.obj:	%.asm
	$(ASM) $(ASMOPTS) $<

%.lst:	%.asm
	$(ASM) $(ASMOPTS) $<

all: util.obj

util.obj:	makefile util.asm util.inc $(CMNFILES)


listcode: 
	enscript $(PRNOPTS) diag.lst 


clean:
	rm -f *.lst *.map *.obj
