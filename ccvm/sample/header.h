#ifndef _HEADER_H_
#define _HEADER_H_

#define _CCVM_STR2(x) #x
#define _CCVM_STR1(x) _CCVM_STR2(x)
#define _CCVM_STR(x) _CCVM_STR1(x)

#define CCVM_IMPORT(index) \
    __attribute__((section(".text.ccvm.import." _CCVM_STR(index))))

#define CCVM_EXPORT(index) \
    __attribute__((section(".text.ccvm.export." _CCVM_STR(index))))

int a();
void b();

#endif
