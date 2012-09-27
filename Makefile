SUB = parse \
      mi \
      6 \
      util \
      libstd

include mk/c.mk
include config.mk

check: all
	make -C test check
