.SUFFIXES:
GENSRC= \
	dial.myr \
	dir.myr \
	env.myr \
	ifreq.myr \
	resolve.myr \
	sys.myr \
	syscall.s \
	syserrno.myr \
	syswrap.myr \
	syswrap-ss.myr \
	util.s \
	wait.myr \

SYSLIB=sys
SYSSRC= \
	sys.myr \
        syscall.s \
        syserrno.myr \
        systypes.myr \
        ifreq.myr \
        util.s \

SYSASMSRC= \
	syscall.s \
	util.s

STDLIB=std
STDSRC= \
	alloc.myr \
	bigint.myr \
	bitset.myr \
	blat.myr \
	bytebuf.myr \
	chartype.myr \
	cmp.myr \
	clear.myr \
	dial.myr \
	die.myr \
	dir.myr \
	dirname.myr \
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
	putint.myr \
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
	syswrap-ss.myr \
	swap.myr \
	try.myr \
	types.myr \
	units.myr \
	utf.myr \
	varargs.myr \
	wait.myr \

include ../config.mk

all: lib$(STDLIB).a $(MYRBIN)


# HACK HACK HACK.
# Ideally we would have patterns for the
# foo+arch-suffix.myr instead of a loop,
# but that leads to a combinatorial explosion
# of patterns that Make will try, leading to
# millions of files getting statted. This
# works around that.
COPYTARG = for targ in \
	   	$(SYSCLASS)-$(SYS)-$(ARCH) \
		$(SYSCLASS)-$(SYS) \
		$(SYSCLASS)-$(ARCH) \
		$(SYSCLASS) \
		$(SYS)-$(ARCH) \
		$(SYS) \
		$(ARCH); do \
		echo "trying $*+$$targ.$$ext"; \
		if [ -f $*+$$targ.$$ext ] ; then  \
		echo $$targ; \
		echo cp $*+$$targ.$$ext $@; \
		cp $*+$$targ.$$ext $@; \
		exit 0; \
	fi; \
	done; \

%.myr:
	@export ext=myr; \
	$(COPYTARG)

%.s:
	@export ext=s; \
	$(COPYTARG)

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
.PHONY: clean check test
clean:
	rm -f $(OBJ)
	rm -f $(USE)
	rm -f $(GENSRC)
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

check: test

test:
	mbld test

../6/6m:
	cd ..; $(MAKE)
