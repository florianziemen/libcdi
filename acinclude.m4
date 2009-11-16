dnl
dnl UD_CHECK_CFINT
dnl
dnl Check C / Fortran interface
dnl
AC_DEFUN([UD_CHECK_CFINT],
  [
   AC_CACHE_CHECK([whether the C / Fortran interface works],[ud_c_cfint],
     [AC_LANG_PUSH([C])
      ud_save_CFLAGS=$CFLAGS
      CFLAGS="$CFLAGS $CFINT"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include "$1"],[])],
        [ud_c_cfint=yes],
        [ud_c_cfint=no])
      CFLAGS=$ud_save_CFLAGS
      AC_LANG_POP([C])])
   AS_IF([test x$ud_c_cfint = xyes],
     [AC_DEFINE(HAVE_CF_INTERFACE, [1],
        [Define if C / Fortran interface cfortran.h works])])
  ])
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
