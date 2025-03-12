
/*
 * This file is used to generate C enums in header files.
 * Do not use anything other than "export enum" in this file.
 */

export enum IROpcode {
    INSTR_LABEL_RELATIVE,   // label, address_offset
    INSTR_LABEL_ABSOLUTE,   // label, address_offset
    INSTR_WRITE_CONST,      // reg => [value]
    INSTR_READ_CONST,       // reg <= [value]
    INSTR_WRITE_REG,        // reg => [addrReg]
    INSTR_READ_REG,         // reg <= [addrReg]
    INSTR_JUMP_COND_LABEL,  // label, op2 = condition
    INSTR_JUMP_CONST,       // address
    INSTR_CALL_CONST,       // address
    INSTR_JUMP_LABEL,       // label
    INSTR_JUMP_REG,         // reg
    INSTR_CALL_REG,         // reg
    INSTR_PUSH,             // reg, op2 = 1..4 bytes
    INSTR_PUSH_BLOCK_CONST, // reg, op2 = optional, value = block size
    INSTR_PUSH_BLOCK_LABEL, // reg, op2 = optional, label = label containing block size
    INSTR_BIN_OP,           // srcReg, dstReg, op2 = operator
    INSTR_RETURN,           //
    INSTR_LABEL_ALIAS,      // labelAlias = label
    INSTR_HOST,             // value = host function index
    INSTR_POP,              // reg, op2 = 1..4 bytes, TODO: is signed needed?
    INSTR_POP_BLOCK_CONST,  // value = bytes
    INSTR_BIN_OP_CONST,     // reg = reg ?? value
    INSTR_NOOP,             // value = bytes
    INSTR_PUSH_BLOCK_REG,   // dstReg = block size srcReg
    INSTR_BIN_OP_F32,       // srcReg, dstReg, op2 = operator
    INSTR_BIN_OP_F64,       // srcReg, dstReg, op2 = operator

    INSTR_JUMP_COND_INSTR,
    INSTR_JUMP_INSTR,

    INSTR_DATA,
    INSTR_WORD,
    INSTR_FILL,
    INSTR_EMPTY,
    INSTR_MARKER,
    /*INSTR_DISPOSABLE_BEGIN,
    INSTR_DISPOSABLE_END,
    INSTR_LABEL,*/
};

export enum IRBinOpcode {
    BIN_OP_ADD = 0x2B,
    BIN_OP_SUB = 0x2D,
    BIN_OP_ADDC = 0x88,
    BIN_OP_SUBC = 0x8a,
    BIN_OP_BITAND = 0x26,
    BIN_OP_BITXOR = 0x5E,
    BIN_OP_BITOR = 0x7C,
    BIN_OP_MUL = 0x2A,
    BIN_OP_SHL = 0x3C,
    BIN_OP_SHR = 0x8b,
    BIN_OP_SAR = 0x3E,
    BIN_OP_DIV = 0x2F,
    BIN_OP_UDIV = 0x83,
    BIN_OP_CMP = 0xFF,
    BIN_OP_MOV = 0xFE,
};

export enum IRFloatOpcode {
    FLOAT_BIN_OP_ADD = 0x2B,
    FLOAT_BIN_OP_SUB = 0x2D,
    FLOAT_BIN_OP_MUL = 0x2A,
    FLOAT_BIN_OP_DIV = 0x2F,
    FLOAT_BIN_OP_NEG = 0x81,      // TOK_NEG
    FLOAT_BIN_OP_CONV = 0xFF,
};

export enum IRCmpOpcode {
    CMP_OP_ULT = 0x92,
    CMP_OP_UGE = 0x93,
    CMP_OP_EQ = 0x94,
    CMP_OP_NE = 0x95,
    CMP_OP_ULE = 0x96,
    CMP_OP_UGT = 0x97,
    CMP_OP_Nset = 0x98,
    CMP_OP_Nclear = 0x99,
    CMP_OP_LT = 0x9c,
    CMP_OP_GE = 0x9d,
    CMP_OP_LE = 0x9e,
    CMP_OP_GT = 0x9f,
};
