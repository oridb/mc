#!/bin/sh
#set -x
export PATH=.:$PATH
export MYR_MC=../6/6m
export MYR_MUSE=../muse/muse
ARGS=$*
NFAILURES=0
NPASSES=0

build() {
	rm -f $1 $1.o $1.s $1.use
	../obj/mbld/mbld -Bnone -o '' -b $1 -I../obj/lib/std -I../obj/lib/sys -I../obj/lib/regex -r../rt/_myrrt.o $1.myr
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

belongto() {
	elem="$1"; shift
	subset="$1"; shift

	IFS=','
	for v in $subset; do
		if [ "$elem" = "$v" ]; then
			return 0
		fi
	done
	return 1
}

# Should build and run
B() {
	test="$1"; shift
	type="$1"; shift

	if [ -n "$MTEST_SUBSET" ] && ! belongto "$test" "$MTEST_SUBSET"; then
		return 1
	fi

	if [ $# -gt 0 ]; then
		res="$1"; shift
	fi
	if [ $# -gt 0 ]; then
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
	if [ -n "$MTEST_SUBSET" ] && ! belongto "$test" "$MTEST_SUBSET"; then
		return 1
	fi

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
