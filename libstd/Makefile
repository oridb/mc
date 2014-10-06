SYSLIB=sys
SYSSRC= \
	sys.myr \
	systypes.myr \
	ifreq.myr \

SYSASMSRC= \
	syscall.s \
	util.s


STDLIB=std
STDSRC= \
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
	syswrap.myr \
	swap.myr \
	try.myr \
	types.myr \
	units.myr \
	utf.myr \
	varargs.myr \
	waitstatus.myr \

include ../config.mk

all: lib$(STDLIB).a $(MYRBIN)

%.myr: %+$(SYS)-$(ARCH).myr
	cp $< $@

%.myr: %+$(SYS).myr
	cp $< $@

%.myr: %+$(ARCH).myr
	cp $< $@

%.s: %+$(SYS)-$(ARCH).s
	cp $< $@

%.s: %+$(ARCH).s
	cp $< $@

lib$(STDLIB).a: $(STDSRC) $(ASMSRC) lib$(SYSLIB).a ../6/6m
	../myrbuild/myrbuild -I. -C../6/6m -M../muse/muse -l $(STDLIB) $(STDSRC) $(STDASMSRC)

lib$(SYSLIB).a: $(SYSSRC) $(SYSASMSRC) ../6/6m
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -l $(SYSLIB) $(SYSSRC) $(SYSASMSRC)

OBJ=$(STDSRC:.myr=.o) $(SYSSRC:.myr=.o) $(STDASMSRC:.s=.o) $(SYSASMSRC:.s=.o)
USE=$(STDSRC:.myr=.use) $(SYSSRC:.myr=.use) $(STDLIB)
.PHONY: clean
clean:
	rm -f $(OBJ)
	rm -f $(USE)
	rm -f lib$(STDLIB).a lib$(SYSLIB).a

install: all
	mkdir -p  $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)
	install libstd.a $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)
	install libsys.a $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)
	install std $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)
	install sys $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)

uninstall:
	rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr/libstd.a)
	rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr/std)

../6/6m:
	cd ..; $(MAKE)
