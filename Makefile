SUB = parse \
      mi \
      6 \
      muse \
      rt \
      doc

EXTRA=buildmyr
EXTRACLEAN=cleanmyr
EXTRAINSTALL=installmyr
EXTRAUNINSTALL=uninstallmyr

include mk/c.mk
include config.mk

check: all
	./mbldwrap.sh test:runtest
	./mbldwrap.sh test

bench: all
	$(MAKE) -C bench bench

.PHONY: bootstrap
bootstrap: buildmyr
	cp mbld/mbld xmbld
	./genbootstrap.sh

buildmyr:
	./mbldwrap.sh

cleanmyr:
	./mbldwrap.sh clean

installmyr:
	./mbldwrap.sh install

uninstallmyr:
	./mbldwrap.sh uninstall
