LIB=libparse.a
OBJ=\
	dump.o \
	err.o \
	export.o \
	fold.o \
	gram.o \
	infer.o \
	names.o \
	node.o \
	specialize.o \
	stab.o \
	tok.o \
	type.o \
	use.o \

GENHDR=gram.h
CLEAN=gram.c gram.h

DEPS=../util/libutil.a

include ../mk/lexyacc.mk
include ../mk/c.mk
