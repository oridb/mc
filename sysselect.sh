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
for suffix in myr s; do
	for platform in $posixy-$sys-$arch \
		$posixy-$sys \
		$posixy-$arch \
		$posixy \
		$sys-$arch \
		$sys \
		$arch
	do
		if test -f $1+$platform.$suffix; then
			echo $1+$platform.$suffix 
		fi
	done
done

