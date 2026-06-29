/**
 * Debug Operations Messages and Strings.
 *
 * Copyright 2023-26 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#ifndef DCMSG_H_
#define DCMSG_H_
#ifdef __cplusplus
extern "C" {
#endif

/* 
 * To actually define the messages, include with 'DC_MSG_DEF' defined.
 *
 * Others include without 'DC_MSG_DEF' defined to allow access to the
 * messages.
 */
#ifdef DC_MSG_DEF
#define GLOBL
#define MSGV(X) ={X}
#else
#define GLOBL extern
#define MSGV(X)
#endif // DC_MSG_DEF
#define CMSG const char* const

GLOBL CMSG dcm_title MSGV("ZID - Z80 In-circuit Debugger");
GLOBL CMSG dcm_banner MSGV(">>>             ZID - Z80 In-circuit Debugger            <<<\n>>> Hardware: Copyright 2025-26 SilkyDESIGN and CMOS2    <<<\n>>> Software: Copyright 2023-26 AESilky                  <<<\n\n");
GLOBL CMSG dcm_blank    MSGV("");

GLOBL CMSG dcm_regfb    MSGV("SZxHxPNC");
GLOBL CMSG dcm_reghdr   MSGV("  A   B   C   D   E   H   L   F   |   A'  B'  C'  D'  E'  H'  L'  F'");
GLOBL CMSG dcm_regudr   MSGV(" --- --- --- --- --- --- --- ---  |  --- --- --- --- --- --- --- ---");
GLOBL CMSG dcm_regwhdr  MSGV("   IX     IY     PC     SP");
GLOBL CMSG dcm_regwudr  MSGV(" ------ ------ ------ ------");
GLOBL CMSG dcm_ds80     MSGV("--------------------------------------------------------------------------------");
GLOBL CMSG dcm_sp80     MSGV("                                                                                ");
/** @brief Get a string of 'n' spaces (up to 80) */
static inline const char* dcm_dashs(int n) {n %= 80; return (dcm_ds80 + (80 - n));}
/** @brief Get a string of 'n' spaces (up to 80) */
static inline const char* dcm_sps(int n) { n %= 80; return (dcm_sp80 + (80 - n)); }

GLOBL CMSG dcm_breakhit MSGV("BREAK");
GLOBL CMSG dcm_tgtsbc   MSGV("SBC");

/* Common Command Messages */

GLOBL CMSG dcm_done MSGV("DONE");
GLOBL CMSG dcm_invalid_arg MSGV("Invalid argument");
GLOBL CMSG dcm_invalid_dest MSGV("Invalid destination");
GLOBL CMSG dcm_pc MSGV("PC");
GLOBL CMSG dcm_size MSGV("size");


/* Debug Monitor Messages */

GLOBL CMSG dmm_break_hit MSGV("BREAK");
GLOBL CMSG dmm_cmd_unknown MSGV("DM received unknown command");
GLOBL CMSG dmm_fatal_error MSGV("!!!Debug Monitor fatal error !!!");
GLOBL CMSG dmm_status_error MSGV("!!!Invalid status received from Debug Monitor!!!");

#ifdef __cplusplus
}
#endif
#endif // DCMSG_H_
