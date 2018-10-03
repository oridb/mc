OBJ = _myrrt.o
ASMSRC = start.s common.s abort.s

all: _myrrt.o

include ../config.mk

_myrrt.o: _myrrt.s
	as -g -o $@ $^

_myrrt.s: $(ASMSRC)
	cat $(ASMSRC) > $@

%.s: %-$(SYS).s
	cp $< $@

install: all
	mkdir -p  $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)
	install _myrrt.o $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr)

uninstall:
	rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr/_myrrt.o)

clean:
	rm -f _myrrt.o _myrrt.s start.s abort.s
