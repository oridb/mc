.DEFAULT_GOAL=all
_DEPS=$(addprefix .deps/, $(OBJ:.o=.d))

_PCHDRS=$(shell [ -z "$(PCPKGS)" ] || pkg-config --cflags $(PCPKGS))
_PCLIBS=$(shell [ -z "$(PCPKGS)" ] ||pkg-config --libs $(PCPKGS))

_LIBSRCHPATHS=$(addprefix -L, $(dir $(DEPS)))
_LIBINCPATHS=$(addprefix -I, $(dir $(DEPS))) $(_PCHDRS)
_LIBPATHS=$(addprefix -l, $(patsubst lib%.a,%,$(notdir $(DEPS)))) $(_PCLIBS)

# yeah, I should probably remove -Werror, but it's nice for developing alone.
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-array-bounds -g
CFLAGS += -MMD -MP -MF .deps/$(subst /,-,$*).d

LIB ?= $(INSTLIB)
BIN ?= $(INSTBIN)

# disable implicit rules.
.SUFFIXES:
.SECONDARY:
.PHONY: clean clean-gen clean-bin clean-obj clean-misc clean-backups
.PHONY: all _modversions

all: _modversions subdirs $(BIN) $(LIB) $(EXTRA)

$(LIB): $(OBJ) $(EXTRADEP) $(DEPS)
	$(AR) -rcs $@ $(OBJ)

$(BIN): $(OBJ) $(EXTRADEP) $(DEPS)
	$(CC) -o $@ $(OBJ) $(_LIBSRCHPATHS) $(_LIBPATHS) $(LDFLAGS)

$(DEPS):
	@cd $(dir $@) && $(MAKE)

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

clean: subdirs-clean $(EXTRACLEAN)
	rm -f ${BIN} ${OBJ} ${CLEAN} ${LIB}
	rm -rf .deps/

install: subdirs-install $(INSTBIN) $(INSTLIB) $(INSTHDR) $(INSTPKG) $(EXTRAINSTALL)
	@for i in $(INSTBIN); do \
		echo install $(abspath $$i $(DESTDIR)/$(INST_ROOT)/bin); \
		mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/bin); \
		install $$i $(abspath $(DESTDIR)/$(INST_ROOT)/bin); \
	done
	@for i in $(INSTLIB); do \
		echo install -m 644 $$i $(abspath $(DESTDIR)/$(INST_ROOT)/lib); \
		mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/lib); \
		install -m 644 $$i $(abspath $(DESTDIR)/$(INST_ROOT)/lib); \
	done
	@for i in $(INSTHDR); do \
		echo install $$i $(abspath $(DESTDIR)/$(INST_ROOT)/include); \
		mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/include); \
		install $$i $(abspath $(DESTDIR)/$(INST_ROOT)/include); \
	done
	@for i in $(INSTPKG); do \
		echo install $(abspath $$i $(DESTDIR)/$(INST_ROOT)/lib/pkgconfig); \
		mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/lib/pkgconfig); \
		install $(abspath $$i $(DESTDIR)/$(INST_ROOT)/lib/pkgconfig); \
	    done
	@for i in $(INSTMAN); do \
		sect="$${i##*.}"; \
		echo install -m 644 $$i $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${sect}); \
		mkdir -p $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${sect}); \
		install -m 644 $$i $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${sect}); \
	done

subdirs-uninstall:
	@for i in $(SUB); do (\
	    cd $$i && \
	    $(MAKE) uninstall|| \
	    exit 1 \
	); done

uninstall: subdirs-uninstall $(EXTRAUNINSTALL)
	@for i in $(INSTBIN); do \
		echo rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/bin/$$i); \
		rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/bin/$$i); \
	done
	@for i in $(INSTLIB); do \
		echo rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/$$i); \
		rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/$$i); \
	done
	@for i in $(INSTHDR); do \
		echo rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/include/$$i); \
		rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/include/$$i); \
	done
	@for i in $(INSTPKG); do \
		echo rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/pkgconfig/$$i); \
		rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/lib/pkgconfig/$$i); \
	done
	@for i in $(INSTMAN); do \
		sect="$${i##*.}" \
		echo rm -f $$i $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${sect}/$$i); \
		rm -f $(abspath $(DESTDIR)/$(INST_ROOT)/share/man/man$${sect}/$$i); \
	done

%.o: %.c $(GENHDR) .deps/stamp
	$(CC) -c $(CFLAGS) $(_LIBINCPATHS) $<

%.o: %.cpp $(GENHDR) .deps/stamp
	$(CXX) -c $(CFLAGS) $(_LIBINCPATHS) $<

%.o: %.cc $(GENHDR) .deps/stamp
	$(CXX) -c $(CFLAGS) $(_LIBINCPATHS) $<

.deps/stamp:
	mkdir -p .deps && touch .deps/stamp

config.mk: configure
	./configure --redo

_modversions:
	@[ -z "$(PCPKGS)" ] || pkg-config --modversion $(PCPKGS) > /dev/null

-include $(_DEPS)
