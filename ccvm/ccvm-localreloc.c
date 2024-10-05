
#include "ccvm-localreloc.h"


#define SHT_LOCAL_RELOC (SHT_LOUSER + 0x82FE91A)


static Section *local_reloc_section = NULL;


static void setupLocalReloc() {
    TCCState* s = cur_text_section->s1;
    for (int i = 1; i < s->nb_sections; i++) {
        if (s->sections[i]->link == cur_text_section) {
            local_reloc_section = s->sections[i];
            return;
        }
    }
    int len = strlen(cur_text_section->name);
    char *local_name;
    MALLOC_OR_STACK(local_name, len + 13);
    sprintf(local_name, ".ccvm.loc.rel%s", cur_text_section->name);
    local_reloc_section = new_section(s, local_name, SHT_LOCAL_RELOC, 0);
    local_reloc_section->sh_entsize = sizeof(LocalRelocEntry);
    local_reloc_section->link = cur_text_section;
    FREE_OR_STACK(local_name);
}


static void addLocalReloc(int type, uint32_t source, uint32_t target) {
    if (!local_reloc_section || local_reloc_section->link != cur_text_section) {
        setupLocalReloc();
    }
    uint32_t new_offset = cur_text_section->data_offset + sizeof(LocalRelocEntry);
    if (new_offset > cur_text_section->data_allocated)
        section_realloc(cur_text_section, new_offset);
    LocalRelocEntry* entry = (void*)&cur_text_section->data[cur_text_section->data_offset];
    cur_text_section->data_offset = new_offset;
    entry->type = type;
    entry->source = source;
    entry->target = target;
    switch (type)
    {
    case LOCAL_RELOC_ADDR:
        DEBUG_COMMENT("LocalReloc: from 0x%08X to 0x%08X", source, target);
        break;
    case LOCAL_RELOC_LABEL:
        DEBUG_COMMENT("LocalReloc: from label_%d to 0x%08X", source, target);
        break;
    case LOCAL_RELOC_SET_LABEL:
        DEBUG_COMMENT("LocalReloc: set label_%d = 0x%08X", target, source);
        break;
    case LOCAL_RELOC_ALIAS_LABEL:
        DEBUG_COMMENT("LocalReloc: alias label_%d = label_%d", source, target);
        break;
    default:
        break;
    }
}
