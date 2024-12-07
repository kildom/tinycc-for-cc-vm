#if 1
#include "header.h"

CCVM_IMPORT(2, host2);

int host2(int x);

void b() {
    if (host2(12)) {
        host2(13);
    }
}

__attribute__((weak))
__attribute__((section(".ccvm.heap")))
unsigned __ccvm_heap_buffer[1024] = {2};
#endif