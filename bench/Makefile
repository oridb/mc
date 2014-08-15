BIN=runner
OBJ=runner.o
BENCHSRC=intsort.myr \
	 copious-allocs.myr \

include ../config.mk
include ../mk/c.mk

bench: runner $(BENCHSRC:.myr=)
	./runner $(BENCHSRC:.myr=)

$(BENCHSRC:.myr=): $(BENCHSRC)
	../myrbuild/myrbuild -b $@ $@.myr
