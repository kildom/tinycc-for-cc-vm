
extern int x;
int e11 = 1;
int aaaa;
//const char *text = "Some text";

__attribute__((section(".text.cc.vm.import.1")))
void a() {}

int g(char x, short y);

void b();

unsigned long long main(unsigned long long c) {
    return 0x0123456789ABCDEFull * c;//(unsigned long long)(unsigned)&x;
}

__attribute__((section(".text.cc.vm.export.1")))
int f(short c, char x, short z) {
    while (c != 1) {
        c += 2;
        x += 3;
        a();
    }
    return x;
}
