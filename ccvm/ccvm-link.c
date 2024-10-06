
#include <stdbool.h>

#ifdef INTELLISENSE
#define USING_GLOBALS
#include "tcc.h"
#endif

#include "ccvm-link.h"
#include "utils.h"

#define INVALID_EXPORT_NAME "__ccvm_invalid_export_handler"

struct InterfaceSymbol;

typedef struct LinkSymbol {
    ElfW(Sym) *elf_sym;
    int elf_sym_index;
    const char *name;
    struct InterfaceSymbol* interface_symbol;
} LinkSymbol;

typedef struct InterfaceSymbol {
    bool is_export;
    int index;
    const char *name;
    LinkSymbol* link_symbol;
} InterfaceSymbol;

typedef enum {
    OUTPUT_SECTION_UNUSED = -1,
    // RAM sections
    OUTPUT_SECTION_REGISTERS = 0,
    OUTPUT_SECTION_DATA,
    OUTPUT_SECTION_BSS,
    OUTPUT_SECTION_RAM_LAST = OUTPUT_SECTION_BSS,
    // Program sections
    OUTPUT_SECTION_ENTRY,
    OUTPUT_SECTION_RODATA,
    OUTPUT_SECTION_EXPORT_TABLE,
    OUTPUT_SECTION_TEXT,
} OutputSectionType;

typedef struct LinkSection {
    OutputSectionType type;
    const char* name;
    Section* elf_section;
    Section* rel_section;
    Section* local_rel_section;
} LinkSection;

typedef struct OutputMemory {
    uint32_t address;
    int sections_count;
    LinkSection VEC* sections;
} OutputMemory;

static LinkSymbol VEC* link_symbols;

static InterfaceSymbol VEC* exports;
static InterfaceSymbol VEC* imports;

static OutputMemory data_memory;
static OutputMemory program_memory;


static bool interfaceSymbolFromSection(Section *sec, InterfaceSymbol* output) {
    bool is_export;
    int name_len = strlen(sec->name);
    if (name_len < 13 + 3) {
        return false;
    } else if (memcmp(sec->name, ".ccvm.import.", 13) == 0) {
        is_export = false;
    } else if (memcmp(sec->name, ".ccvm.export.", 13) == 0) {
        is_export = true;
    } else {
        return false;
    }
    const char *index_start = &sec->name[13];
    const char *index_end = strchr(index_start, '.');
    if (index_end == NULL || index_end - index_start > 10) return false;
    char index_buf[16];
    memset(index_buf, 0, sizeof(index_buf));
    memcpy(index_buf, index_start, index_end - index_start);
    int index = atoi(index_buf);
    int index_len = sprintf(index_buf, "%d", index);
    if (index_len != index_end - index_start || memcmp(index_buf, index_start, index_len) != 0) {
        tcc_error("Invalid index in host interface.");
        return false;
    }
    output->is_export = is_export;
    output->index = index;
    output->name = index_end + 1;
    output->link_symbol = NULL;
    return true;
}


static void loadHostInterface(TCCState *s1)
{
    // Allocate and fill imports/exports
    vecAlloc(exports, 16);
    vecAlloc(imports, 16);
    for (int i = 1; i < s1->nb_sections; i++) {
        Section* sec = s1->sections[i];
        InterfaceSymbol ifSymbol;
        if (!interfaceSymbolFromSection(sec, &ifSymbol)) continue;
        if (ifSymbol.is_export) {
            vecPushValue(exports, ifSymbol);
        } else {
            vecPushValue(imports, ifSymbol);
        }
    }
}

static const char* text_to_compare;

static void textToCompare(const char* text) {
    text_to_compare = text;
}

static bool startsWith(const char* prefix) {
    int len_text = strlen(text_to_compare);
    int len_prefix = strlen(prefix);
    if (len_text < len_prefix) return false;
    return memcmp(text_to_compare, prefix, len_prefix) == 0;
}

static bool endsWith(const char* postfix) {
    int len_text = strlen(text_to_compare);
    int len_postfix = strlen(postfix);
    if (len_text < len_postfix) return false;
    return memcmp(text_to_compare - len_postfix, postfix, len_postfix) == 0;
}

static bool theSame(const char* b) {
    return strcmp(text_to_compare, b) == 0;
}

