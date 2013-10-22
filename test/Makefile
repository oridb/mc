# don't build anything for 'all'
all: 
	$(MAKE) -C ..

check:
	./runtest.sh

.PHONY: %
%:
	./runtest.sh $@

.PHONY: clean
clean:
	@for i in `awk '/^[A-Z]/{print $$2}' tests`; do \
	    echo rm -f $$i; \
	    rm -f $$i; \
	done

install:
