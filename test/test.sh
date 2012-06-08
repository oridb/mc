#!/bin/bash
export PATH=.:$PATH
export MC=../8/8m
export ASOPT="-g"

function build {
    rm -f $1
    echo $MC $1.myr && \
    $MC $1.myr && \
    mv a.s $1.s && \
    cc $ASOPT -m32 -o $1 $1.s
}

function prints {
    if [ `./$1` -ne $2 ]; then
        echo "FAIL: $1"
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
        fi
    else
        echo "FAIL: $1"
    fi
}

for i in `awk '/^B/{print $2}' tests`; do
    build $i
done

for i in `awk '/^F/{print $2}' tests`; do
    build $i > /dev/null
    if [ $? -eq '1' ]; then
        echo "PASS: $i"
    else
        echo "FAIL: $i"
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
