
#include "header.h"

CCVM_IMPORT(2)
int host2(int x);

void b() {
    if (host2(12)) {
        host2(13);
    }
}
