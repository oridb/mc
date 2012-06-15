SUB = parse \
      opt \
      8 \
      util

-include config.mk
include mk/c.mk

check: all
	make -C test check
