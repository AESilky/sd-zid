#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY:	ROM compbins libs clean

IMGR	:= imager

COMPBINS	:= dbmon.bin sbc.bin
ROMIMG		:= rom.bin

ROM:	$(ROMIMG)

rom.bin:	makefile zid.img $(COMPBINS)
	$(IMGR) zidrom.bin -f=zid.img

compbins: $(COMPBINS)

dbmon.bin:
	cd dbm;make dbmon.bin

sbc.bin:
	cd sbc;make sbc.bin

libs:
	cd diag;make
	cd util;make

clean:
	rm -f *.lst *.map *.obj
	cd dbm;make clean
	cd diag;make clean
	cd sbc;make clean
	cd util;make clean

