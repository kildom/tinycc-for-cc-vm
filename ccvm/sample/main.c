

#include "header.h"

CCVM_IMPORT(3, print);

void print(char* str);

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
