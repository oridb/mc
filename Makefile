MYRLIB=bio
MYRSRC= \
	bio.myr
SUB=test

include config.mk
include mk/myr.mk

check: all
	make -C test check
