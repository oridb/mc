SUB = parse \
      mi \
      util \
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

.PHONY: bench
bench:
	mbld -tbench
	mbld -tbench bench:benchit

.PHONY: bootstrap
bootstrap: buildmyr
	cp mbld/mbld xmbld
	./genbootstrap.sh

buildmyr: subdirs
	./mbldwrap.sh

cleanmyr:
	./mbldwrap.sh clean

installmyr:
	./mbldwrap.sh install

uninstallmyr:
	./mbldwrap.sh uninstall
