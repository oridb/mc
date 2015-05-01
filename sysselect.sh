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
elif test $uname_arch = "amd64"; then
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
			found=true
			echo $1+$platform.$suffix
			exit
		fi
	done
	if test "x$found" = "x"; then
		if test -f $1.$suffix; then
			found=true
			echo $1.$suffix
			exit
		fi
	fi
done

