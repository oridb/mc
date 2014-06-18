#!/bin/bash
export PATH=.:$PATH
export MC=../6/6m
export MU=../muse/muse
export AS=AS
export LD=ld
ARGS=$*
NFAILURES=0
NPASSES=0

function use {
    rm -f $1 $1.o $1.s $1.use
    echo "	"$MU -I ../libstd -o $1.use $1.myr && \
    $MU $1.myr -o $1.use
}

function build {
    rm -f $1 $1.o $1.s $1.use
    ../myrbuild/myrbuild -b $1 -C../6/6m -M../muse/muse -I../libstd $1.myr
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

function shouldskip {
  if [ -z $ARGS ]; then
      return 1
  fi

  for i in $ARGS; do
      if [ $i = $1 ]; then
          return 1
      fi
  done
  return 0
}


# Should build and run
function B {
    if shouldskip $1; then
        return
    fi

    test="$1"; shift
    type="$1"; shift
    res="$1"; shift
    if [ $# > 0 ]; then
        args="$1"; shift
    fi
    build $test
    case $type in
    "E")  expectstatus "$test" "$res" "$input";;
    "P")  expectprint "$test" "$res" "$input";;
    "C")  expectcompare "$test" "$res" "$input";;
    "F")  expectfcompare "$test" "$res" "$args";;
    esac
}

# Should fail
function F {
    if shouldskip $1; then
        return
    fi
    (build $1) > /dev/null
    if [ $? -eq '1' ]; then
        pass $1
    else
        fail $1
    fi
}

# Should generate a usefile
function U {
    return
}

source tests

echo "PASSED ($NPASSED): $PASSED"
if [ -z "$NFAILED" ]; then
    echo "SUCCESS"
else
    echo "FAILURES ($NFAILED): $FAILED"
fi
