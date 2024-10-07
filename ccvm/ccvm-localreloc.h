#ifndef _CCVM_LOCALRELOC_H_
#define _CCVM_LOCALRELOC_H_

#include <stdint.h>

#define LOCAL_RELOC_PREFIX ".ccvm.loc.rel"

/** The address at "target" should point to the "source" address.
 *  This is needed since the code size may change.
 */
#define LOCAL_RELOC_ADDR 0x41

/** The address at "target" should point to address defined by label index "source".
 */
#define LOCAL_RELOC_LABEL 0x42

/** Set the label index "target" to the "source" address.
 */
#define LOCAL_RELOC_SET_LABEL 0x43

/** Make the "source" and "target" labels point the same address.
 */
#define LOCAL_RELOC_ALIAS_LABEL 0x44

/** Single entry in the local relocation section.
 *  The name of section is ".ccvm.loc.rel***", where "***" is a name of the related section.
 */
typedef struct LocalRelocEntry {
    uint32_t note_namesz;
    uint32_t note_descsz;
    uint32_t note_type;
    uint32_t note_name[2];
    uint32_t type;
    uint32_t source;
    uint32_t target;
} LocalRelocEntry;

#define LOCAL_RELOC_NOTE_NAMESZ 8
#define LOCAL_RELOC_NOTE_DESCSZ (sizeof(LocalRelocEntry) - offsetof(LocalRelocEntry, type))
#define LOCAL_RELOC_NOTE_TYPE 0x6D766363
#define LOCAL_RELOC_NOTE_NAME0 0x4D764363uLL
#define LOCAL_RELOC_NOTE_NAME1 0x00526C2DuLL

/** Add local relocation in the section related to current code section.
 */
static void addLocalReloc(int type, uint32_t source, uint32_t target);

static int patchLocalReloc(LocalRelocEntry* entry, int addr_offset, int label_offset, int* next_label);

#endif // _CCVM_LOCALRELOC_H_
