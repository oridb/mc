.SUFFIXES:

%.h %.c: %.y
	yacc -d -o$*.c $<

%.c: %.l
	flex -o$*.c $<
