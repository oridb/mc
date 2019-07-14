#!/bin/sh

if [ $# -ne 2 ]; then
	echo usage: $0 sysname arch
	exit 1
fi

mastergen() {
	sed -e '
	s/\$//g
	:join
		/\\$/{a\

		N
		s/\\\n//
		b join
		}
	2,${
		/^#/!s/\([{}()*,]\)/ \1 /g
	}
	' < syscalls+$1.master | awk -f gencalls.awk $1 $2 > sys+$1-$2.myr
}

hdrgen() {
	sed -e '
	s/\$//g
	:join
		/[,(\\]$/{a\

		N
		s/\\*\n//
		b join
		}
	2,${
		/^#/!s/\([{}()*,]\)/ \1 /g
	}
	' < syscalls+$1.h | tee sysjoined.txt | awk -f gencalls.awk $1 $2 > sys+$1-$2.myr
}

#rm -f have.txt want.txt gentypes+$1-$2.frag
touch have.txt

if [ `uname` = Linux ]; then
	hdrgen $1 $2
	python ./c2myr.py $1 $2
	hdrgen $1 $2
else
	mastergen $1 $2
	python ./c2myr.py $1 $2
	mastergen $1 $2
fi
