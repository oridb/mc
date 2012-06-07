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
    ./$1
    if [ $? -eq $2 ]; then
        echo "PASS: $1"
    else
        echo "FAIL: $1"
    fi
}

for i in `awk '{print $1}' tests`; do
    build $i
done

export IFS='
'
for i in `cat tests`; do
    tst=`echo $i | awk '{print $1}'`
    type=`echo $i | awk '{print $2}'`
    val=`echo $i | awk '{print $3}'`
    case $type in
        E) exitswith $tst $val ;;
        P) prints $tst $val ;;
    esac
done
