
#include "header.h"

CCVM_IMPORT(1)
int host();

int a() {
    return host() ? host() : 10 + host();
}
