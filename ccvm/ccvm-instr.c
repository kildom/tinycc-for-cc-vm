
#include <stdint.h>

#include "ccvm-output.h"

#ifdef INTELLISENSE
#define USING_GLOBALS
#include "tcc.h"
#endif

#define INSTR_MOV_CONST 1
#define INSTR_MOV_REG 2
#define INSTR_JUMP_REG 3
#define INSTR_CALL_REG 4
#define INSTR_JUMP_CONST 5
#define INSTR_CALL_CONST 6
#define INSTR_PUSH_BLOCK 7
#define INSTR_PUSH8 8
#define INSTR_PUSH16 9
#define INSTR_PUSH24 10
#define INSTR_PUSH32 11
#define INSTR_RETURN 13
#define INSTR_JUMP_IF 14
#define INSTR_BIN_OP 15
#define INSTR_CMP 16
#define INSTR_READF64 17
#define INSTR_READF64_BP 18
#define INSTR_READ32 19
#define INSTR_READ32_BP 20
#define INSTR_READS8 21
#define INSTR_READS8_BP 22
#define INSTR_READS16 23
#define INSTR_READS16_BP 24
#define INSTR_READU8 25
#define INSTR_READU8_BP 26
#define INSTR_READU16 27
#define INSTR_READU16_BP 28
#define INSTR_WRITEF64 29
#define INSTR_WRITEF64_BP 30
#define INSTR_WRITE32 31
#define INSTR_WRITE32_BP 32
#define INSTR_WRITE8 33
#define INSTR_WRITE8_BP 34
#define INSTR_WRITE16 35
#define INSTR_WRITE16_BP 36
#define INSTR_READF64_IND 37
#define INSTR_READ32_IND 38
#define INSTR_READS8_IND 39
#define INSTR_READS16_IND 40
#define INSTR_READU8_IND 41
#define INSTR_READU16_IND 42
#define INSTR_WRITEF64_IND 43
#define INSTR_WRITE32_IND 44
#define INSTR_WRITE8_IND 45
#define INSTR_WRITE16_IND 46


static int instrMovConst(int reg, uint32_t value) {
    DEBUG_INSTR("MOV_CONST R%d = 0x%08X", reg, value);
    g8(INSTR_MOV_CONST);
    g8(reg);
    int ret = ind;
    g32(value);
    return ret;
}

static void instrMovReg(int to, int from) {
    DEBUG_INSTR("MOV_REG R%d = R%d", to, from);
    g8(INSTR_MOV_REG);
    g8(to);
    g8(from);
}

static void instrJumpReg(int reg) {
    DEBUG_INSTR("JUMP_REG R%d", reg);
    g8(INSTR_JUMP_REG);
}

static void instrCallReg(int reg) {
    DEBUG_INSTR("CALL_REG R%d", reg);
    g8(INSTR_CALL_REG);
}


static int instrJumpConst() {
    DEBUG_INSTR("JUMP_CONST [...reloc...]");
    g8(INSTR_JUMP_CONST);
    int address = ind;
    g32(0);
    return address;
}

static int instrCallConst() {
    DEBUG_INSTR("CALL_CONST [...reloc...]");
    g8(INSTR_CALL_CONST);
    int address = ind;
    g32(0);
    return address;
}

static void instrPush(int bits, int reg) {
    int instr = INSTR_PUSH8;
    switch (bits)
    {
        case 8: instr = INSTR_PUSH8; break;
        case 16: instr = INSTR_PUSH16; break;
        case 24: instr = INSTR_PUSH24; break;
        case 32: instr = INSTR_PUSH32; break;
        default: tcc_error("Internal error: invalid number of bits to push."); break;
    }
    DEBUG_INSTR("PUSH%d R%d", bits, reg);
    g8(instr);
    g8(reg);
}

static int instrPushBlock(int reg, int size) {
    DEBUG_INSTR("PUSH_BLOCK R%d, %d", reg, size);
    g8(INSTR_PUSH_BLOCK);
    g8(reg);
    int ret = ind;
    g32(size);
    return ret;
}

static void instrReturn(int cleanupWords) {
    DEBUG_INSTR("RETURN %d", cleanupWords);
    g8(INSTR_RETURN);
    g32(cleanupWords);
}


