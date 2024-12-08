
#include <stdint.h>

#include "ccvm-output.h"

#ifdef INTELLISENSE
#define USING_GLOBALS
#include "tcc.h"
#endif


enum {
    INSTR_MOV_REG,          // dstReg = srcReg
    INSTR_MOV_CONST,        // reg = value
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
    INSTR_RETURN,           // value = cleanup words
    INSTR_LABEL_ALIAS,      // labelAlias = label
    INSTR_HOST,             // value = host function index
    INSTR_POP,              // reg, op2 = 1..4 bytes, TODO: is signed needed?
    INSTR_POP_BLOCK_CONST,  // value = bytes
    INSTR_BIN_OP_CONST,     // reg = reg ?? value
};

enum {
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
};

_Static_assert(BIN_OP_ADD == '+', "BIN_OP_ADD");
_Static_assert(BIN_OP_SUB == '-', "BIN_OP_SUB");
_Static_assert(BIN_OP_ADDC == TOK_ADDC2, "BIN_OP_ADDC2");
_Static_assert(BIN_OP_SUBC == TOK_SUBC2, "BIN_OP_SUBC2");
_Static_assert(BIN_OP_BITAND == '&', "BIN_OP_BITAND");
_Static_assert(BIN_OP_BITXOR == '^', "BIN_OP_BITXOR");
_Static_assert(BIN_OP_BITOR == '|', "BIN_OP_BITOR");
_Static_assert(BIN_OP_MUL == '*', "BIN_OP_MUL");
_Static_assert(BIN_OP_SHL == TOK_SHL, "BIN_OP_SHL");
_Static_assert(BIN_OP_SHR == TOK_SHR, "BIN_OP_SHR");
_Static_assert(BIN_OP_SAR == TOK_SAR, "BIN_OP_SAR");
_Static_assert(BIN_OP_DIV == '/', "BIN_OP_DIV");
_Static_assert(BIN_OP_UDIV == TOK_UDIV, "BIN_OP_UDIV");

