
#include <stdbool.h>

#ifdef INTELLISENSE
#define USING_GLOBALS
#include "tcc.h"
#endif

#include "ccvm-link.h"
#include "utils.h"

#define INVALID_EXPORT_NAME "__ccvm_invalid_export_handler"

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
    OUTPUT_SECTION_PROGRAM_LAST = OUTPUT_SECTION_TEXT,
    OUTPUT_SECTION_COUNT = OUTPUT_SECTION_PROGRAM_LAST + 1,
} OutputSectionType;

struct InterfaceSymbol;
struct OutputSection;

typedef struct LinkSymbol {
    int elf_sym_index;
    int elf_section_index;
    const char *name;
    int offset;
    int size;
    bool is_automatic;
    bool is_removed;
    uint32_t real_address;
    struct InterfaceSymbol* interface_symbol;
    struct OutputSection* section;
    struct LinkSymbol* group_next;
} LinkSymbol;

typedef struct InterfaceSymbol {
    bool is_export;
    int index;
    const char *name;
    LinkSymbol* link_symbol;
} InterfaceSymbol;

typedef struct LinkRelocation {
    uint32_t type;
    uint32_t target;
    LinkSymbol* symbol;
} LinkRelocation;

typedef struct OutputSection {
    OutputSectionType type;
    uint32_t address;
    const char* name;
    uint8_t VEC* data;
    LinkRelocation VEC* relocations;
} OutputSection;

static LinkSymbol* VEC* link_symbols;
static int elf_symbol_count;

static InterfaceSymbol VEC* exports;
static InterfaceSymbol VEC* imports;

static OutputSection outputSections[OUTPUT_SECTION_COUNT];

static Section* elf_symtab;
static Section* elf_strtab;
static Section* elf_link_symbols;
static LinkSymbol* invalidExport;

static uint8_t VEC* programMemory;
static uint8_t VEC* dataMemory;

static bool interfaceSymbolFromSection(Section *sec, InterfaceSymbol* output)
{
    TRACE("");
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
    TRACE("");
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

    if (theSame(".ccvm.export.table")) return OUTPUT_SECTION_EXPORT_TABLE;

    if (theSame(".text")) return OUTPUT_SECTION_TEXT;
    if (startsWith(".text.")) return OUTPUT_SECTION_TEXT;

    if (theSame(".data")) return OUTPUT_SECTION_DATA;
    if (startsWith(".data.")) return OUTPUT_SECTION_DATA;

    return OUTPUT_SECTION_UNUSED;
}



