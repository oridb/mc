# don't build anything for 'all'
all: 

check:
	./test.sh
.PHONY: clean
clean:
	@for i in `awk '{print $$1}' tests`; do \
	    echo rm -f $$i; \
	    rm -f $$i; \
	done

install:
