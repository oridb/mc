#!/bin/bash

# We have no dependency handling yet, so this is done via
# a shell script. Also, we want to rebuild everything for
# testing purposes on every run as things stand.

export PATH=.:$PATH
export MC=../6/6m
export MU=../util/muse
export AS=as
export LD=ld
export ASOPT="-g"
case `uname` in
    Darwin) export SYS=osx;;
    Linux) export SYS=linux;;
esac

function use {
    for i in $@; do
        N=`basename $i .myr`
        N=${N%%-$SYS}

        echo $MU -o $N.use $i && \
        $MU -o $N.use $i
    done
}

function build {
    for i in $@; do
        echo $MC $i
        $MC -I. $i
    done
}

function assem {
    for i in $@; do
        N=`basename $i .s`

        echo $AS  -o $N.o $ASOPT $i
        $AS -o $N.o $ASOPT $i
    done
}

# Library source.
ASM="syscall-$SYS.s start-$SYS.s"
MYR="types.myr \
    sys-$SYS.myr \
    die.myr \
    varargs.myr \
    alloc.myr\
    utf.myr \
    fmt.myr \
    rand.myr \
    chartype.myr"

OBJ="$(echo $ASM | sed 's/\.s/.o /g') $(echo $MYR | sed 's/\.myr/.o /g')"
USE="$(echo $MYR | sed 's/\.myr/.use /g' | sed "s/-$SYS//g")"
if [ "$1" = "clean" ]; then
    echo rm -f $OBJ test libstd.a
    rm -f $OBJ test libstd.a
else
    assem $ASM
    use $MYR
    build $MYR

    echo $MU -mo std $USE
    $MU -mo std $USE
    echo ar -rcs libstd.a $OBJ
    ar -rcs libstd.a $OBJ

    # build test program
    build test.myr
    COMP="$LD -o test test.o -L. -lstd"
    echo $COMP
    $COMP
fi
