MYRLIB=bio
MYRSRC= \
	bio.myr

include config.mk
include mk/myr.mk

check: all
	make -C test check
