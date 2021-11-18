check_PROGRAMS = \
  cdi_append \
  cdi_copy \
  cdi_read \
  cdi_write \
  cdi_write_ens \
  cdi_write_hybrid \
  cdi_write_local \
  cdi_write_relativ

if ENABLE_NETCDF
check_PROGRAMS += cdi_write_const
endif

if CREATE_ISOC
check_PROGRAMS += cdi_read_f2003 cdi_write_f2003
endif

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_FCFLAGS  = $(FPP_INCOPT)$(top_srcdir)/src $(FC_MOD_FLAG)../src

LDADD = $(top_builddir)/src/libcdi.la

cdi_append_SOURCES = cdi_append.c
if ENABLE_ALL_STATIC
cdi_append_LDFLAGS = -all-static
endif

cdi_copy_SOURCES = cdi_copy.c
if ENABLE_ALL_STATIC
cdi_copy_LDFLAGS = -all-static
endif

cdi_read_SOURCES = cdi_read.c
if ENABLE_ALL_STATIC
cdi_read_LDFLAGS = -all-static
endif

cdi_write_SOURCES = cdi_write.c
if ENABLE_ALL_STATIC
cdi_write_LDFLAGS = -all-static
endif

cdi_write_ens_SOURCES = cdi_write_ens.c

cdi_write_hybrid_SOURCES = cdi_write_hybrid.c

cdi_write_local_SOURCES = cdi_write_local.c

cdi_write_relativ_SOURCES = cdi_write_relativ.c

cdi_write_const_SOURCES = cdi_write_const.c

cdi_read_f2003_SOURCES = cdi_read_f2003.f90
if ENABLE_ALL_STATIC
cdi_read_f2003_LDFLAGS = -all-static
endif
cdi_read_f2003_LDADD = $(top_builddir)/src/libcdi_f2003.la $(top_builddir)/src/libcdi.la

cdi_write_f2003_SOURCES = cdi_write_f2003.f90
if ENABLE_ALL_STATIC
cdi_write_f2003_LDFLAGS = -all-static
endif
cdi_write_f2003_LDADD = $(top_builddir)/src/libcdi_f2003.la $(top_builddir)/src/libcdi.la

if FORTRAN_MOD_UC
cdi_read_f2003.$(OBJEXT) cdi_write_f2003.$(OBJEXT): $(top_builddir)/src/MO_CDI.$(FCMODEXT)
else
cdi_read_f2003.$(OBJEXT) cdi_write_f2003.$(OBJEXT): $(top_builddir)/src/mo_cdi.$(FCMODEXT)
endif

clean-local: clean-local-dirs
.PHONY: clean-local-dirs
clean-local-dirs:
	-rm -rf *.dSYM

EXTRA_DIST = \
  cdi_read_f2003.f90  \
  cdi_write_f2003.f90

# overrides to make regular compilations also use libtool
# static because the libtool libraries in this directory are meant to
# be statically linked and there is no use for the PIC object
COMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CC) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CFLAGS) $(CFLAGS)
FCCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static $(AM_FCFLAGS) $(FCFLAGS)
CXXCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CXX) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CXXFLAGS) $(CXXFLAGS)
