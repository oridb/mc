BIN=runner
OBJ=runner.o
BENCHSRC=intsort.myr \
	 copious-allocs.myr \
	 sha1-compute.myr \
	 bigfactorial.myr \
	 mandelbrot.myr 

include ../config.mk
include ../mk/c.mk

bench: runner cleanbuild
	./runner $(BENCHSRC:.myr=)

.PHONY: cleanbuild
cleanbuild:
	rm -f $(BENCHSRC:.myr=) $(BENCHSRC:.myr=.o) $(BENCHSRC:.myr=.use)
	@for i in $(BENCHSRC:.myr=); do \
	    ../myrbuild/myrbuild -b $$i  -C../6/6m -M../muse/muse -I../libstd -r../rt/_myrrt.o $$i.myr; \
	done

