NECFLAGS = $(subst -Werror,,$(subst -Wall,,$(CFLAGS)))

%.c: %.y
	yacc -d -o$*.c $<

%.c: %.l
	flex -o$*.c $<
