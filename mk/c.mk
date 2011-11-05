DEPSDIR = .deps
DEPS=$(addprefix $(DEPSDIR)/, $(OBJ:.o=.d))

CFLAGS += -Wall -Werror
CFLAGS += -g
CFLAGS += -MMD -MP -MF ${DEPSDIR}/$(subst /,-,$*).d

.PHONY: clean clean-gen clean-bin clean-obj clean-misc clean-backups
.PHONY: all

all: subdirs $(BIN) $(LIB) $(EXTRA)
install: subdirs-install install-bin install-lib install-hdr install-pc

$(LIB): $(OBJ)
	$(AR) -rcs $@ $^

$(BIN): $(OBJ) $(EXTRADEP)
	$(CC) -o $@ $(OBJ) $(LDFLAGS) 

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
	$(CC) -c $(CFLAGS) $<

.deps: 
	mkdir -p $(DEPSDIR)

	
-include $(DEPS)
