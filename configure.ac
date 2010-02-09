#  Process this file with autoconf to produce a configure script.

AC_INIT(cdi, 1.4.3, Uwe.Schulzweida@zmaw.de)

echo "configuring ${PACKAGE_NAME} ${PACKAGE_VERSION}"

CONFIG_ABORT=yes

AC_ARG_VAR(CFINT,  [C/Fortran interface preprocessor flag, e.g. -Df2cFortran]) 
AC_ARG_VAR(AR,     [AR archive command]) 

AC_CONFIG_AUX_DIR(config)
AC_CANONICAL_TARGET

AM_CONFIG_HEADER(src/config.h)
AM_INIT_AUTOMAKE

#  Checks for programs.
AC_PROG_CC([gcc c99])
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_F77
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_CHECK_PROGS(AR, ar, [ar])
AC_CHECK_PROG(SED,sed,sed,false)
AC_CHECK_PROG(GREP,grep,grep,false)

#  Check large file support on 32 bit systems
AC_SYS_LARGEFILE

#  Checks for library functions.
AC_FUNC_MMAP

#  Checks for library malloc.
AC_CHECK_LIB(malloc, mallinfo)

# Checks for structures.
AC_CHECK_MEMBERS([struct stat.st_blksize])

# Checks for the availability of ANSI-C99 functions
AC_CHECK_FUNCS(isnan)

#  ----------------------------------------------------------------------
#  Check for math library
#
AC_CHECK_LIB(m, floor)

#  ----------------------------------------------------------------------
#  Compile application with ZLIB library
#
AC_MSG_CHECKING([whether a location for the ZLIB library was specified])
AC_ARG_WITH([zlib],
            [AS_HELP_STRING([--with-zlib=<directory>],
                            [Specify location of ZLIB library. ZLIB is required for HDF5. Configure will expect to find subdirs include and lib.])],
            [ZLIBDIR=$with_zlib])
if test -z "$ZLIBDIR" ; then
  AC_MSG_RESULT([no])
else
  AC_MSG_RESULT([$ZLIBDIR])
  LDFLAGS="$LDFLAGS -L$ZLIBDIR/lib"
  CPPFLAGS="$CPPFLAGS -I$ZLIBDIR/include"
#  AC_CHECK_LIB(z, xxx, , LDFLAGS="$LDFLAGS")
# AC_SUBST([ZLIBDIR], [$ZLIBDIR])
fi

#  ----------------------------------------------------------------------
#  Compile application with SZLIB library
#
AC_MSG_CHECKING([whether a location for the SZLIB library was specified])
AC_ARG_WITH([szlib],
            [AS_HELP_STRING([--with-szlib=<directory>],
                            [Specify location of SZLIB library. You must specify its location if HDF5 was built with SZLIB. Configure will expect to find subdirs include and lib.])],
            [SZLIBDIR=$with_szlib])
if test -z "$SZLIBDIR" ; then
  AC_MSG_RESULT([no])
else
  AC_MSG_RESULT([$SZLIBDIR])
  LDFLAGS="$LDFLAGS -L$SZLIBDIR/lib"
  CPPFLAGS="$CPPFLAGS -I$SZLIBDIR/include"
  AC_CHECK_LIB(sz, SZ_BufftoBuffCompress, , LDFLAGS="$LDFLAGS")
# AC_SUBST([SZLIBDIR], [$SZLIBDIR])
fi

#  ----------------------------------------------------------------------
#  Compile application with JASPER library
#
AC_MSG_CHECKING([whether a location for the JASPER library was specified])
AC_ARG_WITH([jasper],
            [AS_HELP_STRING([--with-jasper=<directory>],
                            [Specify location of JASPER library. You must specify its location if GRIB_API was built with JASPER. Configure will expect to find subdirs include and lib.])],
            [JASPERDIR=$with_jasper])
if test -z "$JASPERDIR" ; then
  AC_MSG_RESULT([no])
