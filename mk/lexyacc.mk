.SUFFIXES:

%.h %.c: %.y
	rm -f $*.h y.tab.h
	yacc -d -o$*.c $<
	[ -f y.tab.h ] && mv y.tab.h $*.h

%.c: %.l
	flex -o$*.c $<
