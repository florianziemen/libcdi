#!/bin/sh

autoreconf -fvi || exit $?
patch -p 1 --no-backup-if-mismatch --forward -r - -i patch/libtool.m4.arg_spaces.patch -d .
# The program 'patch' exits with exitcode=1 if the patch has already been applied.
# Consider this a normal scenario:
exitcode=$?; if test $exitcode -ne 0 && test $exitcode -ne 1; then exit $exitcode; fi

patch -p 1 --no-backup-if-mismatch --forward -r - -i patch/libtool.m4.nag_wrapper.patch -d .
exitcode=$?; if test $exitcode -ne 0 && test $exitcode -ne 1; then exit $exitcode; fi

autoconf -f || exit $?
# Reset libtool.m4 timestamps to avoid confusing make
touch -r m4/ltversion.m4 m4/libtool.m4 || exit $?

patch -p 1 --no-backup-if-mismatch --forward -r - -i patch/ltmain.sh.nag_pthread.patch -d .
exitcode=$?; if test $exitcode -ne 0 && test $exitcode -ne 1; then exit $exitcode; fi