else
  AC_MSG_RESULT([$JASPERDIR])
  LDFLAGS="$LDFLAGS -L$JASPERDIR/lib"
  CPPFLAGS="$CPPFLAGS -I$JASPERDIR/include"
  AC_CHECK_LIB(jasper, jas_init, , LDFLAGS="$LDFLAGS")
# AC_SUBST([JASPERDIR], [$JASPERDIR])
fi

#  ----------------------------------------------------------------------
#  Compile application with GRIB_API library
#
AC_MSG_CHECKING([whether a location for the GRIB_API library was specified])
AC_ARG_WITH([grib_api],
            [AS_HELP_STRING([--with-grib_api=<directory>],
                            [Specify location of GRIB_API library. Configure will expect to find subdirs include and lib.])],
            [GRIB_APIDIR=$with_grib_api])
if test -z "$GRIB_APIDIR" ; then
  AC_MSG_RESULT([no])
else
  AC_MSG_RESULT([$GRIB_APIDIR])
  LDFLAGS="$LDFLAGS -L$GRIB_APIDIR/lib"
  CPPFLAGS="$CPPFLAGS -I$GRIB_APIDIR/include"
  AC_CHECK_LIB(grib_api, grib_get_message, , LDFLAGS="$LDFLAGS")
# AC_SUBST([GRIB_APIDIR], [$GRIB_APIDIR])
fi

#  ----------------------------------------------------------------------
#  Compile application with HDF5 library
#
AC_MSG_CHECKING([whether a location for the HDF5 library was specified])
AC_ARG_WITH([hdf5],
            [AS_HELP_STRING([--with-hdf5=<directory>],
                            [Specify location of HDF5 library. Configure will expect to find subdirs include and lib.])],
            [HDF5DIR=$with_hdf5])
if test -z "$HDF5DIR" ; then
  AC_MSG_RESULT([no])
else
  AC_MSG_RESULT([$HDF5DIR])

  LDFLAGS="$LDFLAGS -L$HDF5DIR/lib"
  CPPFLAGS="$CPPFLAGS -I$HDF5DIR/include"
  AC_CHECK_LIB([hdf5], [H5Fopen], [], [nc_hdf5_lib_missing=yes], [-lz])
# AC_SUBST([HDF5DIR], [$HDF5DIR])
  if test "x$nc_hdf5_lib_missing" = xyes; then
    AC_MSG_ERROR([Can't find the hdf5 header and/or library, or can't link a program \
    to it. HDF5 must be built with zlib; the location of zlib must be specified for HDF5 with the \
    --with-zlib option. If HDF5 was also built with szlib, then the location of szlib must also be \
    specified with the --with-szlib option.])
  else
    LIBS="${LIBS} -lz"
# Check for the HDF5 High Level library.
    AC_CHECK_LIB([hdf5_hl], [H5DSis_scale], [], [nc_hdf5_hl_lib_missing=yes])
#    if test "x$nc_hdf5_hl_lib_missing" = xyes; then
#      AC_MSG_ERROR([Can't find the hdf5 high-level library. The high-level library is required for netCDF-4.])
#    fi
  fi
fi

#  ----------------------------------------------------------------------
#  Enable GRIB support

AC_ARG_ENABLE(grib,AS_HELP_STRING([--enable-grib],[GRIB support [[default=yes]]]),enable_grib=${enableval},enable_grib=yes)

if test "${enable_grib}" = "yes"; then
  AC_DEFINE(HAVE_LIBGRIB, [1], [Define to 1 for GRIB support])
fi

AC_MSG_CHECKING([for GRIB support])
AC_MSG_RESULT($enable_grib)

#  ----------------------------------------------------------------------
#  Compile interface with internal CGRIBEX library
#
AC_ARG_WITH([cgribex],
            [AC_HELP_STRING([--with-cgribex], [Use the GRIB library [default=yes]])],,
             with_cgribex="yes")

