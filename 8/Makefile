BIN=8m
OBJ=isel.o \
    main.o \
    reduce.o \
    regalloc.o \

CFLAGS+=-I../parse -I../opt
LDFLAGS+=-L../parse -lparse -L../opt -lopt
EXTRADEP=../parse/libparse.a ../opt/libopt.a

include ../mk/lexyacc.mk
include ../mk/c.mk
