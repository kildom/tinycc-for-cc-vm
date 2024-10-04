
#include <stdint.h>

#define USING_GLOBALS
#include "tcc.h"

#define DEBUG_COMMENT(text, ...) printf("                    // " text "\n", ##__VA_ARGS__)
#define DEBUG_INSTR(text, ...) printf("%08X:   " text "\n", ind, ##__VA_ARGS__)

#define MALLOC_OR_STACK(var, required_size) \
    long long _TMP_##var[64 / 8]; \
    var = required_size > 64 ? tcc_malloc(required_size) : (void*)_TMP_##var;

#define FREE_OR_STACK(var) \
    do { \
        if ((void*)var != (void*)_TMP_##var) tcc_free(var); \
    } while (0);

static void g8(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 1;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind] = c;
    ind = ind1;
}

static void g16(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 2;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind++] = c;
    cur_text_section->data[ind++] = c >> 8;
}

static void g24(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 3;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind++] = c;
    cur_text_section->data[ind++] = c >> 8;
    cur_text_section->data[ind++] = c >> 16;
}

static void g32(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 4;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind++] = c;
    cur_text_section->data[ind++] = c >> 8;
    cur_text_section->data[ind++] = c >> 16;
    cur_text_section->data[ind++] = c >> 24;
}

static Section *local_reloc_section = NULL;

typedef struct LocalRelocEntry {
    int type;
    uint32_t source;
    uint32_t target;
} LocalRelocEntry;

#define SHT_LOCAL_RELOC (SHT_LOUSER + 0x82FE91A)

static void setupLocalReloc() {
    TCCState* s = cur_text_section->s1;
    for (int i = 1; i < s->nb_sections; i++) {
        if (s->sections[i]->link == cur_text_section) {
            local_reloc_section = s->sections[i];
            goto leave;
        }
    }
    int len = strlen(cur_text_section->name);
    char *local_name;
    MALLOC_OR_STACK(local_name, len + 13);
    sprintf(local_name, ".ccvm.loc.rel%s", cur_text_section->name);
    local_reloc_section = new_section(s, local_name, SHT_LOCAL_RELOC, 0);
    local_reloc_section->sh_entsize = sizeof(LocalRelocEntry);
    local_reloc_section->link = cur_text_section;
leave:
    FREE_OR_STACK(local_name);
}

#define LOCAL_RELOC_ADDR 1
#define LOCAL_RELOC_LABEL 2
#define LOCAL_RELOC_SET_LABEL 3
#define LOCAL_RELOC_ALIAS_LABEL 4

static void addLocalReloc(int type, uint32_t source, uint32_t target) {
    if (!local_reloc_section || local_reloc_section->link != cur_text_section) {
        setupLocalReloc();
    }
    uint32_t new_offset = cur_text_section->data_offset + sizeof(LocalRelocEntry);
    if (new_offset > cur_text_section->data_allocated)
        section_realloc(cur_text_section, new_offset);
    LocalRelocEntry* entry = &cur_text_section->data[cur_text_section->data_offset];
    cur_text_section->data_offset = new_offset;
    entry->type = type;
    entry->source = source;
    entry->target = target;
    switch (type)
    {
    case LOCAL_RELOC_ADDR:
        DEBUG_COMMENT("LocalReloc: from 0x%08X to 0x%08X", source, target);
        break;
    case LOCAL_RELOC_LABEL:
        DEBUG_COMMENT("LocalReloc: from label_%d to 0x%08X", source, target);
        break;
    case LOCAL_RELOC_SET_LABEL:
        DEBUG_COMMENT("LocalReloc: set label_%d = 0x%08X", target, source);
        break;
    case LOCAL_RELOC_ALIAS_LABEL:
        DEBUG_COMMENT("LocalReloc: alias label_%d = label_%d", source, target);
        break;
    default:
        break;
    }
}

#define R_CCVM_SIMPLE 1
#define R_CCVM_LOCAL 2
#define R_CCVM_LABEL 3

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
#define INSTR_PROLOGUE 12
#define INSTR_RETURN 13
#define INSTR_JUMP_IF 14
#define INSTR_BIN_OP 15
#define INSTR_CMP 16


static void instrMovConst(int reg, uint32_t value) {
    DEBUG_INSTR("MOV_CONST R%d = 0x%08X", reg, value);
    g8(INSTR_MOV_CONST);
    g8(reg);
    g32(value);
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

static void instrPushBlock(int reg, int size) {
    DEBUG_INSTR("PUSH_BLOCK %d => R%d", size, reg);
    g8(INSTR_PUSH_BLOCK);
    g8(reg);
    g32(size);
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
    DEBUG_INSTR("PUSH%d %d => R%d", bits, reg);
    g8(instr);
    g8(reg);
}

static void instrPrologue(int stackWords) {
    if (stackWords < 0) {
        DEBUG_INSTR("PROLOGUE [... to be filled later ...]");
    } else {
        DEBUG_INSTR("PROLOGUE 4 * %d", stackWords);
    }
    g8(INSTR_PROLOGUE);
    g16(stackWords);
}

static void instrReturn() {
    DEBUG_INSTR("RETURN");
    g8(INSTR_RETURN);
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
