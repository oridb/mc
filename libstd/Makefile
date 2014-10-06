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
    dir.myr \
    endian.myr \
    env.myr \
    execvp.myr \
    extremum.myr \
    fltbits.myr \
    fmt.myr \
    fltfmt.myr \
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
    pathjoin.myr \
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
    spork.myr \
    strfind.myr \
    strjoin.myr \
    strsplit.myr \
    strstrip.myr \
    swap.myr \
    sys.myr \
    try.myr \
    types.myr \
    units.myr \
    utf.myr \
    varargs.myr \
    waitstatus.myr \

ASMSRC= \
    syscall.s \
    util.s

include ../config.mk

all: lib$(MYRLIB).a $(MYRBIN)

%.myr: %+$(SYS).myr
	cp $< $@

%.s: %+$(SYS)-x64.s
	cp $< $@

lib$(MYRLIB).a: $(MYRSRC) $(ASMSRC) ../6/6m
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -l $(MYRLIB) $(MYRSRC) $(ASMSRC)

OBJ=$(MYRSRC:.myr=.o) $(ASMSRC:.s=.o)
USE=$(MYRSRC:.myr=.use) $(MYRLIB)
.PHONY: clean
clean:
	rm -f $(OBJ)
	rm -f $(USE)
	rm -f lib$(MYRLIB).a

install: all
	mkdir -p  $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)
	install libstd.a $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)
	install std $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)

uninstall:
	rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr/libstd.a)
	rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr/std)

../6/6m:
	cd ..; $(MAKE)
