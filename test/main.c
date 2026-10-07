/**
 * @brief Test for the Z80 Disassembler Module
 * @file main.c
 * 
 * This calls the Z80 Disassembler for either:
 * 1. All Z80 instructions and some illegal instructions
 * 2. Just the instruction groups specified on the command line
 *   1 = Single Byte instructions
 *   C = 2-byte CB instructions
 *   D = 2-byte DD instructions
 *   E = 2-byte ED instructions
 *   F = 2-byte FD instructions
 *   I = Illegal instructions (a few in 2-byte groups)
 * 3. Instruction opcode bytes specified on the command line (in hex)
 *   X hh hh hh ...
 * 
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 */
#include "z80disasm.h"  // The Disassembler methods
#include "z80allbin.h"  // All instructions binary data

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int line = 1;               // Used for listing output

static void _usage(const char* name);

static void _fmt_byte(char* buf, uint8_t v) {
    sprintf(buf, "%02x", v);
}

static void _fmt_byteh(char* buf, uint8_t v) {
    sprintf(buf, "%02xh", v);
}

static void _fmt_index(char* buf, int8_t i) {
    sprintf(buf, "%+i", i);
}

static void _fmt_word(char* buf, uint16_t v) {
    sprintf(buf, "%04x", v);
}

static void _hndl_disassemble_err(zda_ctx_t* ctx) {
    int s = ctx->status;
    if (s < 0) {
        fprintf(stderr, "Disassembler indicated error: %d %s\n", s, ctx->comment);
        if (s == ZDAE_INVALID_INSTRUCTION) {
            // For invalid instruction, print the bytes given to the disassembler
            fprintf(stderr, " Invalid Instruction: %04X ", ctx->addr);
            for (int i = 0; i < ctx->bi; i++) {
                char* comma = (ctx->bi - i > 1 ? "," : "");
                fprintf(stderr, "%02X%s", ctx->d[i], comma);
            }
            fprintf(stderr, "\n");
        }
    }
    else {
        fprintf(stderr, "Not enough byte values provided. Need at least %d more.\n", s);
    }
}

static void _list_inst(zda_ctx_t* ctx) {
    char buf[3];
    char *comment = (*(ctx->comment) ? "\t\t; " : "");
    printf("%4d %04X ", line++, ctx->addr);
    for (int i = 0; i < Z80INST_MAX_BYTES; i++) {
        const char* db = "  ";
        if (i < ctx->bi) {
            _fmt_byte(buf, ctx->d[i]);
            db = buf;
        }
        printf("%s ", db);
    }
    printf("%s%s%s\n", ctx->stmt, comment, ctx->comment);
}

static void _opt_err(const char* name) {
    fprintf(stderr, "Invalid option.\n");
    _usage(name);
}

static unsigned int _uint_from_hexstr(const char* str, bool* success) {
    char* unparsed;
    *success = true; // Be an optimist
    unsigned long retval = strtoul(str, &unparsed, 16);
    if (*unparsed) {
        retval = 0;
        *success = false;
    }
    return (retval);
}

static void _usage(const char* name) {
    printf("usage: %s [(-h|--help)|(X nn nn ...)|[1][C][D][E][F][I]]\n", name);
    printf(" 1 : 1-Byte instructions\n");
    printf(" C : 2-byte 'CB' instructions\n");
    printf(" D : 2-byte 'DD' instructions\n");
    printf(" E : 2-byte 'ED' instructions\n");
    printf(" F : 2-byte 'FD' instructions\n");
    printf(" I : Illegal instructions\n");
    printf(" X nn nn ... : Opcodes provided as hex byte values\n");
    printf(" -u or --upper : Uppercase disassembly output (must be before 'X' if used)\n");
    printf(" -h or --help : Print this.\n");
    printf(" If no parameters are specified all instruction groups including invalid are used.\n");
}

