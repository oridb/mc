NECFLAGS = $(subst -Werror,,$(subst -Wall,,$(CFLAGS)))

%.c: %.y
	yacc -dt -o$*.c $<

%.c: %.l
	flex -o$*.c $<
