
int g();

typedef struct XX
{
    int x;
    int y;
} XX;

#define _CCVM_STR2(x) #x
#define _CCVM_STR1(x) _CCVM_STR2(x)
#define _CCVM_STR(x) _CCVM_STR1(x)

#define CCVM_IMPORT(index) \
    __attribute__((section(".text.ccvm.import." _CCVM_STR(index))))

#define CCVM_EXPORT(index) \
    __attribute__((section(".text.ccvm.export." _CCVM_STR(index))))

CCVM_IMPORT(0)
void a(int some);

CCVM_EXPORT(0)
const char* f(int x, XX z, char zt, signed char zt2) {
    int a = g();
    int b = g();
    return zt2;
    //return x;
}

/*

ret      <--
rebbp
x
z.x
z.y
zt zt2 . .

*/
#if 0
extern int x;
int e11 = 1;
int aaaa;
//const char *text = "Some text";

__attribute__((section(".text.ccvm.import.1")))
void a() {}

int g(char x, short y);

void b();

unsigned long long main(unsigned long long c) {
    return 0x0123456789ABCDEFull * c;//(unsigned long long)(unsigned)&x;
}

__attribute__((section(".text.ccvm.export.1")))
int f(short c, char x, short z) {
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
#endif