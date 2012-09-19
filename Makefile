SUB = parse \
      mi \
      6 \
      util \
      libmyr

include mk/c.mk
include config.mk

check: all
	make -C test check
