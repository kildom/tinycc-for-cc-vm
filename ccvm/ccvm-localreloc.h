#ifndef _CCVM_LOCALRELOC_H_
#define _CCVM_LOCALRELOC_H_

#include <stdint.h>

/** The address at "target" should point to the "source" address.
 *  This is needed since the code size may change.
 */
#define LOCAL_RELOC_ADDR 1

/** The address at "target" should point to address defined by label index "source".
 */
#define LOCAL_RELOC_LABEL 2

/** Set the label index "target" to the "source" address.
 */
#define LOCAL_RELOC_SET_LABEL 3

/** Make the "source" and "target" labels point the same address.
 */
#define LOCAL_RELOC_ALIAS_LABEL 4

/** Single entry in the local relocation section.
 *  The name of section is ".ccvm.loc.rel***", where "***" is a name of the related section.
 */
typedef struct LocalRelocEntry {
    int type;
    uint32_t source;
    uint32_t target;
} LocalRelocEntry;

/** Add local relocation in the section related to current code section.
 */
static void addLocalReloc(int type, uint32_t source, uint32_t target);

#endif // _CCVM_LOCALRELOC_H_
