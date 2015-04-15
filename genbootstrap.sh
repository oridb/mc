#!/usr/bin/env bash

export MYR_MUSE=../muse/muse
export MYR_MC=../6/6m
export MYR_RT=../rt/_myrrt.o
mbld clean
# The generated shell script should be a compatible bourne
# shell script.
bootscript=bootstrap+`uname -s`-`uname -m`
echo '#!/bin/sh' > bootstrap.sh
echo 'pwd=`pwd`' >> bootstrap.sh
mbld -R. | \
    sed "s:Entering directory '\\(.*\\)':\tcd \$pwd/\\1:g" | \
    sed "s:Leaving directory.*:\tcd \$pwd:g" | \
    sed "s:\\([a-zA-Z0-9_-]*\\)+.*:\`\$pwd/sysselect.sh \1\`:" | \
    grep $'^\t' | \
    sed 's/.*/echo &\n&/' | \
    tee -a bootstrap.sh 
chmod +x bootstrap.sh
