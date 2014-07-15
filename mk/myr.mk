ifneq ($(MYRLIB),)
    _LIBNAME=lib$(MYRLIB).a
endif

all: subdirs $(_LIBNAME) $(MYRBIN) 

subdirs:
	@for i in $(SUB); do (\
	    cd $$i && \
	    $(MAKE) || \
	    exit 1 \
	) || exit 1; done

subdirs-clean:
	@for i in $(SUB); do (\
	    cd $$i && \
	    $(MAKE) clean|| \
	    exit 1 \
	); done

subdirs-install:
	@for i in $(SUB); do (\
	    cd $$i && \
	    $(MAKE) install|| \
	    exit 1 \
	); done

subdirs-uninstall:
	@for i in $(SUB); do (\
	    cd $$i && \
	    $(MAKE) uninstall|| \
	    exit 1 \
	); done

$(_LIBNAME): $(MYRSRC) $(ASMSRC)
	myrbuild -l $(MYRLIB) $^

$(MYRBIN): $(MYRSRC) $(ASMSRC)
	myrbuild -b $(MYRBIN) $^

OBJ=$(MYRSRC:.myr=.o) $(ASMSRC:.s=.o)
USE=$(MYRSRC:.myr=.use) $(MYRLIB)
.PHONY: clean
clean: subdirs-clean
	rm -f $(OBJ)
	rm -f $(USE)
	@if [ ! -z "$(MYRLIB)" ]; then \
	    echo rm -f $(MYRLIB); \
	    rm -f $(MYRLIB); \
	    echo rm -f lib$(MYRLIB).a; \
	    rm -f lib$(MYRLIB).a; \
	fi
	@if [ ! -z "$(MYRBIN)" ]; then \
	    echo rm -f $(MYRBIN); \
	    rm -f $(MYRBIN); \
	    echo rm -f lib$(MYRBIN).a; \
	    rm -f lib$(MYRBIN).a; \
	fi

install: subdirs-install $(MYRBIN) $(_LIBNAME) $(MAN)
	@if [ ! -z "$(MYRBIN)" ]; then \
	    echo install $(MYRBIN) $(abspath $(DESTDIR)/$(INST_ROOT)/bin); \
	    mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/bin); \
	    install $(MYRBIN) $(abspath $(DESTDIR)/$(INST_ROOT)/bin); \
	fi
	@if [ ! -z "$(_LIBNAME)" ]; then \
		echo install -m 644 $(_LIBNAME) $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr); \
		echo install -m 644 $(MYRLIB) $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr); \
		mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr); \
		install -m 644 $(_LIBNAME) $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr); \
		install -m 644 $(MYRLIB) $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr); \
	fi
	@for i in $(MAN); do \
	    MANSECT=$$(echo $$i | awk -F. '{print $$NF}'); \
	    echo mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$$MANSECT); \
	    echo install -m 644 $(MAN) $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${MANSECT}); \
	    mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$$MANSECT); \
	    install -m 644 $(MAN) $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${MANSECT}); \
	done \

uninstall: subdirs-uninstall
	@for i in $(MYRBIN); do \
	    echo rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/bin/$$i); \
	    rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/bin/$$i); \
	done
	@for i in $(_LIBNAME) $(MYRLIB); do \
	    echo rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr/$$i); \
	    rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/myr/$$i); \
	done
	@for i in $(MAN); do \
	    MANSECT=$$(echo $$i | awk -F. '{print $$NF}'); \
	    echo rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${MANSECT}/$$i); \
	    rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${MANSECT}/$$i); \
	done

config.mk:
	./configure
