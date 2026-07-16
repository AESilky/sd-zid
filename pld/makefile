#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY: all clean

PLD_MAIN			:= zid_main
PLD_BRKPCL			:= zid_brkpcl
PLD_BDATA_GPIO 		:= zid_bd_gpio

JEDECS		:= $(PLD_BRKPCL).jed $(PLD_BDATA_GPIO).jed $(PLD_MAIN).jed
			# $(PLD_BA).jed $(PLD_BA_PCH).jed $(PLD_BA_PCL).jed $(PLD_BDATA_GPIO).jed $(PLD_MAIN).jed

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

$(PLD_MAIN).jed: $(PLD_MAIN).tt2 makefile
	$(FIT1508) -i $< $(FITOPTS_1508) $(FITOPTS_TQFP100) $(FITOPTS_CMN)

$(PLD_BRKPCL).jed: $(PLD_BRKPCL).tt2 makefile
	$(FIT1508) -i $< $(FITOPTS_1508) $(FITOPTS_TQFP100) $(FITOPTS_CMN)

$(PLD_BDATA_GPIO).jed: $(PLD_BDATA_GPIO).tt2 makefile
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_TQFP44) $(FITOPTS_CMN)

$(PLD_BA).jed: $(PLD_BA).tt2 makefile $(INC_LOGIC_BA)
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_TQFP44) $(FITOPTS_CMN)

$(PLD_BA_PCH).jed: $(PLD_BA_PCH).tt2 $(INC_LOGIC_BA) $(INC_LOGIC_PCLSR) makefile
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_TQFP44) $(FITOPTS_CMN)

$(PLD_BA_PCL).jed: $(PLD_BA_PCL).tt2 $(INC_LOGIC_BA) $(INC_LOGIC_PCLSR) makefile
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_TQFP44) $(FITOPTS_CMN)

# The BRK2X is no longer used on the board
$(PLD_BRK2X).jed: $(PLD_BRK2X).pld
	$(CUPL) $(CUPLOPTS) $<

# Test for Microchip Issue
ATF1504FIT_TEST.jed: ATF1504FIT_TEST.tt2 makefile
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_TQFP44) $(FITOPTS_CMN)



clean:
	rm -f *.abs *.doc *.fit *.io *.jed *.log *.lst *.oed *.pin *.pla *.sim *.tt2 *.tt3
