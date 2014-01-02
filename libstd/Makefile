MYRLIB=std
MYRSRC= \
    alloc.myr \
    blat.myr \
    chartype.myr \
    cmp.myr \
    die.myr \
    endian.myr \
    env.myr \
    error.myr \
    extremum.myr \
    fmt.myr \
    hashfuncs.myr \
    hasprefix.myr \
    htab.myr \
    intparse.myr \
    ipparse.myr \
    now.myr \
    option.myr \
    optparse.myr \
    rand.myr \
    resolve.myr \
    slappend.myr \
    slcp.myr \
    sldup.myr \
    sleq.myr \
    sljoin.myr \
    slpush.myr \
    slurp.myr \
    sort.myr \
    strfind.myr \
    strjoin.myr \
    strsplit.myr \
    strstrip.myr \
    sys.myr \
    types.myr \
    units.myr \
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

test: libstd.a test.myr ../6/6m
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -b test -I. test.myr


$(MYRLIB): $(MYRSRC) $(ASMSRC) ../6/6m
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -l $(MYRLIB) $(MYRSRC) $(ASMSRC)

OBJ=$(MYRSRC:.myr=.o) $(ASMSRC:.s=.o)
USE=$(MYRSRC:.myr=.use) $(MYRLIB)
.PHONY: clean
clean:
	rm -f $(OBJ) test.o
	rm -f $(USE) test.use
	rm -f lib$(MYRLIB).a

install:
	mkdir -p  $(INST_ROOT)/lib/myr
	install libstd.a $(INST_ROOT)/lib/myr
	install std $(INST_ROOT)/lib/myr

../6/6m:
	cd ..; make
