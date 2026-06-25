/**
 * Number Base support.
 *
 * Copyright 2026 AESilky
 * SPDX-License-Identifier: MIT License
 *
*/
#include "nbase.h"

#include "board.h"
#include "util.h"

// ====================================================================
// Local Constants
// ====================================================================

static const char* _NB_BIN_NAME = "BINARY";
static const char* _NB_DEC_NAME = "DECIMAL";
static const char* _NB_HEX_NAME = "HEX";
static const char* _NB_OCT_NAME = "OCTAL";
static const char* _NB_UNK_NAME = "UNKNOWN";

// ====================================================================
// Data Section
// ====================================================================

typedef struct nbase_ntt_ {
    const char* ni;     // Name indicator
    const nbase_t nb;   // Number Base
} nbase_ntt_t;


static volatile bool _modinit_called;

static nbase_t _nbase;
static nbase_ntt_t nbfid[] = {
    {"binary",NB_BINARY},
    {"bin",NB_BINARY},
    {"b",NB_BINARY},
    {"'",NB_BINARY},
    {"2",NB_BINARY},
    {"decimal",NB_DECIMAL},
    {"dec",NB_DECIMAL},
    {"d",NB_DECIMAL},
    {"t",NB_DECIMAL},
    {"10",NB_DECIMAL},
    {"octal",NB_OCTAL},
    {"oct",NB_OCTAL},
    {"o",NB_OCTAL},
    {"q",NB_OCTAL},
    {"8",NB_OCTAL},
    {"hex",NB_HEX},
    {"h",NB_HEX},
    {"x",NB_HEX},
    {"16",NB_HEX}
};

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

nbase_t nbase_get() {
    return _nbase;
}

nbase_t nbase_from_str(const char* nbs, bool* valid) {
    *valid = false; // start with invalid
    nbase_t nb = _nbase;

    for (int i = 0; i < ARRAY_ELEMENT_COUNT(nbfid); i++) {
        if (stricmp(nbs,nbfid[i].ni) == 0) {
            nb = nbfid[i].nb;
            *valid = true;
            break;
        }
    }
    return nb;
}

const char* nbase_get_name() {
    return nbase_name_str(_nbase);
}

const char* nbase_name_str(nbase_t nb) {
    const char* name = _NB_UNK_NAME;

    switch (nb) {
        case NB_BINARY:
            name = _NB_BIN_NAME;
            break;
        case NB_OCTAL:
            name = _NB_OCT_NAME;
            break;
        case NB_DECIMAL:
            name = _NB_DEC_NAME;
            break;
        case NB_HEX:
            name = _NB_HEX_NAME;
            break;
        default:
            break;
    }
    return name;
}

void nbase_set(nbase_t nb) {
    _nbase = nb;
}

int nbase_width(repsize_t sz) {
    int fw = 0;
    switch (_nbase) {
        case NB_BINARY:
            switch (sz) {
                case RS_UNLIMIT:
                case RS_DWORD:
                    fw = 39;
                    break;
                case RS_BYTE:
                    fw = 9;
                    break;
                case RS_WORD:
                    fw = 19;
                    break;
            }
            break;
        case NB_DECIMAL:
            switch (sz) {
                case RS_UNLIMIT:
                case RS_DWORD:
                    fw = 10;
                    break;
                case RS_BYTE:
                    fw = 3;
                    break;
                case RS_WORD:
                    fw = 5;
                    break;
            }
            break;
        case NB_HEX:
            switch (sz) {
                case RS_UNLIMIT:
                case RS_DWORD:
                    fw = 8;
                    break;
                case RS_BYTE:
                    fw = 2;
                    break;
                case RS_WORD:
                    fw = 4;
                    break;
            }
            break;
        case NB_OCTAL:
            switch (sz) {
                case RS_UNLIMIT:
                case RS_DWORD:
                    fw = 12;
                    break;
                case RS_BYTE:
                    fw = 3;
                    break;
                case RS_WORD:
                    fw = 6;
                    break;
            }
            break;
    }
    return fw;
}

// ====================================================================
// Initialization/Start-Up Methods
// ====================================================================

int nbase_modinit() {
    if (_modinit_called) {
        board_panic("!!! nbase_modinit: Called more than once !!!");
    }
    _modinit_called = true;

    int retval = 0;
    _nbase = NB_DECIMAL;     // Start off with decimal as the number base

    return retval;
}

