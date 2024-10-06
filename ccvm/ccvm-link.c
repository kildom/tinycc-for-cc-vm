
#ifdef INTELLISENSE
#define USING_GLOBALS
#include "tcc.h"
#endif

#include "ccvm-link.h"


static int ccvm_output_file(TCCState *s1, const char *filename)
{
    printf("sections %d\n", s1->nb_sections);
    Section *relsec;
    for (int i = 1; i < s1->nb_sections; i++) {
        printf("%s %d %d\n", s1->sections[i]->name, (int)s1->sections[i]->data_offset, (int)s1->sections[i]->sh_num);
        relsec = s1->sections[i];
    }

    /*Section *strtab = symtab_section->link;
    for (int i = 0; i < strtab->data_offset; i++) {
        printf("%c", strtab->data[i] ? strtab->data[i] : '`');
    }
    printf("\n");
    
    ElfW(Sym) *sym;
    for (int i = 0; i < symtab_section->data_offset; i += sizeof(*sym)) {
        sym = (void*)&symtab_section->data[i];
        printf("%d: 0x%08X 0x%04X 0x%04X '%s'\n", (int)(i / sizeof(*sym)), sym->st_value, sym->st_size, sym->st_other, &strtab->data[sym->st_name]);
    }

    ElfW_Rel* rel;
    for (int i = 0; i < relsec->data_offset; i += sizeof(*rel)) {
        rel = (void*)&relsec->data[i];
        printf("%d: 0x%08X sym=%d type=%d\n", (int)(i / sizeof(*rel)), rel->r_offset, ELF32_R_SYM(rel->r_info), ELF32_R_TYPE(rel->r_info));
    }*/
    return 0;
}

static int ccvm_patch_local_reloc(ElfW(Ehdr)* ehdr, ElfW(Shdr) *shdr, struct SectionMergeInfo *sm_table) {
    int i;
    int offseti;
    Section *s;
    ElfW(Shdr) *sh;
    static int label_offset = 0;
    for(i = 1; i < ehdr->e_shnum; i++) {
        s = sm_table[i].s;
        if (!s)
            continue;
        sh = &shdr[i];
        if (s->sh_type != SHT_NOTE
            || strlen(s->name) <= strlen(LOCAL_RELOC_PREFIX)
            || memcmp(s->name, LOCAL_RELOC_PREFIX, strlen(LOCAL_RELOC_PREFIX)) != 0
        ) {
            continue;
        }
        offseti = sm_table[sh->sh_info].offset;
        printf("Patch %s by %d, label %d\n", s->name, offseti, label_offset);
        uint8_t* ptr = s->data;
        uint8_t* end = ptr + s->data_offset;
        int entries_count = 0;
        int next_label = label_offset;
        while (ptr + sizeof(LocalRelocEntry) <= end) {
            int entry_size = patchLocalReloc((void*)ptr, offseti, label_offset, &next_label);
            printf("    Patch at %d\n", (int)(ptr - s->data));
            ptr += entry_size;
            entries_count++;
        }
        label_offset = next_label;
    }
}