static OutputSectionType getLinkSectionType(const char* name)
{
    textToCompare(name);

    if (theSame(".ccvm.registers")) return OUTPUT_SECTION_REGISTERS;

    // At this location is .data, but must be moved to the end of the
    // function to avoid conflicts with .data.ro

    if (theSame(".bss")) return OUTPUT_SECTION_BSS;
    if (startsWith(".bss.")) return OUTPUT_SECTION_BSS;
    if (theSame(".common")) return OUTPUT_SECTION_BSS;
    if (startsWith(".common.")) return OUTPUT_SECTION_BSS;

    if (theSame(".ccvm.entry")) return OUTPUT_SECTION_ENTRY;

    if (theSame(".rodata")) return OUTPUT_SECTION_RODATA;
    if (startsWith(".rodata.")) return OUTPUT_SECTION_RODATA;
    if (theSame(".data.ro")) return OUTPUT_SECTION_RODATA;
    if (startsWith(".data.ro.")) return OUTPUT_SECTION_RODATA;
    if (startsWith(".data.") && endsWith(".ro")) return OUTPUT_SECTION_RODATA;

    if (theSame(".ccvm.export.table")) return OUTPUT_SECTION_ENTRY;

    if (theSame(".text")) return OUTPUT_SECTION_TEXT;
    if (startsWith(".text.")) return OUTPUT_SECTION_TEXT;

    if (theSame(".data")) return OUTPUT_SECTION_DATA;
    if (startsWith(".data.")) return OUTPUT_SECTION_DATA;

    return OUTPUT_SECTION_UNUSED;
}

static int linkSectionCmp(const void* pa, const void* pb)
{
    const LinkSection* a = pa;
    const LinkSection* b = pb;
    if (a->type < b->type) return -1;
    if (a->type > b->type) return 1;
    return strcmp(a->name, b->name);
}


static Section* findSection(TCCState *s1, int args_size, const char* text, ...)
{
    va_list ap;
    va_start(ap, text);
    char *name;
    MALLOC_OR_STACK(name, strlen(text) + args_size + 1);
    vsprintf(name, text, ap);
    for (int k = 1; k < s1->nb_sections; k++) {
        Section* sec = s1->sections[k];
        if (strcmp(sec->name, name) == 0) {
            return sec;
        }
    }
    FREE_OR_STACK(name);
    return NULL;
}

