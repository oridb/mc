.SUFFIXES:

NECFLAGS = $(subst -Werror,,$(subst -Wall,,$(CFLAGS)))

%.h %.c: %.y
	yacc -d -o$*.c $<

%.c: %.l
	flex -o$*.c $<
