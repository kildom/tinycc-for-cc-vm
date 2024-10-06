#ifndef _CCVM_LINK_H_
#define _CCVM_LINK_H_

struct SectionMergeInfo;

static int ccvm_output_file(TCCState *s1, const char *filename);

static int ccvm_patch_local_reloc(ElfW(Ehdr)* ehdr, ElfW(Shdr) *shdr, struct SectionMergeInfo *sm_table);

#endif // _CCVM_LINK_H_
