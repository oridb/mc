# don't build anything for 'all'
all: 

check:
	./build.sh
	./test.sh
.PHONY: clean
clean:
	rm -f $(TESTBIN) $(TESTBIN:=.o) $(TESTBIN:=.s)

install:
