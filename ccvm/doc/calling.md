
Calling convention:
 * arguments are on stack only
 * each argument is aligned to its natural alignment
 * entire arguments block is aligned to 32 bits
 * arguments are removed from stack by caller
 * arguments are pushed from the last to the first argument
   (they show in memory from the first to the last argument).
 * calling host function use the same convention, so imported
   functions must be handled by dedicated wrapper.


```
(0) initial state:
    ...stack... |
     BP--^      ^--SP

(1) before call:
    ...stack... | PARAMETERS |
     BP--^                   ^--SP

(2) after call:
    ...stack... | PARAMETERS | old PC | old BP |
                                           BP--^--SP

(3) after prologue:
    ...stack... | PARAMETERS | old PC | old BP | LOCALS | ...
                                           BP--^              ^--SP

(4) after return:
    ...stack... | PARAMETERS |
     BP--^                   ^--SP

(5) after parameter removal:
    ...stack... |
     BP--^      ^--SP

Example:
...
(0)
PUSH R0
PUSH R1
(1)
CALL my_function
(4)
POP_BLOCK R3, 8
(5)
...

my_function:   # arguments (int, int)
(2)
PUSH_BLOCK R0, 4   # 4 bytes of local variables
(3)
...
RETURN
```

`CALL` instruction operations:
 * push PC
 * set PC = destination address
 * push BP
 * set BP = SP

`RETURN N` instruction operations:
 * set SP = BP
 * pop BP
 * pop PC
 * set SP = SP + 4 * N
