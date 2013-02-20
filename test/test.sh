#!/bin/bash
export PATH=.:$PATH
export MC=../6/6m
export MU=../muse/muse
export AS=AS
export LD=ld
NFAILURES=0

function use {
    rm -f $1 $1.o $1.s $1.use
    echo "	"$MU -I ../libstd -o $1.use $1.myr && \
    $MU $1.myr -o $1.use
}

function build {
    rm -f $1 $1.o $1.s $1.use
    ../myrbuild/myrbuild -b $1 $1.myr -C../6/6m -M../muse/muse -I../libstd
}

function prints {
    if [ "`./$1`" != "$2" ]; then
        echo "FAIL: $1"
        FAILED="$FAILED $1"
        NFAILED=$[$NFAILED + 1]
    else
        echo "PASS: $1"
    fi
}

function exitswith {
    if [ -e $1 ]; then
        ./$1
        if [ $? -eq $2 ]; then
            echo "PASS: $1"
        else
            echo "FAIL: $1"
            FAILED="$FAILED $1"
            NFAILED=$[$NFAILED + 1]
        fi
    else
        echo "FAIL: $1"
        FAILED="$FAILED $1"
        NFAILED=$[$NFAILED + 1]
    fi
}

# When broken, these tests have taken down machines by
# using all available resources. This should be disallowed.
ulimit -c unlimited # core size
ulimit -d 16382     # data segment: 16m
ulimit -f 16382     # file size
ulimit -m 32768     # total memory
ulimit -s 8192      # 8 meg stack
ulimit -t 30        # 30 second CPU time
ulimit -v 32768     # virtual memory

for i in `awk '/^B/{print $2}' tests`; do
    build $i
done

for i in `awk '/^U/{print $2}' tests`; do
    use $i
done

for i in `awk '/^F/{print $2}' tests`; do
    (build $i) > /dev/null
    if [ $? -eq '1' ]; then
        echo "PASS: $i"
    else
        echo "FAIL: $i"
        FAILED="$FAILED $i"
        NFAILED=$[$NFAILED + 1]
    fi
done

export IFS='
'
for i in `awk '/^B/{print $0}' tests`; do
    tst=`echo $i | awk '{print $2}'`
    type=`echo $i | awk '{print $3}'`
    val=`echo $i | awk '{print $4}'`
    case $type in
        E) exitswith $tst $val ;;
        P) prints $tst $val ;;
    esac
done
if [ -z "$NFAILED" ]; then
    echo "SUCCESS"
else
    echo "FAILURES ($NFAILED)": $FAILED
fi
