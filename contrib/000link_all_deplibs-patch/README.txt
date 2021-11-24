This patch is borrowed from the Debian project:
http://deb.debian.org/debian/pool/main/libt/libtool/libtool_2.4.6-15.debian.tar.xz

The comment for the patch in the aformentioned archive says:
## Do not link against deplibs.  This is not needed for shared libs
## on atleast ELF systems since those already know which libs they
## need themself.  This seems to break a few things and will be fixed
## in a better way in a future upstream version.
