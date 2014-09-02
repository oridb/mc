BIN=runner
OBJ=runner.o
BENCHSRC=intsort.myr \
	 copious-allocs.myr \

include ../config.mk
include ../mk/c.mk

bench: runner cleanbuild
	./runner $(BENCHSRC:.myr=)

.PHONY: cleanbuild
cleanbuild:
	rm -f $(BENCHSRC:.myr=) $(BENCHSRC:.myr=.o) $(BENCHSRC:.myr=.use)
	@for i in $(BENCHSRC:.myr=); do \
	    ../myrbuild/myrbuild -b $$i $$i.myr; \
	done

