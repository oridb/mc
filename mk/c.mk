.DEFAULT_GOAL=all
_DEPSDIR = .deps
_DEPS=$(addprefix $(_DEPSDIR)/, $(OBJ:.o=.d))

_LIBSRCHPATHS=$(addprefix -L, $(dir $(DEPS)))
_LIBINCPATHS=$(addprefix -I, $(dir $(DEPS)))
_LIBPATHS=$(addprefix -l, $(patsubst lib%.a,%,$(notdir $(DEPS))))

CFLAGS += -Wall -Werror -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
CFLAGS += -g
CFLAGS += -MMD -MP -MF ${_DEPSDIR}/$(subst /,-,$*).d

.PHONY: clean clean-gen clean-bin clean-obj clean-misc clean-backups
.PHONY: all

all: subdirs $(BIN) $(LIB) $(EXTRA)
install: subdirs-install install-bin install-lib install-hdr install-pc

$(LIB): $(OBJ) $(DEPS)
	$(AR) -rcs $@ $(OBJ)

$(BIN): $(OBJ) $(EXTRADEP) $(DEPS)
	$(CC) -o $@ $(OBJ) $(_LIBSRCHPATHS) $(_LIBPATHS)

.PHONY: $(DEPS)
$(DEPS):
	@for i in $(dir $(DEPS)); do (\
	    cd $$i && \
	    $(MAKE) || \
	    exit 1 \
	) || exit 1; done

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


clean: subdirs-clean 
	rm -f ${BIN} ${OBJ} ${CLEAN}


install-bin: $(INSTBIN)
	mkdir -p $(INST_ROOT)/bin
	test -z "$(INSTBIN)" || install $(INSTBIN) $(INST_ROOT)/bin

install-lib: $(INSTLIB)
	mkdir -p $(INST_ROOT)/lib
	test -z "$(INSTLIB)" || install $(INSTLIB) $(INST_ROOT)/lib

install-hdr: $(INSTHDR)
	mkdir -p $(INST_ROOT)/$(HDRDIR)/include
	test -z "$(INSTHDR)" || install $(INSTHDR) $(INST_ROOT)/include

install-pc: $(INSTPKG)
	mkdir -p $(INST_ROOT)/pkgconfig
	test -z "$(INSTPC)" || install $(INSTPC) $(INST_ROOT)/pkgconfig

clean-backups:
	find ./ -name .*.sw* -exec rm -f {} \;
	find ./ -name *.bak -exec rm -f {} \;

%.o: %.c .deps
	$(CC) -c $(CFLAGS) $(_LIBINCPATHS) $<

.deps: 
	mkdir -p $(_DEPSDIR)

	
-include $(_DEPS)
