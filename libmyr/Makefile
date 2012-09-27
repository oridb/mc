include ../config.mk

all: libstd.a

libstd.a:
	./bld.sh
.PHONY: clean
clean:
	./bld.sh clean

install:
	mkdir -p  $(INST_ROOT)/myr/lib/
	install libstd.a $(INST_ROOT)/myr/lib/
	install std $(INST_ROOT)/myr/lib/
