/*
 * All Z80 machine (binary) instructions.
 *
 * Copyright 2026, AESilky
 * Using information from the Zilog Z80 Assembly Language Programming Manual
 * Copyright 1977, Zilog Inc
 *
 * SPDX-License-Identifier: MIT License
 */

#include "z80allbin.h"


/*** ===================================================================== ***/
static uint8_t const _z80_1byte[] = {
    0x00,               // NOP
    0x01, NNL, NNH,     // LD   BC,NN
    0x02,               // LD   (BC),A
    0x03,               // INC  BC
    0x04,               // INC  B
    0x05,               // DEC  B
    0x06, N,            // LD   B,N
    0x07,               // RLCA
    0x08,               // EX   AF,AF'
    0x09,               // ADD  HL,BC
    0x0A,               // LD   A,(BC)
    0x0B,               // DEC  BC
    0x0C,               // INC  C
    0x0D,               // DEC  C
    0x0E, N,            // LD   C,N
    0x0F,               // RRCA
    0x10, DIS_,         // DJNZ DIS
    0x11, NNL, NNH,     // LD   DE,NN
    0x12,               // LD   (DE),A
    0x13,               // INC  DE
    0x14,               // INC  D
    0x15,               // DEC  D
    0x16, N,            // LD   D,N
    0x17,               // RLA
    0x18, DIS_,         // JR   DIS
    0x19,               // ADD  HL,DE
    0x1A,               // LD   A,(DE)
    0x1B,               // DEC  DE
    0x1C,               // INC  E
    0x1D,               // DEC  E
    0x1E, N,            // LD   E,N
    0x1F,               // RRA
    0x20, DIS_,         // JR   NZ,DIS
    0x21, NNL, NNH,     // LD   HL,NN
    0x22, NNL, NNH,     // LD   (NN),HL
    0x23,               // INC  HL
    0x24,               // INC  H
    0x25,               // DEC  H
    0x26, N,            // LD   H,N
    0x27,               // DAA
    0x28, DIS_,         // JR   Z,DIS
    0x29,               // ADD  HL,HL
    0x2A, NNL, NNH,     // LD   HL,(NN)
    0x2B,               // DEC  HL
    0x2C,               // INC  L
    0x2D,               // DEC  L
    0x2E, N,            // LD   L,N
    0x2F,               // CPL
    0x30, DIS_,         // JR   NC,DIS
    0x31, NNL, NNH,     // LD   SP,NN
    0x32, NNL, NNH,     // LD   (NN),A
    0x33,               // INC  SP
    0x34,               // INC  (HL)
    0x35,               // DEC  (HL)
    0x36, N,            // LD   (HL),N
    0x37,               // SCF
    0x38, DIS_,         // JR   C,DIS
    0x39,               // ADD  HL,SP
    0x3A, NNL, NNH,     // LD   A,(NN)
    0x3B,               // DEC  SP
    0x3C,               // INC  A
    0x3D,               // DEC  A
    0x3E, N,            // LD   A,N
    0x3F,               // CCF
    0x40,               // LD   B,B
    0x41,               // LD   B,C
    0x42,               // LD   B,D
    0x43,               // LD   B,E
    0x44,               // LD   B,H
    0x45,               // LD   B,L
    0x46,               // LD   B,(HL)
    0x47,               // LD   B,A
    0x48,               // LD   C,B
    0x49,               // LD   C,C
    0x4A,               // LD   C,D
    0x4B,               // LD   C,E
    0x4C,               // LD   C,H
    0x4D,               // LD   C,L
    0x4E,               // LD   C,(HL)
    0x4F,               // LD   C,A
    0x50,               // LD   D,B
    0x51,               // LD   D,C
    0x52,               // LD   D,D
    0x53,               // LD   D,E
    0x54,               // LD   D,H
    0x55,               // LD   D,L
    0x56,               // LD   D,(HL)
    0x57,               // LD   D,A
    0x58,               // LD   E,B
    0x59,               // LD   E,C
    0x5A,               // LD   E,D
    0x5B,               // LD   E,E
    0x5C,               // LD   E,H
    0x5D,               // LD   E,L
    0x5E,               // LD   E,(HL)
    0x5F,               // LD   E,A
    0x60,               // LD   H,B
    0x61,               // LD   H,C
    0x62,               // LD   H,D
    0x63,               // LD   H,E
    0x64,               // LD   H,H
    0x65,               // LD   H,L
    0x66,               // LD   H,(HL)
    0x67,               // LD   H,A
    0x68,               // LD   L,B
    0x69,               // LD   L,C
    0x6A,               // LD   L,D
    0x6B,               // LD   L,E
    0x6C,               // LD   L,H
    0x6D,               // LD   L,L
    0x6E,               // LD   L,(HL)
    0x6F,               // LD   L,A
    0x70,               // LD   (HL),B
    0x71,               // LD   (HL),C
    0x72,               // LD   (HL),D
    0x73,               // LD   (HL),E
    0x74,               // LD   (HL),H
    0x75,               // LD   (HL),L
    0x76,               // HALT
    0x77,               // LD   (HL),A
    0x78,               // LD   A,B
    0x79,               // LD   A,C
    0x7A,               // LD   A,D
    0x7B,               // LD   A,E
    0x7C,               // LD   A,H
    0x7D,               // LD   A,L
    0x7E,               // LD   A,(HL)
    0x7F,               // LD   A,A
    0x80,               // ADD  A,B
    0x81,               // ADD  A,C
    0x82,               // ADD  A,D
    0x83,               // ADD  A,E
    0x84,               // ADD  A,H
    0x85,               // ADD  A,L
    0x86,               // ADD  A,(HL)
    0x87,               // ADD  A,A
    0x88,               // ADC  A,B
    0x89,               // ADC  A,C
    0x8A,               // ADC  A,D
    0x8B,               // ADC  A,E
    0x8C,               // ADC  A,H
    0x8D,               // ADC  A,L
    0x8E,               // ADC  A,(HL)
    0x8F,               // ADC  A,A
    0x90,               // SUB  B
    0x91,               // SUB  C
    0x92,               // SUB  D
    0x93,               // SUB  E
    0x94,               // SUB  H
    0x95,               // SUB  L
    0x96,               // SUB  (HL)
    0x97,               // SUB  A
    0x98,               // SBC  A,B
    0x99,               // SBC  A,C
    0x9A,               // SBC  A,D
    0x9B,               // SBC  A,E
    0x9C,               // SBC  A,H
    0x9D,               // SBC  A,L
    0x9E,               // SBC  A,(HL)
    0x9F,               // SBC  A,A
    0xA0,               // AND  B
    0xA1,               // AND  C
    0xA2,               // AND  D
    0xA3,               // AND  E
    0xA4,               // AND  H
    0xA5,               // AND  L
    0xA6,               // AND  (HL)
    0xA7,               // AND  A
    0xA8,               // XOR  B
    0xA9,               // XOR  C
    0xAA,               // XOR  D
    0xAB,               // XOR  E
    0xAC,               // XOR  H
    0xAD,               // XOR  L
    0xAE,               // XOR  (HL)
    0xAF,               // XOR  A
    0xB0,               // OR   B
    0xB1,               // OR   C
    0xB2,               // OR   D
    0xB3,               // OR   E
    0xB4,               // OR   H
    0xB5,               // OR   L
    0xB6,               // OR   (HL)
    0xB7,               // OR   A
    0xB8,               // CP   B
    0xB9,               // CP   C
    0xBA,               // CP   D
    0xBB,               // CP   E
    0xBC,               // CP   H
    0xBD,               // CP   L
    0xBE,               // CP   (HL)
    0xBF,               // CP   A
    0xC0,               // RET  NZ
    0xC1,               // POP  BC
    0xC2, NNL, NNH,     // JP   NZ,NN
    0xC3, NNL, NNH,     // JP   NN
    0xC4, NNL, NNH,     // CALL NZ,NN
    0xC5,               // PUSH BC
    0xC6, N,            // ADD  A,N
    0xC7,               // RST  0
    0xC8,               // RET  Z
    0xC9,               // RET
    0xCA, NNL, NNH,     // JP   Z,NN
    0xCC, NNL, NNH,     // CALL Z,NN
    0xCD, NNL, NNH,     // CALL NN
    0xCE, N,            // ADC  A,N
    0xCF,               // RST  8
    0xD0,               // RET  NC
    0xD1,               // POP  DE
    0xD2, NNL, NNH,     // JP   NC,NN
    0xD3, N,            // OUT  N,A
    0xD4, NNL, NNH,     // CALL NC,NN
    0xD5,               // PUSH DE
    0xD6, N,            // SUB  N
    0xD7,               // RST  10H
    0xD8,               // RET  C
    0xD9,               // EXX
    0xDA, NNL, NNH,     // JP   C,NN
    0xDB, N,            // IN   A,N
    0xDC, NNL, NNH,     // CALL C,NN
    0xDE, N,            // SBC  A,N
    0xDF,               // RST  18H
    0xE0,               // RET  PO
    0xE1,               // POP  HL
    0xE2, NNL, NNH,     // JP   PO,NN
    0xE3,               // EX   (SP),HL
    0xE4, NNL, NNH,     // CALL PO,NN
    0xE5,               // PUSH HL
    0xE6, N,            // AND  N
    0xE7,               // RST  20H
    0xE8,               // RET  PE
    0xE9,               // JP   (HL)
    0xEA, NNL, NNH,     // JP   PE,NN
    0xEB,               // EX   DE,HL
    0xEC, NNL, NNH,     // CALL PE,NN
    0xEE, N,            // XOR  N
    0xEF,               // RST  28H
    0xF0,               // RET  P
    0xF1,               // POP  AF
    0xF2, NNL, NNH,     // JP   P,NN
    0xF3,               // DI
    0xF4, NNL, NNH,     // CALL P,NN
    0xF5,               // PUSH AF
    0xF6, N,            // OR   N
    0xF7,               // RST  30H
    0xF8,               // RET  M
    0xF9,               // LD   SP,HL
    0xFA, NNL, NNH,     // JP   M,NN
    0xFB,               // EI
    0xFC, NNL, NNH,     // CALL M,NN
    0xFE, N,            // CP   N
    0xFF,               // RST  38H
};

