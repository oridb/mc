#!/bin/bash

# We have no dependency handling yet, so this is done via
# a shell script. Also, we want to rebuild everything for
# testing purposes on every run as things stand.

export PATH=.:$PATH
export MC=8m
export MU=muse
export CC=cc
export ASOPT="-g"
case `uname` in
    Darwin) export SYS=osx;;
    Linux) export SYS=linux;;
esac

function use {
    for i in $@; do
        N=`basename $i .myr`

        echo $MU -o $N.use $i && \
        $MU -o $N.use $i
    done
}

function build {
    for i in $@; do
        N=`basename $i .myr`

        echo $MC $i && \
            $MC -I. $i
    done
}

function assem {
    for i in $@; do
        $CC $ASOPT -m32 -c $i
    done
}

ASM=syscall-$SYS.s
# Myrddin source must be specified in dependency order
MYR="sys-$SYS.myr \
    types.myr \
    alloc.myr \
    die.myr \
    hello.myr"
OBJ="$(echo $ASM | sed 's/\.s/.o /g') $(echo $MYR | sed 's/\.myr/.o /g')"
assem $ASM
use $MYR
build $MYR

echo $CC -m32 -o hello $OBJ
$CC -m32 -o hello $OBJ

