#!/bin/sh

for arg in "$@"; do
  case ${arg} in
    --help | -h)
      cat <<EOF
Usage: $0 [OPTION]

Generate the configure script and other Autotools files.

Options:
  -h, --help              display this help and exit
  -d, --download          download the most recent versions
                          of \`config.guess' and \`config.sub'

EOF
    exit 0
    ;;
    --download | -d)
      download=yes
    ;;
    *)
      echo "ERROR: unrecognized option '${arg}': try \`$0 --help' for more information" >&2
      exit 2
    ;;
  esac
done

script_dir=`echo "$0" | sed 's@[^/]*$@@'`
echo "Running autoreconf in $script_dir"
(unset CDPATH) >/dev/null 2>&1 && unset CDPATH
cd "$script_dir"

autoreconf -fvi || exit $?

# The following is not needed as long as we do not patch libtool.m4:

# # Patch libtool.m4:
# for patch in \
#   ; do
#     echo "applying $patch"
#     patch -p 1 --no-backup-if-mismatch --forward -r - -i $patch -d .
#     # The program 'patch' exits with exitcode=1 if the patch has already been applied.
#     # Consider this a normal scenario:
#     exitcode=$?; if test $exitcode -ne 0 && test $exitcode -ne 1; then exit $exitcode; fi
# done
# autoconf -f || exit $?
# # Reset libtool.m4 timestamps to avoid confusing make:
# touch -r m4/ltversion.m4 m4/libtool.m4 || exit $?

# Patch the rest of the files:
for patch in \
  contrib/00nagfor-libtool-patch/nagfor-libtool-2.4.6.patch \
  contrib/01aix-deplib-rpath-patch/aix-deplib-libtool.patch \
  contrib/03ltmain-ld-groups-patch/ltmain-ld-groups-libtool-2.4.6.patch \
  contrib/04ltmain-xlinker-patch/ltmain-xlinker-patch.patch \
  contrib/05macos-nagfor-patch/macos-nagfor.patch \
  contrib/06ltmain_nag_pthread-patch/ltmain_nag_pthread.patch \
  contrib/07ltmain-early-xcompile-patch/ltmain-early-xcompile-libtool-2.4.6.patch \
  contrib/08ltmain-parallel-printf-patch/ltmain-parallel-printf.patch \
  ; do
    echo "applying $patch"
    patch -p 1 --no-backup-if-mismatch --forward -r - -i $patch -d .
    exitcode=$?; if test $exitcode -ne 0 && test $exitcode -ne 1; then exit $exitcode; fi
done

if test x"$download" = xyes; then
  url_base='https://git.savannah.gnu.org/cgit/config.git/plain'

  for file in config.guess config.sub; do
    echo "downloading $file"
    src="$url_base/$file"; dst="config/$file"
    wget -o /dev/null -O "$dst" "$src" || {
      echo "ERROR: failed to download from $src" >&2
      exit 1; }
  done
fi