/*** ===================================================================== ***/
uint8_t const _z80_2byteCB[] = {
    0xCB, 0x00,          // RLC  B
    0xCB, 0x01,          // RLC  C
    0xCB, 0x02,          // RLC  D
    0xCB, 0x03,          // RLC  E
    0xCB, 0x04,          // RLC  H
    0xCB, 0x05,          // RLC  L
    0xCB, 0x06,          // RLC  (HL)
    0xCB, 0x07,          // RLC  A
    0xCB, 0x08,          // RRC  B
    0xCB, 0x09,          // RRC  C
    0xCB, 0x0A,          // RRC  D
    0xCB, 0x0B,          // RRC  E
    0xCB, 0x0C,          // RRC  H
    0xCB, 0x0D,          // RRC  L
    0xCB, 0x0E,          // RRC  (HL)
    0xCB, 0x0F,          // RRC  A
    0xCB, 0x10,          // RL   B
    0xCB, 0x11,          // RL   C
    0xCB, 0x12,          // RL   D
    0xCB, 0x13,          // RL   E
    0xCB, 0x14,          // RL   H
    0xCB, 0x15,          // RL   L
    0xCB, 0x16,          // RL   (HL)
    0xCB, 0x17,          // RL   A
    0xCB, 0x18,          // RR   B
    0xCB, 0x19,          // RR   C
    0xCB, 0x1A,          // RR   D
    0xCB, 0x1B,          // RR   E
    0xCB, 0x1C,          // RR   H
    0xCB, 0x1D,          // RR   L
    0xCB, 0x1E,          // RR   (HL)
    0xCB, 0x1F,          // RR   A
    0xCB, 0x20,          // SLA  B
    0xCB, 0x21,          // SLA  C
    0xCB, 0x22,          // SLA  D
    0xCB, 0x23,          // SLA  E
    0xCB, 0x24,          // SLA  H
    0xCB, 0x25,          // SLA  L
    0xCB, 0x26,          // SLA  (HL)
    0xCB, 0x27,          // SLA  A
    0xCB, 0x28,          // SRA  B
    0xCB, 0x29,          // SRA  C
    0xCB, 0x2A,          // SRA  D
    0xCB, 0x2B,          // SRA  E
    0xCB, 0x2C,          // SRA  H
    0xCB, 0x2D,          // SRA  L
    0xCB, 0x2E,          // SRA  (HL)
    0xCB, 0x2F,          // SRA  A
    0xCB, 0x38,          // SRL  B
    0xCB, 0x39,          // SRL  C
    0xCB, 0x3A,          // SRL  D
    0xCB, 0x3B,          // SRL  E
    0xCB, 0x3C,          // SRL  H
    0xCB, 0x3D,          // SRL  L
    0xCB, 0x3E,          // SRL  (HL)
    0xCB, 0x3F,          // SRL  A
    0xCB, 0x40,          // BIT  0,B
    0xCB, 0x41,          // BIT  0,C
    0xCB, 0x42,          // BIT  0,D
    0xCB, 0x43,          // BIT  0,E
    0xCB, 0x44,          // BIT  0,H
    0xCB, 0x45,          // BIT  0,L
    0xCB, 0x46,          // BIT  0,(HL)
    0xCB, 0x47,          // BIT  0,A
    0xCB, 0x48,          // BIT  1,B
    0xCB, 0x49,          // BIT  1,C
    0xCB, 0x4A,          // BIT  1,D
    0xCB, 0x4B,          // BIT  1,E
    0xCB, 0x4C,          // BIT  1,H
    0xCB, 0x4D,          // BIT  1,L
    0xCB, 0x4E,          // BIT  1,(HL)
    0xCB, 0x4F,          // BIT  1,A
    0xCB, 0x50,          // BIT  2,B
    0xCB, 0x51,          // BIT  2,C
    0xCB, 0x52,          // BIT  2,D
    0xCB, 0x53,          // BIT  2,E
    0xCB, 0x54,          // BIT  2,H
    0xCB, 0x55,          // BIT  2,L
    0xCB, 0x56,          // BIT  2,(HL)
    0xCB, 0x57,          // BIT  2,A
    0xCB, 0x58,          // BIT  3,B
    0xCB, 0x59,          // BIT  3,C
    0xCB, 0x5A,          // BIT  3,D
    0xCB, 0x5B,          // BIT  3,E
    0xCB, 0x5C,          // BIT  3,H
    0xCB, 0x5D,          // BIT  3,L
    0xCB, 0x5E,          // BIT  3,(HL)
    0xCB, 0x5F,          // BIT  3,A
    0xCB, 0x60,          // BIT  4,B
    0xCB, 0x61,          // BIT  4,C
    0xCB, 0x62,          // BIT  4,D
    0xCB, 0x63,          // BIT  4,E
    0xCB, 0x64,          // BIT  4,H
    0xCB, 0x65,          // BIT  4,L
    0xCB, 0x66,          // BIT  4,(HL)
    0xCB, 0x67,          // BIT  4,A
    0xCB, 0x68,          // BIT  5,B
    0xCB, 0x69,          // BIT  5,C
    0xCB, 0x6A,          // BIT  5,D
    0xCB, 0x6B,          // BIT  5,E
    0xCB, 0x6C,          // BIT  5,H
    0xCB, 0x6D,          // BIT  5,L
    0xCB, 0x6E,          // BIT  5,(HL)
    0xCB, 0x6F,          // BIT  5,A
    0xCB, 0x70,          // BIT  6,B
    0xCB, 0x71,          // BIT  6,C
    0xCB, 0x72,          // BIT  6,D
    0xCB, 0x73,          // BIT  6,E
    0xCB, 0x74,          // BIT  6,H
    0xCB, 0x75,          // BIT  6,L
    0xCB, 0x76,          // BIT  6,(HL)
    0xCB, 0x77,          // BIT  6,A
    0xCB, 0x78,          // BIT  7,B
    0xCB, 0x79,          // BIT  7,C
    0xCB, 0x7A,          // BIT  7,D
    0xCB, 0x7B,          // BIT  7,E
    0xCB, 0x7C,          // BIT  7,H
    0xCB, 0x7D,          // BIT  7,L
    0xCB, 0x7E,          // BIT  7,(HL)
    0xCB, 0x7F,          // BIT  7,A
    0xCB, 0x80,          // RES  0,B
    0xCB, 0x81,          // RES  0,C
    0xCB, 0x82,          // RES  0,D
    0xCB, 0x83,          // RES  0,E
    0xCB, 0x84,          // RES  0,H
    0xCB, 0x85,          // RES  0,L
    0xCB, 0x86,          // RES  0,(HL)
    0xCB, 0x87,          // RES  0,A
    0xCB, 0x88,          // RES  1,B
    0xCB, 0x89,          // RES  1,C
    0xCB, 0x8A,          // RES  1,D
    0xCB, 0x8B,          // RES  1,E
    0xCB, 0x8C,          // RES  1,H
    0xCB, 0x8D,          // RES  1,L
    0xCB, 0x8E,          // RES  1,(HL)
    0xCB, 0x8F,          // RES  1,A
    0xCB, 0x90,          // RES  2,B
    0xCB, 0x91,          // RES  2,C
    0xCB, 0x92,          // RES  2,D
    0xCB, 0x93,          // RES  2,E
    0xCB, 0x94,          // RES  2,H
    0xCB, 0x95,          // RES  2,L
    0xCB, 0x96,          // RES  2,(HL)
    0xCB, 0x97,          // RES  2,A
    0xCB, 0x98,          // RES  3,B
    0xCB, 0x99,          // RES  3,C
    0xCB, 0x9A,          // RES  3,D
    0xCB, 0x9B,          // RES  3,E
    0xCB, 0x9C,          // RES  3,H
    0xCB, 0x9D,          // RES  3,L
    0xCB, 0x9E,          // RES  3,(HL)
    0xCB, 0x9F,          // RES  3,A
    0xCB, 0xA0,          // RES  4,B
    0xCB, 0xA1,          // RES  4,C
    0xCB, 0xA2,          // RES  4,D
    0xCB, 0xA3,          // RES  4,E
    0xCB, 0xA4,          // RES  4,H
    0xCB, 0xA5,          // RES  4,L
    0xCB, 0xA6,          // RES  4,(HL)
    0xCB, 0xA7,          // RES  4,A
    0xCB, 0xA8,          // RES  5,B
    0xCB, 0xA9,          // RES  5,C
    0xCB, 0xAA,          // RES  5,D
    0xCB, 0xAB,          // RES  5,E
    0xCB, 0xAC,          // RES  5,H
    0xCB, 0xAD,          // RES  5,L
    0xCB, 0xAE,          // RES  5,(HL)
    0xCB, 0xAF,          // RES  5,A
    0xCB, 0xB0,          // RES  6,B
    0xCB, 0xB1,          // RES  6,C
    0xCB, 0xB2,          // RES  6,D
    0xCB, 0xB3,          // RES  6,E
    0xCB, 0xB4,          // RES  6,H
    0xCB, 0xB5,          // RES  6,L
    0xCB, 0xB6,          // RES  6,(HL)
    0xCB, 0xB7,          // RES  6,A
    0xCB, 0xB8,          // RES  7,B
    0xCB, 0xB9,          // RES  7,C
    0xCB, 0xBA,          // RES  7,D
    0xCB, 0xBB,          // RES  7,E
    0xCB, 0xBC,          // RES  7,H
    0xCB, 0xBD,          // RES  7,L
    0xCB, 0xBE,          // RES  7,(HL)
    0xCB, 0xBF,          // RES  7,A
    0xCB, 0xC0,          // SET  0,B
    0xCB, 0xC1,          // SET  0,C
    0xCB, 0xC2,          // SET  0,D
    0xCB, 0xC3,          // SET  0,E
    0xCB, 0xC4,          // SET  0,H
    0xCB, 0xC5,          // SET  0,L
    0xCB, 0xC6,          // SET  0,(HL)
    0xCB, 0xC7,          // SET  0,A
    0xCB, 0xC8,          // SET  1,B
    0xCB, 0xC9,          // SET  1,C
    0xCB, 0xCA,          // SET  1,D
    0xCB, 0xCB,          // SET  1,E
    0xCB, 0xCC,          // SET  1,H
    0xCB, 0xCD,          // SET  1,L
    0xCB, 0xCE,          // SET  1,(HL)
    0xCB, 0xCF,          // SET  1,A
    0xCB, 0xD0,          // SET  2,B
    0xCB, 0xD1,          // SET  2,C
    0xCB, 0xD2,          // SET  2,D
    0xCB, 0xD3,          // SET  2,E
    0xCB, 0xD4,          // SET  2,H
    0xCB, 0xD5,          // SET  2,L
    0xCB, 0xD6,          // SET  2,(HL)
    0xCB, 0xD7,          // SET  2,A
    0xCB, 0xD8,          // SET  3,B
    0xCB, 0xD9,          // SET  3,C
    0xCB, 0xDA,          // SET  3,D
    0xCB, 0xDB,          // SET  3,E
    0xCB, 0xDC,          // SET  3,H
    0xCB, 0xDD,          // SET  3,L
    0xCB, 0xDE,          // SET  3,(HL)
    0xCB, 0xDF,          // SET  3,A
    0xCB, 0xE0,          // SET  4,B
    0xCB, 0xE1,          // SET  4,C
    0xCB, 0xE2,          // SET  4,D
    0xCB, 0xE3,          // SET  4,E
    0xCB, 0xE4,          // SET  4,H
    0xCB, 0xE5,          // SET  4,L
    0xCB, 0xE6,          // SET  4,(HL)
    0xCB, 0xE7,          // SET  4,A
    0xCB, 0xE8,          // SET  5,B
    0xCB, 0xE9,          // SET  5,C
    0xCB, 0xEA,          // SET  5,D
    0xCB, 0xEB,          // SET  5,E
    0xCB, 0xEC,          // SET  5,H
    0xCB, 0xED,          // SET  5,L
    0xCB, 0xEE,          // SET  5,(HL)
    0xCB, 0xEF,          // SET  5,A
    0xCB, 0xF0,          // SET  6,B
    0xCB, 0xF1,          // SET  6,C
    0xCB, 0xF2,          // SET  6,D
    0xCB, 0xF3,          // SET  6,E
    0xCB, 0xF4,          // SET  6,H
    0xCB, 0xF5,          // SET  6,L
    0xCB, 0xF6,          // SET  6,(HL)
    0xCB, 0xF7,          // SET  6,A
    0xCB, 0xF8,          // SET  7,B
    0xCB, 0xF9,          // SET  7,C
    0xCB, 0xFA,          // SET  7,D
    0xCB, 0xFB,          // SET  7,E
    0xCB, 0xFC,          // SET  7,H
    0xCB, 0xFD,          // SET  7,L
    0xCB, 0xFE,          // SET  7,(HL)
    0xCB, 0xFF,          // SET  7,A
};

