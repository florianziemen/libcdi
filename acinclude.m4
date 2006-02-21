dnl
dnl UD_CHECK_CFINT
dnl
dnl Check C / Fortran interface
dnl
define(UD_CHECK_CFINT,
[
   AC_MSG_CHECKING([whether the C / Fortran interface works])
   changequote(,)
   changequote([,])
   echo '#include "XXX"' | sed -e "s-XXX-$1-" > conftest.c
   rm -f conftest.o
   $CC -c $CFLAGS $CFINT  conftest.c >& conftestout
   rm -f conftest.c conftestout
   if test -f conftest.o ; then
       AC_MSG_RESULT(yes)
       AC_DEFINE(HAVE_CF_INTERFACE, [1], [Define if C / Fortran interface cfortran.h works])
   else
       AC_MSG_RESULT(no)
   fi
   rm -f conftest.o
])
