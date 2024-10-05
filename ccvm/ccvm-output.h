#ifndef _CCVM_OUTPUT_H_
#define _CCVM_OUTPUT_H_

#define DEBUG_COMMENT(text, ...) printf("                    // " text "\n", ##__VA_ARGS__)
#define DEBUG_INSTR(text, ...) printf("%08X:   " text "\n", ind, ##__VA_ARGS__)

#define MALLOC_OR_STACK(var, required_size) \
    long long _TMP_##var[64 / 8]; \
    var = required_size > 64 ? tcc_malloc(required_size) : (void*)_TMP_##var;

#define FREE_OR_STACK(var) \
    do { \
        if ((void*)var != (void*)_TMP_##var) tcc_free(var); \
    } while (0);

static void g8(int c);
static void g16(int c);
static void g24(int c);
static void g32(int c);

#endif // _CCVM_OUTPUT_H_
