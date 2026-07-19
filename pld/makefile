#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY: all clean

ZID_MAIN			:= zid_main
ZID_BRKPCL			:= zid_brkpcl
ZID_BDATA_GPIO 		:= zid_bd_gpio

BPEXP_DISPKBD		:= dispkbd


JEDECS		:= $(ZID_BRKPCL).jed $(ZID_BDATA_GPIO).jed $(ZID_MAIN).jed $(BPEXP_DISPKBD).jed

CUPL		:= cupl.exe
FIT1502		:= fit1502.exe
FIT1504		:= fit1504.exe
FIT1508		:= fit1508.exe		# C:/Wincupl/WinCupl/Fitters/fit1508.exe

CUPLOPTS		:= -kbm2nlxj	# 22Vxx, 16Vxx: Quine-McCluskey minimization
CUPLOPTS15XX	:= -ksm4lj  	# ATF15xx: Optimize product term usage (-ks)
								#  Don't expand XOR (-kx let fitter use XOR gate),
								#  m4 Espresso minimization (Good for fitter designs)

FITOPTS_CMN		:= -CPUL -JTAG ON
FITOPTS_1502	:= -tech ATF1502AS
FITOPTS_1504	:= -tech ATF1504AS
FITOPTS_1508	:= -tech ATF1508AS
FITOPTS_PLCC44	:= -device PLCC44
FITOPTS_TQFP44	:= -device TQFP44
FITOPTS_TQFP100	:= -device TQFP100

%.tt2:	%.pld
	$(CUPL) $(CUPLOPTS15XX) $<

all:	$(JEDECS)

$(ZID_MAIN).jed: $(ZID_MAIN).tt2 makefile
	$(FIT1508) -i $< $(FITOPTS_1508) $(FITOPTS_TQFP100) $(FITOPTS_CMN)

$(ZID_BRKPCL).jed: $(ZID_BRKPCL).tt2 makefile
	$(FIT1508) -i $< $(FITOPTS_1508) $(FITOPTS_TQFP100) $(FITOPTS_CMN)

$(ZID_BDATA_GPIO).jed: $(ZID_BDATA_GPIO).tt2 makefile
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_TQFP44) $(FITOPTS_CMN)

$(BPEXP_DISPKBD).jed: $(BPEXP_DISPKBD).tt2 makefile
	$(FIT1502) -i $< $(FITOPTS_1502) $(FITOPTS_TQFP44) $(FITOPTS_CMN)

clean:
	rm -f *.abs *.doc *.fit *.io *.jed *.log *.lst *.oed *.pin *.pla *.sim *.tt2 *.tt3
