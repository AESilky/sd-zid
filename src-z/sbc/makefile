#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#

# Define a variable with the current date/time
.PHONY:	all clean cleanall listing

BUILD_DT 	:= $(shell date +"\"%Y%m%d-%H:%M\"")

ASM		:= uasz80
ALINK	:= ulink
IMGR	:= imager

BASEPATH	:= ../
CMNPATH		:= $(BASEPATH)cmn/

CMNFILES	:= $(BASEPATH)version.inc $(CMNPATH)board.inc $(CMNPATH)stddef.inc

INCS		:= \
		storage.inc\
		$(BASEPATH)diag/diag.inc

LOBJS	:= \
	sbc1.obj \
	storage.obj

ROMIMG	:= rom.bin

ASMOPTS	:= -li$(BASEPATH) -i./ -d _BLD_DT=$(BUILD_DT)


%.obj:	%.asm
	$(ASM) $(ASMOPTS) $<

%.lst:	%.asm
	$(ASM) $(ASMOPTS) $<

all:	$(LOBJS)

sbc1.obj:	makefile sbc1.asm $(CMNFILES) $(INCS)

storage.obj: makefile storage.asm storage.inc $(CMNFILES)

sbc.bin:	makefile $(LOBJS) LIBS sbc.lnk
	$(ALINK) -f=sbc.lnk

rom.bin:	makefile dbmon.bin
	$(IMGR) rom.bin -f=zid.img

LIBS:
	cd $(BASEPATH);make libs

ROM:	$(ROMIMG)


listing: sbc1.lst
	enscript --no-header --font="Courier8" sbc1.lst 


clean:
	rm -f *.lst *.map *.obj
	rm -f $(ROMIMG)

cleanall:
	cd $(BASEPATH);make clean
