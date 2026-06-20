#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY:	all clean cleanall LIBS listcode listmap ROM

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

ASMOPTS	:= -li$(BASEPATH) -li$(CMNPATH) -i./ -d _BLD_DT=$(BUILD_DT)

PRNOPTS := --media=Letter --landscape --lines-per-page=68 --no-header --font="Courier8" --truncate-lines --no-job-header

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

listcode:	
	enscript $(PRNOPTS) dbmon.lst 

listmap:	
	enscript $(PRNOPTS) dbmon.map 

liststo:	
	enscript $(PRNOPTS) storage.lst 

clean:
	rm -f *.lst *.map *.obj
	rm -f $(ROMIMG)

cleanall:
	cd $(BASEPATH);make clean
