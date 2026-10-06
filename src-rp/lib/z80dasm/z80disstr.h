/**
 * @brief Z80 Disassembler - String/Message Builder
 * @file z80disstr.c
 *
 * Builds messages from Z80 mnemonics, conditions, and values.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */

#ifndef Z80DISSTR_H_
#define Z80DISSTR_H_

/*
 * To actually define the tokens, include this file with 'ZDA_PHRASE_DEF' defined.
 *
 * Others include without 'ZDA_PHRASE_DEF' defined to allow access to the
 * tokenized messages.
 */
#ifdef ZDA_PHRASE_DEF
#define GLOBL
#define MSGV(X) ={X}
#else
#define GLOBL extern
#define MSGV(X)
#endif // ZDA_PHRASE_DEF
#define CMSG const char

// Phrases

GLOBL CMSG zdp_ADC[]    MSGV("adc");
GLOBL CMSG zdp_ADD[]    MSGV("add");
GLOBL CMSG zdp_AND[]    MSGV("and");
GLOBL CMSG zdp_BIT[]	MSGV("bit");
GLOBL CMSG zdp_CALL[]	MSGV("call");
GLOBL CMSG zdp_CCF[]	MSGV("ccf");
GLOBL CMSG zdp_CP[]	    MSGV("cp");
GLOBL CMSG zdp_CPD[]	MSGV("cpd");
GLOBL CMSG zdp_CPDR[]	MSGV("cpdr");
GLOBL CMSG zdp_CPI[]    MSGV("cpi");
GLOBL CMSG zdp_CPIR[]	MSGV("cpir");
GLOBL CMSG zdp_CPL[]	MSGV("cpl");
GLOBL CMSG zdp_DAA[]	MSGV("daa");
GLOBL CMSG zdp_DEC[]	MSGV("dec");
GLOBL CMSG zdp_DI[]	    MSGV("di");
GLOBL CMSG zdp_DJNZ[]	MSGV("djnz");
GLOBL CMSG zdp_EI[]	    MSGV("ei");
GLOBL CMSG zdp_EX[]	    MSGV("ex");
GLOBL CMSG zdp_EXX[]	MSGV("exx");
GLOBL CMSG zdp_HALT[]	MSGV("halt");
GLOBL CMSG zdp_IM[]	    MSGV("im");
GLOBL CMSG zdp_IN[]	    MSGV("in");
GLOBL CMSG zdp_INC[]	MSGV("inc");
GLOBL CMSG zdp_IND[]	MSGV("ind");
GLOBL CMSG zdp_INDR[]	MSGV("indr");
GLOBL CMSG zdp_INR[]	MSGV("inr");
GLOBL CMSG zdp_INI[]	MSGV("ini");
GLOBL CMSG zdp_INIR[]	MSGV("inir");
GLOBL CMSG zdp_JP[]	    MSGV("jp");
GLOBL CMSG zdp_JR[]	    MSGV("jr");
GLOBL CMSG zdp_LD[]	    MSGV("ld");
GLOBL CMSG zdp_LDD[]	MSGV("ldd");
GLOBL CMSG zdp_LDDR[]	MSGV("lddr");
GLOBL CMSG zdp_LDR[]	MSGV("ldr");
GLOBL CMSG zdp_LDI[]	MSGV("ldi");
GLOBL CMSG zdp_LDIR[]	MSGV("ldir");
GLOBL CMSG zdp_NC[]	    MSGV("nc");
GLOBL CMSG zdp_NEG[]	MSGV("neg");
GLOBL CMSG zdp_NOP[]	MSGV("nop");
GLOBL CMSG zdp_NZ[]	    MSGV("nz");
GLOBL CMSG zdp_OR[]	    MSGV("or");
GLOBL CMSG zdp_OTDR[]	MSGV("otdr");
GLOBL CMSG zdp_OTIR[]	MSGV("otir");
GLOBL CMSG zdp_OUT[]	MSGV("out");
GLOBL CMSG zdp_OUTD[]	MSGV("outd");
GLOBL CMSG zdp_OUTI[]	MSGV("outi");
GLOBL CMSG zdp_PE[]	    MSGV("pe");
GLOBL CMSG zdp_PO[]	    MSGV("po");
GLOBL CMSG zdp_POP[]	MSGV("pop");
GLOBL CMSG zdp_PUSH[]	MSGV("push");
GLOBL CMSG zdp_RES[]	MSGV("res");
GLOBL CMSG zdp_RET[]	MSGV("ret");
GLOBL CMSG zdp_RETI[]	MSGV("reti");
GLOBL CMSG zdp_RETN[]	MSGV("retn");
GLOBL CMSG zdp_RL[]	    MSGV("rl");
GLOBL CMSG zdp_RLA[]	MSGV("rla");
GLOBL CMSG zdp_RLC[]	MSGV("rlc");
GLOBL CMSG zdp_RLCA[]	MSGV("rlca");
GLOBL CMSG zdp_RLD[]	MSGV("rld");
GLOBL CMSG zdp_RR[]	    MSGV("rr");
GLOBL CMSG zdp_RRA[]	MSGV("rra");
GLOBL CMSG zdp_RRC[]	MSGV("rrc");
GLOBL CMSG zdp_RRCA[]	MSGV("rrca");
GLOBL CMSG zdp_RRD[]	MSGV("rrd");
GLOBL CMSG zdp_RST[]	MSGV("rst");
GLOBL CMSG zdp_SBC[]	MSGV("sbc");
GLOBL CMSG zdp_SCF[]	MSGV("scf");
GLOBL CMSG zdp_SET[]	MSGV("set");
GLOBL CMSG zdp_SLA[]	MSGV("sla");
GLOBL CMSG zdp_SRA[]	MSGV("sra");
GLOBL CMSG zdp_SRL[]	MSGV("srl");
GLOBL CMSG zdp_SUB[]	MSGV("sub");
GLOBL CMSG zdp_XOR[]	MSGV("xor");

//

GLOBL CMSG zdp_AF[]     MSGV("af");
GLOBL CMSG zdp_BC[]	    MSGV("bc");
GLOBL CMSG zdp_DE[]	    MSGV("de");
GLOBL CMSG zdp_HL[]	    MSGV("hl");
GLOBL CMSG zdp_SP[]	    MSGV("sp");
GLOBL CMSG zdp_IX[]	    MSGV("ix");
GLOBL CMSG zdp_IY[]	    MSGV("iy");
GLOBL CMSG zdp_IXp[]	MSGV("ix+");
GLOBL CMSG zdp_IYp[]	MSGV("iy+");

/**
 * @brief Concatenate a string to the destination surrounded by parentheses
 * @ingroup zdstr
 * 
 * 
 * @param dest The destination buffer (must be large enough)
 * @param src The source string to concatenate
 */
extern void dstrparcat(char* dest, const char* src);

#endif // Z80DISSTR_H_
