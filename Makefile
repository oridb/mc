SUB = parse \
      8

-include config.mk
include mk/c.mk

check:
	make -C test check
