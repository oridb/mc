BIN=8m
OBJ=main.o \
    simp.o \
    gen.o \

CFLAGS+=-I../parse
LDFLAGS+=-L../parse -lparse
EXTRADEP=../parse/libparse.a

include ../mk/lexyacc.mk
include ../mk/c.mk

