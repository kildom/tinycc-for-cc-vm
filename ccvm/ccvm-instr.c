
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
    INSTR_PUSH_BLOCK_CONST, // reg, value = block size
    INSTR_PUSH_BLOCK_LABEL, // reg, label = label containing block size
    INSTR_CMP,              // srcReg, dstReg, op2 = comparison operator
    INSTR_BIN_OP,           // srcReg, dstReg, op2 = operator
    INSTR_RETURN,           // value = cleanup words
    INSTR_LABEL_ALIAS,      // labelAlias = label
};


typedef struct CCVMInstr {
    union {
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
        int32_t address_offset;
        int32_t labelAlias;
    };
    union {
        uint32_t value;
        uint32_t address;
        uint32_t label;
    };
} CCVMInstr;


static CCVMInstr* genInstr(uint8_t opcode)
{
    int ind1;
    if (nocode_wanted) {
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
    genInstr(INSTR_MOV_CONST)->reg = reg;
}

static void instrMovConst(int reg, uint32_t value) {
    DEBUG_INSTR("MOV_CONST R%d = 0x%08X", reg, value);
    CCVMInstr* instr = genInstr(INSTR_MOV_CONST);
    instr->reg = reg;
    instr->value = value;
}

static void instrMovReg(int to, int from) {
    DEBUG_INSTR("MOV_REG R%d = R%d", to, from);
    CCVMInstr* instr = genInstr(INSTR_MOV_REG);
    instr->dstReg = to;
    instr->srcReg = from;
}

static void instrJumpReg(int is_call, int reg) {
    DEBUG_INSTR("%s_REG R%d", is_call ? "CALL" : "JUMP", reg);
    genInstr(is_call ? INSTR_CALL_REG : INSTR_JUMP_REG)->reg = reg;
}

static void instrJumpLabel(int label) {
    DEBUG_INSTR("JUMP_CONST label_%d", label);
    genInstr(INSTR_JUMP_LABEL)->label = label;
}

static void instrJumpReloc(int is_call, Sym* sym) {
    DEBUG_INSTR("%s_CONST %s", is_call ? "CALL" : "JUMP", get_tok_str(sym->v, NULL));
    addReloc(sym, ind, RELOC_INSTR);
    genInstr(is_call ? INSTR_CALL_CONST : INSTR_JUMP_CONST);
}

static void instrPush(int bits, int reg) {
    int op2 = 1;
    switch (bits)
    {
        case 8: op2 = 1; break;
        case 16: op2 = 2; break;
        case 24: op2 = 3; break;
        case 32: op2 = 4; break;
        default: tcc_error("Internal error: invalid number of bits to push."); break;
    }
    DEBUG_INSTR("PUSH%d R%d", bits, reg);
    CCVMInstr* instr = genInstr(INSTR_PUSH);
    instr->op2 = op2;
    instr->reg = reg;
}

static void instrPushBlockLabel(int reg, int label) {
    DEBUG_INSTR("PUSH_BLOCK R%d, label_%d", reg, label);
    CCVMInstr* instr = genInstr(INSTR_PUSH_BLOCK_LABEL);
    instr->reg = reg;
    instr->label = label;
}

static void instrPushBlockConst(int reg, int size) {
    DEBUG_INSTR("PUSH_BLOCK R%d, %d", reg, size);
    CCVMInstr* instr = genInstr(INSTR_PUSH_BLOCK_CONST);
    instr->reg = reg;
    instr->value = size;
}

static void instrReturn(int cleanupWords) {
    DEBUG_INSTR("RETURN %d", cleanupWords);
    genInstr(INSTR_RETURN)->value = cleanupWords;
}


static void instrJumpCondLabel(int op, int label) {
    DEBUG_INSTR("JUMP_IF 0x%02X [...reloc...]", op);
    CCVMInstr* instr = genInstr(INSTR_JUMP_COND_LABEL);
    instr->op2 = op;
    instr->label = label;
}

static void instrBinOp(int op, int a, int b)
{
    DEBUG_INSTR("BIN_OP 0x%02X R%d R%d", op, a, b);
    CCVMInstr* instr = genInstr(INSTR_BIN_OP);
    instr->op2 = op;
    instr->dstReg = a;
    instr->srcReg = b;
}

static void instrCmp(int a, int b)
{
    DEBUG_INSTR("CMP R%d R%d", a, b);
    CCVMInstr* instr = genInstr(INSTR_CMP);
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

static void instrRWReloc(int read, Sym* sym, int reg, int bits, int sign_extend)
{
    char format[128];
    char str[128];
    strcpy(format, read ? "READ" : "WRITE");
    uint8_t op2 = instrReadWriteOp2(format, NULL, bits, sign_extend, 0);
    sprintf(str, format, reg);
    DEBUG_INSTR("%s%s", str, get_tok_str(sym->v, NULL));
    addReloc(sym, ind, RELOC_INSTR);
    CCVMInstr* instr = genInstr(read ? INSTR_READ_CONST : INSTR_WRITE_CONST);
    instr->op2 = op2;
    instr->reg = reg;
}

static void instrRWConst(int read, int reg, int value, int bits, int sign_extend, int bp)
{
    char format[128];
    char str[128];
    strcpy(format, read ? "READ" : "WRITE");
    uint8_t op2 = instrReadWriteOp2(format, NULL, bits, sign_extend, bp);
    sprintf(str, format, reg);
    DEBUG_INSTR("%s0x%08X", str, value);
    CCVMInstr* instr = genInstr(read ? INSTR_READ_CONST : INSTR_WRITE_CONST);
    instr->op2 = op2;
    instr->reg = reg;
    instr->value = value;
}

static void instrRWInd(int read, int reg, int addrReg, int bits, int sign_extend)
{
    char format[128];
    char str[128];
    strcpy(format, read ? "READ" : "WRITE");
    uint8_t op2 = instrReadWriteOp2(format, NULL, bits, sign_extend, 0);
    sprintf(str, format, reg);
    DEBUG_INSTR("%s[R%d]", str, addrReg);
    CCVMInstr* instr = genInstr(read ? INSTR_READ_REG : INSTR_WRITE_REG);
    instr->op2 = op2;
    instr->reg = reg;
    instr->addrReg = addrReg;
}

static void instrLabel(int label, int relative, int offset)
{
    DEBUG_INSTR("LABEL label_%d = %s0x%08X", label, relative ? "rel " : "", offset);
    CCVMInstr* instr = genInstr(relative ? INSTR_LABEL_RELATIVE : INSTR_LABEL_ABSOLUTE);
    instr->label = label;
    instr->address_offset = offset;
}

static void instrLabelAlias(int labelA, int labelB)
{
    DEBUG_INSTR("ALIAS label_%d = label_%d", labelA, labelB);
    CCVMInstr* instr = genInstr(INSTR_LABEL_ALIAS);
    instr->label = labelA;
    instr->labelAlias = labelB;
}
