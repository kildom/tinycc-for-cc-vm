
#include "header.h"

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