enum {
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

_Static_assert(CMP_OP_ULT == TOK_ULT, "CMP_OP_ULT");
_Static_assert(CMP_OP_UGE == TOK_UGE, "CMP_OP_UGE");
_Static_assert(CMP_OP_EQ == TOK_EQ, "CMP_OP_EQ");
_Static_assert(CMP_OP_NE == TOK_NE, "CMP_OP_NE");
_Static_assert(CMP_OP_ULE == TOK_ULE, "CMP_OP_ULE");
_Static_assert(CMP_OP_UGT == TOK_UGT, "CMP_OP_UGT");
_Static_assert(CMP_OP_Nset == TOK_Nset, "CMP_OP_Nset");
_Static_assert(CMP_OP_Nclear == TOK_Nclear, "CMP_OP_Nclear");
_Static_assert(CMP_OP_LT == TOK_LT, "CMP_OP_LT");
_Static_assert(CMP_OP_GE == TOK_GE, "CMP_OP_GE");
_Static_assert(CMP_OP_LE == TOK_LE, "CMP_OP_LE");
_Static_assert(CMP_OP_GT == TOK_GT, "CMP_OP_GT");


typedef struct CCVMInstr {
    struct {
        uint8_t opcode;
        uint8_t op2;
        union {
            uint8_t reg;
            uint8_t dstReg;
        };
        union {
            uint8_t srcReg;
            uint8_t addrReg;
        };
    };
    union {
        uint32_t value;
        uint32_t address;
        uint32_t label;
    };
    union {
        int32_t address_offset;
        int32_t labelAlias;
    };
} CCVMInstr;


static CCVMInstr* genInstr(uint8_t opcode, uint8_t force_output)
{
    int ind1;
    if (nocode_wanted && !force_output) {
        static CCVMInstr dummy;
        return &dummy;
    }
    ind1 = ind + sizeof(CCVMInstr);
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    CCVMInstr* res = (CCVMInstr*)&cur_text_section->data[ind];
    ind = ind1;
    memset(res, 0, sizeof(CCVMInstr));
    res->opcode = opcode;
    return res;
}

static void addReloc(Sym* sym, uint32_t address, int type)
{
    DEBUG_COMMENT("ElfReloc: %s", get_tok_str(sym->v, NULL));
    greloc(cur_text_section, sym, address, type);
}


static void instrMovReloc(int reg, Sym* sym) {
    DEBUG_INSTR("MOV_CONST R%d = %s", reg, get_tok_str(sym->v, NULL));
    addReloc(sym, ind, RELOC_INSTR);
    genInstr(INSTR_MOV_CONST, 0)->reg = reg;
}

static void instrMovConst(int reg, uint32_t value) {
    DEBUG_INSTR("MOV_CONST R%d = 0x%08X", reg, value);
    CCVMInstr* instr = genInstr(INSTR_MOV_CONST, 0);
    instr->reg = reg;
    instr->value = value;
}

static void instrMovReg(int to, int from) {
    DEBUG_INSTR("MOV_REG R%d = R%d", to, from);
    CCVMInstr* instr = genInstr(INSTR_MOV_REG, 0);
    instr->dstReg = to;
    instr->srcReg = from;
}

static void instrJumpReg(int is_call, int reg) {
    DEBUG_INSTR("%s_REG R%d", is_call ? "CALL" : "JUMP", reg);
    genInstr(is_call ? INSTR_CALL_REG : INSTR_JUMP_REG, 0)->reg = reg;
}

static void instrJumpLabel(int label) {
    DEBUG_INSTR("JUMP_CONST label_%d", label);
    genInstr(INSTR_JUMP_LABEL, 0)->label = label;
}

static void instrJumpReloc(int is_call, Sym* sym) {
    DEBUG_INSTR("%s_CONST %s", is_call ? "CALL" : "JUMP", get_tok_str(sym->v, NULL));
    addReloc(sym, ind, RELOC_INSTR);
    genInstr(is_call ? INSTR_CALL_CONST : INSTR_JUMP_CONST, 0);
}

static void instrPush(int bits, int reg) {
    int op2 = 1;
    switch (bits)
    {
        case 8: op2 = 1; break;
        case 16: op2 = 2; break;
        case 24: op2 = 3; break;
        case 32: op2 = 4; break;
        default: tcc_error("Internal error: invalid number of bits to push %d.", bits); break;
    }
    DEBUG_INSTR("PUSH%d R%d", bits, reg);
    CCVMInstr* instr = genInstr(INSTR_PUSH, 0);
    instr->op2 = op2;
    instr->reg = reg;
}

static void instrPushBlockLabel(int reg, int label, uint8_t optional) {
    DEBUG_INSTR("PUSH_BLOCK R%d, label_%d", reg, label);
    CCVMInstr* instr = genInstr(INSTR_PUSH_BLOCK_LABEL, 0);
    instr->reg = reg;
    instr->label = label;
    instr->op2 = optional;
}

static void instrPushBlockConst(int reg, int size, uint8_t optional) {
    DEBUG_INSTR("PUSH_BLOCK R%d, %d", reg, size);
    CCVMInstr* instr = genInstr(INSTR_PUSH_BLOCK_CONST, 0);
    instr->reg = reg;
    instr->value = size;
    instr->op2 = optional;
}

static void instrPopBlockConst(int size) {
    DEBUG_INSTR("POP_BLOCK %d", size);
    CCVMInstr* instr = genInstr(INSTR_POP_BLOCK_CONST, 0);
    instr->value = size;
}

static void instrReturn() {
    DEBUG_INSTR("RETURN");
    genInstr(INSTR_RETURN, 0);
}


static void instrJumpCondLabel(int op, int label) {
    DEBUG_INSTR("JUMP_IF 0x%02X [...reloc...]", op);
    CCVMInstr* instr = genInstr(INSTR_JUMP_COND_LABEL, 0);
    instr->op2 = op;
    instr->label = label;
}

static void instrBinOpConst(int op, int a, int value)
{
    DEBUG_INSTR("BIN_OP_CONST 0x%02X R%d 0x%08X", op, a, value);
    CCVMInstr* instr = genInstr(INSTR_BIN_OP_CONST, 0);
    instr->op2 = op;
    instr->dstReg = a;
    instr->value = value;
}

static void instrBinOp(int op, int a, int b)
{
    DEBUG_INSTR("BIN_OP 0x%02X R%d R%d", op, a, b);
    CCVMInstr* instr = genInstr(INSTR_BIN_OP, 0);
    instr->op2 = op;
    instr->dstReg = a;
    instr->srcReg = b;
}

static uint8_t instrReadWriteOp2(char* str, int* value, int bits, int sign_extend, int bp)
{
    uint8_t res = 0;
    if (sign_extend) {
        strcat(str, "S");
        res |= 0x80;
    }
    switch (bits) {
        case 8: res |= 0; strcat(str, "8 R%d, "); break;
        case 16: res |= 1; strcat(str, "16 R%d, "); break;
        case 32: res |= 2; strcat(str, "32 R%d, "); break;
        case 64: res |= 3; strcat(str, "64 X%d, "); break;
        default: tcc_error("Internal error: invalid number of bits to read."); strcat(str, "?? R%d, "); break;
    }
    if (bp) {
        strcat(str, "[BP] - ");
        res |= 0x40;
        if (value) *value = -*value;
    }
    return res;
}

static void instrRWReloc(int read, Sym* sym, int reg, int offset, int bits, int sign_extend)
{
    char format[128];
    char str[128];
    strcpy(format, read ? "READ" : "WRITE");
    uint8_t op2 = instrReadWriteOp2(format, NULL, bits, sign_extend, 0);
    sprintf(str, format, reg);
    DEBUG_INSTR("%s%s", str, get_tok_str(sym->v, NULL));
    addReloc(sym, ind, RELOC_INSTR);
    CCVMInstr* instr = genInstr(read ? INSTR_READ_CONST : INSTR_WRITE_CONST, 0);
    instr->op2 = op2;
    instr->reg = reg;
    instr->value = offset;
}

static void instrRWConst(int read, int reg, int value, int bits, int sign_extend, int bp)
{
    char format[128];
    char str[128];
    strcpy(format, read ? "READ" : "WRITE");
    uint8_t op2 = instrReadWriteOp2(format, NULL, bits, sign_extend, bp);
    sprintf(str, format, reg);
    DEBUG_INSTR("%s0x%08X", str, value);
    CCVMInstr* instr = genInstr(read ? INSTR_READ_CONST : INSTR_WRITE_CONST, 0);
    instr->op2 = op2;
    instr->reg = reg;
    instr->value = value;
}

static void instrRWInd(int read, int reg, int addrReg, int bits, int sign_extend)
{
    if (addrReg == 49) {
        addrReg = 0;
    }
    char format[128];
    char str[128];
    strcpy(format, read ? "READ" : "WRITE");
    uint8_t op2 = instrReadWriteOp2(format, NULL, bits, sign_extend, 0);
    sprintf(str, format, reg);
    DEBUG_INSTR("%s[R%d]", str, addrReg);
    CCVMInstr* instr = genInstr(read ? INSTR_READ_REG : INSTR_WRITE_REG, 0);
    instr->op2 = op2;
    instr->reg = reg;
    instr->addrReg = addrReg;
}

static void instrLabel(int label, int relative, int offset)
{
    DEBUG_INSTR("LABEL label_%d = %s0x%08X", label, relative ? "rel " : "", offset);
    CCVMInstr* instr = genInstr(relative ? INSTR_LABEL_RELATIVE : INSTR_LABEL_ABSOLUTE, 1);
    instr->label = label;
    instr->address_offset = offset;
}

static void instrLabelAlias(int labelA, int labelB)
{
    DEBUG_INSTR("ALIAS label_%d = label_%d", labelA, labelB);
    CCVMInstr* instr = genInstr(INSTR_LABEL_ALIAS, 1);
    instr->label = labelA;
    instr->labelAlias = labelB;
}
