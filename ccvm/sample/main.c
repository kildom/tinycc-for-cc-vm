#if 1

#include "header.h"
#include "../ccvm-enums.h"

CCVM_IMPORT(3, print);

void print(char* str);

int fy(short c, char x, short z) {
back:
    if (c == 0 ? x == 12 && z == 34 : x == 99) {
        x += 2;
    }
    while (c != 1) {
        c += 2;
        x += 3;
        a();
        if (x > 12) goto non12;
    }
    return x;
non12:
    goto back;
}

long long fll(long long x, long long y, long long z)
{
    return (x * x + y * y + z * z) * (x + 1);
}

extern short buffer[256];

short* with_offset = &buffer[33];

short buffer[256];

__attribute__((weak))
void fx() {
    a();
    b();
    a();
    b();
    a();
    b();
    a();
    b();
    a();
    b();
    a();
    b();
    a();
    b();
    a();
    b();
}

CCVM_EXPORT(7, main);

unsigned short counter;

int main() {
    ++counter;
    fx();
    if (a()) {
        a();
        b();
    }
    print("Hello, world!");
    return 0;
}

CCVM_EXPORT(0, some);

void some() {
    fx();
}


CCVM_EXPORT(2, exported_abc);

void exported_abc() {
}

__attribute__((weak))
void __ccvm_invalid_export_handler() {

}

#ifdef __ccvm_float_64__
#define _CCVM_XREG(reg) unsigned reg
#else
#define _CCVM_XREG(reg)
#endif


__attribute__((section(".ccvm.registers")))
struct {
    unsigned R0;
    _CCVM_XREG(X0);
    unsigned R1;
    _CCVM_XREG(X1);
    unsigned R2;
    _CCVM_XREG(X2);
    unsigned R3;
    _CCVM_XREG(X3);
    unsigned SP;
    unsigned PC;
    unsigned BP;
    unsigned FLAGS;
    unsigned AUX;
    unsigned STASH;
    unsigned char initialized;
} __ccvm_registers;

void __attribute__((constructor)) myInitializer() {
    fx();
    fx();
    fx();
    fx();
    fx();
    fx();
    fx();
}

void __attribute__((destructor)) myCleaner() {
    fx();
    fx();
    fx();
    fx();
}

typedef struct CCVMInstr {
    unsigned char opcode;
    unsigned char op2;
    unsigned char dstReg;
    unsigned char srcReg;
    unsigned int value1;
    unsigned int value2;
} CCVMInstr;

void __ccvm_c_startup__();

extern char __ccvm_export_table_begin__;
extern char __ccvm_export_table_end__;
extern char __ccvm_section_stack_begin__;
extern char __ccvm_section_stack_end__;
extern char __ccvm_section_rodata_begin__;

__attribute__((section(".text")))
__attribute__((packed))
CCVMInstr _ccvm_entry[] = {

    // READ uint8 __ccvm_registers.initialized
    { .opcode = INSTR_READ_CONST,  .dstReg = 3, .op2 = 0, .value1 = (unsigned int)&__ccvm_registers.initialized, },
    { .opcode = INSTR_BIN_OP_CONST, .op2 = BIN_OP_CMP,  .dstReg = 3, .value1 = 1, },
    { .opcode = INSTR_JUMP_COND_LABEL,  .op2 = CMP_OP_EQ, .value1 = 1 },
    { .opcode = INSTR_WRITE_CONST, .dstReg = 2, .op2 = 0, .value1 = (unsigned int)&__ccvm_registers.initialized, },
    { .opcode = INSTR_BIN_OP_CONST, .op2 = BIN_OP_MOV, .dstReg = 2, .value1 = (unsigned int)&__ccvm_section_stack_end__, },
    { .opcode = INSTR_WRITE_CONST, .dstReg = 2, .op2 = 2, .value1 = (unsigned int)&__ccvm_registers.SP, },
    { .opcode = INSTR_WRITE_CONST, .dstReg = 2, .op2 = 2, .value1 = (unsigned int)&__ccvm_registers.BP, },
    //{ .opcode = INSTR_BIN_OP_CONST, .op2 = BIN_OP_MOV, .dstReg = 2, .value1 = (unsigned int)&__ccvm_section_rodata_begin__, },
    //{ .opcode = INSTR_WRITE_CONST, .dstReg = 2, .op2 = 2, .value1 = (unsigned int)&__ccvm_registers.BP2, },
    { .opcode = INSTR_PUSH, .srcReg = 0, .op2 = 4 },
    { .opcode = INSTR_CALL_CONST, .value1 = (unsigned int)&__ccvm_c_startup__ },
    { .opcode = INSTR_POP, .srcReg = 0, .op2 = 4 },
    { .opcode = INSTR_LABEL_RELATIVE, .value1 = 1, .value2 = 0 },
    { .opcode = INSTR_BIN_OP_CONST, .dstReg = 0, .value1 = 2, .op2 = BIN_OP_SHL },        // MUL R0, 4
    { .opcode = INSTR_BIN_OP_CONST, .dstReg = 0, .op2 = BIN_OP_ADD, .value1 = (unsigned int)&__ccvm_export_table_begin__ },
                                                                               // ADD R3, __ccvm_export_table_begin__
    { .opcode = INSTR_READ_REG,  .dstReg = 0, .srcReg = 0, .op2 = 2 },   // READ32 R3, [R3]  # or READ16U for small model
    { .opcode = INSTR_CALL_REG,  .dstReg = 0 },                                 // CALL R3
    { .opcode = INSTR_HOST,      .value1 = (unsigned int)(-1) },                 // HOST -1 # Return to host
    
                                                
    /*{ .opcode = INSTR_PUSH,      .dstReg = 3, .op2 = 4, },                      // PUSH R3
    { .opcode = INSTR_READ_REG,  .dstReg = 3, .srcReg = 3, .op2 = 0 / *??* / },   // READ32 R3, [R3]  # or READ16U for small model
    { .opcode = INSTR_CALL_REG,  .dstReg = 3 },                                 // CALL R3
    //{ .opcode = INSTR_POP,       .dstReg = 3 },                               // POP R3
    //{ .opcode = INSTR_HOST,    .value = (unsigned int)(-1) },                 // HOST -1 # Return to host
    */
};

