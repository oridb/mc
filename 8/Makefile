BIN=8m
OBJ=isel.o \
    main.o \
    reduce.o \

CFLAGS+=-I../parse
LDFLAGS+=-L../parse -lparse
EXTRADEP=../parse/libparse.a

include ../mk/lexyacc.mk
include ../mk/c.mk
