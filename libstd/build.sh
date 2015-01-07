#!/bin/bash

if [ -z "`which xmbld`" ]; then
    ../myrbuild/myrbuild -I. -C$MYR_MC -M$MYR_MUSE $@
else
    mbld $@
fi

