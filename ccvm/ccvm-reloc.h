#ifndef _CCVM_LOCALRELOC_H_
#define _CCVM_LOCALRELOC_H_

#include <stdint.h>

#define LOCAL_RELOC_PREFIX ".ccvm.loc.rel"

#define RELOC_DATA            1 /// Store value directly in 32-bit word
#define RELOC_INSTR_RELATIVE0 2 /// Instruction immediate relative to the next byte
#define RELOC_INSTR_RELATIVE1 3 /// Instruction immediate relative to one byte after next
#define RELOC_INSTR_ABSOLUTE  4 /// Instruction immediate absolute
#define RELOC_ADD_FLAG_CONST  4 /// Add this flag if relocation does not points to any address, but contains constant value
#define RELOC_ALLOWED_MAX 8
#define RELOC_RESERVED_0 9
#define RELOC_RESERVED_1 10
#define RELOC_RESERVED_2 11
#define RELOC_RESERVED_3 12
#define RELOC_NUM 13

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

/** Set "target" to constant value contained in "source".
 */
#define LOCAL_RELOC_CONST 0x45

/** Single entry in the local relocation section.
 *  The name of section is ".ccvm.loc.rel***", where "***" is a name of the related section.
 */
typedef struct LocalRelocEntry {
    uint32_t note_namesz;
    uint32_t note_descsz;
    uint32_t note_type;
    uint32_t note_name[2];
    uint32_t cmd;
    uint32_t source;
    uint32_t target;
    uint32_t type;
} LocalRelocEntry;

#define LOCAL_RELOC_NOTE_NAMESZ 8
#define LOCAL_RELOC_NOTE_DESCSZ (sizeof(LocalRelocEntry) - offsetof(LocalRelocEntry, cmd))
#define LOCAL_RELOC_NOTE_TYPE 0x6D766363
#define LOCAL_RELOC_NOTE_NAME0 0x4D764363uLL
#define LOCAL_RELOC_NOTE_NAME1 0x00526C2DuLL

/** Add local relocation in the section related to current code section.
 */
static void addLocalReloc(int cmd, uint32_t source, uint32_t target, uint32_t type);

static int patchLocalReloc(LocalRelocEntry* entry, int addr_offset, int label_offset, int* next_label);

#endif // _CCVM_LOCALRELOC_H_