int main(int argc, char** argv){
    const char* name = *argv++; argc--;
    uint16_t addr = 0;          // Used for disassembly
    zda_ctx_t ctx;
    int ds; // Disassembler status
    size_t elements; // Element count for the instruction groups
    bool uc = false;

    // Do all if no arguments were given
    bool b1;    // Single-byte instructions
    bool cb;    // CB group 2-byte instructions
    bool dd;    // DD group 2-byte instructions
    bool ed;    // ED group 2-byte instructions
    bool fd;    // FD group 2-byte instructions
    bool ig;    // Illegal (2-byte) instructions
    bool xb = false;    // Assume the groups will be used
    b1 = cb = dd = ed = fd = ig = (argc > 0 ? false : true);

    while (argc > 0) {
        char opt;
        char* opts = *(argv);
        argv++; argc--;
        while ((opt = *opts)) {
            switch (opt) {
                case '1':
                    b1 = true;
                    break;
                case 'C':
                    cb = true;
                    break;
                case 'D':
                    dd = true;
                    break;
                case 'E':
                    ed = true;
                    break;
                case 'F':
                    fd = true;
                    break;
                case 'I':
                    ig = true;
                    break;
                case 'X':
                    b1 = cb = dd = ed = fd = ig = false; // If bytes specified, don't do groups
                    xb = true;
                    goto OPTSEND_;
                case '-':
                    // check for "-u" or "--upper"
                    if (strcmp(opts, "-u") == 0 || strcmp(opts, "--upper") == 0) {
                        uc = true;
                        // skip to next arg;
                        while(*opts) opts++;
                        b1 = cb = dd = ed = fd = ig = (argc > 0 ? false : true);
                        continue;
                    }
                    // check for "-h" or "--help"
                    if (strcmp(opts, "-h") != 0 && strcmp(opts, "--help") != 0) {
                        _opt_err(name);
                        goto ERR_RET_;
                    }
                    _usage(name);
                    goto FINALLY_;
                default:
                    _opt_err(name);
                    goto ERR_RET_;
            }
            opts++;
        }
    }
OPTSEND_:
    // Initialize the Disassembler
    ds = zda_modinit(_fmt_byteh, _fmt_word, _fmt_index, uc);
    if (ds != 0) {
        fprintf(stderr, "Disassembler init error: %s\n", ds);
        goto ERR_RET_;
    }
    if (b1) {
        printf("Single Byte Instructions...\n");
        elements = z1b_len();
        for (int i = 0; i < elements; i++) {
            int8_t s = zda_begin(&ctx, addr, *(z80_1byte + i));
            if (s == 0) {
                goto NEXT_1_;
            }
            while (s > 0 && ++i < elements) {
                // feed the disassembler additional bytes
                // s indicates the minimum needed, but we only feed one at a time
                addr++; // move to next address
                s = zda_next(&ctx, *(z80_1byte + i));
            }
            if (s < 0 || i >= elements) {
                // There was a problem.
                if (s < 0) {
                    fprintf(stderr, "Disassembler indicated error: %d %s\n", s, ctx.comment);
                    goto ERR_RET_;
                }
                if (i >= elements) {
                    fprintf(stderr, "Not enough byte values provided. Need at least %d more.\n", s);
                    goto ERR_RET_;
                }
            }
        NEXT_1_:
            // s is 0, list the disassembly
            _list_inst(&ctx);
            addr++;
        }
    }
    if (cb) {
        printf("\nTwo Byte 'CB' Instructions...\n");
        elements = z2bCB_len();
        for (int i = 0; i < elements; i++) {
            int8_t s = zda_begin(&ctx, addr, *(z80_2byteCB + i));
            if (s == 0) {
                goto NEXT_C_;
            }
            while (s > 0 && ++i < elements) {
                // feed the disassembler additional bytes
                // s indicates the minimum needed, but we only feed one at a time
                addr++; // move to next address
                s = zda_next(&ctx, *(z80_2byteCB + i));
            }
            if (s < 0 || i >= elements) {
                // There was a problem.
                _hndl_disassemble_err(&ctx);
                goto ERR_RET_;
            }
        NEXT_C_:
            // s is 0, list the disassembly
            _list_inst(&ctx);
            addr++;
        }
    }
    if (dd) {
        printf("\nTwo Byte 'DD' Instructions...\n");
        elements = z2bDD_len();
        for (int i = 0; i < elements; i++) {
            int8_t s = zda_begin(&ctx, addr, *(z80_2byteDD + i));
            if (s == 0) {
                goto NEXT_D_;
            }
            while (s > 0 && ++i < elements) {
                // feed the disassembler additional bytes
                // s indicates the minimum needed, but we only feed one at a time
                addr++; // move to next address
                s = zda_next(&ctx, *(z80_2byteDD + i));
            }
            if (s < 0 || i >= elements) {
                // There was a problem.
                _hndl_disassemble_err(&ctx);
                goto ERR_RET_;
            }
        NEXT_D_:
            // s is 0, list the disassembly
            _list_inst(&ctx);
            addr++;
        }
    }
    if (ed) {
        printf("\nTwo Byte 'ED' Instructions...\n");
        elements = z2bED_len();
        for (int i = 0; i < elements; i++) {
            int8_t s = zda_begin(&ctx, addr, *(z80_2byteED + i));
            if (s == 0) {
                goto NEXT_E_;
            }
            while (s > 0 && ++i < elements) {
                // feed the disassembler additional bytes
                // s indicates the minimum needed, but we only feed one at a time
                addr++; // move to next address
                s = zda_next(&ctx, *(z80_2byteED + i));
            }
            if (s < 0 || i >= elements) {
                // There was a problem.
                _hndl_disassemble_err(&ctx);
                goto ERR_RET_;
            }
        NEXT_E_:
            // s is 0, list the disassembly
            _list_inst(&ctx);
            addr++;
        }
    }
    if (fd) {
        printf("\nTwo Byte 'FD' Instructions...\n");
        elements = z2bFD_len();
        for (int i = 0; i < elements; i++) {
            int8_t s = zda_begin(&ctx, addr, *(z80_2byteFD + i));
            if (s == 0) {
                goto NEXT_F_;
            }
            while (s > 0 && ++i < elements) {
                // feed the disassembler additional bytes
                // s indicates the minimum needed, but we only feed one at a time
                addr++; // move to next address
                s = zda_next(&ctx, *(z80_2byteFD + i));
            }
            if (s < 0 || i >= elements) {
                // There was a problem.
                _hndl_disassemble_err(&ctx);
                goto ERR_RET_;
            }
        NEXT_F_:
            // s is 0, list the disassembly
            _list_inst(&ctx);
            addr++;
        }
    }
    if (ig) {
        printf("\nInvalid Instructions...\n");
        elements = zInvalid_len();
        for (int i = 0; i < elements; i++) {
            int8_t s = zda_begin(&ctx, addr, *(z80_invalid + i));
            if (s == 0) {
                goto NEXT_I_;
            }
            while (s > 0 && ++i < elements) {
                // feed the disassembler additional bytes
                // s indicates the minimum needed, but we only feed one at a time
                addr++; // move to next address
                s = zda_next(&ctx, *(z80_invalid + i));
            }
            if (s == ZDAE_INVALID_INSTRUCTION) {
                // This is what we expect
                char buf[3];
                zda_invalid(&ctx);
                char* comment = (*(ctx.comment) ? "\t\t; " : "");
                printf("%4d %04X ", line++, ctx.addr);
                for (int j = 0; j < Z80INST_MAX_BYTES; j++) {
                    const char* db = "  ";
                    if (j < ctx.bi) {
                        _fmt_byte(buf, ctx.d[j]);
                        db = buf;
                    }
                    printf("%s ", db);
                }
                printf("%s%s%s\n", ctx.stmt, comment, ctx.comment);
            }
            else {
                // There was a problem.
                fprintf(stderr, "Disassembler did not report invalid instruction for: ");
                for (int k = 0; k < ctx.bi; k++) {
                    fprintf(stderr, "%02X ", ctx.d[k]);
                }
                fprintf(stderr, "\n");
                goto ERR_RET_;
            }
        NEXT_I_:
            addr++;
        }
    }
    if (xb) {
        // Use the rest of the arguments as hex bytes
        //
        // First, run through them all to make sure they are valid
        for (int i = 0; i < argc; i++) {
            bool success;
            unsigned int b = _uint_from_hexstr(*(argv + i), &success);
            if (!success || b > 255) {
                fprintf(stderr, "The argument '%s' is not a valid hex byte\n", *argv);
                goto ERR_RET_;
            }
        }
        // Okay, they are all valid hex bytes. Start calling the disassemble.
        for (int i = 0; i < argc; i++) {
            bool success;
            unsigned int b = _uint_from_hexstr(*(argv + i), &success);
            // no need to check success - all arguments were checked above
            int8_t s = zda_begin(&ctx, addr, b);
            if (s == 0) {
                goto NEXT_X_;
            }
            while (s > 0 && ++i < argc) {
                // feed the disassembler additional bytes
                // s indicates the minimum needed, but we only feed one at a time
                addr++; // move to next address
                b = _uint_from_hexstr(*(argv + i), &success);
                s = zda_next(&ctx, b);
            }
            if (s < 0 || i >= argc) {
                // There was a problem.
                _hndl_disassemble_err(&ctx);
                goto ERR_RET_;
            }
        NEXT_X_:
            // s is 0, list the disassembly
            _list_inst(&ctx);
            addr++;
        }
    }
FINALLY_:
    printf("DONE\n");
    return 0;
ERR_RET_:
    return 1;
}
