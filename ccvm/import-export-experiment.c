
void f() {
    printf("sections %d\n", s->nb_sections);
    Section *relsec;
    for (int i = 1; i < s->nb_sections; i++) {
        printf("%s %d\n", s->sections[i]->name, (int)s->sections[i]->data_offset);
        relsec = s->sections[i];
    }

    Section *strtab = symtab_section->link;
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
    }
}

void f2() {

    //s->

    // todo: Put it to special section

    Section *sec = ad->section;

    if (sec
        && (s->type.t & VT_BTYPE) == VT_FUNC
        && s->type.ref != NULL
        && strlen(sec->name) > 18
        && (memcmp(sec->name, ".text.ccvm.import.", 18) == 0
            || memcmp(sec->name, ".text.ccvm.export.", 18) == 0)
    ) {
        int is_export = sec->name[11] == 'e';
        int index = atoi(&sec->name[18]);
        printf("VM %s %s @%d\n", is_export ? "export" : "import", get_tok_str(s->v, NULL), index);
        for(Sym *param = s->type.ref->next; param; param = param->next) {
            CType* type = &param->type;
            int align;
            int size = type_size(type, &align);
            printf("    %s, size=%d, align=%d, t=0x%03X\n", get_tok_str(param->v, NULL), size, align, type->t);
            if ((type->t & VT_BTYPE) == VT_STRUCT) {
                printf("        struct %s\n", get_tok_str(type->ref->v, NULL));
            } else if ((type->t & VT_BTYPE) == VT_PTR) {
                printf("        pointer %p\n", type->ref);
            }
        }
        // return type
        {
            CType* type = &s->type.ref->type;
            int align;
            int size = type_size(type, &align);
            printf("    return size=%d, align=%d, t=0x%03X\n", size, align, type->t);
            if ((type->t & VT_BTYPE) == VT_STRUCT) {
                printf("        struct %s\n", get_tok_str(type->ref->v, NULL));
            } else if ((type->t & VT_BTYPE) == VT_PTR) {
                CType* type2 = &type->ref->type;
                int align;
                int size = type_size(type2, &align);
                printf("        pointer size=%d, align=%d, t=0x%03X\n", size, align, type2->t);
            }
        }
    }

}