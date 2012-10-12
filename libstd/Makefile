MYRLIB=std
MYRSRC= \
    alloc.myr \
    chartype.myr \
    die.myr \
    fmt.myr \
    option.myr \
    rand.myr \
    sys.myr \
    types.myr \
    utf.myr \
    varargs.myr \

ASMSRC= \
    start.s \
    syscall.s \
    util.s

include ../config.mk

all: $(MYRLIB) $(MYRBIN)

sys.myr: sys-$(SYS).myr
	cp sys-$(SYS).myr sys.myr

syscall.s: syscall-$(SYS).s
	cp syscall-$(SYS).s syscall.s

start.s: start-$(SYS).s
	cp start-$(SYS).s start.s


$(MYRLIB): $(MYRSRC) $(ASMSRC)
	../myrbuild/myrbuild -C../6/6m -M../muse/muse -l $(MYRLIB) $^

OBJ=$(MYRSRC:.myr=.o) $(ASMSRC:.s=.o)
JUNKASM=$(MYRSRC:.myr=.s)
USE=$(MYRSRC:.myr=.use) $(MYRLIB)
.PHONY: clean
clean:
	rm -f $(OBJ)
	rm -f $(USE)
	rm -f $(JUNKASM)
	rm -f lib$(MYRLIB).a


install:
	mkdir -p  $(INST_ROOT)/lib/myr
	install libstd.a $(INST_ROOT)/lib/myr
	install std $(INST_ROOT)/lib/myr
