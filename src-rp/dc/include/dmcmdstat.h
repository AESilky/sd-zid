#ifndef DMCMDSTAT_H_
#define DMCMDSTAT_H_

#define DM_CMDSTAT_IND 0x80  // Bit-7 on for DM CTRL Reg read/write

/**
 * The following are status the Debug Monitor (DM) writes to the CTRL register
 * Note: The names match those used in the DM assembler code
 * Also Note: DM status values always have Bit-7 set
 */
typedef enum DM_STATUS_VAL_ {
    DMIDLE      = 0x80,     // The Debug Monitor is idle
    DMOPDONE    = 0x82,     // DOC requested operation complete
    DMOKCMDRD   = 0x84,     // OK, reading command
    DMTESTRUN   = 0x86,     // The Auto-Test has started
    DMCMDUK     = 0x88,     // Requested command unknown
    DMSSDONE    = 0x8A,     // Single-Step done
    DMBRKHIT    = 0x8C,     // Breakpoint hit
    DMTGTGO     = 0x8E,     // DM transitioning to Target Mode (GO or SS)
                            //  (don't interrupt for a bit)
    DMTESTDONE  = 0xB6,     // DM Auto-Test complete (RUN is 86)
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
    DMVER           = 0x81, //
    DMTGTMP         = 0x82, // Use the Target's memory/ports for Get/Put operations (default)
    DMDBGRMP        = 0x83, // Use the Debugger's memory/ports for Get/Put operations
    //
    DMATEST         = 0x84, // Auto Test
    //
    DMPBRKPNTS      = 0x85, // Breakpoint Configure
    DMBRKPEN        = 0x86, // Breakpoint Enable/Disable
    //
    DMGREGALL       = 0x88,	// 
    DMPREGALL       = 0x89,	// 
    DMGIEDV         = 0x8A,	// 
    DMPIEDV         = 0x8B,	// 
    DMGREG          = 0x90,	//  90..9F
    DMPREG          = 0xA0,	//  A0..AF
    DMGRP           = 0xB0,	//  B0..B3
    DMPRP           = 0xB4,	//  B4..B7
    DMGM            = 0xB8,	// Get mem byte(s)
    DMPM            = 0xB9,	// Put mem byte(s)
    DMGP            = 0xBA,	// Get port byte(s)
    DMPP            = 0xBB,	// Put port byte(s)
    //
    DMGO            = 0xBC, // Target GO
    DMGOWREG        = 0xBD, // Go w/Registers
    DMSTEP          = 0xBE, // STEP
    DMSTEPWREG      = 0xBF, // Step w/Registers
    //
    CMD_NONE        = 0x00  // NOT A COMMAND!!! - Cannot be sent. Flag for NO OP IN PROGRESS.
} dm_cmd_t;
#define NOCMD ((dm_cmd_t)0)

/** Register mask values for the low 4 - bits for DMGREG and DMPREG */
typedef enum DM_REG_MASK_ {
    DMRFP_M     = 0x0,	// 
    DMRAP_M     = 0x1,	// 
    DMRCP_M     = 0x2,	// 
    DMRBP_M     = 0x3,	// 
    DMREP_M     = 0x4,	// 
    DMRDP_M     = 0x5,	// 
    DMRLP_M     = 0x6,	// 
    DMRHP_M     = 0x7,	// 
    DMRF_M      = 0x8,	// 
    DMRA_M      = 0x9,	// 
    DMRC_M      = 0xA,	// 
    DMRB_M      = 0xB,	// 
    DMRE_M      = 0xC,	// 
    DMRD_M      = 0xD,	// 
    DMRL_M      = 0xE,	// 
    DMRH_M      = 0xF,	// 
} dm_reg_mask_t;

typedef enum DM_BRK_OP_ {
    DMBRKOP_IF  = 0x1,	// Instruction Fetch
    DMBRKOP_MW  = 0x2,	// Memory Write
    DMBRKOP_MR  = 0x3,	// Memory Read
    DMBRKOP_AM  = 0x4,	// Any Memory Operation(except Refresh)
    DMBRKOP_PW  = 0x5,	// Port(I / O) Write
    DMBRKOP_PR  = 0x6,	// Port(I / O) Read
    DMBRKOP_AP  = 0x7	// Any Port(I / O) Operation
} dm_brk_op_t;

#define DMBRKEN_M   0x08    // Enable / Disable
#define DMBRKUA_M   0x10    // Use Address(bit mask)


#endif // DMCMDSTAT_H_
