#!/bin/sh

export PATH=`pwd`/6:`pwd`/muse:$PATH
# this should be a bourne compatible shell script.
if test `uname` = Plan9; then
	export MYR_MUSE=$O.out
	export MYR_MC=$O.out
	export MYR_RT=`pwd`/rt/_myrrt.$O
	BOOT="./mk/bootstrap/bootstrap+`uname -s`-`uname -m`.sh"
else
	export MYR_MUSE=muse
	export MYR_MC=6m
	export MYR_RT=`pwd`/rt/_myrrt.o
	BOOT="./mk/bootstrap/bootstrap+`uname -s`-`uname -m`.sh"
fi

if [ -z "$@" ]; then
    mbld || ./mbld/mbld || $BOOT
else
    mbld $@ || ./mbld/mbld $@ || \
        (echo "Unable to run mbld $@; have you build successfully"; false)
fi
