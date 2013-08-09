SUB = parse \
      mi \
      6 \
      muse \
      myrbuild \
      myrtypes \
      libstd \
      doc

include mk/c.mk
include config.mk

check: all
	make -C test check
