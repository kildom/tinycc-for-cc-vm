
OBJ_DIR := bin
OBJ:=
TARGET := $(OBJ_DIR)/tcc

CFLAGS := -O0 -g -DCONFIG_TCC_SEMLOCK=0 -DTCC_TARGET_CCVM=1 -DONE_SOURCE=1

CC = gcc

all: $(TARGET)

clean:
	rm -Rf $(OBJ_DIR)

run: $(TARGET) __RUN_ALWAYS__
	./bin/tcc -c a.c -Iinclude -o a.o

$(TARGET): tcc.c Makefile
	mkdir -p $(dir $@)
	$(CC) -MMD $(CFLAGS) tcc.c -o $@

__RUN_ALWAYS__:

-include $(TARGET).d