static int instrJumpCond(int op) {
    DEBUG_INSTR("JUMP_IF 0x%02X [...reloc...]", op);
    g8(INSTR_JUMP_IF);
    int address = ind;
    g32(0);
    return address;
}

static void instrBinOp(int op, int a, int b)
{
    DEBUG_INSTR("BIN_OP 0x%02X R%d R%d", op, a, b);
    g8(INSTR_BIN_OP);
    g8(a);
    g8(op);
    g8(b);
}

static void instrCmp(int a, int b)
{
    DEBUG_INSTR("CMP R%d R%d", a, b);
    g8(INSTR_CMP);
    g8(a);
    g8(b);
}


static int instrReadConst(int reg, int value, int bits, int sign_extend, int bp)
{
    const char *bpStr = "";
    int bpAdd = 0;
    if (bp) {
        bpStr = "[BP] - ";
        bpAdd = 1;
        value = -value;
    }
    if (bits == 64) {
        DEBUG_INSTR("READF64 X%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_READF64 + bpAdd);
    } else if (bits == 32) {
        DEBUG_INSTR("READ32 R%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_READ32 + bpAdd);
    } else if (sign_extend) {
        DEBUG_INSTR("READS%d R%d, %s0x%08X", bits, reg, bpStr, value);
        g8(bits == 8 ? INSTR_READS8 + bpAdd : INSTR_READS16 + bpAdd);
    } else {
        DEBUG_INSTR("READU%d R%d, %s0x%08X", bits, reg, bpStr, value);
        g8(bits == 8 ? INSTR_READU8 + bpAdd : INSTR_READU16 + bpAdd);
    }
    int ret = ind;
    g32(value);
    return ret;
}

static void instrReadInd(int to, int from, int bits, int sign_extend)
{
    if (bits == 64) {
        DEBUG_INSTR("READF64 X%d, [R%d]", to, from);
        g8(INSTR_READF64_IND);
    } else if (bits == 32) {
        DEBUG_INSTR("READ32 R%d, [R%d]", to, from);
        g8(INSTR_READ32_IND);
    } else if (sign_extend) {
        DEBUG_INSTR("READS%d R%d, [R%d]", bits, to, from);
        g8(bits == 8 ? INSTR_READS8_IND : INSTR_READS16_IND);
    } else {
        DEBUG_INSTR("READU%d R%d, [R%d]", bits, to, from);
        g8(bits == 8 ? INSTR_READU8_IND : INSTR_READU16_IND);
    }
}

static int instrWriteConst(int reg, int value, int bits, int bp)
{
    const char *bpStr = "";
    int bpAdd = 0;
    if (bp) {
        bpStr = "[BP] - ";
        bpAdd = 1;
        value = -value;
    }
    if (bits == 64) {
        DEBUG_INSTR("WRITEF64 X%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_WRITEF64 + bpAdd);
    } else if (bits == 32) {
        DEBUG_INSTR("WRITE32 R%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_WRITE32 + bpAdd);
    } else {
        DEBUG_INSTR("WRITE%d R%d, %s0x%08X", bits, reg, bpStr, value);
        g8(bits == 8 ? INSTR_WRITE8 + bpAdd : INSTR_WRITE16 + bpAdd);
    }
    int ret = ind;
    g32(value);
    return ret;
}

static void instrWriteInd(int to, int from, int bits)
{
    if (bits == 64) {
        DEBUG_INSTR("WRITEF64 X%d, [R%d]", to, from);
        g8(INSTR_WRITEF64_IND);
    } else if (bits == 32) {
        DEBUG_INSTR("WRITE32 R%d, [R%d]", to, from);
        g8(INSTR_WRITE32_IND);
    } else {
        DEBUG_INSTR("WRITE%d R%d, [R%d]", bits, to, from);
        g8(bits == 8 ? INSTR_WRITE8_IND : INSTR_WRITE16_IND);
    }
}

static void addReloc(Sym* sym, uint32_t address)
{
    DEBUG_COMMENT("ElfReloc: %s", get_tok_str(sym->v, NULL));
    greloc(cur_text_section, sym, address, 1);
}

