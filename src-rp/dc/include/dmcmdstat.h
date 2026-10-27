#ifndef DMCMDSTAT_H_
#define DMCMDSTAT_H_

#define DM_CMDSTAT_IND 0x80  // Bit-7 on for DM CTRL Reg read/write

/**
 * The following are status the Debug Monitor (DM) writes to the CTRL register
 * Note: The names match those used in the DM assembler code
 * Also Note: DM status values always have Bit-7 set
 */
typedef enum DM_STATUS_VAL_ {
    DMINIT      = 0x80,     // The Debug Monitor has finished initialization
                            // it will read the CTRL register and resend this
                            // status repeatedly until we respond with DC_INITRDY 
    DMOPDONE    = 0x82,     // DOC requested operation complete
    DMOKCMDRD   = 0x84,     // OK, reading command
    DMCMDUK     = 0x88,     // Requested command unknown
    DMSSDONE    = 0x8A,     // Single-Step done
    DMBRKHIT    = 0x8C,     // Breakpoint hit
    DMTGTGO     = 0x8E,     // DM transitioning to Target Mode
                            //  (don't interrupt for a bit)
    DMTESTDONE  = 0xB0,     // DM Auto-Test complete
    DMRSTEXEC   = 0xBE,     // DM executed a RST instruction (ERROR)
} dm_stat_val_t;
#define DMSTATSBC_M 0x01    //  Bit added to status if target is SBC


/**
 * The following are the Debug Monitor (DM) commands. The DM reads these from
 * the CTRL register after it has sent a status.
 * Note1: The names match those used in the DM assembler code. Refer to the
 *  DM assembler code for the full descriptions (not repeated here).
 * 
 * Note2: DM command values always have Bit-7 set (80-FF)
 *  Bit-7 clear and Bit-6 set is used for SBC function status (40-7F).
 */
typedef enum DM_CMDS_ {
    DMNOP           = 0x80,
    DCVER           = 0x81, //
    DCTGTMP         = 0x82, // Use the Target's memory/ports for Get/Put operations (default)
    DCDBGRMP        = 0x83, // Use the Debugger's memory/ports for Get/Put operations
    //
    DCATEST         = 0x84, // Auto Test
    //
    DCPBRKPNTS      = 0x85, // Breakpoint Configure
    DCBRKPEN        = 0x86, // Breakpoint Enable/Disable
    //
    DCGREGALL       = 0x88,	// 
    DCPREGALL       = 0x89,	// 
    DCGIEDV         = 0x8A,	// 
    DCPIEDV         = 0x8B,	// 
    DCGREG          = 0x90,	//  90..9F
    DCPREG          = 0xA0,	//  A0..AF
    DCGRP           = 0xB0,	//  B0..B3
    DCPRP           = 0xB4,	//  B4..B7
    DCGM            = 0xB8,	// Get mem byte(s)
    DCPM            = 0xB9,	// Put mem byte(s)
    DCGP            = 0xBA,	// Get port byte(s)
    DCPP            = 0xBB,	// Put port byte(s)
    //
    DCGO            = 0xBC, // Target GO
    DCGOWREG        = 0xBD, // Go w/Registers
    DCSTEP          = 0xBE, // STEP
    DCSTEPWREG      = 0xBF, // Step w/Registers
} dm_cmd_t;
#define NOCMD ((dm_cmd_t)0)

/** Register mask values for the low 4 - bits for DCGREG and DCPREG */
typedef enum DM_REG_MASK_ {
    DCRFP_M     = 0x0,	// 
    DCRAP_M     = 0x1,	// 
    DCRCP_M     = 0x2,	// 
    DCRBP_M     = 0x3,	// 
    DCREP_M     = 0x4,	// 
    DCRDP_M     = 0x5,	// 
    DCRLP_M     = 0x6,	// 
    DCRHP_M     = 0x7,	// 
    DCRF_M      = 0x8,	// 
    DCRA_M      = 0x9,	// 
    DCRC_M      = 0xA,	// 
    DCRB_M      = 0xB,	// 
    DCRE_M      = 0xC,	// 
    DCRD_M      = 0xD,	// 
    DCRL_M      = 0xE,	// 
    DCRH_M      = 0xF,	// 
} dm_reg_mask_t;

typedef enum DM_BRK_OP_ {
    DCBRKOP_IF  = 0x1,	// Instruction Fetch
    DCBRKOP_MW  = 0x2,	// Memory Write
    DCBRKOP_MR  = 0x3,	// Memory Read
    DCBRKOP_AM  = 0x4,	// Any Memory Operation(except Refresh)
    DCBRKOP_PW  = 0x5,	// Port(I / O) Write
    DCBRKOP_PR  = 0x6,	// Port(I / O) Read
    DCBRKOP_AP  = 0x7	// Any Port(I / O) Operation
} dm_brk_op_t;

#define DCBRKEN_M   0x08    // Enable / Disable
#define DCBRKUA_M   0x10    // Use Address(bit mask)


#endif // DMCMDSTAT_H_
