#
# Copyright 2026 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY:	all clean cleanall LIBS listing ROM

# Define a variable with the current date/time
BUILD_DT 	:= $(shell date +"\"%Y%m%d-%H:%M\"")
$(info BUILD_DT is: $(BUILD_DT))

ASM		:= uasz80
ALINK	:= ulink

BASEPATH	:= .
CMNPATH		:= $(BASEPATH)

CMNFILES	:= 

INCS		:= 

LOBJS	:= \
	regions.obj

ROMIMG	:= rom.bin

ASMOPTS	:= -li$(BASEPATH) -li$(CMNPATH) -i./ -d _BLD_DT=$(BUILD_DT)

%.obj:	%.asm
	$(ASM) $(ASMOPTS) $<

%.lst:	%.asm
	$(ASM) $(ASMOPTS) $<

all:	$(LOBJS)

regions.obj:	makefile regions.asm $(CMNFILES) $(INCS)


LIBS:
	cd $(BASEPATH);make

rom.bin:	makefile $(LOBJS) LIBS regions.lnk
	ulink -f=regions.lnk

ROM:	$(ROMIMG)

listing:	regions.lst
	enscript --no-header --font="Courier8" boot.lst 

clean:
	rm -f *.lst *.map *.obj
	rm -f $(ROMIMG)

cleanall:	clean
	cd $(BASEPATH);make clean
