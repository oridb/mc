SUB = parse \
      mi \
      6 \
      muse \
      myrbuild \
      rt \
      libstd \
      doc

include mk/c.mk
include config.mk

check: all
	$(MAKE) -C libstd check
	$(MAKE) -C test check

bench: all
	$(MAKE) -C bench bench
