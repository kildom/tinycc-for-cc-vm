#ifdef TARGET_DEFS_ONLY

#include "ccvm-reloc.h"

#define EM_TCC_TARGET 0x87E2

#define R_DATA_32  RELOC_DATA
#define R_DATA_PTR RELOC_DATA
#define R_JMP_SLOT RELOC_RESERVED_0
#define R_GLOB_DAT RELOC_RESERVED_1
#define R_COPY     RELOC_RESERVED_2
#define R_RELATIVE RELOC_RESERVED_3

#define R_NUM      RELOC_NUM

#define ELF_START_ADDR 0x00400000
#define ELF_PAGE_SIZE 0x10000

#define PCRELATIVE_DLLPLT 1
#define RELOCATE_DLLPLT 1

#else /* !TARGET_DEFS_ONLY */

ST_FUNC int code_reloc (int reloc_type)
{
    return -1;
}

ST_FUNC int gotplt_entry_type (int reloc_type)
{
    return -1;
}

ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset, struct sym_attr *attr)
{
    return 123;
}


ST_FUNC void relocate_plt(TCCState *s1)
{
}


ST_FUNC void relocate(TCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val)
{
}

#endif /* !TARGET_DEFS_ONLY */
