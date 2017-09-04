#!/bin/sh

# this should be a bourne compatible shell script.
export PATH=`pwd`:`pwd`/6:`pwd`/muse:$PATH
if test `uname` = Plan9; then
	O=6
	export MYR_MUSE=`pwd`/muse/$O.out
	export MYR_MC=`pwd`/6/$O.out
	export MYR_RT=`pwd`/rt/_myrrt.$O
else
	export MYR_MUSE=muse
	export MYR_MC=6m
	export MYR_RT=`pwd`/rt/_myrrt.o
fi
BOOT="./mk/bootstrap/bootstrap+`uname -s`-`uname -m`.sh"

MBLD=mbld
if [ -f obj/mbld/mbld ]; then
	MBLD=obj/mbld/mbld
elif [ -f mbld/mbld ]; then
	MBLD=mbld/mbld
fi

bootstrap() {
	($BOOT && mbld/mbld $@) || \
		(echo "Unable to run mbld $@; have you build successfully"; exit 1)
}

$MBLD $@ || bootstrap $@
