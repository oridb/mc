BIN=8m
OBJ=isel.o \
    main.o \
    simp.o \
    reduce.o \

CFLAGS+=-I../parse
LDFLAGS+=-L../parse -lparse
EXTRADEP=../parse/libparse.a

../libparse.a:
	make -C ../parse

include ../mk/lexyacc.mk
include ../mk/c.mk
