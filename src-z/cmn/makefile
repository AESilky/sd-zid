#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY:	all clean cleanall listbrd liststd

# Define a variable with the current date/time
BUILD_DT 	:= $(shell date +"\"%Y%m%d-%H:%M\"")
$(info BUILD_DT is: $(BUILD_DT))

ASM		:= uasz80
ALINK	:= ulink
IMGR	:= imager

BASEPATH	:= ../
CMNPATH		:= $(BASEPATH)cmn/

CMNFILES	:= $(BASEPATH)version.inc

INCS		:= 

LSTS	:= board.lst stddef.lst

ROMIMG	:= 

ASMOPTS	:= -li$(BASEPATH) -i./ -d _BLD_DT=$(BUILD_DT)

PRNOPTS := --media=Letter --landscape --lines-per-page=68 --no-header --font="Courier8" --truncate-lines --no-job-header

%.obj:	%.asm
	$(ASM) $(ASMOPTS) $<

%.lst:	%.inc
	$(ASM) $(ASMOPTS) $<

all:	$(LSTS)

listbrd:	
	enscript $(PRNOPTS) board.lst 

liststd:	
	enscript $(PRNOPTS) stddef.lst 

clean:
	rm -f *.obj *.lst

cleanall:
	cd $(BASEPATH);make clean
