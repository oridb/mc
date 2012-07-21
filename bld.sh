#!/bin/bash

# We have no dependency handling yet, so this is done via
# a shell script. Also, we want to rebuild everything for
# testing purposes on every run as things stand.

export PATH=.:$PATH
export MC=8m
export MU=muse
export CC=cc
export ASOPT="-g"

function use {
    N=`basename $1 .myr`

    echo $MU -o $N.use $1 && \
    $MU -o $N.use $1
}

function build {
    N=`basename $1 .myr`

    echo $MC $1 && \
    $MC -I. $1
}

function assem {
    $CC $ASOPT -m32 -c $1
}

use sys.myr
use types.myr 
assem syscall.s
build sys.myr
build hello.myr
build alloc.myr

$CC -m32 -o hello sys.o hello.o syscall.o
