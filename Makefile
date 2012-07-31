SUB = parse \
      opt \
      6 \
      util

-include config.mk
include mk/c.mk

check: all
	make -C test check
