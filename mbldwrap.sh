#!/bin/sh

# this should be a bourne compatible shell script.
if test -f ./mbld/mbld; then
	./mbld/mbld $@
elif test `uname` = Plan9; then
	echo "PLAN 9 BOOTSTRAP"
	export MYR_MUSE=../muse/$O.out
	export MYR_MC=../$O/$O.out
	export MYR_RT=../rt/_myrrt.$O
	./mk/bootstrap/bootstrap+`uname -s`-`uname -m`.sh
else
	echo "POSIX BOOTSTRAP"
	export MYR_MUSE=../muse/muse
	export MYR_MC=../6/6m
	export MYR_RT=../rt/_myrrt.o
	./mk/bootstrap/bootstrap+`uname -s`-`uname -m`.sh
fi
exit 0
