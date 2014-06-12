MYRLIB=std
MYRSRC= \
    alloc.myr \
    bigint.myr \
    bitset.myr \
    blat.myr \
    chartype.myr \
    cmp.myr \
    dial.myr \
    die.myr \
    endian.myr \
    env.myr \
    execvp.myr \
    extremum.myr \
    floatbits.myr \
    fmt.myr \
    hashfuncs.myr \
    hasprefix.myr \
    hassuffix.myr \
    htab.myr \
    ifreq.myr \
    intparse.myr \
    ipparse.myr \
    mk.myr \
    now.myr \
    option.myr \
    optparse.myr \
    rand.myr \
    resolve.myr \
    result.myr \
    search.myr \
    slcp.myr \
    sldup.myr \
    sleq.myr \
    slfill.myr \
    sljoin.myr \
    slpush.myr \
    slput.myr \
    slurp.myr \
    sort.myr \
    strfind.myr \
    strjoin.myr \
    strsplit.myr \
    strstrip.myr \
    sys.myr \
    try.myr \
    types.myr \
    units.myr \
    utf.myr \
    varargs.myr \
    waitstatus.myr \

ASMSRC= \
    start.s \
    syscall.s \
    util.s

include ../config.mk

all: lib$(MYRLIB).a $(MYRBIN) test

sys.myr: sys-$(SYS).myr
	cp sys-$(SYS).myr sys.myr

ifreq.myr: ifreq-$(SYS).myr
	cp ifreq-$(SYS).myr ifreq.myr

waitstatus.myr: waitstatus-$(SYS).myr
	cp waitstatus-$(SYS).myr waitstatus.myr

syscall.s: syscall-$(SYS).s
	cp syscall-$(SYS).s syscall.s

start.s: start-$(SYS).s
	cp start-$(SYS).s start.s

test: libstd.a test.myr ../6/6m
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -b test -I. test.myr


lib$(MYRLIB).a: $(MYRSRC) $(ASMSRC) ../6/6m
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

uninstall:
	rm -f $(INST_ROOT)/lib/myr/libstd.a
	rm -f $(INST_ROOT)/lib/myr/std

../6/6m:
	cd ..; make