/*** ===================================================================== ***/
uint8_t const _z80_2byteDD[] = {
    0xDD, 0x09,             // ADD  IX,BC
    0xDD, 0x19,             // ADD  IX,DE
    0xDD, 0x21, NNL, NNH,   // LD   IX,NN
    0xDD, 0x22, NNL, NNH,   // LD   (NN),IX
    0xDD, 0x23,             // INC  IX
    0xDD, 0x29,             // ADD  IX,IX
    0xDD, 0x2A, NNL, NNH,   // LD   IX,(NN)
    0xDD, 0x2B,             // DEC  IX
    0xDD, 0x34, IND,        // INC  (IX+IND)
    0xDD, 0x35, IND,        // DEC  (IX+IND)
    0xDD, 0x36, IND, N,     // LD   (IX+IND),N
    0xDD, 0x39,             // ADD  IX,SP
    0xDD, 0x46, IND,        // LD   B,(IX+IND)
    0xDD, 0x4E, IND,        // LD   C,(IX+IND)
    0xDD, 0x56, IND,        // LD   D,(IX+IND)
    0xDD, 0x5E, IND,        // LD   E,(IX+IND)
    0xDD, 0x66, IND,        // LD   H,(IX+IND)
    0xDD, 0x6E, IND,        // LD   L,(IX+IND)
    0xDD, 0x70, IND,        // LD   (IX+IND),B
    0xDD, 0x71, IND,        // LD   (IX+IND),C
    0xDD, 0x72, IND,        // LD   (IX+IND),D
    0xDD, 0x73, IND,        // LD   (IX+IND),E
    0xDD, 0x74, IND,        // LD   (IX+IND),H
    0xDD, 0x75, IND,        // LD   (IX+IND),L
    0xDD, 0x77, IND,        // LD   (IX+IND),A
    0xDD, 0x7E, IND,        // LD   A,(IX+IND)
    0xDD, 0x86, IND,        // ADD  A,(IX+IND)
    0xDD, 0x8E, IND,        // ADC  A,(IX+IND)
    0xDD, 0x96, IND,        // SUB  (IX+IND)
    0xDD, 0x9E, IND,        // SBC  A,(IX+IND)
    0xDD, 0xA6, IND,        // AND  (IX+IND)
    0xDD, 0xAE, IND,        // XOR  (IX+IND)
    0xDD, 0xB6, IND,        // OR   (IX+IND)
    0xDD, 0xBE, IND,        // CP   (IX+IND)
    0xDD, 0xE1,             // POP  IX
    0xDD, 0xE3,             // EX   (SP),IX
    0xDD, 0xE5,             // PUSH IX
    0xDD, 0xE9,             // JP   (IX)
    0xDD, 0xF9,             // LD   SP,IX
    0xDD, 0xCB, IND, 0x06,  // RLC  (IX+IND)
    0xDD, 0xCB, IND, 0x0E,  // RRC  (IX+IND)
    0xDD, 0xCB, IND, 0x16,  // RL   (IX+IND)
    0xDD, 0xCB, IND, 0x1E,  // RR   (IX+IND)
    0xDD, 0xCB, IND, 0x26,  // SLA  (IX+IND)
    0xDD, 0xCB, IND, 0x2E,  // SRA  (IX+IND)
    0xDD, 0xCB, IND, 0x3E,  // SRL  (IX+IND)
    0xDD, 0xCB, IND, 0x46,  // BIT  0,(IX+IND)
    0xDD, 0xCB, IND, 0x4E,  // BIT  1,(IX+IND)
    0xDD, 0xCB, IND, 0x56,  // BIT  2,(IX+IND)
    0xDD, 0xCB, IND, 0x5E,  // BIT  3,(IX+IND)
    0xDD, 0xCB, IND, 0x66,  // BIT  4,(IX+IND)
    0xDD, 0xCB, IND, 0x6E,  // BIT  5,(IX+IND)
    0xDD, 0xCB, IND, 0x76,  // BIT  6,(IX+IND)
    0xDD, 0xCB, IND, 0x7E,  // BIT  7,(IX+IND)
    0xDD, 0xCB, IND, 0x86,  // RES  0,(IX+IND)
    0xDD, 0xCB, IND, 0x8E,  // RES  1,(IX+IND)
    0xDD, 0xCB, IND, 0x96,  // RES  2,(IX+IND)
    0xDD, 0xCB, IND, 0x9E,  // RES  3,(IX+IND)
    0xDD, 0xCB, IND, 0xA6,  // RES  4,(IX+IND)
    0xDD, 0xCB, IND, 0xAE,  // RES  5,(IX+IND)
    0xDD, 0xCB, IND, 0xB6,  // RES  6,(IX+IND)
    0xDD, 0xCB, IND, 0xBE,  // RES  7,(IX+IND)
    0xDD, 0xCB, IND, 0xC6,  // SET  0,(IX+IND)
    0xDD, 0xCB, IND, 0xCE,  // SET  1,(IX+IND)
    0xDD, 0xCB, IND, 0xD6,  // SET  2,(IX+IND)
    0xDD, 0xCB, IND, 0xDE,  // SET  3,(IX+IND)
    0xDD, 0xCB, IND, 0xE6,  // SET  4,(IX+IND)
    0xDD, 0xCB, IND, 0xEE,  // SET  5,(IX+IND)
    0xDD, 0xCB, IND, 0xF6,  // SET  6,(IX+IND)
    0xDD, 0xCB, IND, 0xFE,  // SET  7,(IX+IND)
};