static int ccvm_output_file(TCCState *s1, const char *filename)
{
    Section* symtab = NULL;
    Section* strtab = NULL;
    LinkSymbol* invalidExport = NULL;

    tcc_enter_state(s1);

    // Load host interface
    loadHostInterface(s1);

    // Search for symtab and strtab
    for (int i = 1; i < s1->nb_sections; i++) {
        Section* sec = s1->sections[i];
        if (strcmp(sec->name, ".symtab") == 0) {
            symtab = sec;
        } else if (strcmp(sec->name, ".strtab") == 0) {
            strtab = sec;
        }
    }
    if (symtab == NULL) symtab = new_section(s1, ".symtab", SHT_SYMTAB, 0);
    if (strtab == NULL) strtab = new_section(s1, ".strtab", SHT_STRTAB, 0);

    // Load all symbols and resolve host interface for them
    vecAlloc(link_symbols, symtab->data_offset / sizeof(Elf32_Sym));
    for (int i = 0; i < symtab->data_offset / sizeof(Elf32_Sym); i++) {
        Elf32_Sym* elf_symbol = &((Elf32_Sym*)symtab->data)[i];
        LinkSymbol* link_symbol = vecReserve(link_symbols);
        const char* name = (const char*)&strtab->data[elf_symbol->st_name];
        link_symbol->name = name;
        link_symbol->elf_sym = elf_symbol;
        link_symbol->elf_sym_index = i;
        link_symbol->interface_symbol = NULL;
        for (InterfaceSymbol *if_sym = exports; if_sym < vecEnd(exports); if_sym++) {
            if (if_sym->name && strcmp(name, if_sym->name) == 0) {
                if_sym->link_symbol = link_symbol;
                link_symbol->interface_symbol = if_sym;
            }
        }
        for (InterfaceSymbol *if_sym = imports; if_sym < vecEnd(imports); if_sym++) {
            if (if_sym->name && strcmp(name, if_sym->name) == 0) {
                if_sym->link_symbol = link_symbol;
                if (link_symbol->interface_symbol) {
                    tcc_error("Symbol '%s' cannot be imported and exported at the same time.", name);
                    continue;
                }
                link_symbol->interface_symbol = if_sym;
            }
        }
        if (strcmp(name, INVALID_EXPORT_NAME) == 0) {
            invalidExport = link_symbol;
        }
        printf("%s, value 0x%08X, size %d, bind %d, type %d, other %d, shndx %d, export %d, import %d\n", name, elf_symbol->st_value, elf_symbol->st_size,
            ELF32_ST_BIND(elf_symbol->st_info), ELF32_ST_TYPE(elf_symbol->st_info),
            elf_symbol->st_other, elf_symbol->st_shndx,
            link_symbol->interface_symbol && link_symbol->interface_symbol->is_export ? link_symbol->interface_symbol->index : -1,
            link_symbol->interface_symbol && !link_symbol->interface_symbol->is_export ? link_symbol->interface_symbol->index : -1
        );
        vecPush(link_symbols);
    }

    // Verify host interface
    for (InterfaceSymbol *if_sym = exports; if_sym < vecEnd(exports); if_sym++) {
        if (if_sym->name && (!if_sym->link_symbol || if_sym->link_symbol->elf_sym->st_shndx == 0)) {
            tcc_error("Missing definition of exported function '%s'.", if_sym->name);
        }
    }

    for (InterfaceSymbol *if_sym = imports; if_sym < vecEnd(imports); if_sym++) {
        if (if_sym->name && !if_sym->link_symbol) {
            tcc_error("Missing declaration of imported function '%s'.", if_sym->name);
        }
        if (if_sym->name && if_sym->link_symbol->elf_sym->st_shndx != 0) {
            tcc_error("Function body is not allowed for imported function '%s'.", if_sym->name);
        }
    }

    if (!invalidExport || invalidExport->elf_sym->st_shndx == 0) {
        tcc_error("Undefined symbol '" INVALID_EXPORT_NAME "'");
        return -1;
    }

    // Create .ccvm.export.table
    Section* export_table = new_section(s1, ".ccvm.export.table", SHT_PROGBITS, SHF_ALLOC);
    cur_text_section = export_table;
    ind = cur_text_section->data_offset;
    for (int i = 0; i < vecSize(exports); i++) {
        if (exports[i].link_symbol) {
            g32(0);
            put_elf_reloca(symtab_section, cur_text_section, 4 * i, 1, exports[i].link_symbol->elf_sym_index, 0);
        } else {
            g32(0);
            put_elf_reloca(symtab_section, cur_text_section, 4 * i, 1, invalidExport->elf_sym_index, 0);
        }
    }
    cur_text_section->data_offset = ind;

    // Create array of link sections, that goes into final output
    LinkSection VEC* link_sections;
    vecAlloc(link_sections, 50);

    for (int i = 1; i < s1->nb_sections; i++) {
        Section* sec = s1->sections[i];
        LinkSection* ls = &link_sections[vecSize(link_sections)];
        ls->type = getLinkSectionType(sec->name);
        if (ls->type == OUTPUT_SECTION_UNUSED) {
            continue;
        }
        ls->name = sec->name;
        ls->elf_section = sec;
        ls->rel_section = sec->reloc;
        if (!ls->rel_section) {
            ls->rel_section = findSection(s1, strlen(sec->name), ".rel%s", sec->name);
        }
        ls->local_rel_section = findSection(s1, strlen(sec->name), ".ccvm.loc.rel%s", sec->name);
        vecPush(link_sections);
    }

    // Sort and assign link sections to memories
    qsort(link_sections, vecSize(link_sections), sizeof(LinkSection), linkSectionCmp);

    data_memory.address = 0;
    vecAlloc(data_memory.sections, 4);
    program_memory.address = 0x40000000;
    vecAlloc(program_memory.sections, 5);

    for (int i = 0; i < vecSize(link_sections); i++) {
        LinkSection* ls = &link_sections[i];
        if (ls->type <= OUTPUT_SECTION_RAM_LAST) {
            vecPushValue(data_memory.sections, *ls);
        } else {
            vecPushValue(program_memory.sections, *ls);
        }
        printf("link section %s, rel %s, loc rel %s\n", ls->name,
            ls->rel_section ? ls->rel_section->name : "none",
            ls->local_rel_section ? ls->local_rel_section->name : "none"
            );
    }

    for (int i = 1; i < s1->nb_sections; i++) {
        Section* sec = s1->sections[i];
        printf("    section %d, %s %d\n", i, sec->name, (int)sec->data_offset);
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
    tcc_exit_state(s1);
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