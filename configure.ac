#  Process this file with autoconf to produce a configure script.

AC_INIT([cdi], [1.4.5], [http://code.zmaw.de/projects/cdi])

echo "configuring ${PACKAGE_NAME} ${PACKAGE_VERSION}"

CONFIG_ABORT=yes
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Checks for username, hostname and system type
USERNAME=$LOGNAME
if test -z "$USERNAME" ; then USERNAME=$USER; fi;
if test -z "$USERNAME" ; then USERNAME="unknown"; fi;
AC_DEFINE_UNQUOTED([USER_NAME],["$USERNAME"], [User name])
AC_SUBST([USER_NAME],["$USERNAME"])
AS_IF([test -z "$HOST"],
      [HOST=unknown
       AS_IF([test -x /bin/hostname],[HOST=$(hostname)],
             [AS_IF([test -x /bin/uname],
                    [HOST=$(uname -n)])])])
AC_DEFINE_UNQUOTED([HOST_NAME],["$HOST"],[Host name])
AC_SUBST([HOST_NAME],["$HOST"])
AC_DEFINE_UNQUOTED([SYSTEM_TYPE],["$ac_cv_build"], [System type])
AC_SUBST([SYSTEM_TYPE],["$ac_cv_build"])

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([src/config.h])

# Set up libtool.
AC_MSG_NOTICE([setting up libtool])
LT_INIT

# Check building environment
AC_CHECK_TOOL([CC],[gcc],[:])
AC_PROG_CC_C99
AC_PROG_FC
AC_PROG_F77
AC_PROG_RANLIB
AC_PROG_CXX
AC_CHECK_PROG(SED,sed,sed,false)
AC_CHECK_PROG(GREP,grep,grep,false)
AC_PROG_INSTALL
AC_CHECK_TOOL([AR],[ar],[:])
AC_CHECK_TOOL([CPP],[cpp],[:])
AC_CHECK_TOOL([LD],[ld],[:])
AC_CHECK_TOOL([NM],[nm],[:])
AC_CHECK_TOOL([AS],[as],[:])
AC_CHECK_TOOL([DLLTOOL],[dlltool],[:])
AC_CHECK_TOOL([OBJDUMP],[objdump],[:])
AC_CHECK_TOOL([STRIP],[strip],[:])
AC_CHECK_TOOL([RANLIB],[ranlib],[:])
#  ----------------------------------------------------------------------
# Check large file support on 32 bit systems
AC_SYS_LARGEFILE
#  ----------------------------------------------------------------------
# Checks for library functions.
AC_FUNC_MMAP
#  ----------------------------------------------------------------------
# Checks for library malloc.
AC_CHECK_LIB(malloc, malloc)
#  ----------------------------------------------------------------------
# Checks for structures.
AC_CHECK_MEMBERS([struct stat.st_blksize])
#  ----------------------------------------------------------------------
# Checks for header files
AC_CHECK_HEADERS(malloc.h)
#  ----------------------------------------------------------------------
# Checks for the availability of functions
AC_CHECK_FUNCS(mallinfo)
#  ----------------------------------------------------------------------
# Checks for the availability of ANSI-C99 functions
AC_CHECK_FUNCS(isnan)
#  ----------------------------------------------------------------------
#  Check for math library
AC_CHECK_LIB(m, floor)
#  ----------------------------------------------------------------------
#  Create a pure statically linked binary
AC_ARG_ENABLE([all-static],
              [AS_HELP_STRING([--enable-all-static],
                             [Create an additional executable, which only statically linked [default=no]])],
              [enable_all_static=${enableval}],
              [enable_all_static=no])
AM_CONDITIONAL([CREATE_ALL_STATIC],[test x$enable_all_static != 'xno'])
#  ----------------------------------------------------------------------
#  Checks for pthread library
AC_ARG_WITH([threads],
            [AC_HELP_STRING([--with-threads=<directory>],
                            [Use the pthread library [default=yes]])],
            [],
            [with_threads=yes])
AS_CASE([$with_threads],
        [no],[AC_MSG_CHECKING([for pthread library])
              AC_MSG_RESULT([suppressed])],
        [yes],[AC_CHECK_HEADERS(pthread.h)
               AC_CHECK_LIB([pthread],[pthread_create])
               AC_SUBST([THREADS_LDFLAGS],[" -lpthread"])
               AC_SUBST([THREADS_INCLUDE],[""])],
        [*],[THREADS_ROOT=$with_threads
             LDFLAGS="$LDFLAGS -L$THREADS_ROOT/lib"
             CPPFLAGS="$CPPFLAGS -I$THREADS_ROOT/include"
             AC_CHECK_HEADERS(pthread.h)
             AC_CHECK_LIB([pthread],[pthread_create])
             AC_SUBST([THREADS_LDFLAGS],[" -L$THREADS_ROOT/lib -lpthread"])
             AC_SUBST([THREADS_INCLUDE],[" -I$THREADS_ROOT/include"])])
#  ----------------------------------------------------------------------
#  Link application to ZLIB library, needed for netcdf
AC_ARG_WITH([zlib],
            [AS_HELP_STRING([--with-zlib=<yes|no|directory> (default=yes)],[location of ZLIB compression library (lib and include subdirs), nec. for HDF5/NETCDF4])],
            [AS_CASE(["$with_zlib"],
                     [no],[AC_MSG_CHECKING([for ZLIB library])
                           AC_MSG_RESULT([suppressed])],
                     [yes],[AC_CHECK_HEADERS(zlib.h)
                            AC_SEARCH_LIBS([deflate],[z])
                            AC_SUBST([ZLIB_LDFLAGS],[" -lz"])],
                     [*],[ZLIB_ROOT=$with_zlib
                          LDFLAGS="$LDFLAGS -L$ZLIB_ROOT/lib"
                          CPPFLAGS="$CPPFLAGS -I$ZLIB_ROOT/include"
                          AC_CHECK_HEADERS(zlib.h)
                          AC_SEARCH_LIBS([deflate],[z])
                          AC_SUBST([ZLIB_LDFLAGS],[" -L$ZLIB_ROOT/lib -lz"])])],
                     [AC_CHECK_HEADERS(zlib.h)
                      AC_SEARCH_LIBS([deflate],[z])
                      AC_SUBST([ZLIB_LDFLAGS],[" -lz"])])
#  ----------------------------------------------------------------------
#  Compile application with SZLIB library, nedded for GRIB1 or for linking against hdf5/netcdf4
AC_ARG_WITH([szlib],
            [AS_HELP_STRING([--with-szlib=<yes|no|directory> (default=no)],[location of szlib library, optional for GRIB1 and NETCDF4 compression])],
            [AS_CASE(["$with_szlib"],
                     [no],[AC_MSG_CHECKING([for szlib library])
                           AC_MSG_RESULT([suppressed])],
                     [yes],[AC_CHECK_HEADERS(szlib.h)
                            AC_SEARCH_LIBS([SZ_BufftoBuffCompress],
                                           [sz],
                                           [AC_DEFINE([HAVE_LIBSZ],[1],[Define to 1 for SZIP support])],
                                           [AC_MSG_ERROR([Could not link to szlib])])
                            AC_SUBST([SZLIB_LDFLAGS],[" -lsz"])
                            AC_SUBST([SZLIB_INCLUDE],[""])],
                     [*],[SZLIB_ROOT=$with_szlib
                          AS_IF([test -d "$SZLIB_ROOT"],
                                [LDFLAGS="$LDFLAGS -L$SZLIB_ROOT/lib"
                                 CPPFLAGS="$CPPFLAGS -I$SZLIB_ROOT/include"
                                 AC_CHECK_HEADERS(szlib.h)
                                 AC_SEARCH_LIBS([SZ_BufftoBuffCompress],
                                                [sz],
                                                [AC_DEFINE([HAVE_SZLIB],[1],[Define to 1 for SZIP support])],
                                                [AC_MSG_ERROR([Could not link to szlib])])
                                 AC_SUBST([SZLIB_LDFLAGS],[" -L$SZLIB_ROOT/lib -lsz"])
                                 AC_SUBST([SZLIB_INCLUDE],[" -I$SZLIB_ROOT/include"])],
                                [AC_MSG_NOTICE([$SZLIB_ROOT is not a directory! SZLIB suppressed])])])],
            [AC_MSG_CHECKING([for szlib library])
             AC_MSG_RESULT([suppressed])])
#  ----------------------------------------------------------------------
#  Link application with HDF5 library, required for netcdf4
AC_ARG_WITH([hdf5],
            [AS_HELP_STRING([--with-hdf5=<yes|no|directory> (default=no)],[location of hdf5 library, NETCDF4 requires hdf5 high level interface])],
            [AS_CASE(["$with_hdf5"],
                     [no],[AC_MSG_CHECKING([for hdf5 library])
                           AC_MSG_RESULT([suppressed])],
                     [yes],[AC_CHECK_HEADERS([hdf5.h])
                            AC_SEARCH_LIBS([H5Fopen],
                                           [hdf5],
                                           [AC_DEFINE([HAVE_LIBHDF5],[1],[Define to 1 for HDF5 support])],
                                           [AC_MSG_ERROR([Cannot link to hdf5 library! It is required for Netcdf4])])
                            AC_SEARCH_LIBS([H5DSis_scale],
                                           [hdf5_hl],
                                           [have_hdf5_hl=yes],
                                           [AC_MSG_NOTICE([Cannot find hdf5 high level interface! It is required for netCDF4.])
                                            have_hdf5_hl=no])
                            AS_IF([test "x$have_libhdf5_hl" = 'xyes'],
                                  [AC_SUBST([HDF5_LDFLAGS],[" -lhdf5_hl -lhdf5"])],
                                  [AC_SUBST([HDF5_LDFLAGS],[" -lhdf5"])])
                            AC_SUBST([HDF5_INCLUDE],[""])],
                     [*],[HDF5_ROOT=$with_hdf5
                          AS_IF([test -d "$HDF5_ROOT"],
                                [LDFLAGS="$LDFLAGS -L$HDF5_ROOT/lib"
                                 CPPFLAGS="$CPPFLAGS -I$HDF5_ROOT/include"
                                 AC_CHECK_HEADERS([hdf5.h])
                                 AC_SEARCH_LIBS([H5Fopen],
                                                [hdf5],
                                                [AC_DEFINE([HAVE_LIBHDF5],[1],[Define to 1 for HDF5 support])],
                                                [AC_MSG_ERROR([Cannot link to hdf5! It is required for netCDF4.])])
                                 AC_SEARCH_LIBS([H5DSis_scale],
                                                [hdf5_hl],
                                                [have_hdf5_hl=yes],
                                                [AC_MSG_NOTICE([Cannot link to hdf5 high level interface! It is required for netCDF4.\
                                                                HDF5 must be built with zlib; the location of zlib must be specified for HDF5 with the \
                                                                --with-zlib option. If HDF5 was also built with szlib, then the location of szlib must also be \
                                                                specified with the --with-szlib option..])
                                                have_hdf5_hl=no])
                                 AS_IF([test "x$have_libhdf5_hl" = 'xyes'],
                                       [AC_SUBST([HDF5_LDFLAGS],[" -L$HDF5_ROOT/lib -lhdf5_hl -lhdf5"])],
                                       [AC_SUBST([HDF5_LDFLAGS],[" -L$HDF5_ROOT/lib -lhdf5"])])
                                 AC_SUBST([HDF5_INCLUDE],[" -I$HDF5_ROOT/include"])],
                                [AC_MSG_NOTICE([$HDF5_ROOT is not a directory! HDF5 suppressed])])])],
            [AC_MSG_CHECKING([for hdf5 library])
             AC_MSG_RESULT([suppressed])])
#  ----------------------------------------------------------------------
#  Compile application with netcdf
AC_ARG_WITH([netcdf],
            [AS_HELP_STRING([--with-netcdf=<yes|no|directory> (default=yes)],[location of netcdf library (lib and include subdirs)])],
            [AS_CASE(["$with_netcdf"],
                     [no],[AC_MSG_CHECKING([for netcdf library])
                           AC_MSG_RESULT([suppressed])],
                     [yes],[AC_CHECK_HEADERS([netcdf.h])
                            AC_SEARCH_LIBS([nc_open],
                                           [netcdf],
                                           [AC_DEFINE([HAVE_LIBNETCDF],[1],[Define to 1 for NETCDF support])],
                                           [AC_MSG_ERROR([Could not link to netcdf library])])
                            AC_SUBST([NETCDF_LDFLAGS],[" -lnetcdf"])
                            AC_SUBST([NETCDF_INCLUDE],[""])],
                     [*],[NETCDF_ROOT=$with_netcdf
                          AS_IF([test -d "$NETCDF_ROOT"],
                                [LDFLAGS="$LDFLAGS -L$NETCDF_ROOT/lib"
                                 CPPFLAGS="$CPPFLAGS -I$NETCDF_ROOT/include"
                                 AC_CHECK_HEADERS([netcdf.h])
                                 AC_SEARCH_LIBS([nc_open],
                                                [netcdf],
                                                [AC_DEFINE([HAVE_LIBNETCDF],[1],[Define to 1 for NETCDF support])],
                                                [AC_MSG_ERROR([Could not link to netcdf library])])
                                 AC_SUBST([NETCDF_LDFLAGS],[" -L$NETCDF_ROOT/lib -lnetcdf"])
                                 AC_SUBST([NETCDF_INCLUDE],[" -I$NETCDF_ROOT/include"])],
                                [AC_MSG_NOTICE([$NETCDF_ROOT is not a directory! NETCDF suppressed])])])],
            [AC_MSG_CHECKING([for NETCDF library])
             AC_MSG_RESULT([suppressed])])
#  ----------------------------------------------------------------------
#  Link application with JASPER library (needed for GRIB2 compression)
AC_ARG_WITH([jasper],
            [AS_HELP_STRING([--with-jasper=<directory>],
                            [Specify location of JASPER library. You must specify its location if GRIB_API was built with JASPER.])],
            [AS_CASE(["$with_jasper"],
                     [no],[AC_MSG_CHECKING([for jasper library])
                           AC_MSG_RESULT([suppressed])],
                     [yes],[AC_CHECK_HEADERS([jasper.h])
                            AC_SEARCH_LIBS([jas_init],[jasper],,
                                           [AC_MSG_ERROR([Could not link to jasper library! Required for GRIB_API])])
                            AC_SUBST([JASPER_LDFLAGS],[" -ljasper"])],
                     [*],[JASPER_ROOT=$with_jasper
                          AS_IF([test -d "$JASPER_ROOT"],
                                [LDFLAGS="$LDFLAGS -L$JASPER_ROOT/lib"
                                 CPPFLAGS="$CPPFLAGS -I$JASPER_ROOT/include"
                                 AC_SEARCH_LIBS([jas_init],
                                                [jasper],
                                                [],
                                                [AC_MSG_ERROR([Could not link to jasper library! Required for GRIB_API])])
                                 AC_SUBST([JASPER_LDFLAGS],[" -L$JASPER_ROOT/lib -ljasper"])],
                                [AC_MSG_ERROR([$JASPER_ROOT is not a directory! JASPER suppressed])])])],
            [AC_MSG_CHECKING([for the JASPER library])
             AC_MSG_RESULT([suppressed])])
#  ----------------------------------------------------------------------
#  Compile application with GRIB_API library (for GRIB2 support)
AC_ARG_WITH([grib_api],
            [AS_HELP_STRING([--with-jasper=<yes|no|directory>],
                            [library for grib2 compression; if a directory is given, it will be used as a value for --with-jasper-root])],
            [AS_CASE(["$with_grib_api"],
                     [no],[AC_MSG_CHECKING([for GRIB_API library])
                           AC_MSG_RESULT([suppressed])],
                     [yes],[AC_CHECK_HEADERS([grib_api.h])
                            AC_SEARCH_LIBS([grib_get_message],
                                           [grib_api],
                                           [AC_DEFINE([HAVE_LIBGRIB_API],[1],[GRIB_API library is present if defined to 1])],
                                           [AC_MSG_ERROR([Could not link to grib_api library])])],
                     [*],[GRIB_API_ROOT=$with_grib_api
                          AS_IF([test -d "$GRIB_API_ROOT"],
                                [LDFLAGS="$LDFLAGS -L$GRIB_API_ROOT/lib"
                                 CPPFLAGS="$CPPFLAGS -I$GRIB_API_ROOT/include"
                                 AC_CHECK_HEADERS([grib_api.h])
                                 AC_SEARCH_LIBS([grib_get_message],
                                                [grib_api],
                                                [AC_DEFINE([HAVE_LIBGRIB_API],[1],[GRIB_API library is present if defined to 1])],
                                                [AC_MSG_ERROR([Could not link to grib_api library])],
                                                [-lopenjpeg])
                                 AC_SUBST([GRIB_API_LDFLAGS],[" -L$GRIB_API_ROOT/lib -lgrib_api"])
                                 AC_SUBST([GRIB_API_INCLUDE],[" -I$GRIB_API_ROOT/include"])],
                                [AC_MSG_ERROR([$GRIB_API_ROOT is not a directory! GRIB_API suppressed])])])],
            [AC_MSG_CHECKING([for the GRIB_API library])
             AC_MSG_RESULT([suppressed])])
#  ----------------------------------------------------------------------
#  Enable GRIB support
AC_MSG_CHECKING([for GRIB support])
AC_ARG_ENABLE([grib],
              [AS_HELP_STRING([--enable-grib],[GRIB support [default=yes]])],
              [AS_IF([test "x$enable_grib" != 'xno'],
                     [AC_DEFINE(HAVE_LIBGRIB, [1], [Define to 1 for GRIB support])
                      enable_grib=yes])],
              [AC_DEFINE(HAVE_LIBGRIB, [1], [Define to 1 for GRIB support])
               enable_grib=yes])
AC_MSG_RESULT([$enable_grib])
AC_SUBST([ENABLE_GRIB],[$enable_grib])
#  ----------------------------------------------------------------------
#  Compile interface with internal CGRIBEX library
AC_MSG_CHECKING([for CGRIBEX support])
AC_ARG_ENABLE([cgribex],
              [AC_HELP_STRING([--enable-cgribex],[Use the CGRIBEX library [default=yes]])],
              [AS_IF([test "x$enable_cgribex" != 'xno'],
                     [AC_DEFINE(HAVE_LIBCGRIBEX,[1],[Define to 1 for GRIB1 decoding/encoding with cgribex])
                      enable_cgribex=yes])],
              [AC_DEFINE(HAVE_LIBCGRIBEX,[1],[Define to 1 for GRIB1 decoding/encoding with cgribex])
               enable_cgribex=yes])
AC_MSG_RESULT([$enable_cgribex])
AC_SUBST([ENABLE_CGRIBEX],[$enable_cgribex])
#  ----------------------------------------------------------------------
#  Compile interface with internal SERVICE library
AC_MSG_CHECKING([for SERVICE support])
AC_ARG_ENABLE([service],
              [AC_HELP_STRING([--enable-service],[Use the service library [default=yes]])],
              [AS_IF([test "x$enable_service" != 'xno'],
                     [AC_DEFINE(HAVE_LIBSERVICE,[1],[Define to 1 for SERVICE interface])
                      enable_service=yes])],
              [AC_DEFINE(HAVE_LIBSERVICE,[1],[Define to 1 for SERVICE interface])
               enable_service=yes])
AC_MSG_RESULT([$enable_service])
AC_SUBST([ENABLE_SERVICE],[$enable_service])
#  ----------------------------------------------------------------------
#  Compile interface with internal EXTRA library
AC_MSG_CHECKING([for EXTRA support])
AC_ARG_ENABLE([extra],
              [AC_HELP_STRING([--enable-extra],[Use the extra library [default=yes]])],
              [AS_IF([test "x$enable_extra" != 'xno'],
                     [AC_DEFINE(HAVE_LIBEXTRA,[1],[Define to 1 for EXTRA interface])
                      enable_extra=yes])],
              [AC_DEFINE(HAVE_LIBEXTRA,[1],[Define to 1 for EXTRA interface])
               enable_extra=yes])
AC_MSG_RESULT([$enable_extra])
AC_SUBST([ENABLE_EXTRA],[$enable_extra])
#  ----------------------------------------------------------------------
#  Compile interface with internal IEG library
AC_MSG_CHECKING([for IEG support])
AC_ARG_ENABLE([ieg],
              [AC_HELP_STRING([--enable-ieg],[Use the ieg library [default=yes]])],
              [AS_IF([test "x$enable_ieg" != 'xno'],
                     [AC_DEFINE(HAVE_LIBIEG,[1],[Define to 1 for IEG interface])
                      enable_ieg=yes])],
              [AC_DEFINE(HAVE_LIBIEG,[1],[Define to 1 for IEG interface])
               enable_ieg=yes])
AC_MSG_RESULT([$enable_ieg])
AC_SUBST([ENABLE_IEG],[$enable_ieg])
#  ----------------------------------------------------------------------
# Compile with MPI support
AC_ARG_ENABLE(mpi,AS_HELP_STRING([--enable-mpi],[Compile with MPI compiler [default=no]]),enable_mpi=${enableval},enable_mpi=no)
AS_IF([test "${enable_mpi}" = "yes"],
      [AS_IF([test "${host_os:0:3}" = "aix"],
             [replace_cc="mpcc_r"],
             [replace_cc="mpicc"])],
      [replace_cc=$CC])
#  ----------------------------------------------------------------------
#  Create the Fortran Interface via iso_c_binding module (Fortran 2003 Standard)
AC_ARG_ENABLE(iso-c-interface,
              AS_HELP_STRING([--enable-iso-c-interface],
                             [Create Fortran Interface via iso_c_bindings facility of F2003 [default=no]]).
              ,enable_isoc=${enableval},enable_isoc=no)
AM_CONDITIONAL([CREATE_ISOC],[test $enable_isoc = 'yes'])
AS_IF([test "x${enable_isoc}" != "xyes"],[AC_SUBST([FC],$F77)])
#  Check the module extension of the fortran compiler
ACX_SL_FC_MOD_SUFFIX(,[FCMODEXT=mod])
# -----------------------------------------------------------------------
# Check for SWIG - Generator for script-language bindings
AC_ARG_ENABLE(swig,
              [AS_HELP_STRING([--enable-swig],[use swig to create extra bindings [default=no] (EXPERIMENTAL)])],
              [AC_CHECK_PROG(SWIG,swig,swig)],
              [enable_swig=no])
AM_CONDITIONAL(WITH_SWIG,[test "x$SWIG" != "x"])
#  ----------------------------------------------------------------------
#  Create the Ruby Interface via swig
AC_ARG_ENABLE(ruby,
  [AS_HELP_STRING([--enable-ruby],[ruby language bindings [default=no] (EXPERIMENTAL)])],
  [AC_CHECK_PROG(RUBY,ruby,ruby)
   RUBY_INCLUDES="-I$($RUBY config/interface.rb)"
   save_CPPFLAGS=$CPPFLAGS
   CPPFLAGS="$CPPFLAGS $RUBY_INCLUDES"
   AC_CHECK_HEADER([ruby.h],,[
                            enable_ruby=no
                            RUBY=
                           ])
   CPPFLAGS=$save_CPPFLAGS
   AS_IF([test "x$RUBY" != "x"],
         [AS_IF([test "x$SWIG" = "x"],
                [AC_MSG_NOTICE([Python bindings: Swig is disabled, pregenerated files will be used. Use '--enable-swig' for swig usage])])],
                [AS_IF([test `$SWIG -help 2>&1 | $GREP -c '\-ruby *- Generate'` = 0],[AC_MSG_NOTICE([No (swig does not support -ruby option)])
                                                                                        enable_ruby=no
                                                                                       ])])],
  [enable_ruby=no])
AM_CONDITIONAL(WITH_RUBY,[test "x$enable_ruby" != "xno"])
#  ----------------------------------------------------------------------
#  Create the Ruby Interface via swig
AC_ARG_ENABLE(python,
  [AS_HELP_STRING([--enable-python],[python language bindings [default=no] (EXPERIMENTAL)])],
  [AC_CHECK_PROG(PYTHON,python,python)
   PYTHON_INCLUDES="-I$($PYTHON -c 'from distutils import sysconfig;print sysconfig.get_python_lib(1,1)' | $SED 's/lib/include/')"
   save_CPPFLAGS=$CPPFLAGS
   CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
   AC_CHECK_HEADER([Python.h],,[
                             enable_python="No (missing header)"
                             PYTHON=
                             ])
   CPPFLAGS=$save_CPPFLAGS
   AS_IF([test "x$PYTHON" != "x"],
         [AS_IF([test "x$SWIG" = "x"],
                [AC_MSG_NOTICE([Python bindings: Swig is disabled, pregenerated files will be used. Use '--enable-swig' for swig usage])])],
                [AS_IF([test `$SWIG -help 2>&1 | $GREP -c '\-python *- Generate'` = 0],[AC_MSG_NOTICE([No (swig does not support -python option)])
                                                                                        enable_python=no
                                                                                       ])])],
  [enable_python=no])
AM_CONDITIONAL(WITH_PYTHON, [test "x$enable_python" = "xyes"])
#  ----------------------------------------------------------------------
AM_CONDITIONAL(CREATE_INTERFACES, [test  "x$enable_ruby" = "xyes" -o "x$enable_python" = "xyes"])
#  ----------------------------------------------------------------------
#  Check C / Fortran interface
ACX_CHECK_CFINT([$srcdir/src/cfortran.h])

AC_SUBST([CPPFLAGS])

AS_IF([test "x${replace_cc}" != "x"],[CC=$replace_cc])

AC_OUTPUT(Makefile src/Makefile interfaces/Makefile app/Makefile tests/Makefile examples/Makefile config.settings)

#  ----------------------------------------------------------------------
#  Show configuration
AS_ECHO(["#======================================================================="])
AC_MSG_NOTICE([CDI is configured with the following options:])
cat config.settings
AS_ECHO(["#======================================================================="])

AS_IF([test "$ac_cv_prog_cc_c99" = "no"],
      [AC_MSG_NOTICE([

  Warning: The C compiler does not accept ANSI C99 source code!
])])
AC_MSG_NOTICE([

  Configuration completed.

    You can now say 'make' to compile the CDI package and 'make install' to install it afterwards.
])
