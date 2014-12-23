</$objtype/mkfile

SUB = parse \
      mi \
      6 \
      muse \
      myrbuild \
      rt \
      libstd \
      doc

all:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS
	}
nuke:V: $SUB
	rm -f config.h
	rm -f config.mk
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS nuke
	}

clean:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS clean
	}

install:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS install
	}

uninstall:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS
	}

config.h:
	echo '#define Instroot "'/'"' > config.h
	echo '#define Asmcmd {"6a", "-o", NULL}' >> config.h
	echo '#define Linkcmd {"ar", "ru", NULL}' >> config.h
	echo '#define Symprefix "_"' >> config.h
	echo '#define Defaultasm Plan9' >> config.h

