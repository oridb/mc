#!/bin/bash

MC=../8/8m
ASOPT="-g"

function build {
    rm $1
    echo $MC $1.myr && \
    $MC $1.myr && \
    mv a.s $1.s && \
    cc $ASOPT -m32 -o $1 $1.s
}

build main
build add
build struct_oneval
build struct
build array
build call
build loop
build fib
build slice

exit 0
