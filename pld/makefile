#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY: all clean

PLD_MAIN	:= zid_main
PLD_BRKADDR	:= zid_ba
PLD_BRK2X	:= zid_brk2x_ctrl

#JEDECS		:= $(PLD_BRKADDR).jed $(PLD_MAIN).jed
# !!! TEMP - Only zid_main for Microchip Case 01764879 investigation
JEDECS		:= $(PLD_MAIN).jed

CUPL		:= cupl.exe
FIT1502		:= fit1502.exe
FIT1504		:= fit1504.exe
FIT1508		:= fit1508.exe

CUPLOPTS15		:= -kskxm4nlxj  # ATF15xx: Optimize product term usage Don't expand XOR (let fitter use XOR gate),
								#  m4 Espresso minimization (Good for fitter designs)
CUPLOPTS		:= -kbm2nlxj	# 22Vxx, 16Vxx: Quine-McCluskey minimization

FITOPTS_CMN		:= -CPUL -JTAG ON
FITOPTS_1502	:= -tech ATF1502AS
FITOPTS_1504	:= -tech ATF1504AS
FITOPTS_1508	:= -tech ATF1508AS
FITOPTS_PLCC44	:= -device PLCC44
FITOPTS_TQFP44	:= -device TQFP44
FITOPTS_TQFP100	:= -device TQFP100

%.tt2:	%.pld
	$(CUPL) $(CUPLOPTS15) $<

all:	$(JEDECS)

$(PLD_MAIN).jed: $(PLD_MAIN).tt2 makefile
	$(FIT1508) -i $< $(FITOPTS_1508) $(FITOPTS_TQFP100) $(FITOPTS_CMN)

$(PLD_BRKADDR).jed: $(PLD_BRKADDR).tt2 makefile
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_TQFP44) $(FITOPTS_CMN)

# The BRK2X is no longer used on the board
$(PLD_BRK2X).jed: $(PLD_BRK2X).pld
	$(CUPL) $(CUPLOPTS) $<

clean:
	rm -f *.abs *.doc *.fit *.io *.jed *.log *.lst *.oed *.pin *.pla *.sim *.tt2 *.tt3
