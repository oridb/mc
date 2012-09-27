SUB = parse \
      mi \
      6 \
      muse \
      myrbuild \
      libstd

include mk/c.mk
include config.mk

check: all
	make -C test check
