#ifndef _CCVM_LOCALRELOC_H_
#define _CCVM_LOCALRELOC_H_

#include <stdint.h>

#define RELOC_DATA  1 /// Store value directly in 32-bit word
#define RELOC_INSTR 2 /// Instruction immediate
//#define RELOC_ADD_FLAG_CONST 4 /// Add this flag if relocation does not points to any address, but contains constant value
//#define RELOC_ALLOWED_MAX 8
#define RELOC_RESERVED_0 9
#define RELOC_RESERVED_1 10
#define RELOC_RESERVED_2 11
#define RELOC_RESERVED_3 12
#define RELOC_NUM 13

#endif // _CCVM_LOCALRELOC_H_
