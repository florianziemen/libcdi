#!/bin/sh

autoreconf -fvi || exit $?
patch -p 1 -i patch/ltmain_sh.patch -d .
