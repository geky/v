TARGET = mu
LIBTARGET = libmu.a

CC = gcc
AR = ar

SRC += var.c mem.c err.c
SRC += num.c str.c tbl.c fn.c
SRC += parse.c lex.c vm.c
SRC += mu.c
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)
ASM := $(SRC:.c=.s)

#CFLAGS += -O2
#CFLAGS += -Os -s
CFLAGS += -O0 -g3 -gdwarf-2 -ggdb -DMU_DEBUG
CFLAGS += -include stdio.h
CFLAGS += -foptimize-sibling-calls -freg-struct-return
CFLAGS += -m32
CFLAGS += -Wall -Winline

LFLAGS += -lm


all: $(TARGET)

lib: $(LIBTARGET)

asm: $(ASM)

include $(DEP)


$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

$(LIBTARGET): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c %.d
	$(CC) -c $(CFLAGS) $< -o $@

%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@

%.s: %.c
	$(CC) -S -fverbose-asm $(CFLAGS) $< -o $@


clean:
	-rm $(TARGET) $(LIBTARGET)
	-rm $(OBJ)
	-rm $(DEP)
	-rm $(ASM)
