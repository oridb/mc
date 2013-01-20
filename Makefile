SUB = parse \
      mi \
      6 \
      muse \
      myrbuild \
      libstd \
      doc

include mk/c.mk
include config.mk

check: all
	make -C test check
