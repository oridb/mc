ifneq ($(MYRLIB),)
    _LIBNAME=lib$(MYRLIB).a
endif

all: $(_LIBNAME) $(MYRBIN)

$(_LIBNAME): $(MYRSRC) $(ASMSRC)
	myrbuild -l $(MYRLIB) $^

$(MYRBIN): $(MYRSRC) $(ASMSRC)
	myrbuild -b $(MYRBIN) $^

OBJ=$(MYRSRC:.myr=.o) $(ASMSRC:.s=.o)
JUNKASM=$(MYRSRC:.myr=.s)
USE=$(MYRSRC:.myr=.use) $(MYRLIB)
.PHONY: clean
clean:
	rm -f $(OBJ)
	rm -f $(USE)
	rm -f $(JUNKASM) $(CLEANEXTRA)
	rm -f $(_LIBNAME) $(MYRBIN)

install: install-bin install-lib

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

config.mk:
	./configure
