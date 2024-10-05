
OBJ_DIR := bin
OBJ:=
TARGET := $(OBJ_DIR)/ccvm-tcc

CFLAGS := -O0 -g -DCONFIG_TCC_SEMLOCK=0 -DTCC_TARGET_CCVM=1 -DONE_SOURCE=1 -iquote. -I. -I.. -Wno-format-truncation

CC = gcc

all: $(TARGET)

clean:
	rm -Rf $(OBJ_DIR)

run: $(TARGET) __RUN_ALWAYS__
	./bin/ccvm-tcc -c sample.c -I../include -o bin/a.o

$(TARGET): ../tcc.c Makefile
	-mv ../config.h ../config-backup.h  > /dev/null 2>&1 ; rm -f ../config.h > /dev/null 2>&1
	mkdir -p $(dir $@)
	$(CC) -MMD $(CFLAGS) ../tcc.c -o $@

__RUN_ALWAYS__:

-include $(TARGET).d
