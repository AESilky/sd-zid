#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#

# Define a variable with the current date/time
.PHONY:	all BIN LIBS clean cleanall listcode listmap liststo

BUILD_DT 	:= $(shell date +"\"%Y%m%d-%H:%M\"")

ASM		:= uasz80
ALINK	:= ulink
IMGR	:= imager

BASEPATH	:= ../
CMNPATH		:= $(BASEPATH)cmn/

CMNFILES	:= $(BASEPATH)version.inc $(CMNPATH)board.inc $(CMNPATH)stddef.inc
LIBFILES	:= $(BASEPATH)util/util.inc

INCS		:= \
		storage.inc\
		$(BASEPATH)diag/diag.inc

LOBJS	:= \
	sbc1.obj \
	storage.obj

ROMIMG	:= sbc1.obj

ASMOPTS	:= -li$(BASEPATH) -li$(CMNPATH) -i./ -d _BLD_DT=$(BUILD_DT)

PRNOPTS := --media=Letter --landscape --lines-per-page=68 --no-header --font="Courier8" --truncate-lines --no-job-header

%.obj:	%.asm
	$(ASM) $(ASMOPTS) $<

%.lst:	%.asm
	$(ASM) $(ASMOPTS) $<

all:	$(LOBJS)

sbc1.obj:	makefile sbc1.asm $(CMNFILES) $(LIBFILES) $(INCS)

storage.obj: makefile storage.asm storage.inc $(CMNFILES)

sbc.bin:	makefile $(LOBJS) LIBS sbc.lnk
	$(ALINK) -f=sbc.lnk

LIBS:
	cd $(BASEPATH);make libs

BIN:	$(ROMIMG)


listcode:	
	enscript $(PRNOPTS) sbc1.lst 

listmap:	
	enscript $(PRNOPTS) scb1.map 

liststo:	
	enscript $(PRNOPTS) storage.lst 

clean:
	rm -f *.lst *.map *.obj
	rm -f $(ROMIMG)

cleanall:
	cd $(BASEPATH);make clean
