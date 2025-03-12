
OBJ_DIR := bin
OBJ:=
TARGET := $(OBJ_DIR)/ccvm-tcc

CFLAGS := -O0 -g -DCONFIG_TCC_SEMLOCK=0 -DTCC_TARGET_CCVM=1 -DONE_SOURCE=1 -iquote. -I. -I.. -Wno-format-truncation

CFLAGS += -DCCVM_FLOAT_64=0

CC = gcc

all: $(TARGET)

clean:
	rm -Rf $(OBJ_DIR)

run_compile: $(TARGET) __RUN_ALWAYS__
	./bin/ccvm-tcc -c sample/main.c -I../include -o bin/sample_main.o
	./bin/ccvm-tcc -c sample/a.c -I../include -o bin/sample_a.o
	./bin/ccvm-tcc -c sample/b.c -I../include -o bin/sample_b.o

run: run_compile $(TARGET) __RUN_ALWAYS__
	./bin/ccvm-tcc -Wl,-nostdlib bin/sample_main.o bin/sample_a.o bin/sample_b.o -o bin/sample.bin

$(TARGET): ../tcc.c ccvm-enums.h Makefile
	-mv ../config.h ../config-backup.h  > /dev/null 2>&1 ; rm -f ../config.h > /dev/null 2>&1
	mkdir -p $(dir $@)
	$(CC) -MMD $(CFLAGS) ../tcc.c -o $@

ccvm-enums.h: bytecode/enums.ts Makefile
	$(CC) -x c -c -E -Dexport= -o $@ bytecode/enums.ts

__RUN_ALWAYS__:

-include $(TARGET).d
