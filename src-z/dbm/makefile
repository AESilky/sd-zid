#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY:	all clean cleanall LIBS listing ROM

# Define a variable with the current date/time
BUILD_DT 	:= $(shell date +"\"%Y%m%d-%H:%M\"")
$(info BUILD_DT is: $(BUILD_DT))

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
	dbmon.obj \
	storage.obj

ROMIMG	:= rom.bin

ASMOPTS	:= -li$(BASEPATH) -i./ -d _BLD_DT=$(BUILD_DT)

%.obj:	%.asm
	$(ASM) $(ASMOPTS) $<

%.lst:	%.asm
	$(ASM) $(ASMOPTS) $<

all:	$(LOBJS)

dbmon.obj:		makefile dbmon.asm $(CMNFILES) $(INCS)

storage.obj:	makefile storage.asm storage.inc $(CMNFILES)

dbmon.bin:	makefile $(LOBJS) LIBS dbmon.lnk
	$(ALINK) -f=dbmon.lnk

rom.bin:	makefile dbmon.bin
	$(IMGR) rom.bin -f=zid.img

LIBS:
	cd $(BASEPATH);make libs

ROM:	$(ROMIMG)

listing:	dbmon.lst
	enscript --no-header --font="Courier8" dbmon.lst 

clean:
	rm -f *.lst *.map *.obj
	rm -f $(ROMIMG)

cleanall:
	cd $(BASEPATH);make clean
