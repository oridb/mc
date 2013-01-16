MYRLIB=std
MYRSRC= \
    alloc.myr \
    chartype.myr \
    die.myr \
    extremum.myr \
    fmt.myr \
    maybe.myr \
    option.myr \
    rand.myr \
    slurp.myr \
    sys.myr \
    types.myr \
    utf.myr \
    varargs.myr \

ASMSRC= \
    start.s \
    syscall.s \
    util.s

include ../config.mk

all: $(MYRLIB) $(MYRBIN) test

sys.myr: sys-$(SYS).myr
	cp sys-$(SYS).myr sys.myr

syscall.s: syscall-$(SYS).s
	cp syscall-$(SYS).s syscall.s

start.s: start-$(SYS).s
	cp start-$(SYS).s start.s

test: libstd.a test.myr
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -b test -I. test.myr


$(MYRLIB): $(MYRSRC) $(ASMSRC)
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -l $(MYRLIB) $^

OBJ=$(MYRSRC:.myr=.o) $(ASMSRC:.s=.o)
JUNKASM=$(MYRSRC:.myr=.s)
USE=$(MYRSRC:.myr=.use) $(MYRLIB)
.PHONY: clean
clean:
	rm -f $(OBJ) test.o
	rm -f $(USE) test.use
	rm -f $(JUNKASM) test.s
	rm -f lib$(MYRLIB).a

install:
	mkdir -p  $(INST_ROOT)/lib/myr
	install libstd.a $(INST_ROOT)/lib/myr
	install std $(INST_ROOT)/lib/myr