if test "${with_cgribex}" = "yes" ; then
  AC_DEFINE(HAVE_LIBCGRIBEX, [1], [Define to 1 for GRIB1 decoding/encoding with cgribex])
fi

AC_MSG_CHECKING([for CGRIBEX support])
AC_MSG_RESULT($with_cgribex)

#  ----------------------------------------------------------------------
#  Compile interface with internal SERVICE library
#
AC_ARG_WITH([service],
            [AC_HELP_STRING([--with-service],
                            [Use the internal SERVICE library [default=yes]])],,
             with_service="yes")

if test "${with_service}" = "yes" ; then
  AC_DEFINE(HAVE_LIBSERVICE, [1], [Define to 1 for SERVICE interface])
fi

AC_MSG_CHECKING([for SERVICE support])
AC_MSG_RESULT($with_service)

#  ----------------------------------------------------------------------
#  Create the Fortran Interface via iso_c_binding module (Fortran 2003 Standard)
#
AC_ARG_ENABLE(iso-c-interface,
              AS_HELP_STRING([--enable-iso-c-interface],
                             [Create Fortran Interface via iso_c_bindings facility of F2003 [[default=no]]]).
              ,enable_isoc=${enableval},enable_isoc=no)
AM_CONDITIONAL([CREATE_ISOC],[test $enable_isoc = 'yes'])
if test "${enable_isoc}" = "yes"; then
  AC_PROG_FC
else
  AC_SUBST([FC],$F77)
fi
#  Check the module extension of the fortran compiler
ACX_SL_FC_MOD_SUFFIX(,[FCMODEXT=mod])

# -----------------------------------
# Check for SWIG - Generator for script-language bindings
AC_ARG_ENABLE(swig,
  [AS_HELP_STRING([--enable-swig],[use swig to create extra bindings [default=no]])],
  [],[enable_swig=no])

if test "x$enable_swig" != "xyes"; then
  use_swig="no (disabled)"
else
  AC_CHECK_PROG(SWIG,swig,swig)
  if test "x$SWIG" = "x"; then
    use_swig="No (swig not available)"
  else
    use_swig="Yes"
  fi
fi
AM_CONDITIONAL(WITH_SWIG, [test "x$SWIG" != "x"])

#  ----------------------------------------------------------------------
#  Create the Ruby Interface via swig
#
AC_ARG_ENABLE(ruby,
  [AS_HELP_STRING([--enable-ruby],[ruby language bindings [default=no]])],
  [], [enable_ruby=no])

if test "x$enable_ruby" != "xyes"; then
  use_ruby="No (disabled)"
else
  AC_CHECK_PROG(RUBY,ruby,ruby)
  RUBY_INCLUDES="-I$($RUBY config/interface.rb)"
  save_CPPFLAGS=$CPPFLAGS
  CPPFLAGS="$CPPFLAGS $RUBY_INCLUDES"
  AC_CHECK_HEADER(ruby.h,,[
                           use_ruby="No (missing header)"
                           RUBY=
                           ])
  CPPFLAGS=$save_CPPFLAGS
  if test "x$RUBY" = "x"; then
    use_ruby="No (ruby not available)"
  else
    use_ruby="Yes"
    if test "x$use_swig" != "xYes"; then
      echo "Ruby bindings: Swig is disabled, pregenerated files will be used. Use '--enable-swig' for swig usage"
    else
      if test `$SWIG -help 2>&1 | $GREP -c '\-ruby *- Generate'` = 0; then
        use_ruby="No (swig does not support -ruby option)"
      fi
    fi
  fi
fi
AM_CONDITIONAL(WITH_RUBY, [test "x$use_ruby" = "xYes"])

#  ----------------------------------------------------------------------
#  Create the Ruby Interface via swig
#
AC_ARG_ENABLE(python,
  [AS_HELP_STRING([--enable-python],[python language bindings [default=no]])],
  [], [enable_python=no])

