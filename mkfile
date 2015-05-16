</$objtype/mkfile

SUB = parse \
      mi \
      6 \
      muse \
      rt \
      doc

all:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS
	}
	ape/psh mbldwrap.sh

nuke:V: $SUB
	rm -f config.h
	rm -f config.mk
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS nuke
	}
	ape/psh mbldwrap.sh clean

clean:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS clean
	}
	ape/psh mbldwrap.sh clean

install:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS install
	}
	ape/psh mbldwrap.sh install

bootstrap:V:
	ape/psh genbootstrap.sh

uninstall:V: $SUB config.h
	for(dir in $SUB)@{
		cd $dir
		mk $MKFLAGS uninstall
	}
	ape/psh mbldwrap.sh uninstall

check:V: all
	cd test
	mk check

config.h:
	echo '#define Instroot "/amd64"' > config.h
	echo '#define Asmcmd {"6a", "-o", NULL}' >> config.h
	echo '#define Arcmd {"ar", "ru", NULL}' >> config.h
	echo '#define Ldcmd {"6l", "-l", "-o", NULL}' >> config.h
	echo '#define Symprefix ""' >> config.h
	echo '#define Defaultasm Plan9' >> config.h
	echo '#define Objsuffix ".6"' >> config.h

