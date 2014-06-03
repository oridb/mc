MAN=mc.1 \
    muse.1 \
    myrbuild.1 \

include ../config.mk

all:
	
install:
	@echo install -m 644 $(MAN) $(INST_ROOT)/share/man/man1; \
	mkdir -p $(INST_ROOT)/share/man/man1; \
	install -m 644 $(MAN) $(INST_ROOT)/share/man/man1; \

uninstall: $(MAN)
	@for i in $^; do \
	    echo rm -f $(INST_ROOT)/share/man/man1/$$i; \
	    rm -f $(INST_ROOT)/share/man/man1/$$i; \
	done

clean:

