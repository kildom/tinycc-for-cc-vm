

## Output program structure

**Data memory at address 0x00000000**

It does not appear in the output file.
It is used during linking to arrange symbols in data memory.

Contents (in order):

 * `.ccvm.registers`
   * loads `.ccvm.registers`
   * contains registers and other internal variables
   * defined by the standard library
 * `.data`
   * loads `.data`, `.data.*`
   * will be initialized on startup from program memory
 * `.bss`
   * loads `.bss`, `.bss.*`, `.common`, `.common.*`
   * Normally this sections is zero-initialized, but since the cc-vm
     data memory is cleared at startup (for security reasons),
     the program does not need to do anything.
 * Stack, it does not generate any section, but it defines the symbols:
   * `__ccvm_stack_begin`, `__ccvm_stack_end`, `__ccvm_stack_size`
 * Heap, it does not generate any section, but it defines the symbols:
   * `__ccvm_heap_begin` - beginning of the heap
   * `__ccvm_heap_initial_end`, `__ccvm_heap_initial_size` - end and
     size of the heap. It is called "initial" because the cc-vm may
     support growable memory. Actual end and size may change on runtime
     in that case.

**Program memory at address 0x40000000**

It is read-only memory containing program bytecode and read-only data.
Entire content of this memory is stored in the output binary file.

 * `.rodata`
   * loads `.data.ro`, `.data.*.ro`, `.data.ro.*`, `.rodata`, `.rodata.*`
   * Loads also `.ccvm.entry` - contains only one jump
     instruction to the guest entry point defined by the standard library.
     It is sorted always as the first.
   * Loads also `.ccvm.export.table` - automatically generated section
     of exported functions pointers.
     It is sorted always as the last.
 * `.text`
   * loads `.text`, `.text.*`
   * It is subject of bytecode shrinking during linking,
     using `.rel*` and `.ccvm.loc.rel*` as relocations.
   * The standard library can add code as arrays of uint8_t (instructions)
     or uint32_t (references). Order does matter, so single function should
     be in single section, e.g. `.text.ccvm.startup.stdlib.*`.
     When basic initialization is done, it calls C function from
     that takes care of the rest of initialization.
   * At the end, it contains automatically generated wrappers for
     imported functions. Generated only if pointer to imported function
     is needed.
 * `.data`
   * Load position of `.data` section.

All input sections within the output section are sorted.

Symbols exported by the linker:

 - `__ccvm_sec_***_begin`, `__ccvm_sec_***_end`, `__ccvm_sec_***_size`:
   Beginning, ending and size of each output section
 - `__ccvm_load_sec_***_begin`, `__ccvm_load_sec_***_end`:
   Beginning and ending load address of a section.
   This is different than above addresses only for `.data` section
   since it is loaded in program memory, but relocation address is
   in the data memory.

## Linking stages

* Merge all inputs into one, also, patching the relocations.
* Resolve symbols and assign them to relocations.
* Load local relocations and resolve their labels.
* Generate temporary output without shrinking.
* Resolve all symbols locations and assign them to relocations.
* Generate output using smallest possible sizes calculated
  from the relocations. Generate also memory mapping.
* Resolve all symbols locations and assign them to relocations.
* Generate the output.
