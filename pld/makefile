#
# Copyright 2025-26 AESilky
# SPDX-License-Identifier: MIT License
#
.PHONY: all clean

PLD_MAIN	:= zid_main
PLD_BRK2N	:= zid_brk2n

JEDECS		:= $(PLD_MAIN).jed $(PLD_BRK2N).jed

CUPL		:= cupl.exe
FIT1502		:= fit1502.exe
FIT1504		:= fit1504.exe
FIT1508		:= fit1508.exe

CUPLOPTS		:= -m1nlxj
FITOPTS_CMN		:= -CPUL -JTAG ON
FITOPTS_1502	:= -tech ATF1502AS
FITOPTS_1504	:= -tech ATF1504AS
FITOPTS_1508	:= -tech ATF1508AS
FITOPTS_PLCC44	:= -device PLCC44
FITOPTS_TQFP44	:= -device TQFP44
FITOPTS_TQFP100	:= -device TQFP100

%.tt2:	%.pld
	$(CUPL) $(CUPLOPTS) $<

all:	$(JEDECS)

$(PLD_MAIN).jed: $(PLD_MAIN).tt2
	$(FIT1508) -i $< $(FITOPTS_1508) $(FITOPTS_TQFP100) $(FITOPTS_CMN)

$(PLD_BRK2N).jed: $(PLD_BRK2N).tt2
	$(FIT1504) -i $< $(FITOPTS_1504) $(FITOPTS_PLCC44) $(FITOPTS_CMN)

clean:
	rm -f *.abs *.doc *.fit *.io *.jed *.log *.lst *.pin *.pla *.sim *.tt2 *.tt3
