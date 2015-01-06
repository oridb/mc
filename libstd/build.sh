#!/bin/bash

if [ -z "`which xmbld`" ]; then
    ../myrbuild/myrbuild -C$MYR_MC -M$MYR_MUSE $@
else
    mbld $@
fi

