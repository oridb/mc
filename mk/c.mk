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


clean: subdirs-clean 
	rm -f ${BIN} ${OBJ} ${CLEAN}


install-bin: $(INSTBIN)
	@if [ ! -z "$(INSTBIN)" ]; then \
		echo install $(INSTBIN) $(INST_ROOT)/bin; \
		mkdir -p $(INST_ROOT)/bin; \
		install $(INSTBIN) $(INST_ROOT)/bin; \
	fi

install-lib: $(INSTLIB)
	@if [ ! -z "$(INSTLIB)" ]; then \
		echo install $(INSTLIB) $(INST_ROOT)/lib; \
		mkdir -p $(INST_ROOT)/lib; \
		install $(INSTLIB) $(INST_ROOT)/lib; \
	fi

install-hdr: $(INSTHDR)
	@if [ ! -z "$(INSTHDR)" ]; then \
		echo install $(INSTHDR) $(INST_ROOT)/include; \
		mkdir -p $(INST_ROOT)/include; \
		install $(INSTHDR) $(INST_ROOT)/include; \
	fi

install-pc: $(INSTPKG)
	@if [ ! -z "$(INSTPKG)" ]; then \
		echo install $(INSTPKG) $(INST_ROOT)/lib/pkgconfig; \
		mkdir -p $(INST_ROOT)/lib/pkgconfig; \
		install $(INSTPKG) $(INST_ROOT)/lib/pkgconfig; \
	fi

clean-backups:
	find ./ -name .*.sw* -exec rm -f {} \;
	find ./ -name *.bak -exec rm -f {} \;

%.o: %.c .deps
	$(CC) -c $(CFLAGS) $(_LIBINCPATHS) $<

.deps: 
	mkdir -p $(_DEPSDIR)

config.mk: configure
	./configure

-include $(_DEPS)
