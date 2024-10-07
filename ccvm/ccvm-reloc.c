
#include "ccvm-reloc.h"


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
    sprintf(local_name, LOCAL_RELOC_PREFIX "%s", cur_text_section->name);
    local_reloc_section = new_section(s, local_name, SHT_NOTE, 0);
    local_reloc_section->sh_entsize = sizeof(LocalRelocEntry);
    local_reloc_section->link = cur_text_section;
    local_reloc_section->sh_info = cur_text_section->sh_num;
    FREE_OR_STACK(local_name);
}


static void addLocalReloc(int cmd, uint32_t source, uint32_t target, uint32_t type) {
    if (!local_reloc_section || local_reloc_section->link != cur_text_section) {
        setupLocalReloc();
    }
    uint32_t new_offset = local_reloc_section->data_offset + sizeof(LocalRelocEntry);
    if (new_offset > local_reloc_section->data_allocated)
        section_realloc(local_reloc_section, new_offset);
    LocalRelocEntry* entry = (void*)&local_reloc_section->data[local_reloc_section->data_offset];
    local_reloc_section->data_offset = new_offset;
    entry->note_namesz = LOCAL_RELOC_NOTE_NAMESZ;
    entry->note_descsz = LOCAL_RELOC_NOTE_DESCSZ;
    entry->note_type = LOCAL_RELOC_NOTE_TYPE;
    entry->note_name[0] = LOCAL_RELOC_NOTE_NAME0;
    entry->note_name[1] = LOCAL_RELOC_NOTE_NAME1;
    entry->cmd = cmd;
    entry->source = source;
    entry->target = target;
    entry->type = type;
    switch (cmd)
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
    case LOCAL_RELOC_CONST:
        DEBUG_COMMENT("LocalReloc: const 0x%08X = %d", target, source);
        break;
    default:
        break;
    }
}

static int patchLocalReloc(LocalRelocEntry* entry, int addr_offset, int label_offset, int* next_label)
{
    int name_size = (entry->note_namesz + 3) & ~3;
    int desc_size = (entry->note_descsz + 3) & ~3;
    int size = 4 + 4 + 4 + name_size + desc_size;

    if (entry->note_namesz != LOCAL_RELOC_NOTE_NAMESZ
        || entry->note_descsz != LOCAL_RELOC_NOTE_DESCSZ
        || entry->note_type != LOCAL_RELOC_NOTE_TYPE
        || entry->note_name[0] != LOCAL_RELOC_NOTE_NAME0
        || entry->note_name[1] != LOCAL_RELOC_NOTE_NAME1
    ) {
        return size;
    }

    switch (entry->cmd)
    {
    case LOCAL_RELOC_ADDR:
        entry->source += addr_offset;
        entry->target += addr_offset;
        break;
    case LOCAL_RELOC_LABEL:
        entry->source += label_offset;
        entry->target += addr_offset;
        if (entry->source >= *next_label) *next_label = entry->source + 1;
        break;
    case LOCAL_RELOC_SET_LABEL:
        entry->source += addr_offset;
        entry->target += label_offset;
        if (entry->target >= *next_label) *next_label = entry->target + 1;
        break;
    case LOCAL_RELOC_ALIAS_LABEL:
        entry->source += label_offset;
        entry->target += label_offset;
        if (entry->source >= *next_label) *next_label = entry->source + 1;
        if (entry->target >= *next_label) *next_label = entry->target + 1;
        break;
    case LOCAL_RELOC_CONST:
        entry->target += addr_offset;
        break;
    default:
        break;
    }

    return size;
}
