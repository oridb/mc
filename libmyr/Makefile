include ../config.mk

all: 
	./bld.sh
.PHONY: clean
clean:

install:
	mkdir -p  $(INST_ROOT)/myr/lib/
	install libstd.a $(INST_ROOT)/myr/lib/
	install std $(INST_ROOT)/myr/lib/
