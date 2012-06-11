LIB=libopt.a
OBJ=cfg.o \
    df.o \

CFLAGS+=-I../parse
LDFLAGS+=-L../parse -lparse
EXTRADEP=../parse/libparse.a

include ../mk/lexyacc.mk
include ../mk/c.mk
