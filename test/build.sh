#!/bin/bash

MC=../8/8m

function build {
    $MC $1.myr && \
    mv a.s $1.s && \
    cc -m32 -o $1 $1.s
}

build main
build add
build struct
