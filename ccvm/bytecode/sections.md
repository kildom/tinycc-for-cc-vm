
Data memory (`0x00000000`):
* `.ccvm.registers` - VM registers area
* `.data` - initialized data
* `.bss`, `.common` - uninitialized data
* `.ccvm.heap`, - uninitialized area for heap memory

Program memory (`0x40000000`):
* `.ccvm.entry` - entry jump
* `.rodata`, `.data.ro`, `.data.*.ro` - read only data
* `.ccvm.export.table` - automatically generated table of exported functions pointers
* `.init_array` - initializers function pointers
* `.fini_array` - finalizers function pointers
* `.text` - program
* copy of `.data` - initialization data

> TODO: Mark beginning and ending of function (in prologue and epilogue).
> It should allow removal of unused functions.
