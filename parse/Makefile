BIN=pt
OBJ=dump.o \
    ds.o \
    gram.o \
    infer.o \
    main.o \
    names.o \
    node.o \
    pickle.o \
    tok.o \
    type.o \
    use.o \
    util.o

CLEAN=gram.c

include ../mk/lexyacc.mk
include ../mk/c.mk

ops.o: ../mc/ops.c
	$(CC) -c $(CFLAGS) $<