/*** ===================================================================== ***/
uint8_t const _z80_2byteED[] = {
    0xED, 0x40,             // IN   B,(C)
    0xED, 0x41,             // OUT  (C),B
    0xED, 0x42,             // SBC  HL,BC
    0xED, 0x43, NNL, NNH,   // LD   (NN),BC
    0xED, 0x44,             // NEG
    0xED, 0x45,             // RETN
    0xED, 0x46,             // IM   0
    0xED, 0x47,             // LD   I,A
    0xED, 0x48,             // IN   C,(C)
    0xED, 0x49,             // OUT  (C),C
    0xED, 0x4A,             // ADC  HL,BC
    0xED, 0x4B, NNL, NNH,   // LD   BC,(NN)
    0xED, 0x4D,             // RETI
    0xED, 0x50,             // IN   D,(C)
    0xED, 0x51,             // OUT  (C),D
    0xED, 0x52,             // SBC  HL,DE
    0xED, 0x53, NNL, NNH,   // LD   (NN),DE
    0xED, 0x56,             // IM   1
    0xED, 0x57,             // LD   A,I
    0xED, 0x58,             // IN   E,(C)
    0xED, 0x59,             // OUT  (C),E
    0xED, 0x5A,             // ADC  HL,DE
    0xED, 0x5B, NNL, NNH,   // LD   DE,(NN)
    0xED, 0x5E,             // IM   2
    0xED, 0x60,             // IN   H,(C)
    0xED, 0x61,             // OUT  (C),H
    0xED, 0x62,             // SBC  HL,HL
    0xED, 0x67,             // RRD
    0xED, 0x68,             // IN   L,(C)
    0xED, 0x69,             // OUT  (C),L
    0xED, 0x6A,             // ADC  HL,HL
    0xED, 0x6F,             // RLD
    0xED, 0x72,             // SBC  HL,SP
    0xED, 0x73, NNL, NNH,   // LD   (NN),SP
    0xED, 0x78,             // IN   A,(C)
    0xED, 0x79,             // OUT  (C),A
    0xED, 0x7A,             // ADC  HL,SP
    0xED, 0x7B, NNL, NNH,   // LD   SP,(NN)
    0xED, 0xA0,             // LDI
    0xED, 0xA1,             // CPI
    0xED, 0xA2,             // INI
    0xED, 0xA3,             // OUTI
    0xED, 0xA8,             // LDD
    0xED, 0xA9,             // CPD
    0xED, 0xAA,             // IND
    0xED, 0xAB,             // OUTD
    0xED, 0xB0,             // LDIR
    0xED, 0xB1,             // CPIR
    0xED, 0xB2,             // INIR
    0xED, 0xB3,             // OTIR
    0xED, 0xB8,             // LDDR
    0xED, 0xB9,             // CPDR
    0xED, 0xBA,             // INDR
    0xED, 0xBB,             // OTDR
};

