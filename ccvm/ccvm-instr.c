
#include <stdint.h>

#include "ccvm-output.h"

#ifdef INTELLISENSE
#define USING_GLOBALS
#include "tcc.h"
#endif


enum {
    INSTR_MOV,
    INSTR_JUMP,
    INSTR_CALL,
    INSTR_PUSH_BLOCK,
    INSTR_RETURN,
    INSTR_JUMP_IF,
    INSTR_READF64,
    INSTR_READ32,
    INSTR_READS8,
    INSTR_READS16,
    INSTR_READU8,
    INSTR_READU16,
    INSTR_WRITEF64,
    INSTR_WRITE32,
    INSTR_WRITE8,
    INSTR_WRITE16,
    INSTR_PUSH8,
    INSTR_PUSH16,
    INSTR_PUSH24,
    INSTR_PUSH32,
    INSTR_BIN_OP,
    INSTR_CMP,

    INSTR_FLAG_BP = 0x80,
    INSTR_FLAG_PROG = 0x40,
    INSTR_FLAG_IMM32 = 0x18,
    INSTR_FLAG_IMM16 = 0x10,
    INSTR_FLAG_IMM8 = 0x08,
    INSTR_FLAG_REG = 0x00,
};


static void gImm(int value, int flags)
{
    if (value < 128 && value >= -128) {
        g8(INSTR_FLAG_IMM8 | flags);
        g8(value);
    } else if (value < 32768 && value >= -32768) {
        g8(INSTR_FLAG_IMM16 | flags);
        g16(value);
    } else if (value < 0x40000000 + 128 && value >= 0x40000000 - 128) {
        g8(INSTR_FLAG_IMM8 | INSTR_FLAG_PROG | flags);
        g8(value - 0x40000000);
    } else if (value < 0x40000000 + 32768 && value >= 0x40000000 - 32768) {
        g8(INSTR_FLAG_IMM16 | INSTR_FLAG_PROG | flags);
        g16(value - 0x40000000);
    } else {
        g8(INSTR_FLAG_IMM32 | flags);
        g32(value);
    }
}

static int instrMovReloc(int reg) {
    DEBUG_INSTR("MOV_CONST R%d = [...reloc...]", reg);
    g8(INSTR_MOV);
    g8(INSTR_FLAG_IMM32);
    int ret = ind;
    g32(0);
    g8(reg);
    return ret;
}

static void instrMovConst(int reg, uint32_t value) {
    DEBUG_INSTR("MOV_CONST R%d = 0x%08X", reg, value);
    g8(INSTR_MOV);
    gImm(value, 0);
    g8(reg);
}

static void instrMovReg(int to, int from) {
    DEBUG_INSTR("MOV_REG R%d = R%d", to, from);
    g8(INSTR_MOV);
    g8(INSTR_FLAG_REG);
    g8(from);
    g8(to);
}

static void instrJumpReg(int reg) {
    DEBUG_INSTR("JUMP_REG R%d", reg);
    g8(INSTR_JUMP);
    g8(INSTR_FLAG_REG);
    g8(reg);
}

static void instrCallReg(int reg) {
    DEBUG_INSTR("CALL_REG R%d", reg);
    g8(INSTR_CALL);
    g8(INSTR_FLAG_REG);
    g8(reg);
}


static int instrJumpConst() {
    DEBUG_INSTR("JUMP_CONST [...reloc...]");
    g8(INSTR_JUMP);
    g8(INSTR_FLAG_IMM32);
    int address = ind;
    g32(0);
    return address;
}

static int instrCallConst() {
    DEBUG_INSTR("CALL_CONST [...reloc...]");
    g8(INSTR_CALL);
    g8(INSTR_FLAG_IMM32);
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
    g8(INSTR_FLAG_REG);
    g8(reg);
}

static int instrPushBlockReloc(int reg) {
    DEBUG_INSTR("PUSH_BLOCK R%d, [...reloc...]", reg);
    g8(INSTR_PUSH_BLOCK);
    g8(INSTR_FLAG_IMM32);
    int ret = ind;
    g32(0);
    g8(reg);
    return ret;
}

static void instrPushBlockConst(int reg, int size) {
    DEBUG_INSTR("PUSH_BLOCK R%d, %d", reg, size);
    g8(INSTR_PUSH_BLOCK);
    gImm(size, 0);
    g8(reg);
}

static void instrReturn(int cleanupWords) {
    DEBUG_INSTR("RETURN %d", cleanupWords);
    g8(INSTR_RETURN);
    gImm(cleanupWords, 0);
}


