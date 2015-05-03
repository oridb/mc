#!/bin/sh

export MYR_MUSE=../muse/muse
export MYR_MC=../6/6m
export MYR_RT=../rt/_myrrt.o

# this should be a bourne compatible shell script.
if test -f ./mbld/mbld; then
	./mbld/mbld $@
else
	./bootstrap.sh
fi
