#!/bin/bash

# We have no dependency handling yet, so this is done via
# a shell script. Also, we want to rebuild everything for
# testing purposes on every run as things stand.

export PATH=.:$PATH
export MC=../../8/8m
export MU=../../util/muse
export CC=cc
export ASOPT="-g"

function use {
    N=`basename $1 .myr`

    echo $MU $1 -o $N.use && \
    $MU $1 -o $N.use
}

function build {
    N=`basename $1 .myr`

    echo $MC $1 && \
    $MC $1 -I.
}

function assem {
    $CC $ASOPT -m32 -c $1
}

assem syscall.s
use sys.myr
build sys.myr
build hello.myr

$CC -m32 -o hello sys.o hello.o syscall.o
