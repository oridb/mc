#!/usr/bin/env bash
export PATH=.:$PATH
export MC=../6/6m
export MU=../muse/muse
export AS=AS
export LD=ld
ARGS=$*
NFAILURES=0
NPASSES=0

function build {
    rm -f $1 $1.o $1.s $1.use
    mbld -b $1 -C../6/6m -M../muse/muse -I../lib/std -I../lib/sys -r../rt/_myrrt.o $1.myr
}

function pass {
    PASSED="$PASSED $1"
    NPASSED=$[$NPASSED + 1]
}

function fail {
    echo "FAIL: $1"
    FAILED="$FAILED $1"
    NFAILED=$[$NFAILED + 1]
}

function expectstatus {
    ./$1 $3
    if [ $? -eq $2 ]; then
        pass $1
        return
    else
        fail $1
    fi
}

function expectprint {
    if [ "`./$1 $3`" != "$2" ]; then
        fail $1
    else
        pass $1
    fi
}


function expectcompare {
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

function expectfcompare {
    ./$1 $3
    if cmp data/$1-expected $2; then
        pass $1
    else
        fail $1
    fi
}

# Should build and run
function B {
    test="$1"; shift
    type="$1"; shift
    res="$1"; shift
    if [ $# > 0 ]; then
        args="$1"; shift
    fi
    build $test
    case $type in
    "E")  expectstatus "$test" "$res";;
    "P")  expectprint "$test" "$res";;
    "C")  expectcompare "$test" "$res";;
    "F")  expectfcompare "$test" "$res" "$args";;
    esac
}

# Should fail
function F {
    (build $1) > /dev/null
    if [ $? -eq '1' ]; then
        pass $1
    else
        fail $1
    fi
}

source tests

echo "PASSED ($NPASSED): $PASSED"
if [ -z "$NFAILED" ]; then
    echo "SUCCESS"
else
    echo "FAILURES ($NFAILED): $FAILED"
    exit 1
fi
