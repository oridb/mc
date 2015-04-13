#!/bin/sh

uname_sys=`uname -s`
posixy="posixy"
if test $uname_sys = "Linux"; then
	sys="linux"
elif test $uname_sys = "Darwin"; then
	sys="osx"
elif test $uname_sys = "FreeBSD"; then
	sys="freebsd"
elif test $uname_sys = "Plan9"; then
	sys="plan9"
        posixy="NOTPOSIXY"
fi

uname_arch=`uname -m`
if test $uname_arch = "x86_64"; then
	arch="x64"
fi

# check for system prefixes on .myr src
if test -f "$1+$sys-$arch.myr"; then
	echo "$1+$sys-$arch.myr"
elif test -f $1+$posixy-$arch.myr; then
	echo $1+$posixy-$arch.myr
elif test -f "$1+$sys.myr"; then
	echo "$1+$sys.myr"
elif test -f $1+$posixy.myr; then
	echo $1+$posixy.myr
elif test -f $1+$arch; then
	echo $1+$arch.myr
# check for system prefixes on .s src
elif test -f "$1+$sys-$arch.s"; then
	echo "$1+$sys-$arch.s"
elif test -f $1+$posixy-$arch.s; then
	echo $1+$posixy-$arch.s
elif test -f "$1+$sys.s"; then
	echo "$1+$sys.s"
elif test -f $1+$posixy.s; then
	echo $1+$posixy.s
elif test -f $1+$arch; then
	echo $1+$arch.s
fi
