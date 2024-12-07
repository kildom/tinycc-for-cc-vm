#include "header.h"

CCVM_IMPORT(11, test1);
int test1();

CCVM_IMPORT(12, test2);
int test2();

CCVM_IMPORT(13, test3);
int test3();

int aaaaaaaa() {
    return test1() ? test2() : test3();
}

#if 1

CCVM_IMPORT(1, host);
int host();


__attribute__((section(".text.some")))
int a() {
    return host() ? host() : 10 + host();
}

__attribute__((constructor))
void f() {
    host();
}

CCVM_IMPORT(4, print2);

void print2(char* str);

void fxx() {
    print2("ABC");
}

typedef void (*func_ptr_t)();

func_ptr_t ptrs[] = {
    fxx,
    f,
};

static int abc;
static int* abcptr = &abc + 1;

__attribute__((weak))
__attribute__((section(".ccvm.heap")))
unsigned __ccvm_heap_buffer[1024] = {1, 2, 3};
#endif