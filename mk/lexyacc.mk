NECFLAGS = $(subst -Werror,,$(subst -Wall,,$(CFLAGS)))

%.o: %.y
	yacc -dt -o$*.c $<
	$(CC) -c $(NECFLAGS) $*.c

%.o: %.l
	flex -o$*.c $<
	$(CC) -c $(NECFLAGS) $*.c
