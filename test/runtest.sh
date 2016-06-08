#!/bin/sh
#set -x
export PATH=.:$PATH
export MC=../6/6m
export MU=../muse/muse
export AS=AS
export LD=ld
ARGS=$*
NFAILURES=0
NPASSES=0

build() {
	rm -f $1 $1.o $1.s $1.use
	echo mbld -b $1 -C../6/6m -M../muse/muse -I../lib/std -I../lib/sys -r../rt/_myrrt.o $1.myr
	../mbld/mbld -b $1 -C../6/6m -M../muse/muse -I../lib/std -I../lib/sys -r../rt/_myrrt.o $1.myr
}

pass() {
	PASSED="$PASSED $1"
	NPASSED=$(($NPASSED + 1))
	echo "!}>> ok"
}

fail() {
	FAILED="$FAILED $1"
	NFAILED=$(($NFAILED + 1))
	echo "!}>> fail $1"
}

expectstatus() {
	./$1 $3
	if [ $? -eq $2 ]; then
		pass $1
		return
	else
		fail $1
	fi
}

expectprint() {
	if [ "`./$1 $3`" != "$2" ]; then
		fail $1
	else
		pass $1
	fi
}

expectcompare() {
	if [ x"" !=  x"$TMPDIR" ]; then 
		t=$TMPDIR/myrtest-$1-$RANDOM
	else
		t=/tmp/myrtest-$1-$RANDOM
	fi
	./$1 $3 > $t
	if cmp $t data/$1-expected; then
		pass $1
	else
		fail $1
	fi
	rm -f $t
}

expectfcompare() {
	./$1 $3
	if cmp data/$1-expected $2; then
		pass $1
	else
		fail $1
	fi
}

# Should build and run
B() {
	test="$1"; shift
	type="$1"; shift
	res="$1"; shift
	if [ $# > 0 ]; then
		args="$1"; shift
	fi
	echo "test $test <<{!"
	build $test
	case $type in
	"E")  expectstatus "$test" "$res";;
	"P")  expectprint "$test" "$res";;
	"C")  expectcompare "$test" "$res";;
	"F")  expectfcompare "$test" "$res" "$args";;
	esac
}

# Should fail
F() {
	echo "test $1 <<{!"
	(build $1) > /dev/null 2>1
	if [ $? -eq '1' ]; then
		pass $1
	else
		fail $1
	fi
}

echo "MTEST $(egrep '^[BF]' tests | wc -l)"
. tests

echo "PASSED ($NPASSED): $PASSED"
if [ -z "$NFAILED" ]; then
	echo "SUCCESS"
else
	echo "FAILURES ($NFAILED): $FAILED"
	exit 1
fi
