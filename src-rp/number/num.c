/**
 * Number operations support.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "num.h"

#include "board.h"
#include "nbase.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// ====================================================================
// Local Constants
// ====================================================================



// ====================================================================
// Data Section
// ====================================================================

static volatile bool _modinit_called;


// ====================================================================
// Local/Private Method Declarations
// ====================================================================



// ====================================================================
// Run-After/Delay/Sleep Methods
// ====================================================================



// ====================================================================
// Message Handler Methods
// ====================================================================



// ====================================================================
// IRQ Methods
// ====================================================================



// ====================================================================
// Local/Private Methods
// ====================================================================



// ====================================================================
// Public Methods
// ====================================================================

bool num_binstr(char* buf, uint32_t v, repsize_t rs) {
    bool ovf = false;
    int nbls = 8;
    bool lz = false;        // If wordsize is 0, do 8 nibbles without leading 0 groups
    if (rs != RS_UNLIMIT) {
        lz = true;          // otherwise, display leading 0 nibbles
        nbls = rs / 4;      // nibbles to display
        long ubnd = ((long)1 << rs);
        if (v >= ubnd) {
            ovf = true;
            v &= ubnd;
        }
    }
    for (int i = nbls; i > 0; i--) {
        int shft = (4 * (i - 1));
        uint8_t n = lowByte(((v & (0x0F << shft)) >> shft));
        if (n > 0 || lz) {
            lz = (n | lz);
            for (int j = 0; j < 4; j++) {
                char d = ((n & 0x08) ? '1' : '0');
                *buf++ = d;
                n = n << 1;
            }
            if (i > 1) {
                char sp = ((i % 2 == 0) ? '.' : ' ');
                *buf++ = sp;
            }
        }
        *buf = '\000';
    }
    return ovf;
}


bool num_decstr(char* buf, uint v, repsize_t rs) {
    bool ovf = false; // No overflow
    uint vc = v;    // Copy of value that is used
    const char* fmt = "%u";

    if (rs == RS_BYTE) {
        fmt = "%3u";
        if (v > 0xff) {
            ovf = true;
            vc = lowByte(v);
        }
    }
    else if (rs == RS_WORD) {
        fmt = "%5u";
        if (v > 0xffff) {
            ovf = true;
            vc = (v & 0xffff);
        }
    }
    else if (rs == RS_DWORD) {
        fmt = "%10u";
    }
    sprintf(buf, fmt, vc);

    return ovf;
}


bool num_hexstr(char* buf, uint v, repsize_t rs, bool uc) {
    bool ovf = false; // No overflow
    uint vc = v;    // Copy of value that is used
    const char* fmt = (uc ? "%X" : "%x");

    if (rs == RS_BYTE) {
        fmt = (uc ? "%02X" : "%02x");
        if (v > 0xff) {
            ovf = true;
            vc = lowByte(v);
        }
    }
    else if (rs == RS_WORD) {
        fmt = (uc ? "%04X" : "%04x");
        if (v > 0xffff) {
            ovf = true;
            vc = (v & 0xffff);
        }
    }
    else if (rs == RS_DWORD) {
        fmt = (uc ? "%08X" : "%08x");
    }
    sprintf(buf, fmt, vc);

    return ovf;
}


bool num_octstr(char* buf, uint v, repsize_t rs) {
    bool ovf = false; // No overflow
    uint vc = v;    // Copy of value that is used
    const char* fmt = "%#o";

    if (rs == RS_BYTE) {
        fmt = "%03o";
        if (v > 0xff) {
            ovf = true;
            vc = lowByte(v);
        }
    }
    else if (rs == RS_WORD) {
        fmt = "%06o";
        if (v > 0xffff) {
            ovf = true;
            vc = (v & 0xffff);
        }
    }
    else if (rs == RS_DWORD) {
        fmt = "%012o";
    }
    sprintf(buf, fmt, vc);

    return ovf;
}


uint32_t num_valprovider(const char* str, repsize_t sz, valstatus_t* status) {
    uint32_t v = 0;
    uint32_t dv;                // Digit value
    uint32_t dm;                // Digit multiplier
    uint64_t mv = (1u << sz) - 1;
    nbase_t nb = nbase_get();   // Start with the default base
    char c;
    int lc;
    int i = 0;

    // We only support numbers that start with a digit
    if (!isdigit((int)*str)) {
        goto _err_invdig;
    }
    // See if there is a BASE indicator
    int l = strlen(str);
    if (!isdigit((int)*(str+(l-1)))) {
        lc = (int)tolower((unsigned char)*(str + (l - 1)));
        if (lc == 'h' || lc == 'x') {
            nb = NB_HEX;
            l--;                // don't process the indicator
        }
        else if (lc == 'o' || lc == 'q') {
            nb = NB_OCTAL;
            l--;                // don't process the indicator
        }
        else if (lc == 't' || lc == '.') {
            nb = NB_DECIMAL;
            l--;                // don't process the indicator
        }
        else if (lc == '\'') {
            nb = NB_BINARY;
            l--;
        }
    }
    if (l == 0) {
        goto _err_invdig;
    }
    // Try to convert
    dm = (uint32_t)nb;
    *status = VP_OK;    // Assume we can convert to a value.
    for (i = 0; i < l; i++) {
        c = (unsigned char)*(str+i);
        if (!isdigit(c)) {
            if (nb != NB_HEX) {
                goto _err_invdig;
            }
            lc = (tolower(c) - (int)'a') + 10;
            if (lc < 10 || lc > 15) {
                goto _err_invdig;
            }
            dv = (uint32_t)lc;
        }
        else {
            dv = (uint32_t)(c - '0');
            if (dv >= (uint32_t)nb) {
                goto _err_invdig;
            }
        }
        v = (v * dm) + dv;
        if (sz != RS_UNLIMIT && v > mv) {
            *status = VP_INV_SIZE;
            goto _finally;
        }
    }
_finally:
    return v;
_err_invdig:
    *status = VP_INV_DIGIT;
    v = i; // return the index to the character that is invalid
    goto _finally;
}

bool num_valstr_nb(char* buf, uint v, repsize_t rs, bool uc) {
    bool ovf = false; // No overflow
    nbase_t nb = nbase_get();

    switch (nb) {
        case NB_BINARY:
            ovf = num_binstr(buf, v, rs);
            break;
        case NB_DECIMAL:
            ovf = num_decstr(buf, v, rs);
            break;
        case NB_HEX:
            ovf = num_hexstr(buf, v, rs, uc);
            break;
        case NB_OCTAL:
            ovf = num_octstr(buf, v, rs);
            break;
    }
    return ovf;
}


// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

int num_modinit() {
    if (_modinit_called) {
        board_panic("!!! num_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    int retval = 0;

    return retval;
}