static int instrJumpCond(int op) {
    DEBUG_INSTR("JUMP_IF 0x%02X [...reloc...]", op);
    g8(INSTR_JUMP_IF);
    g8(INSTR_FLAG_IMM32);
    int address = ind;
    g32(0);
    g8(op);
    return address;
}

static void instrBinOp(int op, int a, int b)
{
    DEBUG_INSTR("BIN_OP 0x%02X R%d R%d", op, a, b);
    g8(INSTR_BIN_OP);
    g8(INSTR_FLAG_REG);
    g8(b);
    g8(a);
    g8(op);
}

static void instrCmp(int a, int b)
{
    DEBUG_INSTR("CMP R%d R%d", a, b);
    g8(INSTR_CMP);
    g8(INSTR_FLAG_REG);
    g8(a);
    g8(b);
}


static int instrReadConst(int reg, int value, int bits, int sign_extend, int bp)
{
    const char *bpStr = "";
    if (bp) {
        bpStr = "[BP] - ";
        value = -value;
    }
    if (bits == 64) {
        DEBUG_INSTR("READF64 X%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_READF64);
    } else if (bits == 32) {
        DEBUG_INSTR("READ32 R%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_READ32);
    } else if (sign_extend) {
        DEBUG_INSTR("READS%d R%d, %s0x%08X", bits, reg, bpStr, value);
        g8(bits == 8 ? INSTR_READS8 : INSTR_READS16);
    } else {
        DEBUG_INSTR("READU%d R%d, %s0x%08X", bits, reg, bpStr, value);
        g8(bits == 8 ? INSTR_READU8 : INSTR_READU16);
    }
    g8(INSTR_FLAG_IMM32 | (bp ? INSTR_FLAG_BP : 0));
    int ret = ind;
    g32(value);
    g8(reg);
    return ret;
}

static void instrReadInd(int to, int from, int bits, int sign_extend)
{
    if (bits == 64) {
        DEBUG_INSTR("READF64 X%d, [R%d]", to, from);
        g8(INSTR_READF64);
    } else if (bits == 32) {
        DEBUG_INSTR("READ32 R%d, [R%d]", to, from);
        g8(INSTR_READ32);
    } else if (sign_extend) {
        DEBUG_INSTR("READS%d R%d, [R%d]", bits, to, from);
        g8(bits == 8 ? INSTR_READS8 : INSTR_READS16);
    } else {
        DEBUG_INSTR("READU%d R%d, [R%d]", bits, to, from);
        g8(bits == 8 ? INSTR_READU8 : INSTR_READU16);
    }
    g8(INSTR_FLAG_REG);
    g8(from);
    g8(to);
}

static int instrWriteConst(int reg, int value, int bits, int bp)
{
    const char *bpStr = "";
    if (bp) {
        bpStr = "[BP] - ";
        value = -value;
    }
    if (bits == 64) {
        DEBUG_INSTR("WRITEF64 X%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_WRITEF64);
    } else if (bits == 32) {
        DEBUG_INSTR("WRITE32 R%d, %s0x%08X", reg, bpStr, value);
        g8(INSTR_WRITE32);
    } else {
        DEBUG_INSTR("WRITE%d R%d, %s0x%08X", bits, reg, bpStr, value);
        g8(bits == 8 ? INSTR_WRITE8 : INSTR_WRITE16);
    }
    g8(INSTR_FLAG_IMM32 | (bp ? INSTR_FLAG_BP : 0));
    int ret = ind;
    g32(value);
    g8(reg);
    return ret;
}

static void instrWriteInd(int to, int from, int bits)
{
    if (bits == 64) {
        DEBUG_INSTR("WRITEF64 X%d, [R%d]", to, from);
        g8(INSTR_WRITEF64);
    } else if (bits == 32) {
        DEBUG_INSTR("WRITE32 R%d, [R%d]", to, from);
        g8(INSTR_WRITE32);
    } else {
        DEBUG_INSTR("WRITE%d R%d, [R%d]", bits, to, from);
        g8(bits == 8 ? INSTR_WRITE8 : INSTR_WRITE16);
    }
    g8(INSTR_FLAG_REG);
    g8(from);
    g8(to);
}

static void addReloc(Sym* sym, uint32_t address, int type)
{
    DEBUG_COMMENT("ElfReloc: %s", get_tok_str(sym->v, NULL));
    greloc(cur_text_section, sym, address, type);
}

