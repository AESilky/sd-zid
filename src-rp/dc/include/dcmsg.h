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
#define CMSG const char*

GLOBL CMSG dcm_title MSGV("ZID - Z80 In-circuit Debugger");
GLOBL CMSG dcm_banner MSGV(">>>             ZID - Z80 In-circuit Debugger            <<<\n>>> Hardware: Copyright 2025-26 SilkyDESIGN and CMOS2    <<<\n>>> Software: Copyright 2023-26 AESilky                  <<<\n\n");


#ifdef __cplusplus
}
#endif
#endif // DCMSG_H_
