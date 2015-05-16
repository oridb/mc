#!/bin/sh

if test `uname` = Plan9; then
    export MYR_MUSE=../muse/6.out
    export MYR_MC=../6/6.out
    export MYR_RT=../rt/_myrrt.$O
else
    export MYR_MUSE=../muse/muse
    export MYR_MC=../6/6m
    export MYR_RT=../rt/_myrrt.o
fi


./mbldwrap.sh
cp mbld/mbld xmbld
./xmbld clean

# The generated shell script should be a compatible bourne
# shell script.
bootscript=mk/bootstrap/bootstrap+`uname -s`-`uname -m`.sh
echo '#!/bin/sh' > $bootscript
echo 'pwd=`pwd`' >> $bootscript
./xmbld -Rnone | \
    sed "s:Entering directory '\\(.*\\)':	cd \$pwd/\\1:g" | \
    sed "s:Leaving directory.*:	cd \$pwd:g" | \
    sed "s:\\([a-zA-Z0-9_-]*\\)+.*:\`\$pwd/sysselect.sh \1\`:" | \
    grep '^	' | \
    sed 's/.*/echo &;&/' | \
    tee -a $bootscript
chmod +x $bootscript
rm ./xmbld