/*** ===================================================================== ***/
uint8_t const _z80_2byteFD[] = {
    0XFD, 0x09,             // ADD  IY,BC
    0XFD, 0x19,             // ADD  IY,DE
    0XFD, 0x21, NNL, NNH,   // LD   IY,NN
    0XFD, 0x22, NNL, NNH,   // LD   (NN),IY
    0XFD, 0x23,             // INC  IY
    0XFD, 0x29,             // ADD  IY,IY
    0XFD, 0x2A, NNL, NNH,   // LD   IY,(NN)
    0XFD, 0x2B,             // DEC  IY
    0XFD, 0x34, IND,        // INC  (IY+IND)
    0XFD, 0x35, IND,        // DEC  (IY+IND)
    0XFD, 0x36, IND, N,     // LD   (IY+IND),N
    0XFD, 0x39,             // ADD  IY,SP
    0XFD, 0x46, IND,        // LD   B,(IY+IND)
    0XFD, 0x4E, IND,        // LD   C,(IY+IND)
    0XFD, 0x56, IND,        // LD   D,(IY+IND)
    0XFD, 0x5E, IND,        // LD   E,(IY+IND)
    0XFD, 0x66, IND,        // LD   H,(IY+IND)
    0XFD, 0x6E, IND,        // LD   L,(IY+IND)
    0XFD, 0x70, IND,        // LD   (IY+IND),B
    0XFD, 0x71, IND,        // LD   (IY+IND),C
    0XFD, 0x72, IND,        // LD   (IY+IND),D
    0XFD, 0x73, IND,        // LD   (IY+IND),E
    0XFD, 0x74, IND,        // LD   (IY+IND),H
    0XFD, 0x75, IND,        // LD   (IY+IND),L
    0XFD, 0x77, IND,        // LD   (IY+IND),A
    0XFD, 0x7E, IND,        // LD   A,(IY+IND)
    0XFD, 0x86, IND,        // ADD  A,(IY+IND)
    0XFD, 0x8E, IND,        // ADC  A,(IY+IND)
    0XFD, 0x96, IND,        // SUB  (IY+IND)
    0XFD, 0x9E, IND,        // SBC  A,(IY+IND)
    0XFD, 0xA6, IND,        // AND  (IY+IND)
    0XFD, 0xAE, IND,        // XOR  (IY+IND)
    0XFD, 0xB6, IND,        // OR   (IY+IND)
    0XFD, 0xBE, IND,        // CP   (IY+IND)
    0XFD, 0xE1,             // POP  IY
    0XFD, 0xE3,             // EX   (SP),IY
    0XFD, 0xE5,             // PUSH IY
    0XFD, 0xE9,             // JP   (IY)
    0XFD, 0xF9,             // LD   SP,IY
    0XFD, 0xCB, IND, 0x06,  // RLC  (IY+IND)
    0XFD, 0xCB, IND, 0x0E,  // RRC  (IY+IND)
    0XFD, 0xCB, IND, 0x16,  // RL   (IY+IND)
    0XFD, 0xCB, IND, 0x1E,  // RR   (IY+IND)
    0XFD, 0xCB, IND, 0x26,  // SLA  (IY+IND)
    0XFD, 0xCB, IND, 0x2E,  // SRA  (IY+IND)
    0XFD, 0xCB, IND, 0x3E,  // SRL  (IY+IND)
    0XFD, 0xCB, IND, 0x46,  // BIT  0,(IY+IND)
    0XFD, 0xCB, IND, 0x4E,  // BIT  1,(IY+IND)
    0XFD, 0xCB, IND, 0x56,  // BIT  2,(IY+IND)
    0XFD, 0xCB, IND, 0x5E,  // BIT  3,(IY+IND)
    0XFD, 0xCB, IND, 0x66,  // BIT  4,(IY+IND)
    0XFD, 0xCB, IND, 0x6E,  // BIT  5,(IY+IND)
    0XFD, 0xCB, IND, 0x76,  // BIT  6,(IY+IND)
    0XFD, 0xCB, IND, 0x7E,  // BIT  7,(IY+IND)
    0XFD, 0xCB, IND, 0x86,  // RES  0,(IY+IND)
    0XFD, 0xCB, IND, 0x8E,  // RES  1,(IY+IND)
    0XFD, 0xCB, IND, 0x96,  // RES  2,(IY+IND)
    0XFD, 0xCB, IND, 0x9E,  // RES  3,(IY+IND)
    0XFD, 0xCB, IND, 0xA6,  // RES  4,(IY+IND)
    0XFD, 0xCB, IND, 0xAE,  // RES  5,(IY+IND)
    0XFD, 0xCB, IND, 0xB6,  // RES  6,(IY+IND)
    0XFD, 0xCB, IND, 0xBE,  // RES  7,(IY+IND)
    0XFD, 0xCB, IND, 0xC6,  // SET  0,(IY+IND)
    0XFD, 0xCB, IND, 0xCE,  // SET  1,(IY+IND)
    0XFD, 0xCB, IND, 0xD6,  // SET  2,(IY+IND)
    0XFD, 0xCB, IND, 0xDE,  // SET  3,(IY+IND)
    0XFD, 0xCB, IND, 0xE6,  // SET  4,(IY+IND)
    0XFD, 0xCB, IND, 0xEE,  // SET  5,(IY+IND)
    0XFD, 0xCB, IND, 0xF6,  // SET  6,(IY+IND)
    0XFD, 0xCB, IND, 0xFE,  // SET  7,(IY+IND)
};

