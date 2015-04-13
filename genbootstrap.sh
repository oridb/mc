#!/usr/bin/env bash

export MYR_MUSE=../muse/muse
export MYR_MC=../6/6m

mbld clean
# The generated shell script should be a compatible bourne
# shell script.
echo '#!/bin/sh' > bootstrap.sh
echo 'pwd=`pwd`' >> bootstrap.sh
mbld | \
    sed "s:Entering directory '\\(.*\\)':\tcd \$pwd/\1:g" | \
    sed "s:Leaving directory.*:\tcd \$pwd:g" | \
    grep $'^\t' | \
    sed 's/.*/echo &\n&/' | \
    tee -a bootstrap.sh 
chmod +x bootstrap.sh