__attribute__((section(".text.ccvm.entry")))
__attribute__((packed))
CCVMInstr _ccvm_entry_jump[] = {
    { .opcode = INSTR_JUMP_CONST, .value1 = (unsigned int)&_ccvm_entry},
};

extern char __ccvm_section_bss_begin__;

void* ret_bss() {
    return &__ccvm_section_bss_begin__;
}

__attribute__((weak))
__attribute__((section(".ccvm.heap")))
unsigned __ccvm_heap_buffer[1024] = {99};

long long addll(long long a, long long b) {
    return a + b;
}

long long subll(long long a, long long b) {
    return a - b;
}

CCVM_EXPORT(5, goto_ptr_test);

void* goto_ptr_test(void* x) {
    if (x == (void*)0) {
        goto start;
    } else {
        goto *x;
    }
start:
    return &&next;
next:
    return &&next2;
next2:
    return (void*)0;
}

__attribute__((section(".init_array.my")))
int fsskjdhfgjasdf = 1;
__attribute__((section(".init_array.my")))
int fsskjdhfgjasdf2 = 2;


CCVM_EXPORT(6, goto_test);

void* goto_test(void* x) {
    if (x == (void*)0) {
        goto start;
    } else {
        goto next;
    }
start:
    return (void*)0;
next:
    return (void*)1; // TODO: Hardfault if this line is return 1;
}


extern char __ccvm_section_data_begin__;
extern char __ccvm_load_section_data_begin__;
extern char __ccvm_section_data_end__;

typedef void (*init_fini_func_t)(void);

extern init_fini_func_t __ccvm_section_init_begin__;
extern init_fini_func_t __ccvm_section_init_end__;

void memcpy(void *dest, const void *src, unsigned int size);

void __ccvm_c_startup__() {
    init_fini_func_t* ptr;
    memcpy(&__ccvm_section_data_begin__, &__ccvm_load_section_data_begin__, (unsigned int)&__ccvm_section_data_end__ - (unsigned int)&__ccvm_section_data_begin__);
    for (ptr = &__ccvm_section_init_begin__; ptr < &__ccvm_section_init_end__; --ptr) {
        (*ptr)();
    }
}

void memcpy(void *dest, const void* src, unsigned int size) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (size--) {
        *d++ = *s++;
    }
}

int arr[16];

typedef double T;

T test_arr(T x) {
    return -x;
}

int test_mod(int x, int y) {
    return x % y;
}

long long test_mul64(long long x, long long y) {
    return x * y;
}

CCVM_EXPORT(4, testall);

void testall() {
    static int arr[] = {
        (int)(void*)&test_arr,
        (int)(void*)&test_mod,
        (int)(void*)&test_arr,
    };
}

/*
CCVM_EXPORT(10, test_arr_float);
CCVM_EXPORT(11, test_arr_double);
CCVM_EXPORT(12, test_arr_ldouble);
float test_arr_float(float index) {
    return index + 1.0f;
}

double test_arr_double(double index) {
    return index + 1.0;
}

long double test_arr_ldouble(long double index) {
    return index + 1.0l;
}
*/

#endif