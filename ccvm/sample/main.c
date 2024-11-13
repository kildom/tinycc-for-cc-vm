

#include "header.h"

CCVM_IMPORT(3, print);

void print(char* str);

int fy(short c, char x, short z) {
back:
    if (c == 0 ? x == 12 && z == 34 : x == 99) {
        x++;
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

int main() {
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

__attribute__((section(".ccvm.registers")))
struct {
    unsigned R0;
    unsigned X0;
    unsigned R1;
    unsigned X1;
    unsigned R2;
    unsigned X2;
    unsigned R3;
    unsigned X3;
    unsigned SP;
    unsigned PC;
    unsigned BP;
    unsigned BP2;
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

static void _ccvm_entry() {

}

#define INSTR_JUMP_CONST 5

__attribute__((section(".ccvm.entry")))
__attribute__((packed))
struct
{
    unsigned char opcode;
    unsigned int address;
} __attribute__((packed)) __ccvm_entry_instruction __attribute__((packed)) = {
    INSTR_JUMP_CONST,
    (unsigned int)&_ccvm_entry,
};


__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_registers_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_registers_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_registers_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_data_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_data_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_data_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_bss_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_bss_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_bss_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_entry_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_entry_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_entry_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_rodata_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_rodata_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_rodata_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_export_table_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_export_table_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_ccvm_export_table_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_text_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_text_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_section_text_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_stack_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_stack_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_stack_size;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_heap_begin;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_heap_initial_end;
__attribute__((section(".ccvm.link.symbols"))) char __ccvm_heap_initial_size;

void* ret_bss() {
    return &__ccvm_section_bss_begin;
}

__attribute__((weak))
__attribute__((section(".ccvm.heap")))
unsigned __ccvm_heap_buffer[1024] = {99};
