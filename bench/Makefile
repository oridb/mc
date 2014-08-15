BIN=runner
OBJ=runner.o
BENCHSRC=intsort.myr
BENCHUTIL=

include ../config.mk
include ../mk/c.mk

bench: runner $(BENCHSRC:.myr=)
	./runner $(BENCHSRC:.myr=)

$(BENCHSRC:.myr=): $(BENCHSRC) $(BENCHUTIL)
	../myrbuild/myrbuild -b $@ $@.myr $(BENCHUTIL)
