MAN=myr-regex.3 \

include ../config.mk

all:
	
install:
	@for i in $(MAN); do \
	    MANSECT=$$(echo $$i | awk -F. '{print $$NF}'); \
	    echo mkdir -p $(INST_ROOT)/share/man/man$$MANSECT; \
	    echo install -m 644 $(MAN) $(INST_ROOT)/share/man/man$${MANSECT}; \
	    mkdir -p $(INST_ROOT)/share/man/man$$MANSECT; \
	    install -m 644 $(MAN) $(INST_ROOT)/share/man/man$${MANSECT}; \
	done \

clean:

