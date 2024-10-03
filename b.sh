#!/bin/bash
set -e

rm -f ./a.out
gcc tcc.c -DCONFIG_TCC_SEMLOCK=0 -DTCC_TARGET_EMB=1 -DONE_SOURCE=1
./a.out -c a.c -Iinclude
