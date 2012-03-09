BIN=8m
OBJ=main.o \
    simp.o \
    gen.o \

CFLAGS+=-I../parse
LDFLAGS+=-L../parse -lparse

include ../mk/lexyacc.mk
include ../mk/c.mk

