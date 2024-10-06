#ifndef _HEADER_H_
#define _HEADER_H_

#define _CCVM_STR2(x) #x
#define _CCVM_STR1(x) _CCVM_STR2(x)
#define _CCVM_STR(x) _CCVM_STR1(x)

#define CCVM_IMPORT(index, name) \
    __attribute__((section(".ccvm.import." _CCVM_STR(index) "." _CCVM_STR(name)))) void __cc_vm__export_indicator_##name##_(){}

#define CCVM_EXPORT(index, name) \
    __attribute__((section(".ccvm.export." _CCVM_STR(index) "." _CCVM_STR(name)))) void __cc_vm__export_indicator_##name##_(){}

int a();
void b();

#endif
