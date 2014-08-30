MYRLIB=date
MYRSRC= \
	date.myr \
	fmt.myr \
	types.myr \
	zoneinfo.myr \

MYRFLAG=-I .

include config.mk
include mk/myr.mk

check: all
	make -C test check
