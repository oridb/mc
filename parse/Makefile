BIN=pt
OBJ=dump.o \
    main.o \
    names.o \
    node.o \
    gram.o \
    tok.o \
    type.o \
    util.o

CLEAN=gram.c

include ../mk/lexyacc.mk
include ../mk/c.mk

ops.o: ../mc/ops.c
	$(CC) -c $(CFLAGS) $<