static Section* findSection(TCCState *s1, int args_size, const char* text, ...)
{
    TRACE("");
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

static void findSymtabStrtab(TCCState *s1)
{
    TRACE("");
    elf_symtab = NULL;
    elf_strtab = NULL;
    elf_link_symbols = NULL;
    for (int i = 1; i < s1->nb_sections; i++) {
        Section* sec = s1->sections[i];
        if (strcmp(sec->name, ".symtab") == 0) {
            elf_symtab = sec;
        } else if (strcmp(sec->name, ".strtab") == 0) {
            elf_strtab = sec;
        } else if (strcmp(sec->name, ".ccvm.link.symbols") == 0) {
            elf_link_symbols = sec;
        }
    }
    if (elf_symtab == NULL) elf_symtab = new_section(s1, ".symtab", SHT_SYMTAB, 0);
    if (elf_strtab == NULL) elf_strtab = new_section(s1, ".strtab", SHT_STRTAB, 0);

}

static void loadSymbols(TCCState *s1)
{
    TRACE("");
    invalidExport = NULL;

    elf_symbol_count = elf_symtab->data_offset / sizeof(Elf32_Sym);
    vecAlloc(link_symbols, elf_symbol_count + 1024);

    for (int i = 0; i < elf_symbol_count; i++) {
        Elf32_Sym* elf_symbol = &((Elf32_Sym*)elf_symtab->data)[i];
        LinkSymbol* link_symbol = tcc_mallocz(sizeof(LinkSymbol));
        *vecPush(link_symbols) = link_symbol;
        const char* name = (const char*)&elf_strtab->data[elf_symbol->st_name];
        link_symbol->name = name;
        link_symbol->elf_section_index = elf_symbol->st_shndx;
        link_symbol->elf_sym_index = i;
        link_symbol->interface_symbol = NULL;
        link_symbol->offset = elf_symbol->st_value;
        link_symbol->size = elf_symbol->st_size;
        link_symbol->section = NULL;
        link_symbol->group_next = link_symbol;
        link_symbol->is_automatic = elf_link_symbols != NULL
            && elf_symbol->st_shndx == elf_link_symbols->sh_num;
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
    }
}

static void verifyHostInterface(TCCState *s1)
{
    TRACE("");
    for (InterfaceSymbol *if_sym = exports; if_sym < vecEnd(exports); if_sym++) {
        if (if_sym->name && (!if_sym->link_symbol || if_sym->link_symbol->elf_section_index == 0)) {
            tcc_error("Missing definition of exported function '%s'.", if_sym->name);
        }
    }

    for (InterfaceSymbol *if_sym = imports; if_sym < vecEnd(imports); if_sym++) {
        if (if_sym->name && !if_sym->link_symbol) {
            tcc_error("Missing declaration of imported function '%s'.", if_sym->name);
        }
        if (if_sym->name && if_sym->link_symbol->elf_section_index != 0) {
            tcc_error("Function body is not allowed for imported function '%s'.", if_sym->name);
        }
    }

    if (!invalidExport || invalidExport->elf_section_index == 0) {
        tcc_error("Undefined symbol '" INVALID_EXPORT_NAME "'");
    }
}

static void createExportTable(TCCState *s1)
{
    TRACE("");
    Section* export_table = new_section(s1, ".ccvm.export.table", SHT_PROGBITS, SHF_ALLOC);
    cur_text_section = export_table;
    ind = cur_text_section->data_offset;
    for (int i = 0; i < vecSize(exports); i++) {
        if (exports[i].link_symbol) {
            g32(0);
            put_elf_reloca(symtab_section, cur_text_section, 4 * i, RELOC_DATA, exports[i].link_symbol->elf_sym_index, 0);
        } else {
            g32(0);
            put_elf_reloca(symtab_section, cur_text_section, 4 * i, RELOC_DATA, invalidExport->elf_sym_index, 0);
        }
    }
    cur_text_section->data_offset = ind;
}

static const char* outputSectionName(OutputSectionType type)
{
    switch (type)
    {
        case OUTPUT_SECTION_REGISTERS: return "ccvm_registers";
        case OUTPUT_SECTION_DATA: return "data";
        case OUTPUT_SECTION_BSS: return "bss";
        case OUTPUT_SECTION_ENTRY: return "ccvm_entry";
        case OUTPUT_SECTION_RODATA: return "rodata";
        case OUTPUT_SECTION_EXPORT_TABLE: return "ccvm_export_table";
        case OUTPUT_SECTION_TEXT: return "text";
        default: tcc_error("Internal");
    }
}


static LinkSymbol* addCustomSymbol(OutputSection *section, uint32_t offset, const char *text, ...)
{
    TRACE("");
    va_list ap;
    va_start(ap, text);
    char name[128];
    vsprintf(name, text, ap);
    LinkSymbol* symbol = tcc_mallocz(sizeof(LinkSymbol));
    *vecPush(link_symbols) = symbol;
    symbol->elf_sym_index = 0;
    symbol->elf_section_index = 0;
    symbol->name = tcc_strdup(name);
    symbol->interface_symbol = NULL;
    symbol->section = section;
    symbol->offset = offset;
    symbol->size = 0;
    symbol->group_next = symbol;
    symbol->is_automatic = false;
    return symbol;
}

static LinkSymbol* getLabelSymbol(LinkSymbol* VEC* * label_to_symbol, OutputSection* output, int label, int nb)
{
    TRACE("");
    LinkSymbol* symbol = *vecEnsure(*label_to_symbol, label);
    if (symbol == NULL) {
        symbol = addCustomSymbol(output, -1, "ccvm.loc.rel.lbl.%d.%d.%d", output->type, nb, label);
        (*label_to_symbol)[label] = symbol;
    }
    return symbol;
}

static void joinSymbolGroups(LinkSymbol* a, LinkSymbol* b)
{
    TRACE("");
    LinkSymbol* last = a->group_next;
    while (last->group_next != a) {
        last = last->group_next;
    }
    last->group_next = b->group_next;
    b->group_next = a;
}


static void copyToOutputSection(TCCState *s1, OutputSection* output, Section* VEC* input)
{
    TRACE("");

    char symbol_name_buf[128];

    for (int k = 0; k < vecSize(input); k++) {
        // Find related input section
        Section* sec = input[k];
        Section* sec_rel = findSection(s1, strlen(sec->name), ".rel%s", sec->name);
        Section* sec_local_rel = findSection(s1, strlen(sec->name), ".ccvm.loc.rel%s", sec->name);

        // Append data
        int offset_adjust = vecSize(output->data);
        if (sec->sh_addralign > 1) {
            int padding = (sec->sh_addralign - (offset_adjust % sec->sh_addralign)) % sec->sh_addralign;
            if (padding > 0) {
                memset(vecPushMulti(output->data, padding), 0, padding);// TODO: clear memory automatically in vector
                offset_adjust += padding;
            }
        }
        if (sec->data) {
            vecPushMultiValue(output->data, sec->data, sec->data_offset);
        } else {
            memset(vecPushMulti(output->data, sec->data_offset), 0, sec->data_offset);// TODO: clear memory automatically in vector
        }

        // Append adjusted relocations
        for (int i = 0; sec_rel && i < sec_rel->data_offset / sizeof(Elf32_Rel); i++) {
            Elf32_Rel* elf_rel = (Elf32_Rel*)sec_rel->data + i;
            uint32_t offset = elf_rel->r_offset;
            uint32_t type = ELF32_R_TYPE(elf_rel->r_info);
            uint32_t sym_index = ELF32_R_SYM(elf_rel->r_info);
            if (sym_index >= vecSize(link_symbols)) tcc_error("ELF: Invalid symbol index in section '%s'.", sec_rel->name);
            LinkSymbol* symbol = link_symbols[sym_index];
            LinkRelocation* rel = vecPush(output->relocations);
            rel->type = type;
            rel->target = offset_adjust + offset;
            rel->symbol = symbol;
        }

        // Convert local relocations into symbols plus normal relocations and append them
        LinkSymbol* VEC* label_to_symbol;
        vecAlloc(label_to_symbol, sec_local_rel ? sec_local_rel->data_offset / sizeof(LocalRelocEntry) / 2 : 0);

        for (int i = 0; sec_local_rel && i < sec_local_rel->data_offset / sizeof(LocalRelocEntry); i++) {
            LocalRelocEntry* elf_rel = (LocalRelocEntry*)sec_local_rel->data + i;
            switch (elf_rel->cmd)
            {
            case LOCAL_RELOC_ADDR: {
                LinkSymbol* symbol = addCustomSymbol(output, offset_adjust + elf_rel->source, "ccvm.loc.rel.adr.%d.%d.%d", output->type, k, i);
                LinkRelocation* rel = vecPush(output->relocations);
                rel->type = elf_rel->type;
                rel->target = offset_adjust + elf_rel->target;
                rel->symbol = symbol;
                break;
            }
            case LOCAL_RELOC_LABEL: {
                int label = elf_rel->source;
                LinkSymbol* symbol = getLabelSymbol(&label_to_symbol, output, elf_rel->source, k);
                LinkRelocation* rel = vecPush(output->relocations);
                rel->type = elf_rel->type;
                rel->target = offset_adjust + elf_rel->target;
                rel->symbol = symbol;
                break;
            }
            case LOCAL_RELOC_SET_LABEL:
                // Will be done later, first, all aliases must be set
                break;
            case LOCAL_RELOC_ALIAS_LABEL: {
                LinkSymbol* a = getLabelSymbol(&label_to_symbol, output, elf_rel->source, k);
                LinkSymbol* b = getLabelSymbol(&label_to_symbol, output, elf_rel->target, k);
                joinSymbolGroups(a, b);
                break;
            }
            case LOCAL_RELOC_CONST: {
                LinkSymbol* symbol = addCustomSymbol(NULL, elf_rel->source, "ccvm.loc.rel.c.%d.%d.%d", output->type, k, i);
                LinkRelocation* rel = vecPush(output->relocations);
                rel->type = elf_rel->type;
                rel->target = offset_adjust + elf_rel->target;
                rel->symbol = symbol;
                break;
            }
            default:
                break;
            }
        }

        printf("--%s\n", sec->name);
        for (int i = 0; sec_local_rel && i < sec_local_rel->data_offset / sizeof(LocalRelocEntry); i++) {
            LocalRelocEntry* entry = (LocalRelocEntry*)sec_local_rel->data + i;
            switch (entry->cmd)
            {
            case LOCAL_RELOC_ADDR:
                break;
            case LOCAL_RELOC_LABEL:
                printf("--- use %d for 0x%08X\n", entry->source, entry->target);
                break;
            case LOCAL_RELOC_SET_LABEL:
                printf("--- set %d to 0x%08X\n", entry->target, entry->source);
                break;
            case LOCAL_RELOC_ALIAS_LABEL:
                printf("--- alias %d == %d\n", entry->target, entry->source);
                break;
            default:
                break;
            }
        }

        // Assign offsets to local relocation labels
        for (int i = 0; sec_local_rel && i < sec_local_rel->data_offset / sizeof(LocalRelocEntry); i++) {
            LocalRelocEntry* elf_rel = (LocalRelocEntry*)sec_local_rel->data + i;
            if (elf_rel->cmd == LOCAL_RELOC_SET_LABEL) {
                int offset = offset_adjust + elf_rel->source;
                int label = elf_rel->target;
                LinkSymbol* symbol = getLabelSymbol(&label_to_symbol, output, label, k);
                LinkSymbol* s = symbol;
                do {
                    if (s->offset != -1 && s->offset != offset) {
                        tcc_error("Internal: Local relocation label defined twice with different offsets.");
                    }
                    s->offset = offset;
                    s = s->group_next;
                } while (s != symbol);
            }
        }

        // Check local relocation consistency
        for (LinkSymbol** psym = label_to_symbol; psym < vecEnd(label_to_symbol); psym++) {
            LinkSymbol* sym = *psym;
            if (sym != NULL && sym->offset == -1) {
                tcc_error("Internal: Local relocation label was referenced but not defined.");
            }
        }

        vecFree(label_to_symbol);

        // Adjust associated symbols from elf, newly added symbols already has correct section and offset
        for (int i = 0; i < elf_symbol_count; i++) {
            LinkSymbol* sym = link_symbols[i];
            if (sym->section == NULL && sym->elf_section_index == sec->sh_num) {
                sym->section = output;
                sym->offset += offset_adjust;
            }
        }
    }

    printf("Output section %s\n", output->name);
    for (LinkRelocation* rel = output->relocations; rel < vecEnd(output->relocations); rel++) {
        printf("    reloc %d, %s => %d\n", rel->type, rel->symbol ? rel->symbol->name : "?", rel->target);
    }
}

static int sectionNameCmp(const void* pa, const void* pb)
{
    const Section* a = *(Section**)pa;
    const Section* b = *(Section**)pb;
    return strcmp(a->name, b->name);
}

static void copySections(TCCState *s1)
{
    TRACE("");

    Section* VEC* section_by_type[OUTPUT_SECTION_COUNT];

    memset(outputSections, 0, sizeof(outputSections));

    // Allocate vectors
    for (int i = 0; i < OUTPUT_SECTION_COUNT; i++) {
        vecAlloc(section_by_type[i], 8);
        outputSections[i].type = i;
        outputSections[i].name = outputSectionName(i);
        vecAlloc(outputSections[i].relocations, 64);
        vecAlloc(outputSections[i].data, 1024);
    }

    // Push sections to vectors by type
    for (int i = 1; i < s1->nb_sections; i++) {
        Section* sec = s1->sections[i];
        OutputSectionType type = getLinkSectionType(sec->name);
        if (type == OUTPUT_SECTION_UNUSED) {
            continue;
        }
        vecPushValue(section_by_type[type], sec);
    }

    // Sort sections within output sections
    for (int i = 0; i < OUTPUT_SECTION_COUNT; i++) {
        qsort(section_by_type[i], vecSize(section_by_type[i]), sizeof(Section*), sectionNameCmp);
    }

    for (int i = 0; i < OUTPUT_SECTION_COUNT; i++) {
        printf("Output section %d %s\n", i, outputSectionName(i));
        for (Section** s = section_by_type[i]; s < vecEnd(section_by_type[i]); s++) {
            printf("    %s\n", (*s)->name);
        }
    }

    // Verify if all required sections are present
    if (vecSize(section_by_type[OUTPUT_SECTION_REGISTERS]) == 0) {
        tcc_error("Missing '.ccvm.registers' section.");
    }

    if (vecSize(section_by_type[OUTPUT_SECTION_ENTRY]) == 0) {
        tcc_error("Missing '.ccvm.entry' section.");
    }

    if (vecSize(section_by_type[OUTPUT_SECTION_TEXT]) == 0) {
        tcc_error("Missing '.text' section.");
    }

    for (int i = 0; i < OUTPUT_SECTION_COUNT; i++) {
        copyToOutputSection(s1, &outputSections[i], section_by_type[i]);
    }

    // Cleanup the memory
    for (int i = 0; i < OUTPUT_SECTION_COUNT; i++) {
        vecFree(section_by_type[i]);
    }
}

struct
{
    uint32_t stackBegin;
    uint32_t stackSize;
    uint32_t stackEnd;
    uint32_t heapBegin;
    uint32_t heapSize;
    uint32_t heapEnd;
} locations;


static void generateSection(TCCState *s1, uint32_t* addr, OutputSection* sec, uint8_t VEC* *destination)
{
    TRACE("");

    sec->address = *addr;

    if (destination) {
        vecPushMultiValue(*destination, sec->data, vecSize(sec->data));
        *addr += vecSize(sec->data);
        int padding = ALIGN_UP(*addr, 4) - *addr;
        vecPushMulti(*destination, padding);
        *addr += padding;
    } else {
        *addr += ALIGN_UP(*addr + vecSize(sec->data), 4);
    }

}

static void generateBytecode(TCCState *s1)
{
    TRACE("");

    vecResize(programMemory, 0);
    vecResize(dataMemory, 0);

    uint32_t addr = 0;
    generateSection(s1, &addr, &outputSections[OUTPUT_SECTION_REGISTERS], &dataMemory);
    generateSection(s1, &addr, &outputSections[OUTPUT_SECTION_DATA], &dataMemory);
    generateSection(s1, &addr, &outputSections[OUTPUT_SECTION_BSS], NULL);
    locations.stackBegin = ALIGN_UP(addr, 4);
    locations.stackEnd = locations.stackBegin + locations.stackSize;
    locations.heapBegin = ALIGN_UP(addr, locations.stackEnd);
    locations.heapSize = locations.heapEnd - locations.heapBegin;
    addr = 0x40000000;
    generateSection(s1, &addr, &outputSections[OUTPUT_SECTION_ENTRY], &programMemory);
    generateSection(s1, &addr, &outputSections[OUTPUT_SECTION_RODATA], &programMemory);
    generateSection(s1, &addr, &outputSections[OUTPUT_SECTION_EXPORT_TABLE], &programMemory);
    generateSection(s1, &addr, &outputSections[OUTPUT_SECTION_TEXT], &programMemory);
}

static int ccvm_output_file(TCCState *s1, const char *filename)
{
    TRACE("");

    vecAlloc(programMemory, 1024);
    vecAlloc(dataMemory, 1024);

    tcc_enter_state(s1);

    // Load host interface
    loadHostInterface(s1);

    // Search for symtab and strtab
    findSymtabStrtab(s1);

    // Load all symbols and resolve host interface for them
    loadSymbols(s1);

    // Verify host interface
    verifyHostInterface(s1);

    // Create .ccvm.export.table
    createExportTable(s1);

    // Copy input sections into the output sections
    copySections(s1);

    generateBytecode(s1);

    FILE* f = fopen(filename, "wb");
    fwrite(programMemory, 1, vecSize(programMemory), f);
    fclose(f);

    // TODO: Create reference graph and remove unused symbols, entry points are .ccvm.export.table and .ccvm.entry sections

    for (LinkSymbol** psym = link_symbols; psym < vecEnd(link_symbols); psym++) {
        LinkSymbol* sym = *psym;
        printf("Symbol '%s', sec '%s', 0x%08X, sz %d, if %s,%s\n", sym->name, sym->section ? sym->section->name : "NULL", sym->offset, sym->size, sym->interface_symbol ? sym->interface_symbol->name : "-", sym->is_automatic ? " AUTO,": "");
    }

    // Create array of link sections, that goes into final output
    //loadLinkSections(s1);

    //addLinkerSymbols(s1);

    for (int i = 1; i < s1->nb_sections; i++) {
        Section* sec = s1->sections[i];
        printf("    section %d, %s %d %d %d\n", i, sec->name, (int)sec->data_offset, sec->sh_type, sec->sh_flags);
    }

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
        int offset = sm_table[i].offset;
        if (s->sh_type != SHT_NOTE
            || strlen(s->name) <= strlen(LOCAL_RELOC_PREFIX)
            || memcmp(s->name, LOCAL_RELOC_PREFIX, strlen(LOCAL_RELOC_PREFIX)) != 0
        ) {
            continue;
        }
        offseti = sm_table[sh->sh_info].offset;
        printf("Patch %s by %d, label %d, source start %d\n", s->name, offseti, label_offset, offset);
        for (int i = offset / sizeof(LocalRelocEntry); i < s->data_offset / sizeof(LocalRelocEntry); i++) {
            LocalRelocEntry* entry = (LocalRelocEntry*)s->data + i;
            switch (entry->cmd)
            {
            case LOCAL_RELOC_ADDR:
                printf("--- addr 0x%08X to 0x%08X\n", entry->source, entry->target);
                break;
            case LOCAL_RELOC_LABEL:
                printf("--- use %d for 0x%08X\n", entry->source, entry->target);
                break;
            case LOCAL_RELOC_SET_LABEL:
                printf("--- set %d to 0x%08X\n", entry->target, entry->source);
                break;
            case LOCAL_RELOC_ALIAS_LABEL:
                printf("--- alias %d == %d\n", entry->target, entry->source);
                break;
            default:
                break;
            }
        }

        uint8_t* ptr = s->data + offset;
        uint8_t* end = s->data + s->data_offset;
        int entries_count = 0;
        int next_label = label_offset;
        while (ptr + sizeof(LocalRelocEntry) <= end) {
            int entry_size = patchLocalReloc((void*)ptr, offseti, label_offset, &next_label);
            printf("    Patch at %d\n", (int)(ptr - s->data));
            ptr += entry_size;
            entries_count++;
        }
        label_offset = next_label;

        for (int i = offset / sizeof(LocalRelocEntry); i < s->data_offset / sizeof(LocalRelocEntry); i++) {
            LocalRelocEntry* entry = (LocalRelocEntry*)s->data + i;
            switch (entry->cmd)
            {
            case LOCAL_RELOC_ADDR:
                printf("--- addr 0x%08X to 0x%08X\n", entry->source, entry->target);
                break;
            case LOCAL_RELOC_LABEL:
                printf("--- use %d for 0x%08X\n", entry->source, entry->target);
                break;
            case LOCAL_RELOC_SET_LABEL:
                printf("--- set %d to 0x%08X\n", entry->target, entry->source);
                break;
            case LOCAL_RELOC_ALIAS_LABEL:
                printf("--- alias %d == %d\n", entry->target, entry->source);
                break;
            default:
                break;
            }
        }

    }
}