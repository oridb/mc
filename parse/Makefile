BIN=pt
OBJ=bitset.o \
    dump.o \
    gram.o \
    htab.o \
    infer.o \
    main.o \
    names.o \
    node.o \
    pickle.o \
    stab.o \
    tok.o \
    type.o \
    use.o \
    util.o

CLEAN=gram.c

include ../mk/lexyacc.mk
include ../mk/c.mk

ops.o: ../mc/ops.c
	$(CC) -c $(CFLAGS) $<
