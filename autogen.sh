#!/bin/sh

autoreconf -fvi || exit $?
patch --forward --no-backup-if-mismatch -p1 -r - -i patch/libtool.m4.nag_wrapper.patch
# The program 'patch' exits with exitcode=1 if the patch has already been applied.
# Consider this a normal scenario:
exitcode=$?; test $exitcode -ne 0 && test $exitcode -ne 1 && exit $exitcode

patch --forward --no-backup-if-mismatch -p1 -r - -i patch/libtool.m4.arg_spaces.patch
exitcode=$?; test $exitcode -ne 0 && test $exitcode -ne 1 && exit $exitcode

# Rebuild configure if you need to patch M4 macros:
autoconf -f || exit $?

# Reset libtool.m4 timestamps to avoid confusing make:
touch -r m4/ltversion.m4 m4/libtool.m4 || exit $?

patch --forward --no-backup-if-mismatch -p1 -r - -i patch/ltmain.sh.nag_pthread.patch
exitcode=$?; test $exitcode -ne 0 && test $exitcode -ne 1 && exit $exitcode

# All went fine since we have not exited before:
exit 0