if test "x$enable_python" != "xyes"; then
  use_python="No (disabled)"
else
  AC_CHECK_PROG(PYTHON,python,python)
  PYTHON_INCLUDES="-I$($PYTHON -c 'from distutils import sysconfig;print sysconfig.get_python_lib(1,1)' | $SED 's/lib/include/')"
  save_CPPFLAGS=$CPPFLAGS
  CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
  AC_CHECK_HEADER(Python.h,,[
                             use_python="No (missing header)"
                             PYTHON=
                             ])
  CPPFLAGS=$save_CPPFLAGS
  if test "x$PYTHON" = "x"; then
    use_python="No (python not available)"
  else
    use_python="Yes"
    if test "x$use_swig" != "xYes"; then
      echo "Python bindings: Swig is disabled, pregenerated files will be used. Use '--enable-swig' for swig usage"
    else
      if test `$SWIG -help 2>&1 | $GREP -c '\-python *- Generate'` = 0; then
        use_python="No (swig does not support -python option)"
      fi
    fi
  fi
fi
AM_CONDITIONAL(WITH_PYTHON, [test "x$use_python" = "xYes"])
#  ----------------------------------------------------------------------
AM_CONDITIONAL(CREATE_INTERFACES, [test  "x$use_ruby" = "xYes" -o "x$use_python" = "xYes"])
#  ----------------------------------------------------------------------
#  Compile interface with internal EXTRA library
#
AC_ARG_WITH([extra],
            [AC_HELP_STRING([--with-extra],
                            [Use the internal EXTRA library [default=yes]])],,
             with_extra="yes")

if test "${with_extra}" = "yes" ; then
  AC_DEFINE(HAVE_LIBEXTRA, [1], [Define to 1 for EXTRA interface])
fi

AC_MSG_CHECKING([for EXTRA support])
AC_MSG_RESULT($with_extra)

#  ----------------------------------------------------------------------
#  Compile interface with internal IEG library
#
AC_ARG_WITH([ieg],
            [AC_HELP_STRING([--with-ieg],
                            [Use the internal IEG library [default=yes]])],,
             with_ieg="yes")

if test "${with_ieg}" = "yes" ; then
  AC_DEFINE(HAVE_LIBIEG, [1], [Define to 1 for IEG interface])
fi

AC_MSG_CHECKING([for IEG support])
AC_MSG_RESULT($with_ieg)

#  ----------------------------------------------------------------------
# Begin DAP (aka DODS) support
# Store pre-DAP LIBS value to restore from later
LIBS_no_DAP="${LIBS}"

AC_ARG_ENABLE(dap,AS_HELP_STRING([--enable-dap],[Build OPeNDAP/DODS-enabled clients if possible [[default=no]]]),enable_dap=${enableval},enable_dap=no)

