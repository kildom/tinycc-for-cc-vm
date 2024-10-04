#ifdef TARGET_DEFS_ONLY

#define EM_TCC_TARGET 1

#define R_DATA_32  2
#define R_DATA_PTR 3
#define R_JMP_SLOT 4
#define R_GLOB_DAT 5
#define R_COPY     6
#define R_RELATIVE 7

#define R_NUM      8

#define ELF_START_ADDR 0x00400000
#define ELF_PAGE_SIZE 0x10000

#define PCRELATIVE_DLLPLT 1
#define RELOCATE_DLLPLT 1

#else /* !TARGET_DEFS_ONLY */

#include "tcc.h"


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
