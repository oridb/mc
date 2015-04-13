#!/bin/sh

# this should be a bourne compatible shell script.
if test -f mbld/mbld; then
	./mbld/mbld $@
else
	./bootstrap.sh
fi
