SYSLIB=sys
SYSSRC= \
	sys.myr \
	syserrno.myr \
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
	clear.myr \
	dial.myr \
	die.myr \
	dir.myr \
	endian.myr \
	env.myr \
	errno.myr \
	execvp.myr \
	extremum.myr \
	fltbits.myr \
	fmt.myr \
	fltfmt.myr \
	hashfuncs.myr \
	hasprefix.myr \
	hassuffix.myr \
	htab.myr \
	getint.myr \
	intparse.myr \
	ipparse.myr \
	mk.myr \
	mkpath.myr \
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
	cstrconv.myr \
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
	wait.myr \

include ../config.mk

all: lib$(STDLIB).a $(MYRBIN)

%.myr: %+$(SYSCLASS)-$(ARCH).myr
	cp $< $@

%.myr: %+$(SYSCLASS).myr
	cp $< $@

%.myr: %+$(SYS)-$(ARCH).myr
	cp $< $@

%.myr: %+$(SYS).myr
	cp $< $@

%.myr: %+$(ARCH).myr
	cp $< $@

%.s: %+$(SYSCLASS)-$(ARCH).s
	cp $< $@

%.s: %+$(SYS)-$(ARCH).s
	cp $< $@

%.s: %+$(ARCH).s
	cp $< $@

lib$(STDLIB).a: $(STDSRC) $(ASMSRC) lib$(SYSLIB).a ../6/6m
	export MYR_MC=../6/6m && \
	export MYR_MUSE=../muse/muse && \
	./build.sh -l $(STDLIB) $(STDSRC) $(STDASMSRC)

lib$(SYSLIB).a: $(SYSSRC) $(SYSASMSRC) ../6/6m
	export MYR_MC=../6/6m && \
	export MYR_MUSE=../muse/muse && \
	./build.sh -l $(SYSLIB) $(SYSSRC) $(SYSASMSRC)

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