if test "${enable_dap}" = "yes"; then
   if which ncdap-config > /dev/null; then
      CPPFLAGS="${CPPFLAGS} `ncdap-config --cflags`"
      nc_dap_ver=`ncdap-config --version`
      LIBS="`ncdap-config --libs` ${LIBS}"	
   else
      AC_MSG_ERROR(couldn't find ncdap-config)     
   fi
fi

#  ----------------------------------------------------------------------
# Compile with MPI support
AC_ARG_ENABLE(mpi,AS_HELP_STRING([--enable-mpi],[Compile with MPI compiler [[default=no]]]),enable_mpi=${enableval},enable_mpi=no)

if test "${enable_mpi}" = "yes"; then
   if test "${host_os:0:3}" = "aix"; then
      replace_cc="mpcc_r"
   else
      replace_cc="mpicc"
   fi
else
      replace_cc=$CC	
fi

#  ----------------------------------------------------------------------
#  Is the netcdf present? It has a header file `netcdf.h' and a library
#  `-lnetcdf' and their locations might be specified with the `--with-netcdf'
#  command-line switch. The value is an include path and/or a library path.
#  If the library path is specified then it must be preceded by a comma.
#
AC_ARG_WITH([netcdf],
            [AC_HELP_STRING([--with-netcdf=<directory>],
                            [Specify location of netCDF library. Configure will expect to find subdirs include and lib.])],,
            withval=no)

case $withval in
  yes)
    HAVE_NETCDF="yes"
    AC_CHECK_HEADERS(netcdf.h)
    AC_CHECK_LIB(netcdf, nc_open,, unset HAVE_NETCDF)

    if test -z "$HAVE_NETCDF" -a -n "$CONFIG_ABORT"; then
      AC_MSG_ERROR([Can't find netcdf library, or can't link a program to it.])
    fi
    ;;
  no)
    HAVE_NETCDF="no"
    AC_MSG_CHECKING([for netcdf library])
    AC_MSG_RESULT([suppressed])
    ;;
  *)
    HAVE_NETCDF="yes"
    case "$withval" in
      *,*)
        netcdf_inc="`echo $withval |cut -f1 -d,`"
        netcdf_lib="`echo $withval |cut -f2 -d, -s`"
        ;;
      *)
        if test -n "$withval"; then
          netcdf_inc="$withval/include"
          netcdf_lib="$withval/lib"
        fi
        ;;
    esac

#   Trying to include -I/usr/include and -L/usr/lib is redundant and
#   can mess some compilers up.
    if test "X$netcdf_inc" = "X/usr/include"; then
      netcdf_inc=""
    fi
    if test "X$netcdf_lib" = "X/usr/lib"; then
      netcdf_lib=""
    fi

#   Trying to include -I/usr/local/include and -L/usr/local/lib is redundant and
#   can mess some compilers up.
    if test "X$netcdf_inc" = "X/usr/local/include"; then
      netcdf_inc=""
    fi
    if test "X$netcdf_lib" = "X/usr/local/lib"; then
      netcdf_lib=""
    fi

    saved_CPPFLAGS="$CPPFLAGS"
    saved_LDFLAGS="$LDFLAGS"

    if test -n "$netcdf_inc"; then
      CPPFLAGS="$CPPFLAGS -I$netcdf_inc"
    fi

    AC_CHECK_HEADERS(netcdf.h,, CPPFLAGS="$saved_CPPFLAGS")

    if test -n "$netcdf_lib"; then
      LDFLAGS="$LDFLAGS -L$netcdf_lib"
    fi

    if test "${enable_dap}" = "yes"; then
      AC_CHECK_LIB(nc-dap, nc_open,, LDFLAGS="$saved_LDFLAGS"; unset HAVE_NETCDF)
if test $ac_cv_lib_nc_dap_nc_open = yes; then
cat >>confdefs.h <<_ACEOF
#define HAVE_LIBNETCDF 1
_ACEOF
fi
    else
      AC_CHECK_LIB(netcdf, nc_open,, LDFLAGS="$saved_LDFLAGS"; unset HAVE_NETCDF)
    fi

    if test -z "$HAVE_NETCDF" -a -n "$CONFIG_ABORT"; then
      AC_MSG_ERROR([Can't find netcdf library, or can't link a program to it.])
    fi
    ;;
esac

#  Check C / Fortran interface
UD_CHECK_CFINT([$srcdir/src/cfortran.h])

AC_SUBST([CFINT])
AC_SUBST([CPPFLAGS])
AC_SUBST([AR])

if test "${replace_cc}" != ""; then
    CC=$replace_cc
fi

AC_OUTPUT(Makefile src/Makefile interfaces/Makefile app/Makefile tests/Makefile examples/Makefile)

if test "$ac_cv_prog_cc_c99" = "no" ; then
AC_MSG_NOTICE([

  Warning: The C compiler does not accept ANSI C99 source code!
])
fi

AC_MSG_NOTICE([

  Configuration completed.

    You can now say 'make' to compile the CDI package and 'make install' to install it afterwards.
])
