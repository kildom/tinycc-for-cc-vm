
OBJ_DIR := _build

SRC := \
	./tcc.o \
	./libtcc.c \
	./tccpp.c \
	./tccgen.c \
	./tccdbg.c \
	./tccasm.c \
	./tccelf.c \
	./tccrun.c \
	./arm-gen.c \
	./arm-link.c \
	./arm-asm.c

OBJ := $(patsubst %.c,$(OBJ_DIR)/%.c.o,$(SRC))

TARGET := $(OBJ_DIR)/tcc

ALLFLAGS :=
CFLAGS := -Os -g -DONE_SOURCE=1 -DCONFIG_TCC_SEMLOCK=0 -DLDOUBLE_SIZE=12 -DTCC_TARGET_ARM=1
LDFLAGS :=

CC = gcc


all: $(TARGET)

clean:
	rm -Rf $(OBJ_DIR)

$(TARGET): $(OBJ) Makefile
	$(CC) $(LDFLAGS) $(ALLFLAGS) $(OBJ) -o $@

$(OBJ_DIR)/%.c.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) -MMD -c $(CFLAGS) $(ALLFLAGS) $(word 1,$<) -o $@

-include $(patsubst %.o,%.d,$(OBJ))
