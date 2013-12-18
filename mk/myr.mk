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
	rm -f lib$(MYRLIB).a

install: subdirs-install install-bin install-lib install-man

install-bin: $(MYRBIN)
	@if [ ! -z "$(MYRBIN)" ]; then \
	    echo install $(MYRBIN) $(INST_ROOT)/bin; \
	    mkdir -p $(INST_ROOT)/bin; \
	    install $(MYRBIN) $(INST_ROOT)/bin; \
	fi

install-lib: $(_LIBNAME)
	@if [ ! -z "$(_LIBNAME)" ]; then \
		echo install -m 644 $(_LIBNAME) $(INST_ROOT)/lib/myr; \
		echo install -m 644 $(MYRLIB) $(INST_ROOT)/lib/myr; \
		mkdir -p $(INST_ROOT)/lib/myr; \
		install -m 644 $(_LIBNAME) $(INST_ROOT)/lib/myr; \
		install -m 644 $(MYRLIB) $(INST_ROOT)/lib/myr; \
	fi

install-man:
	@for i in $(MAN); do \
	    MANSECT=$$(echo $$i | awk -F. '{print $$NF}'); \
	    echo mkdir -p $(INST_ROOT)/share/man/man$$MANSECT; \
	    echo install -m 644 $(MAN) $(INST_ROOT)/share/man/man$${MANSECT}; \
	    mkdir -p $(INST_ROOT)/share/man/man$$MANSECT; \
	    install -m 644 $(MAN) $(INST_ROOT)/share/man/man$${MANSECT}; \
	done \

config.mk:
	./configure
