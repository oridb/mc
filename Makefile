SUB = parse \
      mi \
      6 \
      util \
      libmyr

-include config.mk
include mk/c.mk

check: all
	make -C test check
