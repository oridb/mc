NECFLAGS = $(subst -Werror,,$(subst -Wall,,$(CFLAGS)))

%.o: %.y .deps
	yacc -d -o$*.c $<
	$(CC) -c $(NECFLAGS) $*.c

%.o: %.l .deps
	flex -o$*.c $<
	$(CC) -c $(NECFLAGS) $*.c