/*** ===================================================================== ***/
static uint8_t const _z80_invalid[] = {
    0XDD, 0X00,
    0XED, 0XF0,
    0XFD, 0XFF
};


/***
 * @brief Macro to get the number of elements in a static array
 */
#define ARRAY_ELEMENT_COUNT(arr) (sizeof(arr) / sizeof(*arr))

static const size_t _z1b_len = sizeof(_z80_1byte);
static const size_t _z2bCB_len = sizeof(_z80_2byteCB);
static const size_t _z2bDD_len = sizeof(_z80_2byteDD);
static const size_t _z2bED_len = sizeof(_z80_2byteED);
static const size_t _z2bFD_len = sizeof(_z80_2byteFD);
static const size_t _zInvalid_len = sizeof(_z80_invalid);

const uint8_t* const z80_1byte = _z80_1byte;
const size_t z1b_len() {
    return _z1b_len;
}

const uint8_t* const z80_2byteCB = _z80_2byteCB;
const size_t z2bCB_len() {
    return _z2bCB_len;
}

const uint8_t* const z80_2byteDD = _z80_2byteDD;
const size_t z2bDD_len() {
    return _z2bDD_len;
}

const uint8_t* const z80_2byteED = _z80_2byteED;
const size_t z2bED_len() {
    return _z2bED_len;
}
const uint8_t* const z80_2byteFD = _z80_2byteFD;
const size_t z2bFD_len() {
    return _z2bFD_len;
}

const uint8_t* const z80_invalid = _z80_invalid;
const size_t zInvalid_len() {
    return _zInvalid_len;
}
