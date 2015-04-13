SUB = parse \
      mi \
      6 \
      muse \
      rt \
      doc

EXTRA=buildmyr
EXTRACLEAN=cleanmyr

include mk/c.mk
include config.mk

check: all
	$(MAKE) -C test check && \
	$(MAKE) -C libstd check

bench: all
	$(MAKE) -C bench bench

bootstrap.sh: buildmyr
	./genbootstrap.sh

buildmyr:
	./mbldwrap.sh

cleanmyr:
	./mbldwrap.sh clean
